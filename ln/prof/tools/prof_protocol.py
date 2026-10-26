# Copyright (c)  2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
# SPDX-License-Identifier: GPL-2.0-only

"""Decoding of the ln/prof ITM byte stream into profile packets.

The target side lives in ln/prof/src/prof.cpp and shall be kept in sync."""

import logging
import struct
from collections import namedtuple
from dataclasses import dataclass, field
from enum import IntEnum

logger = logging.getLogger(__name__)


class PacketId(IntEnum):
    SEND_PROFILE_EVENT = 0xA5
    DISCLOSE_CONTEXT_NAME = 0xA6


class ContextId(IntEnum):
    SCHEDULER = 1
    INTERRUPT = 2
    FIRST_TASK = 3
    # subsequent tasks


class State(IntEnum):
    ENTER = 0
    EXIT = 1


class Type(IntEnum):
    THREAD = 0
    FN = 1


NAME_MAX_LEN = 64  # sanity cap for corrupted streams

# ITM source packet header: [7:3] stimulus port, [2] 0, [1:0] payload size code.
ITM_PAYLOAD_SIZES = {1: 1, 2: 2, 3: 4}
ITM_OVERFLOW_HEADER = 0x70
# Consecutive well-formed packets required to trust a position after a gap.
ITM_RESYNC_LOCK_PACKETS = 3

ItmItem = namedtuple("ItmItem", ["is_loss", "offset", "payload", "skipped"])


def itm_packet_end(data, pos, port):
    """End offset of an ITM source packet on `port` at `pos`, None if it is not one.

    The end can lie past the data when the packet is cut off."""
    if pos >= len(data):
        return None
    header = data[pos]
    size = ITM_PAYLOAD_SIZES.get(header & 0x07)
    if size is None or header >> 3 != port:
        return None
    return pos + 1 + size


def itm_locks_on(data, pos, port):
    """True if well-formed packets on `port` chain from `pos` up to the lock count or the end."""
    for _ in range(ITM_RESYNC_LOCK_PACKETS):
        if pos == len(data):
            return True
        pos = itm_packet_end(data, pos, port)
        if pos is None or pos > len(data):
            return False
    return True


def itm_parse(data, port):
    """Yield the payload of every ITM packet on `port`.

    Where bytes had to be skipped, or the ITM reported an overflow, a loss item is
    yielded in place. The stream shall carry no other ports."""
    pos = 0
    while pos < len(data):
        end = itm_packet_end(data, pos, port)
        if end is not None:
            if end > len(data):
                break  # cut off by the end of the capture
            yield ItmItem(False, pos, data[pos + 1 : end], 0)
            pos = end
            continue
        if data[pos] == ITM_OVERFLOW_HEADER:
            yield ItmItem(True, pos, b"", 1)
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
        while pos < len(data) and not itm_locks_on(data, pos, port):
            pos += 1
        # a short tail is just a packet cut off by the end of the capture
        if pos < len(data) or len(data) - lost_from > 5:
            yield ItmItem(True, lost_from, b"", pos - lost_from)


class ProfilePacket:

    def __init__(self, data, fn_msb, last_packet_cycle_cnt):
        if len(data) != 8:
            raise ValueError("Data must be exactly 8 bytes long")

        # Unpack the two 32-bit integers
        raw0, raw1 = struct.unpack("<II", data)  # Little-endian format

        # Decode the bitfields from the first 32-bit integer
        self.sync_byte = raw0 & 0xFF
        self.cycle_cnt = last_packet_cycle_cnt + ((raw0 >> 8) & 0xFFFFFF)
        self.context_id = raw1 & 0x7F
        self.state = State((raw1 >> 7) & 0x1)
        self.type = Type((raw1 >> 8) & 0x1)
        self.fn = (fn_msb << 24) | (((raw1 >> 9) & 0x7FFFFF) << 1)

    @property
    def speedscope_event_type(self):
        return "C" if self.state == State.EXIT else "O"

    def __str__(self):
        fn = f"0x{self.fn:08x}" if self.type == Type.FN else "-"
        return (
            f"ProfilePacket(type={self.type.name}, state={self.state.name}, "
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

    def __init__(self, fw_base, fw_size, clk_freq):
        self._fw_base = fw_base
        self._fw_size = fw_size
        self._clk_freq = clk_freq
        self.result = DecodeResult()

    def mark_loss(self):
        # a loss before the first packet has no call stack to break
        if self.result.packets:
            self.result.loss_points.add(len(self.result.packets))

    @staticmethod
    def _payload_word(items, i):
        """The 4-byte payload of items[i], None if it is missing, a loss or another size."""
        if i >= len(items) or items[i].is_loss or len(items[i].payload) != 4:
            return None
        return items[i].payload

    def decode(self, items, i):
        """Decode the record at items[i], return (items consumed, failure reason)."""
        first_word = self._payload_word(items, i)
        if first_word is None:
            return 0, f"unexpected ITM payload size {len(items[i].payload)}"
        try:
            packet_id = PacketId(first_word[0])
        except ValueError:
            return 0, f"unexpected packet ID {first_word[0]:02x}"
        if packet_id == PacketId.DISCLOSE_CONTEXT_NAME:
            return self._decode_context_name(items, i, first_word)
        return self._decode_profile_event(items, i, first_word)

    def _decode_context_name(self, items, i, first_word):
        # The header carries the first two name bytes, further words follow up to the NUL.
        raw0 = struct.unpack("<I", first_word)[0]
        context_id = (raw0 >> 8) & 0x7F
        name_bytes = bytearray((raw0 >> 16).to_bytes(2, "little"))
        consumed = 1
        while b"\0" not in name_bytes and len(name_bytes) < NAME_MAX_LEN:
            if i + consumed >= len(items):
                return len(items) - i, None  # the capture ends inside the name
            word = self._payload_word(items, i + consumed)
            if word is None:
                return 0, "context name cut short"
            name_bytes.extend(word)
            consumed += 1
        if b"\0" not in name_bytes:
            return 0, "context name not terminated"
        self.result.context_names[context_id] = name_bytes
        return consumed, None

    def _decode_profile_event(self, items, i, first_word):
        packets = self.result.packets
        last_packet_cycle_cnt = packets[-1].cycle_cnt if packets else 0
        if i + 1 >= len(items):
            return 1, None  # the capture ends inside the event
        second_word = self._payload_word(items, i + 1)
        if second_word is None:
            return 0, "profile event cut short"
        packet = ProfilePacket(
            first_word + second_word,
            fn_msb=(self._fw_base >> 24) & 0xFF,
            last_packet_cycle_cnt=last_packet_cycle_cnt,
        )

        if (packet.cycle_cnt > self._clk_freq / 10) and not (
            last_packet_cycle_cnt
            <= packet.cycle_cnt
            <= last_packet_cycle_cnt + self._clk_freq / 10
        ):
            return (
                0,
                f"packet cycle count {packet.cycle_cnt} is out of expected range ({last_packet_cycle_cnt}, {last_packet_cycle_cnt + self._clk_freq /10})",
            )

        if packet.type == Type.FN and not (
            self._fw_base <= packet.fn < self._fw_base + self._fw_size
        ):
            return 0, f"packet fn not within firmware memory region, {packet.fn:08x}"

        time_step_us = (packet.cycle_cnt - last_packet_cycle_cnt) / self._clk_freq * 1e6
        logger.log(logging.DEBUG, f"Parsed {packet} after {time_step_us:.3f} us")
        packets.append(packet)
        return 2, None


def decode_stream(data, port, fw_base, fw_size, clk_freq):
    """Decode a captured ITM byte stream into profile packets and context names."""
    decoder = _RecordDecoder(fw_base, fw_size, clk_freq)
    result = decoder.result
    items = list(itm_parse(data, port))
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
