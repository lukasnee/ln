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
    PacketId,
    State,
    Type,
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


def profile_event(delta, context_id, state, type_, fn=0, port=PORT):
    word0 = PacketId.SEND_PROFILE_EVENT | (delta << 8)
    word1 = (
        context_id | (state << 7) | (type_ << 8) | (((fn >> 1) & 0x7FFFFF) << 9)
    )
    return word_packet(word0, port) + word_packet(word1, port)


def fn_enter(fn, delta=10, context_id=3, port=PORT):
    return profile_event(delta, context_id, State.ENTER, Type.FN, fn, port)


def fn_exit(fn, delta=10, context_id=3, port=PORT):
    return profile_event(delta, context_id, State.EXIT, Type.FN, fn, port)


def context_name(context_id, name, port=PORT):
    """Mirrors ln_prof_send_context_name() in prof.cpp."""
    chars = name.encode("ascii") + b"\0" * 5
    words = [
        PacketId.DISCLOSE_CONTEXT_NAME
        | (context_id << 8)
        | (chars[0] << 16)
        | (chars[1] << 24)
    ]
    terminated = chars[0] == 0 or chars[1] == 0
    offset = 2
    while not terminated:
        word = 0
        for i in range(4):
            if terminated:
                break
            char = chars[offset + i]
            word |= char << (8 * i)
            terminated = char == 0
        words.append(word)
        offset += 4
    return b"".join(word_packet(word, port) for word in words)


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
            + fn_enter(FN_A, delta=10)
            + fn_enter(FN_B, delta=5)
            + fn_exit(FN_B, delta=20)
            + profile_event(7, 3, State.ENTER, Type.THREAD)
        )
        result = decode(data)
        self.assertEqual(
            [
                (p.cycle_cnt, p.context_id, p.state, p.type)
                for p in result.packets
            ],
            [
                (10, 3, State.ENTER, Type.FN),
                (15, 3, State.ENTER, Type.FN),
                (35, 3, State.EXIT, Type.FN),
                (42, 3, State.ENTER, Type.THREAD),
            ],
        )
        self.assertEqual([p.fn for p in result.packets[:3]], [FN_A, FN_B, FN_B])
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
        data = bytearray(b"".join(events))
        # a byte from the middle of event 12
        del data[sum(len(e) for e in events[:12]) + 2]
        result = decode(bytes(data))
        self.assertTrue(result.loss_points)
        self.assertGreaterEqual(len(result.packets), 27)
        self.assertTrue(all(p.fn in fns for p in result.packets))
        self.assertEqual(result.packets[-1].fn, fns[-1])
        self.assertTrue(all(0 < i < len(result.packets) for i in result.loss_points))

    def test_overflow_packet_marks_a_loss_between_events(self):
        data = fn_enter(FN_A) + fn_enter(FN_B) + ITM_OVERFLOW + fn_exit(FN_B)
        result = decode(data)
        self.assertEqual(len(result.packets), 3)
        self.assertEqual(result.loss_points, {2})
        self.assertEqual((result.gaps, result.skipped_bytes), (1, 1))

    def test_sync_packet_is_not_a_loss(self):
        data = fn_enter(FN_A) + ITM_SYNC + fn_exit(FN_A)
        result = decode(data)
        self.assertEqual(len(result.packets), 2)
        self.assertEqual((result.loss_points, result.gaps), (set(), 0))

    def test_garbage_before_the_first_packet_breaks_no_stack(self):
        result = decode(b"\xff\xff\x7e" + fn_enter(FN_A) + fn_exit(FN_A))
        self.assertEqual(len(result.packets), 2)
        self.assertEqual(result.loss_points, set())

    def test_cut_off_last_event_is_dropped_silently(self):
        data = fn_enter(FN_A) + fn_exit(FN_A) + fn_enter(FN_B)[:-2]
        result = decode(data)
        self.assertEqual(len(result.packets), 2)
        self.assertEqual((result.loss_points, result.gaps), (set(), 0))

    def test_function_outside_the_firmware_is_rejected(self):
        outside = FW_BASE + FW_SIZE + 0x100
        data = fn_enter(FN_A) + fn_enter(outside) + fn_exit(FN_A)
        result = decode(data)
        self.assertEqual([p.fn for p in result.packets], [FN_A, FN_A])
        self.assertEqual(result.loss_points, {1})

    def test_impossible_time_step_is_rejected(self):
        data = fn_enter(FN_A) + fn_enter(FN_B, delta=0xFFFFFF) + fn_exit(FN_A)
        result = decode(data)
        self.assertEqual([p.fn for p in result.packets], [FN_A, FN_A])
        self.assertEqual(result.loss_points, {1})

    def test_only_the_selected_port_is_decoded(self):
        data = fn_enter(FN_A, port=5) + fn_exit(FN_A, port=5)
        self.assertEqual(len(decode(data, port=5).packets), 2)
        self.assertEqual(len(decode(data, port=0).packets), 0)


class TestFoldAllStacks(unittest.TestCase):
    """prof.py end to end, on streams built from the target's packet layout."""

    SYMBOLS = "08000100 T fnA\n08000200 T fnB\n08000300 T fnC\n"

    # the first event sits at time zero, which the output drops
    WARM_UP = fn_enter(FN_C, delta=1000) + fn_exit(FN_C)

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
                itm_port=PORT,
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
            + fn_exit(FN_B, delta=100)
            + fn_exit(FN_A, delta=100)
        )
        events = self.events_of(self.fold(stream), "main")
        self.assertEqual(
            events,
            [("O", "fnA"), ("O", "fnB"), ("C", "fnB"), ("C", "fnA")],
        )

    def test_lost_events_close_the_open_calls(self):
        stream = (
            context_name(3, "main")
            + self.WARM_UP
            + fn_enter(FN_A, delta=100)
            + fn_enter(FN_B, delta=100)
            + ITM_OVERFLOW
            + fn_exit(FN_B, delta=100)
            + fn_exit(FN_A, delta=100)
            + fn_enter(FN_C, delta=100)
            + fn_exit(FN_C, delta=100)
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
