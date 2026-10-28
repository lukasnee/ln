# Copyright (c)  2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
# SPDX-License-Identifier: GPL-2.0-only

"""Tests for prof.py and prof_protocol.py, run with `python3 -m unittest -v` in this directory."""

import argparse
import json
import os
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import prof
from prof_protocol import (
    CONTEXT_ID_BITS,
    FN_ADDR_BITS,
    ContextId,
    Kind,
    Port,
    decode_stream,
    itm_parse,
)

PORT = 0
FW_BASE = 0x08000000
FW_SIZE = 0x80000
CLK_FREQ = 1_000_000  # one cycle is one microsecond

ITM_OVERFLOW = b"\x70"
ITM_SYNC = b"\x00" * 5 + b"\x80"

FN_A = FW_BASE + 0x100
FN_B = FW_BASE + 0x200
FN_C = FW_BASE + 0x300


def itm_packet(port, payload):
    size_code = {1: 1, 2: 2, 4: 3}[len(payload)]
    return bytes([(port << 3) | size_code]) + payload


def word_packet(word, port=PORT):
    return itm_packet(port, struct.pack("<I", word))


def narrow(value):
    """The value in the narrowest of 1, 2 or 4 bytes."""
    width = 1 if value < 1 << 8 else 2 if value < 1 << 16 else 4
    return value.to_bytes(width, "little")


def fn_enter(fn, delta=10, isr=False, port=PORT):
    """Mirrors __cyg_profile_func_enter() in prof.cpp."""
    time = b""
    if delta >> (32 - FN_ADDR_BITS):
        time = itm_packet(port + Port.TIME, narrow(delta))
        delta = 0
    word = (delta << FN_ADDR_BITS) | ((fn >> 1) & ((1 << FN_ADDR_BITS) - 1))
    enter_port = Port.FN_ENTER_ISR if isr else Port.FN_ENTER_THREAD
    return time + word_packet(word, port + enter_port)


def scope_enter(scope_id, delta=10, isr=False, port=PORT):
    """Mirrors ln_prof_scope_enter() in prof.cpp."""
    enter_port = Port.SCOPE_ENTER_ISR if isr else Port.SCOPE_ENTER_THREAD
    return word_packet((delta << 16) | scope_id, port + enter_port)


def exit_record(delta, isr=False, port=PORT):
    exit_port = port + (Port.FN_EXIT_ISR if isr else Port.FN_EXIT_THREAD)
    return itm_packet(exit_port, narrow(delta))


def switch_to(context_id, delta=10, port=PORT):
    if context_id == ContextId.SCHEDULER:
        return itm_packet(port + Port.SWITCH_OUT, narrow(delta))
    value = (delta << CONTEXT_ID_BITS) | context_id
    return itm_packet(port + Port.SWITCH_IN, narrow(value))


# the events of a thread are only known once it has been switched to
TASK3 = switch_to(3, delta=0)


def context_name(context_id, name, port=PORT):
    """Mirrors ln_prof_send_context_name() in prof.cpp."""
    chars = name.encode("ascii") + b"\0" * 8
    word = context_id
    terminated = False
    for i in range(3):
        if terminated:
            break
        word |= chars[i] << (8 + 8 * i)
        terminated = chars[i] == 0
    words = [(Port.NAME, word)]
    offset = 3
    while not terminated:
        word = 0
        for i in range(4):
            if terminated:
                break
            word |= chars[offset + i] << (8 * i)
            terminated = chars[offset + i] == 0
        words.append((Port.NAME_MORE, word))
        offset += 4
    return b"".join(word_packet(word, port + p) for p, word in words)


def decode(data, port=PORT):
    return decode_stream(data, port, FW_BASE, FW_SIZE, CLK_FREQ)


def name_of(result, context_id):
    return bytes(result.context_names[context_id]).split(b"\0")[0].decode()


class TestItmParse(unittest.TestCase):

    def test_yields_the_payload_of_each_packet(self):
        data = (
            itm_packet(PORT, b"\x01")
            + itm_packet(PORT, b"\x01\x02")
            + word_packet(0xDEADBEEF)
        )
        items = list(itm_parse(data, PORT))
        self.assertEqual(
            [(i.is_loss, i.offset, i.payload) for i in items],
            [
                (False, 0, b"\x01"),
                (False, 2, b"\x01\x02"),
                (False, 5, struct.pack("<I", 0xDEADBEEF)),
            ],
        )

    def test_items_carry_their_port(self):
        data = (
            word_packet(1, port=0) + itm_packet(1, b"\x07") + itm_packet(2, b"\x08\x00")
        )
        items = list(itm_parse(data, [0, 1, 2]))
        self.assertEqual(
            [(i.port, i.payload) for i in items],
            [(0, struct.pack("<I", 1)), (1, b"\x07"), (2, b"\x08\x00")],
        )

    def test_other_port_is_skipped_as_a_gap(self):
        foreign = itm_packet(3, b"abcd")
        data = foreign + b"".join(word_packet(i) for i in range(4))
        items = list(itm_parse(data, PORT))
        self.assertTrue(items[0].is_loss)
        self.assertEqual(items[0].skipped, len(foreign))
        self.assertEqual(len(items), 5)

    def test_overflow_packet_is_a_loss(self):
        data = word_packet(1) + ITM_OVERFLOW + word_packet(2)
        items = list(itm_parse(data, PORT))
        self.assertEqual([i.is_loss for i in items], [False, True, False])
        self.assertEqual(items[1].skipped, 1)

    def test_sync_packet_is_skipped_silently(self):
        data = word_packet(1) + ITM_SYNC + word_packet(2)
        items = list(itm_parse(data, PORT))
        self.assertEqual([i.is_loss for i in items], [False, False])

    def test_cut_off_tail_is_not_a_loss(self):
        data = word_packet(1) + word_packet(2) + word_packet(3)[:3]
        items = list(itm_parse(data, PORT))
        self.assertEqual([i.is_loss for i in items], [False, False])

    def test_resyncs_after_a_lost_byte(self):
        data = bytearray(b"".join(word_packet(i) for i in range(10)))
        del data[7]
        items = list(itm_parse(bytes(data), PORT))
        self.assertTrue(any(i.is_loss for i in items))
        self.assertEqual(items[-1].payload, struct.pack("<I", 9))


class TestDecodeStream(unittest.TestCase):

    def test_decodes_events_and_names(self):
        data = (
            context_name(3, "clid")
            + context_name(4, "Tmr Svc")
            + TASK3
            + fn_enter(FN_A, delta=10)
            + fn_enter(FN_B, delta=5)
            + exit_record(20)
            + switch_to(ContextId.SCHEDULER, delta=7)
        )
        result = decode(data)
        self.assertEqual(
            [(p.cycle_cnt, p.context_id, p.kind) for p in result.packets],
            [
                (0, 3, Kind.CONTEXT_SWITCH),
                (10, 3, Kind.FN_ENTER),
                (15, 3, Kind.FN_ENTER),
                (35, 3, Kind.FN_EXIT),
                (42, ContextId.SCHEDULER, Kind.CONTEXT_SWITCH),
            ],
        )
        self.assertEqual([p.fn for p in result.packets[1:4]], [FN_A, FN_B, FN_B])
        self.assertEqual(name_of(result, 3), "clid")
        self.assertEqual(name_of(result, 4), "Tmr Svc")
        self.assertEqual(result.loss_points, set())
        self.assertEqual(result.gaps, 0)

    def test_names_of_any_length(self):
        for name in ["", "a", "ab", "abc", "abcdef", "i2s2_streamer"]:
            with self.subTest(name=name):
                self.assertEqual(name_of(decode(context_name(5, name)), 5), name)

    def test_lost_byte_is_marked_and_decoding_recovers(self):
        fns = [FW_BASE + 0x100 + 2 * i for i in range(30)]
        events = [fn_enter(fn) for fn in fns]
        data = bytearray(TASK3 + b"".join(events))
        # a byte from the middle of event 12
        del data[len(TASK3) + sum(len(e) for e in events[:12]) + 2]
        result = decode(bytes(data))
        enters = [p for p in result.packets if p.kind == Kind.FN_ENTER]
        self.assertTrue(result.loss_points)
        self.assertGreaterEqual(len(enters), 27)
        # a word cut in the middle swallows the start of the next packet and
        # may decode as one event nothing tells apart from a real one
        self.assertLessEqual(len([p for p in enters if p.fn not in fns]), 1)
        self.assertEqual(enters[-1].fn, fns[-1])
        self.assertTrue(all(0 < i < len(result.packets) for i in result.loss_points))

    def test_overflow_packet_marks_a_loss_between_events(self):
        data = TASK3 + fn_enter(FN_A) + fn_enter(FN_B) + ITM_OVERFLOW + fn_enter(FN_C)
        result = decode(data)
        self.assertEqual(len(result.packets), 4)
        self.assertEqual(result.loss_points, {3})
        self.assertEqual((result.gaps, result.skipped_bytes), (1, 1))

    def test_sync_packet_is_not_a_loss(self):
        data = TASK3 + fn_enter(FN_A) + ITM_SYNC + exit_record(10)
        result = decode(data)
        self.assertEqual(len(result.packets), 3)
        self.assertEqual((result.loss_points, result.gaps), (set(), 0))

    def test_garbage_before_the_first_packet_breaks_no_stack(self):
        result = decode(b"\xff\xff\x7e" + TASK3 + fn_enter(FN_A) + exit_record(10))
        self.assertEqual(len(result.packets), 3)
        self.assertEqual(result.loss_points, set())

    def test_cut_off_last_event_is_dropped_silently(self):
        data = TASK3 + fn_enter(FN_A) + exit_record(10) + fn_enter(FN_B)[:-2]
        result = decode(data)
        self.assertEqual(len(result.packets), 3)
        self.assertEqual((result.loss_points, result.gaps), (set(), 0))

    def test_function_outside_the_firmware_is_rejected(self):
        outside = FW_BASE + 0x2000
        data = TASK3 + fn_enter(FN_A) + fn_enter(outside) + fn_enter(FN_C)
        result = decode_stream(data, PORT, FW_BASE, 0x1000, CLK_FREQ)
        enters = [p for p in result.packets if p.kind == Kind.FN_ENTER]
        self.assertEqual([p.fn for p in enters], [FN_A, FN_C])
        self.assertEqual(result.loss_points, {2})

    def test_a_scope_marked_by_hand_does_not_change_the_code_layout(self):
        data = (
            TASK3
            + fn_enter(FN_A)
            + scope_enter(46)
            + fn_enter(FN_B)
            + exit_record(10)
            + exit_record(10)
            + exit_record(10)
        )
        packets = decode(data).packets[1:]
        self.assertEqual(
            [(p.kind, p.fn) for p in packets],
            [
                (Kind.FN_ENTER, FN_A),
                (Kind.FN_ENTER, 46),
                (Kind.FN_ENTER, FN_B),
                (Kind.FN_EXIT, FN_B),
                (Kind.FN_EXIT, 46),
                (Kind.FN_EXIT, FN_A),
            ],
        )

    def test_a_scope_needs_no_code_layout(self):
        data = switch_to(3, delta=0) + scope_enter(46, isr=False) + exit_record(10)
        kinds = [p.kind for p in decode(data).packets]
        self.assertEqual(kinds, [Kind.CONTEXT_SWITCH, Kind.FN_ENTER, Kind.FN_EXIT])

    def test_impossible_time_step_is_dropped(self):
        data = TASK3 + fn_enter(FN_A) + fn_enter(FN_B, delta=0xFFFFFF) + fn_enter(FN_C)
        result = decode(data)
        self.assertEqual([p.cycle_cnt for p in result.packets], [0, 10, 10, 20])
        self.assertEqual(result.loss_points, {2})

    def test_a_time_step_that_does_not_fit_the_event_is_kept(self):
        data = TASK3 + fn_enter(FN_A, delta=100000 - 1)
        result = decode(data)
        self.assertEqual([p.cycle_cnt for p in result.packets], [0, 99999])
        self.assertEqual(result.loss_points, set())

    def test_a_lone_name_part_is_dropped(self):
        data = TASK3 + context_name(4, "abcdefghij")[5:] + fn_enter(FN_A)
        result = decode(data)
        self.assertEqual(
            [p.kind for p in result.packets], [Kind.CONTEXT_SWITCH, Kind.FN_ENTER]
        )
        self.assertEqual(result.context_names, {})
        self.assertEqual(result.loss_points, {1})

    def test_only_the_selected_port_is_decoded(self):
        data = (
            switch_to(3, delta=0, port=5)
            + fn_enter(FN_A, port=5)
            + exit_record(10, port=5)
        )
        self.assertEqual(len(decode(data, port=5).packets), 3)
        self.assertEqual(len(decode(data, port=20).packets), 0)


class TestExitRecords(unittest.TestCase):

    def summary(self, result):
        return [
            (p.cycle_cnt, p.context_id, p.kind, p.fn)
            for p in result.packets
            if p.kind != Kind.CONTEXT_SWITCH
        ]

    def test_exit_takes_its_function_from_the_open_calls(self):
        data = (
            TASK3
            + fn_enter(FN_A, delta=10)
            + fn_enter(FN_B, delta=5)
            + exit_record(20)
            + exit_record(30)
        )
        self.assertEqual(
            self.summary(decode(data)),
            [
                (10, 3, Kind.FN_ENTER, FN_A),
                (15, 3, Kind.FN_ENTER, FN_B),
                (35, 3, Kind.FN_EXIT, FN_B),
                (65, 3, Kind.FN_EXIT, FN_A),
            ],
        )

    def test_delta_in_any_width(self):
        for delta in [0, 255, 256, 65535, 65536, 90000]:
            with self.subTest(delta=delta):
                result = decode(TASK3 + fn_enter(FN_A, delta=1) + exit_record(delta))
                self.assertEqual(result.packets[-1].cycle_cnt, 1 + delta)

    def test_exit_in_an_interrupt_closes_the_interrupt_call(self):
        data = (
            TASK3
            + fn_enter(FN_A)
            + fn_enter(FN_B, isr=True)
            + exit_record(20, isr=True)
            + exit_record(30)
        )
        packets = decode(data).packets
        self.assertEqual(
            [(p.context_id, p.kind, p.fn) for p in packets[3:]],
            [(ContextId.INTERRUPT, Kind.FN_EXIT, FN_B), (3, Kind.FN_EXIT, FN_A)],
        )

    def test_exits_follow_the_running_thread(self):
        data = (
            switch_to(3)
            + fn_enter(FN_A)
            + switch_to(ContextId.SCHEDULER)
            + switch_to(4)
            + fn_enter(FN_B)
            + exit_record(10)
            + switch_to(ContextId.SCHEDULER)
            + switch_to(3)
            + exit_record(10)
        )
        exits = [p for p in decode(data).packets if p.kind == Kind.FN_EXIT]
        self.assertEqual([(p.context_id, p.fn) for p in exits], [(4, FN_B), (3, FN_A)])

    def test_events_while_the_scheduler_runs_are_rejected(self):
        data = (
            switch_to(ContextId.SCHEDULER)
            + exit_record(10)
            + fn_enter(FN_A)
            + switch_to(3)
            + fn_enter(FN_B)
        )
        kinds = [p.kind for p in decode(data).packets]
        self.assertEqual(
            kinds, [Kind.CONTEXT_SWITCH, Kind.CONTEXT_SWITCH, Kind.FN_ENTER]
        )

    def test_context_switch_carries_the_context_switched_to(self):
        data = switch_to(3, delta=5) + switch_to(ContextId.SCHEDULER, delta=7)
        self.assertEqual(
            [(p.cycle_cnt, p.context_id, p.kind) for p in decode(data).packets],
            [
                (5, 3, Kind.CONTEXT_SWITCH),
                (12, ContextId.SCHEDULER, Kind.CONTEXT_SWITCH),
            ],
        )

    def test_exit_with_no_known_thread_is_rejected(self):
        result = decode(exit_record(10) + fn_enter(FN_A) + TASK3 + fn_enter(FN_B))
        self.assertEqual(
            [p.kind for p in result.packets], [Kind.CONTEXT_SWITCH, Kind.FN_ENTER]
        )

    def test_exit_without_an_open_call_only_advances_the_time(self):
        data = (
            TASK3
            + fn_enter(FN_A, delta=10)
            + exit_record(5)
            + exit_record(7)
            + fn_enter(FN_B, delta=3)
        )
        result = decode(data)
        self.assertEqual([p.cycle_cnt for p in result.packets], [0, 10, 15, 25])
        self.assertEqual(result.loss_points, set())

    def test_loss_forgets_the_open_calls_of_the_contexts_around_it(self):
        data = (
            TASK3
            + fn_enter(FN_A)
            + fn_enter(FN_B)
            + ITM_OVERFLOW
            + exit_record(10)
            + fn_enter(FN_C)
            + exit_record(10)
        )
        result = decode(data)
        # the exit after the gap has nothing to close, the one after FN_C does
        self.assertEqual(
            [(p.kind, p.fn) for p in result.packets if p.fn],
            [
                (Kind.FN_ENTER, FN_A),
                (Kind.FN_ENTER, FN_B),
                (Kind.FN_ENTER, FN_C),
                (Kind.FN_EXIT, FN_C),
            ],
        )
        self.assertEqual(result.loss_points, {3})

    def test_loss_keeps_the_calls_of_other_contexts(self):
        data = (
            TASK3
            + fn_enter(FN_A)
            + fn_enter(FN_B, isr=True)
            + ITM_OVERFLOW
            + fn_enter(FN_C, isr=True)
            + exit_record(10, isr=True)
            + exit_record(10, isr=True)
            + switch_to(3)
            + exit_record(10)
        )
        result = decode(data)
        closed = [
            (p.context_id, p.fn) for p in result.packets if p.kind == Kind.FN_EXIT
        ]
        # the interrupt stack was forgotten around the gap, the thread stack was not
        self.assertEqual(closed, [(ContextId.INTERRUPT, FN_C), (3, FN_A)])

    def test_packets_of_all_ports_resync_after_a_lost_byte(self):
        data = bytearray(
            TASK3 + b"".join(fn_enter(FN_A) + exit_record(300) for _ in range(20))
        )
        del data[len(TASK3) + len(fn_enter(FN_A)) * 5]
        result = decode(bytes(data))
        self.assertTrue(result.loss_points)
        self.assertEqual(result.packets[-1].kind, Kind.FN_EXIT)


class TestFoldAllStacks(unittest.TestCase):
    """prof.py end to end, on streams built from the target's packet layout."""

    SYMBOLS = "08000100 T fnA\n08000200 T fnB\n08000300 T fnC\n"

    # the first event sits at time zero, which the output drops
    WARM_UP = switch_to(3, delta=1000) + fn_enter(FN_C, delta=0) + exit_record(10)

    def fold(self, stream):
        with tempfile.TemporaryDirectory() as tmp:
            symbols_path = os.path.join(tmp, "symbols")
            data_path = os.path.join(tmp, "stream.dat")
            with open(symbols_path, "w") as file:
                file.write(self.SYMBOLS)
            with open(data_path, "wb") as file:
                file.write(stream)
            args = argparse.Namespace(
                nm_symbols_path=symbols_path,
                frame_data_path=data_path,
                itm_base_port=PORT,
                fw_base=FW_BASE,
                fw_size=FW_SIZE,
                clk_freq=CLK_FREQ,
                begin=None,
                end=None,
                cross_section_not_running_segments=False,
                format="speedscope",
                out_ext="json",
                target_prj_src_path=".",
                tracy_import_chrome_path="tracy-import-chrome",
                tracy_keep_chrome_trace=False,
            )
            cwd = os.getcwd()
            os.chdir(tmp)  # the output is written to the working directory
            try:
                self.assertTrue(prof.UcProf(args).fold_all_stacks())
                with open("stream.json") as file:
                    return json.load(file)
            finally:
                os.chdir(cwd)

    @staticmethod
    def events_of(output, profile_name):
        frames = output["shared"]["frames"]
        (profile,) = [p for p in output["profiles"] if p["name"] == profile_name]
        return [(e["type"], frames[e["frame"]]["name"]) for e in profile["events"]]

    def test_clean_stream_gives_balanced_calls(self):
        stream = (
            context_name(3, "main")
            + self.WARM_UP
            + fn_enter(FN_A, delta=100)
            + fn_enter(FN_B, delta=100)
            + exit_record(100)
            + exit_record(100)
        )
        events = self.events_of(self.fold(stream), "main")
        self.assertEqual(
            events,
            [("O", "fnA"), ("O", "fnB"), ("C", "fnB"), ("C", "fnA")],
        )

    def test_a_thread_is_not_running_between_switches_away_and_back(self):
        stream = (
            context_name(3, "main")
            + self.WARM_UP
            + fn_enter(FN_A, delta=100)
            + switch_to(ContextId.SCHEDULER, delta=100)
            + switch_to(3, delta=100)
            + exit_record(100)
        )
        events = self.events_of(self.fold(stream), "main")
        self.assertEqual(
            events,
            [
                ("O", "fnA"),
                ("O", "[not running]"),
                ("C", "[not running]"),
                ("C", "fnA"),
            ],
        )

    def test_lost_events_close_the_open_calls(self):
        stream = (
            context_name(3, "main")
            + self.WARM_UP
            + fn_enter(FN_A, delta=100)
            + fn_enter(FN_B, delta=100)
            + ITM_OVERFLOW
            + exit_record(100)
            + exit_record(100)
            + fn_enter(FN_C, delta=100)
            + exit_record(100)
        )
        events = self.events_of(self.fold(stream), "main")
        self.assertEqual(
            events,
            [
                ("O", "fnA"),
                ("O", "fnB"),
                ("C", "fnB"),
                ("C", "fnA"),
                ("O", "[events lost]"),
                ("C", "[events lost]"),
                ("O", "fnC"),
                ("C", "fnC"),
            ],
        )


if __name__ == "__main__":
    unittest.main()
