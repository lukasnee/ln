# Copyright (c)  2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
# SPDX-License-Identifier: GPL-2.0-only

"""Decoding of the ln/prof ITM byte stream into profile packets.

The target side lives in prof.cpp and shall be kept in sync."""

import logging
import struct
from collections import namedtuple
from dataclasses import dataclass, field
from enum import IntEnum

logger = logging.getLogger(__name__)


class ContextId(IntEnum):
    SCHEDULER = 1
    INTERRUPT = 2
    FIRST_THREAD = 3
    # subsequent threads...


class Kind(IntEnum):
    CONTEXT_SWITCH = 0
    FN_ENTER = 1
    FN_EXIT = 2


# ITM source packet header: [7:3] stimulus port, [2] 0 = instrumentation packet
# (1 = DWT hardware source), [1:0] payload size code.
ITM_PAYLOAD_SIZES = {1: 1, 2: 2, 3: 4}
ITM_OVERFLOW_HEADER = 0x70
# Consecutive well-formed packets required to trust a position after a gap.
ITM_RESYNC_LOCK_PACKETS = 3


class Port(IntEnum):
    """Ports used by ln/prof, as offsets from LN_PROF_ITM_BASE_PORT, see prof.cpp."""

    FN_ENTER_THREAD = 0
    FN_ENTER_ISR = 1
    FN_EXIT_THREAD = 2
    FN_EXIT_ISR = 3
    SWITCH_IN = 4
    SWITCH_OUT = 5
    TIME = 6
    NAME = 7
    NAME_MORE = 8


PORT_COUNT = len(Port)

FN_ADDR_BITS = 18
CONTEXT_ID_BITS = 7

ItmItem = namedtuple("ItmItem", ["is_loss", "offset", "port", "payload", "skipped"])


def itm_packet_end(data, pos, ports):
    """End offset of an ITM source packet on one of `ports` at `pos`, None if it is not one.

    The end can lie past the data when the packet is cut off."""
    if pos >= len(data):
        return None
    header = data[pos]
    size = ITM_PAYLOAD_SIZES.get(header & 0x07)
    if size is None or header >> 3 not in ports:
        return None
    return pos + 1 + size


def itm_locks_on(data, pos, ports):
    """True if well-formed packets on `ports` chain from `pos` up to the lock count or the end."""
    for _ in range(ITM_RESYNC_LOCK_PACKETS):
        if pos == len(data):
            return True
        pos = itm_packet_end(data, pos, ports)
        if pos is None or pos > len(data):
            return False
    return True


def itm_parse(data, ports):
    """Yield the payload of every ITM packet on `ports`, a port number or a collection.

    Where bytes had to be skipped, or the ITM reported an overflow, a loss item is
    yielded in place. The stream shall carry no other ports."""
    ports = frozenset([ports]) if isinstance(ports, int) else frozenset(ports)
    pos = 0
    while pos < len(data):
        end = itm_packet_end(data, pos, ports)
        if end is not None:
            if end > len(data):
                break  # cut off by the end of the capture
            yield ItmItem(False, pos, data[pos] >> 3, data[pos + 1 : end], 0)
            pos = end
            continue
        if data[pos] == ITM_OVERFLOW_HEADER:
            yield ItmItem(True, pos, None, b"", 1)
            pos += 1
            continue
        if data[pos] == 0:
            # synchronization packet: at least 5 zero bytes and 0x80
            zeros_end = pos
            while zeros_end < len(data) and data[zeros_end] == 0:
                zeros_end += 1
            if (
                zeros_end < len(data)
                and data[zeros_end] == 0x80
                and zeros_end - pos >= 5
            ):
                pos = zeros_end + 1
                continue
        lost_from = pos
        pos += 1
        while pos < len(data) and not itm_locks_on(data, pos, ports):
            pos += 1
        # a short tail is just a packet cut off by the end of the capture
        if pos < len(data) or len(data) - lost_from > 5:
            yield ItmItem(True, lost_from, None, b"", pos - lost_from)


class ProfilePacket:

    def __init__(self, cycle_cnt, context_id, kind, fn):
        self.cycle_cnt = cycle_cnt
        self.context_id = context_id
        self.kind = kind
        self.fn = fn

    @property
    def speedscope_event_type(self):
        return "C" if self.kind == Kind.FN_EXIT else "O"

    def __str__(self):
        fn = f"0x{self.fn:08x}" if self.kind != Kind.CONTEXT_SWITCH else "-"
        return (
            f"ProfilePacket(kind={self.kind.name}, "
            f"context_id=0x{self.context_id:02x}, fn={fn}, "
            f"cycle_cnt={self.cycle_cnt})"
        )


@dataclass
class DecodeResult:
    packets: list = field(default_factory=list)
    # context id -> name bytes, NUL terminated
    context_names: dict = field(default_factory=dict)
    # indices into `packets` before which events were lost
    loss_points: set = field(default_factory=set)
    skipped_bytes: int = 0
    gaps: int = 0


class _RecordDecoder:

    def __init__(self, itm_base_port, fw_base, fw_size, clk_freq):
        self._itm_base_port = itm_base_port
        self._fw_base = fw_base
        self._fn_high_bits = fw_base & ~((1 << (FN_ADDR_BITS + 1)) - 1)
        self._fw_size = fw_size
        self._clk_freq = clk_freq
        self.result = DecodeResult()
        self._cycle_cnt = 0
        self._running_context_id = None
        self._open_fns = {}
        self._last_context_id = None
        self._loss_pending = False
        self._loss_context_id = None

    def mark_loss(self):
        # a loss before the first packet has no call stack to break
        if self.result.packets:
            self.result.loss_points.add(len(self.result.packets))
        if not self._loss_pending:
            self._loss_pending = True
            self._loss_context_id = self._last_context_id

    def _begin_record(self, context_id):
        """Forget the open functions a loss made untrustworthy, as the report does.

        A lost record most likely belongs to a context next to the gap."""
        if self._loss_pending:
            self._open_fns.pop(self._loss_context_id, None)
            self._open_fns.pop(context_id, None)
            self._loss_pending = False
        self._last_context_id = context_id

    @staticmethod
    def _value(item):
        return int.from_bytes(item.payload, "little")

    def _word(self, items, i, port_offset):
        """The 4-byte payload of items[i] if it is on that port, None if it is missing, a loss or another size."""
        if (
            i >= len(items)
            or items[i].is_loss
            or items[i].port != self._itm_base_port + port_offset
            or len(items[i].payload) != 4
        ):
            return None
        return struct.unpack("<I", items[i].payload)[0]

    def decode(self, items, i):
        """Decode the record at items[i], return (items consumed, failure reason)."""
        item = items[i]
        port_offset = item.port - self._itm_base_port
        if port_offset in (Port.FN_ENTER_THREAD, Port.FN_ENTER_ISR):
            return self._decode_fn_enter(item, port_offset == Port.FN_ENTER_ISR)
        if port_offset in (Port.FN_EXIT_THREAD, Port.FN_EXIT_ISR):
            return self._decode_exit(item, port_offset == Port.FN_EXIT_ISR)
        if port_offset == Port.SWITCH_IN:
            value = self._value(item)
            return self._decode_switch(
                self._cycle_cnt + (value >> CONTEXT_ID_BITS),
                value & ((1 << CONTEXT_ID_BITS) - 1),
            )
        if port_offset == Port.SWITCH_OUT:
            return self._decode_switch(
                self._cycle_cnt + self._value(item), ContextId.SCHEDULER
            )
        if port_offset == Port.TIME:
            return self._decode_time(item)
        if port_offset == Port.NAME:
            return self._decode_context_name(items, i)
        return 0, f"unexpected record start on port {port_offset}"

    def _decode_context_name(self, items, i):
        raw0 = self._word(items, i, Port.NAME)
        if raw0 is None:
            return 0, f"unexpected ITM payload size {len(items[i].payload)}"
        context_id = raw0 & 0x7F
        name_bytes = bytearray((raw0 >> 8).to_bytes(3, "little"))
        consumed = 1
        while b"\0" not in name_bytes and len(name_bytes) < 64:
            if i + consumed >= len(items):
                return len(items) - i, None  # the capture ends inside the name
            word = self._word(items, i + consumed, Port.NAME_MORE)
            if word is None:
                return 0, "context name cut short"
            name_bytes.extend(word.to_bytes(4, "little"))
            consumed += 1
        if b"\0" not in name_bytes:
            return 0, "context name not terminated"
        self.result.context_names[context_id] = name_bytes
        return consumed, None

    def _cycle_cnt_error(self, cycle_cnt):
        last = self._cycle_cnt
        if (cycle_cnt > self._clk_freq / 10) and not (
            last <= cycle_cnt <= last + self._clk_freq / 10
        ):
            return f"packet cycle count {cycle_cnt} is out of expected range ({last}, {last + self._clk_freq /10})"
        return None

    def _advance(self, cycle_cnt, what):
        time_step_us = (cycle_cnt - self._cycle_cnt) / self._clk_freq * 1e6
        logger.log(logging.DEBUG, f"Parsed {what} after {time_step_us:.3f} us")
        self._cycle_cnt = cycle_cnt

    def _track(self, packet):
        """Follow the running context and the open functions through an event packet."""
        if packet.context_id != ContextId.INTERRUPT:
            self._running_context_id = packet.context_id
        if packet.kind == Kind.FN_ENTER:
            self._open_fns.setdefault(packet.context_id, []).append(packet.fn)

    def _context_of(self, is_isr):
        """The context a thread mode or interrupt event belongs to, None if unknown."""
        if is_isr:
            return ContextId.INTERRUPT
        if self._running_context_id == ContextId.SCHEDULER:
            return None  # no thread mode code runs between two tasks
        return self._running_context_id

    def _accept(self, packet):
        error = self._cycle_cnt_error(packet.cycle_cnt)
        if error:
            return 0, error
        self._begin_record(packet.context_id)
        self._advance(packet.cycle_cnt, packet)
        self._track(packet)
        self.result.packets.append(packet)
        return 1, None

    def _decode_fn_enter(self, item, is_isr):
        if len(item.payload) != 4:
            return 0, f"unexpected ITM payload size {len(item.payload)}"
        context_id = self._context_of(is_isr)
        if context_id is None:
            return 0, "function enter while no thread is known to run"
        word = self._value(item)
        fn = self._fn_high_bits | ((word & ((1 << FN_ADDR_BITS) - 1)) << 1)
        if not (self._fw_base <= fn < self._fw_base + self._fw_size):
            return 0, f"packet fn not within firmware memory region, {fn:08x}"
        return self._accept(
            ProfilePacket(
                self._cycle_cnt + (word >> FN_ADDR_BITS),
                context_id,
                Kind.FN_ENTER,
                fn,
            )
        )

    def _decode_switch(self, cycle_cnt, context_id):
        return self._accept(
            ProfilePacket(cycle_cnt, context_id, Kind.CONTEXT_SWITCH, 0)
        )

    def _decode_time(self, item):
        cycle_cnt = self._cycle_cnt + self._value(item)
        error = self._cycle_cnt_error(cycle_cnt)
        if error:
            return 0, error
        self._advance(cycle_cnt, "time")
        return 1, None

    def _decode_exit(self, item, is_isr):
        context_id = self._context_of(is_isr)
        if context_id is None:
            return 0, "function exit while no thread is known to run"
        cycle_cnt = self._cycle_cnt + self._value(item)
        error = self._cycle_cnt_error(cycle_cnt)
        if error:
            return 0, error

        self._begin_record(context_id)
        open_fns = self._open_fns.get(context_id)
        if not open_fns:
            # entered before the capture or lost, there is nothing to close
            self._advance(cycle_cnt, "unmatched function exit")
            return 1, None
        packet = ProfilePacket(cycle_cnt, context_id, Kind.FN_EXIT, open_fns.pop())
        self._advance(cycle_cnt, packet)
        self.result.packets.append(packet)
        return 1, None


def decode_stream(data, itm_base_port, fw_base, fw_size, clk_freq):
    """Decode a captured ITM byte stream into profile packets and context names.

    `itm_base_port` must match LN_PROF_ITM_BASE_PORT on the target, ln/prof uses
    PORT_COUNT consecutive ports from it.
    """

    decoder = _RecordDecoder(itm_base_port, fw_base, fw_size, clk_freq)
    result = decoder.result
    items = list(itm_parse(data, range(itm_base_port, itm_base_port + PORT_COUNT)))
    in_bad_run = False
    i = 0
    while i < len(items):
        item = items[i]
        if item.is_loss:
            result.skipped_bytes += item.skipped
            result.gaps += 1
            in_bad_run = False
            decoder.mark_loss()
            i += 1
            continue
        consumed, reason = decoder.decode(items, i)
        if consumed:
            in_bad_run = False
            i += consumed
            continue
        # one warning per run of undecodable packets
        if not in_bad_run:
            logger.warning(f"Undecodable record at byte {item.offset}: {reason}")
            decoder.mark_loss()
            in_bad_run = True
        i += 1
    if result.gaps:
        logger.warning(
            f"ITM: {result.skipped_bytes} of {len(data)} bytes skipped in {result.gaps} gaps"
        )
    return result
