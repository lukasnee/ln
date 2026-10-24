# Copyright (c)  2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
# SPDX-License-Identifier: GPL-2.0-only

__version__ = "0.1.0"

import sys
import os
import time
import json
from collections import namedtuple
from enum import IntEnum
import argparse
import re
import logging
import struct

logger = logging.getLogger(__name__)

Symbol = namedtuple("Symbol", ["address", "typ", "fn_name", "file", "line"])
NM_SYMBOLS_REGEX_PATTERN = r"(?P<address>[0-9a-f]+)\s(?P<typ>\w)\s(?P<fn_name>[^\t\n]*)(\t(?P<file>.*):(?P<line>\d+))?"


def context_is_interrupt(context_id):
    return context_id == 0


class PacketId(IntEnum):
    SEND_PROFILE_EVENT = 0xA5
    DISCLOSE_CONTEXT_NAME = 0xA6


NAME_MAX_LEN = 64  # sanity cap for corrupted streams
SWITCHED_OUT_FRAME_NAME = "[switched out]"


class State(IntEnum):
    ENTER = 0
    EXIT = 1


class Type(IntEnum):
    TASK = 0
    FN = 1


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


class UcProf:

    def __init__(self, args):
        self.args = args
        self._profile_packets = []
        # TODO the following variables should not be class variables
        self._begin_cycle_cnt_offset = None
        self._last_cycle_cnt = 0
        self._context_names = {}
        logging.basicConfig(level=args.log_level, format="%(levelname)s| %(message)s")

    def __reload_nm_symbols(self):
        if not self.args.nm_symbols_path:
            logger.error(f"Symbol file path not provided")
            return False
        if not os.path.isfile(self.args.nm_symbols_path):
            logger.error(f"Symbol file does not exist: {self.args.nm_symbols_path}")
            return False
        self._symbols = []
        with open(self.args.nm_symbols_path, "r") as file:
            for line in file:
                match = re.match(NM_SYMBOLS_REGEX_PATTERN, line)
                if match:
                    symbol = Symbol(
                        int(match.group("address"), 16),
                        match.group("typ"),
                        match.group("fn_name"),
                        match.group("file") if match.group("file") else "",
                        int(match.group("line")) if match.group("line") else 0,
                    )
                    if symbol.typ in ["a", "A", "b", "B", "d", "D", "r", "R", "V"]:
                        continue  # skip symbols that are not functions
                    index = 0
                    while (
                        index < len(self._symbols)
                        and symbol.address > self._symbols[index].address
                    ):
                        index += 1
                    self._symbols.insert(index, symbol)
        logger.info(f"Symbols parsed: {len(self._symbols)}")
        if len(self._symbols) == 0:
            logger.info(
                f"No valid symbols found in the file {self.args.nm_symbols_path}"
            )
            return False
        return True

    def __reload_packets_from_file(self):
        if not self.args.frame_data_path:
            logger.error(f"Frame data file path not provided")
            return False
        if not os.path.isfile(self.args.frame_data_path):
            logger.error(f"Frame data file does not exist: {self.args.frame_data_path}")
            return False
        self._profile_packets = []
        with open(self.args.frame_data_path, "rb") as file:
            while True:
                record_start = file.tell()
                try:
                    first_word = self.__read_itm_word(file)
                    if first_word is None:
                        logger.log(logging.WARNING, "Unexpected ITM header")
                        file.seek(record_start + 1)
                        continue

                    try:
                        packet_id = PacketId(first_word[0])
                    except ValueError:
                        logger.log(
                            logging.WARNING,
                            f"Unexpected packet ID: {first_word[0]:02x}",
                        )
                        file.seek(record_start + 1)
                        continue

                    if packet_id == PacketId.DISCLOSE_CONTEXT_NAME:
                        if self.__parse_packet_DISCLOSE_CONTEXT_NAME(file, first_word):
                            continue
                    elif packet_id == PacketId.SEND_PROFILE_EVENT:
                        if self.__parse_packet_SEND_PROFILE_EVENT(file, first_word):
                            continue
                    file.seek(record_start + 1)
                except EOFError:
                    logger.info("Reached end of file")
                    break
        if len(self._profile_packets) == 0:
            logger.info(
                f"No valid packets found in the file {self.args.frame_data_path}"
            )
            return False
        logger.info(f"Profile packets parsed: {len(self._profile_packets)}")
        logger.info(f"Context names disclosed: {len(self._context_names)}")
        return True

    @staticmethod
    def __read_itm_word(file):
        """Return the next 4-byte ITM payload, or None if its ITM header is bad."""
        header = file.read(1)
        payload = file.read(4)
        if not header or len(payload) != 4:
            raise EOFError
        return payload if header[0] == 0x03 else None

    def __parse_packet_DISCLOSE_CONTEXT_NAME(self, file, first_word):
        # The header carries the first two name bytes, further words follow up to the NUL.
        raw0 = struct.unpack("<I", first_word)[0]
        context_id = (raw0 >> 8) & 0x7F
        name_bytes = bytearray((raw0 >> 16).to_bytes(2, "little"))
        while b"\0" not in name_bytes and len(name_bytes) < NAME_MAX_LEN:
            word = self.__read_itm_word(file)
            if word is None:
                return False
            name_bytes.extend(word)
        if b"\0" not in name_bytes:
            return False
        self._context_names[context_id] = name_bytes
        return True

    def __parse_packet_SEND_PROFILE_EVENT(self, file, first_word):
        last_packet_cycle_cnt = (
            self._profile_packets[-1].cycle_cnt if self._profile_packets else 0
        )
        second_word = self.__read_itm_word(file)
        if second_word is None:
            return False
        packet = ProfilePacket(
            first_word + second_word,
            fn_msb=(self.args.fw_base >> 24) & 0xFF,
            last_packet_cycle_cnt=last_packet_cycle_cnt,
        )

        if (packet.cycle_cnt > self.args.clk_freq / 10) and not (
            last_packet_cycle_cnt
            <= packet.cycle_cnt
            <= last_packet_cycle_cnt + self.args.clk_freq / 10
        ):
            logger.log(
                logging.WARNING,
                f"Packet cycle count {packet.cycle_cnt} is out of expected range ({last_packet_cycle_cnt}, {last_packet_cycle_cnt + self.args.clk_freq /10})",
            )
            return False

        if packet.type == Type.FN and not self.__within_fw_boundry(packet.fn):
            logger.log(
                logging.WARNING,
                f"Packet fn not within firmware memory region, {packet.fn:08x}",
            )
            return False

        time_step_us = (
            (packet.cycle_cnt - last_packet_cycle_cnt) / self.args.clk_freq * 1e6
        )
        logger.log(logging.DEBUG, f"Parsed {packet} after {time_step_us:.3f} us")
        self._profile_packets.append(packet)
        return True

    def __within_begin_boundry(self, timestamp):
        return self.args.begin is None or timestamp >= self.args.begin

    def __within_end_boundry(self, timestamp):
        return self.args.end is None or timestamp <= self.args.end

    def __within_fw_boundry(self, addr):
        return self.args.fw_base <= addr < self.args.fw_base + self.args.fw_size

    def __calculate_timestamp(self, cycle_cnt):

        # offset the begin time
        if self._begin_cycle_cnt_offset is None:
            self._begin_cycle_cnt_offset = cycle_cnt

        # handle overflow
        if cycle_cnt < self._last_cycle_cnt:
            cycle_cnt = cycle_cnt + 2**32
        self._last_cycle_cnt = cycle_cnt

        return (cycle_cnt - self._begin_cycle_cnt_offset) / self.args.clk_freq

    def __context_name(self, context_id):
        raw = self._context_names.get(context_id)
        name = bytes(raw).split(b"\0")[0].decode("ascii", "replace") if raw else ""
        return name or f"id={context_id}"

    def __parse_packets_into_speedscope_dict(self):
        # profile is a list of events of a particular thread
        profiles = []
        profiles_by_context_id = {}
        shared_frames_registry = {}
        shared_frames = []
        for idx, packet in enumerate(self._profile_packets):

            # if context_is_interrupt(packet.context_id):
            #     logger.log(
            #         logging.DEBUG-2, f'__parse_packets_into_speedscope_dict: {idx} packet skipped - interrupt')
            #     continue

            timestamp = self.__calculate_timestamp(packet.cycle_cnt)
            if not self.__within_begin_boundry(timestamp):
                logger.log(
                    logging.DEBUG - 4,
                    f"__parse_packets_into_speedscope_dict: {idx} packet skipped - before --begin",
                )
                continue

            if not self.__within_end_boundry(timestamp):
                logger.log(
                    logging.DEBUG - 3,
                    f"__parse_packets_into_speedscope_dict: {idx} packet skipped - after --end",
                )
                break

            if packet.type == Type.TASK:
                symbol = Symbol(0, "U", SWITCHED_OUT_FRAME_NAME, "", 0)
            else:
                symbol = next(
                    (s for s in reversed(self._symbols) if packet.fn >= s.address),
                    None,
                )

            if not symbol:
                symbol = Symbol(0, "U", "fn @ 0x{:08x}".format(packet.fn), "", 0)

            shared_frame_key = (symbol.file, symbol.line, symbol.fn_name)
            if shared_frame_key not in shared_frames_registry:
                shared_frames_registry[shared_frame_key] = len(shared_frames)
                shared_frames.append(
                    {
                        "name": symbol.fn_name,
                        "file": symbol.file,
                        "line": symbol.line,
                        "col": 1,
                    }
                )
            frame_index = shared_frames_registry[shared_frame_key]

            profile = profiles_by_context_id.get(packet.context_id)
            if profile is None:
                profile = {
                    "type": "evented",
                    "name": self.__context_name(packet.context_id),
                    "unit": "seconds",
                    "startValue": 0.0,
                    "endValue": 0.0,
                    "events": [],
                }
                profiles_by_context_id[packet.context_id] = profile
                profiles.append(profile)
            profile["events"].append(
                {
                    "type": packet.speedscope_event_type,
                    "at": timestamp,
                    "frame": frame_index,
                }
            )
            logger.log(
                logging.DEBUG - 5,
                f"{profile['name']}: {packet.speedscope_event_type} {timestamp:.9f} {frame_index}",
            )

        for profile in profiles:
            # remove all closing events at the beginning
            while len(profile["events"]) > 1 and profile["events"][0]["type"] == "C":
                profile["events"].pop(0)

            # remove all events that have zero start time
            while profile and profile["events"][0]["at"] == 0.0:
                profile["events"].pop(0)

        logger.info(f"Profiles parsed: {len(profiles)}")
        for profile in profiles:
            logger.info(f"  {profile['name']}: {len(profile['events'])} events")

        shared_frames.append({"name": "OVERFLOW!", "file": "", "line": 0, "col": 1})
        self.overflow_frame_index = len(shared_frames) - 1

        earliest_start = min([profile["events"][0]["at"] for profile in profiles])
        latest_end = max([profile["events"][-1]["at"] for profile in profiles])

        for profile in profiles:
            profile["events"] = self.__fix_events(profile, shared_frames)
            # pick the same time range for all threads
            profile["startValue"] = earliest_start
            profile["endValue"] = latest_end

        return {
            "$schema": "https://www.speedscope.app/file-format-schema.json",
            "profiles": profiles,
            "shared": {"frames": shared_frames},
            "exporter": "ln/prof",
        }

    def __log_opening_event(
        self, log_level, context, idx, timestamp, frame, call_stack, frames, suffix=""
    ):
        indent = "  " * len(call_stack)
        logger.log(
            log_level,
            f"{context}|{idx:8d}|{timestamp:.9f}|{indent}{frame['name']}{': ' if suffix else ''}{suffix}",
        )

    def __log_closing_event(
        self, log_level, context, idx, timestamp, frame, call_stack, frames, suffix=""
    ):
        indent = "  " * len(call_stack)
        logger.log(
            log_level,
            f"{context}|{idx:8d}|{timestamp:.9f}|{indent}~{frame['name']}{': ' if suffix else ''}{suffix}",
        )

    def __fix_events(self, profile, frames):

        # ensure events are in cronological order by finding last chronological sequence and trimming the rest
        last_break_idx = 0
        for idx, event in enumerate(profile["events"]):
            if idx == 0:
                continue
            if event["at"] < profile["events"][idx - 1]["at"]:
                last_break_idx = idx
        profile["events"] = profile["events"][last_break_idx:]

        call_stack = []
        fixed_events = []
        overflow = False
        for idx, event in enumerate(profile["events"]):

            if event["type"] == "C":
                # TODO think of better ways to handle broken frames
                # TODO make the breaks more visible in the speedscope GUI
                if not call_stack:
                    self.__log_closing_event(
                        logging.WARNING - 1,
                        profile["name"],
                        idx,
                        event["at"],
                        frames[event["frame"]],
                        call_stack,
                        frames,
                        "Call stack is empty - skipping",
                    )
                    call_stack = []
                    continue
                if call_stack[-1] != event["frame"]:
                    logger.log(
                        logging.WARNING - 1,
                        f"Call stack inconsistent on {idx} event: tried to close '{frames[event['frame']]['name']}' instead of '{frames[call_stack[-1]]['name']}'",
                    )
                    logger.log(logging.INFO - 1, "Stack termination (begin)")
                    for frame in reversed(call_stack):
                        fixed_events.append(
                            {
                                "type": "C",
                                "at": profile["events"][idx - 1]["at"],
                                "frame": frame,
                            }
                        )
                        call_stack.pop()
                        self.__log_closing_event(
                            logging.INFO - 1,
                            profile["name"],
                            idx,
                            event["at"],
                            frames[frame],
                            call_stack,
                            frames,
                        )
                    logger.log(logging.INFO - 1, "Stack termination (end)")
                    fixed_events.append(
                        {
                            "type": "O",
                            "at": profile["events"][idx - 1]["at"],
                            "frame": self.overflow_frame_index,
                        }
                    )
                    self.__log_opening_event(
                        logging.INFO - 1,
                        profile["name"],
                        idx,
                        event["at"],
                        frames[self.overflow_frame_index],
                        call_stack,
                        frames,
                    )
                    overflow = True
                    continue
                fixed_events.append(event)
                call_stack.pop()
                self.__log_closing_event(
                    logging.INFO - 1,
                    profile["name"],
                    idx,
                    event["at"],
                    frames[event["frame"]],
                    call_stack,
                    frames,
                )
            else:
                if overflow:
                    fixed_events.append(
                        {
                            "type": "C",
                            "at": event["at"],
                            "frame": self.overflow_frame_index,
                        }
                    )
                    self.__log_closing_event(
                        logging.INFO - 1,
                        profile["name"],
                        idx,
                        event["at"],
                        frames[self.overflow_frame_index],
                        call_stack,
                        frames,
                    )
                    overflow = False
                self.__log_opening_event(
                    logging.INFO - 1,
                    profile["name"],
                    idx,
                    event["at"],
                    frames[event["frame"]],
                    call_stack,
                    frames,
                )
                fixed_events.append(event)
                call_stack.append(event["frame"])

        # "C" all remaining frames
        for frame in reversed(call_stack):
            fixed_events.append({"type": "C", "at": event["at"], "frame": frame})

        return fixed_events

    def __export_to_json(self, speedscope_dict, filename):
        with open(filename, "w") as f:
            json.dump(speedscope_dict, f, indent=2)

    def fold_all_stacks(self):
        if not self.__reload_nm_symbols():
            return False
        if not self.__reload_packets_from_file():
            return False

        speedscope_dict = self.__parse_packets_into_speedscope_dict()
        filename = f"{self.args.frame_data_path.split('/')[-1].split('.')[0]}.{self.args.out_ext}"
        self.__export_to_json(speedscope_dict, filename)
        logger.info(f"Exported {filename}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Tool for processing a ln/prof binary record."
    )
    parser.add_argument("nm_symbols_path", help="Symbol file path")
    parser.add_argument("frame_data_path", help="Frame data file path")
    parser.add_argument("--begin", type=float, help="Timestamp from in seconds")
    parser.add_argument("--end", type=float, help="Timestamp to in seconds")
    parser.add_argument(
        "--clk_freq",
        type=int,
        default=480000000,
        help="Clock frequency used for timestamps",
    )
    parser.add_argument("--log-level", type=int, default=logging.INFO)
    parser.add_argument("--fw_base", type=int, default=0x08000000)
    parser.add_argument("--fw_size", type=int, default=0x80000)
    parser.add_argument("--no_color", action="store_true")
    parser.add_argument(
        "--top", type=int, default=10, help="Process only the top most eventful threads"
    )  # TODO: consider removing this option
    parser.add_argument(
        "--out_ext",
        default="spsc",
        choices=["spsc", "json"],
        help="Output file extension",
    )
    args = parser.parse_args()
    uc_prof = UcProf(args)
    uc_prof.fold_all_stacks()
