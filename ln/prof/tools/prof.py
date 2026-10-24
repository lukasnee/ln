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
import ntpath
import shutil
import struct
import subprocess

logger = logging.getLogger(__name__)

Symbol = namedtuple("Symbol", ["address", "typ", "fn_name", "file", "line"])
NM_SYMBOLS_REGEX_PATTERN = r"(?P<address>[0-9a-f]+)\s(?P<typ>\w)\s(?P<fn_name>[^\t\n]*)(\t(?P<file>.*):(?P<line>\d+))?"


class PacketId(IntEnum):
    SEND_PROFILE_EVENT = 0xA5
    DISCLOSE_CONTEXT_NAME = 0xA6


NAME_MAX_LEN = 64  # sanity cap for corrupted streams


# should match ln/prof/src/prof.cpp
class ContextId(IntEnum):
    SCHEDULER = 1
    INTERRUPT = 2
    FIRST_TASK = 3
    # subsequent tasks


# outside the 7-bit context id range
TRACY_UNKNOWN_CONTEXT_TID = 0x80


INTERRUPT_CONTEXT_NAME = "Interrupts"
NOT_RUNNING_FRAME_NAME = "[not running]"
LOST_EVENTS_FRAME_NAME = "[events lost]"
TRACY_SCHEDULER_ZONE_NAME = "[scheduler]"
TRACY_UNKNOWN_ZONE_NAME = "[unknown]"


class State(IntEnum):
    ENTER = 0
    EXIT = 1


class Type(IntEnum):
    THREAD = 0
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
        # profile index -> [(time, is_running)]
        self._profiles_run_state_trans = {}
        # profile index -> context id
        self._profile_context_ids = []
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
        if context_id == ContextId.INTERRUPT:
            return INTERRUPT_CONTEXT_NAME
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

            # if packet.context_id == ContextId.INTERRUPT:
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

            if packet.type == Type.THREAD:
                symbol = Symbol(0, "U", NOT_RUNNING_FRAME_NAME, "", 0)
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
                self._profile_context_ids.append(packet.context_id)
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

        shared_frames.append(
            {"name": LOST_EVENTS_FRAME_NAME, "file": "", "line": 0, "col": 1}
        )
        self.lost_events_frame_index = len(shared_frames) - 1

        earliest_start = min([profile["events"][0]["at"] for profile in profiles])
        latest_end = max([profile["events"][-1]["at"] for profile in profiles])
        logger.info(f"start: {earliest_start:.9f}")
        logger.info(f"end: {latest_end:.9f}")

        for profile_idx, profile in enumerate(profiles):
            profile["events"] = self.__slice_last_chronological_event_sequence(
                profile["events"]
            )
            self._profiles_run_state_trans[profile_idx] = (
                self.__extract_profile_run_state_transitions(
                    profile, profile_idx, shared_frames
                )
            )
            profile["events"] = self.__repair_lost_profile_events(
                profile, shared_frames
            )
            if self.args.cross_section_not_running_segments:
                self.__cross_section_not_running_segments(
                    profile, profile_idx, shared_frames
                )
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

    @staticmethod
    def __slice_last_chronological_event_sequence(events):
        last_break_idx = 0
        for idx, event in enumerate(events):
            if idx == 0:
                continue
            if event["at"] < events[idx - 1]["at"]:
                last_break_idx = idx
        return events[last_break_idx:]

    def __extract_profile_run_state_transitions(self, profile, profile_idx, frames):
        transitions = []
        for event in profile["events"]:
            is_not_running_marker = (
                frames[event["frame"]]["name"] == NOT_RUNNING_FRAME_NAME
                and self._profile_context_ids[profile_idx] != ContextId.INTERRUPT
            )
            if not is_not_running_marker:
                continue

            is_running = event["type"] == "C"
            if not transitions and not is_running:
                transitions.append((event["at"], True))
            if not transitions or transitions[-1][1] != is_running:
                transitions.append((event["at"], is_running))
        return transitions

    def __repair_lost_profile_events(self, profile, frames):
        call_stack = []
        fixed_events = []
        events_lost = False
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
                            "frame": self.lost_events_frame_index,
                        }
                    )
                    self.__log_opening_event(
                        logging.INFO - 1,
                        profile["name"],
                        idx,
                        event["at"],
                        frames[self.lost_events_frame_index],
                        call_stack,
                        frames,
                    )
                    events_lost = True
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
                if events_lost:
                    fixed_events.append(
                        {
                            "type": "C",
                            "at": event["at"],
                            "frame": self.lost_events_frame_index,
                        }
                    )
                    self.__log_closing_event(
                        logging.INFO - 1,
                        profile["name"],
                        idx,
                        event["at"],
                        frames[self.lost_events_frame_index],
                        call_stack,
                        frames,
                    )
                    events_lost = False
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

    def __cross_section_not_running_segments(self, profile, profile_idx, frames):
        logger.info(
            f"Cross-sectioning not running segments for profile {profile['name']}..."
        )
        call_stack = []
        suspended_stack = None
        fixed_events = []
        for event in profile["events"]:
            is_not_running_marker = (
                frames[event["frame"]]["name"] == NOT_RUNNING_FRAME_NAME
                and self._profile_context_ids[profile_idx] != ContextId.INTERRUPT
            )
            if is_not_running_marker:
                if event["type"] == "O":
                    if suspended_stack is None:
                        suspended_stack = call_stack
                        call_stack = []
                        for frame in reversed(suspended_stack):
                            fixed_events.append(
                                {"type": "C", "at": event["at"], "frame": frame}
                            )
                elif suspended_stack is not None:
                    for frame in suspended_stack:
                        fixed_events.append(
                            {"type": "O", "at": event["at"], "frame": frame}
                        )
                    call_stack = suspended_stack
                    suspended_stack = None
                continue

            if suspended_stack is not None:
                for frame in suspended_stack:
                    fixed_events.append(
                        {"type": "O", "at": event["at"], "frame": frame}
                    )
                call_stack = suspended_stack
                suspended_stack = None

            fixed_events.append(event)
            if event["type"] == "O":
                call_stack.append(event["frame"])
            elif call_stack and call_stack[-1] == event["frame"]:
                call_stack.pop()
        profile["events"] = fixed_events

    def __export_dict_to_json(self, dictionary, filename):
        with open(filename, "w") as f:
            json.dump(dictionary, f, indent=2)

    def __chrome_trace_generate_scheduler_context_events(self, profiles, tid, start):
        """Zones named after whichever thread is running, from the tasks' switch times."""

        def trace_event(ph, at, name):
            return {"ph": ph, "name": name, "pid": 0, "tid": tid, "ts": at * 1e6}

        transitions = sorted(
            (
                (at, profile_idx, running)
                for profile_idx, profile_run_state_trans in self._profiles_run_state_trans.items()
                for at, running in profile_run_state_trans
            ),
            key=lambda t: t[0],
        )
        events = []
        running_idx = None
        zone = None

        def switch_zone(at, name):
            nonlocal zone
            if zone is not None:
                events.append(trace_event("E", at, zone))
            if name is not None:
                events.append(trace_event("B", at, name))
            zone = name

        at = 0.0
        if transitions and transitions[0][0] > start:
            switch_zone(start, TRACY_UNKNOWN_ZONE_NAME)
        for at, profile_idx, running in transitions:
            if running:
                if running_idx == profile_idx:
                    continue
                running_idx = profile_idx
                switch_zone(at, profiles[profile_idx]["name"])
            elif running_idx == profile_idx:
                # between a switch-out and the next switch-in the scheduler is running
                running_idx = None
                switch_zone(at, TRACY_SCHEDULER_ZONE_NAME)
        switch_zone(at, None)
        return events

    def __speedscope_dict_to_chrome_trace_dict(
        self, speedscope_dict, target_prj_src_path
    ):
        """Chrome trace-event JSON, importable by Tracy's tracy-import-chrome."""
        frames = speedscope_dict["shared"]["frames"]
        # Windows/UNC roots (for a Windows-side GUI) must not be treated as POSIX paths
        if re.match(r"^(\\\\|//|[A-Za-z]:)", target_prj_src_path):
            path_mod = ntpath
        else:
            path_mod = os.path
            target_prj_src_path = os.path.abspath(target_prj_src_path)
        trace_events = []
        scheduler_tid = ContextId.SCHEDULER
        trace_events.append(
            {
                "ph": "M",
                "name": "thread_name",
                "pid": 0,
                "tid": scheduler_tid,
                "args": {"name": "Scheduler"},
            }
        )

        def zone_event(event, tid):
            frame = frames[event["frame"]]
            # the Scheduler row already shows when a thread is not running
            if frame["name"] == NOT_RUNNING_FRAME_NAME:
                return None
            trace_event = {
                "ph": "B" if event["type"] == "O" else "E",
                "name": frame["name"],
                "pid": 0,
                "tid": tid,
                "ts": event["at"] * 1e6,
            }
            # undocumented key read by tracy-import-chrome as the zone source location
            if trace_event["ph"] == "B" and frame["file"]:
                # Tracy needs an absolute path to open the source
                path = path_mod.normpath(
                    path_mod.join(target_prj_src_path, frame["file"])
                )
                trace_event["loc"] = f"{path}:{frame['line']}"
            return trace_event

        profiles = speedscope_dict["profiles"]
        trace_events.extend(
            self.__chrome_trace_generate_scheduler_context_events(
                profiles,
                scheduler_tid,
                # Tracy lists threads by first event time, so this keeps Scheduler on top
                min(p["events"][0]["at"] for p in profiles if p["events"]),
            )
        )
        for profile_idx, profile in enumerate(profiles):
            # tid 0 would collide with Tracy's unset crash event and draw a bogus "crash" marker
            tid = self._profile_context_ids[profile_idx] or TRACY_UNKNOWN_CONTEXT_TID
            trace_events.append(
                {
                    "ph": "M",
                    "name": "thread_name",
                    "pid": 0,
                    "tid": tid,
                    "args": {"name": profile["name"]},
                }
            )
            trace_events.extend(
                trace_event
                for event in profile["events"]
                if (trace_event := zone_event(event, tid))
            )
        # stable sort keeps per-thread order for equal timestamps
        trace_events.sort(key=lambda e: e.get("ts", float("-inf")))
        return {"traceEvents": trace_events, "displayTimeUnit": "ns"}

    def fold_all_stacks(self):
        if not self.__reload_nm_symbols():
            return False
        if not self.__reload_packets_from_file():
            return False

        speedscope_dict = self.__parse_packets_into_speedscope_dict()
        base = self.args.frame_data_path.split("/")[-1].split(".")[0]
        if self.args.format == "tracy":
            filename = f"{base}.trace.json"
            tracy_filename = f"{base}.tracy"
            self.__export_dict_to_json(
                self.__speedscope_dict_to_chrome_trace_dict(
                    speedscope_dict, self.args.target_prj_src_path
                ),
                filename,
            )
            importer = shutil.which(self.args.tracy_import_chrome_path)
            if importer is None:
                logger.error(
                    f"'{self.args.tracy_import_chrome_path}' not found; exported {filename} only. "
                    f"Convert with: tracy-import-chrome {filename} {tracy_filename}"
                )
                return False
            if os.path.exists(tracy_filename):
                os.remove(tracy_filename)
            result = subprocess.run(
                [importer, filename, tracy_filename],
                capture_output=True,
                text=True,
            )
            if result.returncode != 0:
                logger.error(
                    f"{importer} failed ({result.returncode}); kept {filename}\n{result.stdout}{result.stderr}"
                )
                return False
            if not self.args.tracy_keep_chrome_trace:
                os.remove(filename)
            logger.info(f"Exported {tracy_filename}")
            return
        filename = f"{base}.{self.args.out_ext}"
        self.__export_dict_to_json(speedscope_dict, filename)
        logger.info(f"Exported {filename}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Tool for processing a ln/prof binary record."
    )
    parser.add_argument("nm_symbols_path", help="Symbol file path")
    parser.add_argument("frame_data_path", help="Frame data file path")
    parser.add_argument(
        "--format",
        default="speedscope",
        choices=["speedscope", "tracy"],
        help="Output format; 'tracy' writes a .tracy file via "
        "tracy-import-chrome (expected to be installed and available in PATH, "
        "see --tracy_import_chrome_path option)",
    )
    parser.add_argument(
        "--tracy_keep_chrome_trace",
        action="store_true",
        help="Keep the intermediate Chrome trace JSON file when exporting to Tracy",
    )
    parser.add_argument(
        "--tracy_import_chrome_path",
        default="tracy-import-chrome",
        help="Path or name of the tracy-import-chrome executable",
    )
    parser.add_argument(
        "--target_prj_src_path",
        default=".",
        help="Directory that relative source paths in the symbols are relative "
        "to (tracy format). Can be used to make source file paths in Tracy "
        "Profiler clickable",
    )
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
    parser.add_argument(
        "--cross_section_not_running_segments",
        action="store_true",
        help="Close a thread's call stack while it is not running and reopen it "
        "on switch in, instead of stacking a '[not running]' frame on top "
        "(the frame is only added for the speedscope format)",
    )
    parser.add_argument("--no_color", action="store_true")
    parser.add_argument(
        "--out_ext",
        default="spsc",
        choices=["spsc", "json"],
        help="Output file extension",
    )
    args = parser.parse_args()
    uc_prof = UcProf(args)
    uc_prof.fold_all_stacks()
