"""Instruction-level checks of the assembled PIO and real C++ packed scan words.

This models the instructions used here, not the hardware bus or electrical timing.
PIOASM_EXE and HUB75_TEST_BINARY can select tools and alternative geometry builds.
"""

import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
PIOASM = (os.environ.get("PIOASM_EXE") or shutil.which("pioasm")
          or str(Path.home() / ".pico-sdk" / "tools" / "2.3.1" / "pioasm" / "pioasm.exe"))
NATIVE = Path(os.environ.get("HUB75_TEST_BINARY", ROOT / "build" / "host-tests" / "cti_tests.exe"))
if not NATIVE.exists() and NATIVE.suffix == ".exe":
    NATIVE = NATIVE.with_suffix("")

OE = 1 << 13
LAT = 1 << 12
CLK = 1 << 11


class Playback:
    """Subset of PIO v1: right-shift OUT, SET, JMP, blocking PULL, MOV Y,Y."""

    def __init__(self, instructions, words, stall=False):
        self.instructions = instructions
        self.words = words
        self.pc = self.osr = self.x = self.y = 0
        self.shifted = 32
        self.index = self.cycles = 0
        self.pins = OE
        self.stall = stall
        self.stalled_indices = set()
        self.pixels = []
        self.latched = []
        self.on_times = []
        self.on_start = None
        self.clock_times = []

    def output(self, pins):
        previous = self.pins
        if (previous ^ pins) & (31 << 6):
            assert pins & OE, "Row address changed while illuminated"
            assert not (pins & (CLK | LAT)), "Row address changed during clock/latch"
        if (previous ^ pins) & LAT:
            assert pins & OE, "Latch changed while illuminated"
        if pins & CLK and not previous & CLK:
            assert pins & OE and not pins & LAT, "Clock edge while illuminated/latching"
            self.pixels.append((pins >> 6 & 31, pins & 63))
            self.clock_times.append(self.cycles)
        if pins & LAT and not previous & LAT:
            assert not pins & CLK
            self.latched.append((pins >> 6 & 31, self.pixels))
            self.pixels = []
        if previous & OE and not pins & OE:
            assert not pins & (CLK | LAT)
            self.on_start = self.cycles
        if not previous & OE and pins & OE:
            assert self.on_start is not None
            self.on_times.append(self.cycles - self.on_start)
            self.on_start = None
        self.pins = pins

    def execute(self, instruction, injected=False):
        side = (instruction >> 10) & 7
        delay = (instruction >> 8) & 3
        self.output((self.pins & ~(7 << 11)) | side << 11)
        opcode = instruction >> 13
        branch = None
        exec_word = None
        if opcode == 4:
            assert instruction & 0x80 and instruction & 0x20, "Expected blocking PULL"
            conditional = bool(instruction & 0x40)
            if not conditional or self.shifted >= 30:
                assert self.pins & OE, "FIFO access must be blanked"
                if self.stall and self.index not in self.stalled_indices:
                    self.stalled_indices.add(self.index)
                    self.cycles += 7  # Inject starvation at every actual FIFO read.
                    return False
                assert self.index < len(self.words), "PIO read beyond complete scan stream"
                self.osr = self.words[self.index]
                self.index += 1
                self.shifted = 0
        elif opcode == 3:
            count = instruction & 31 or 32
            value = self.osr & ((1 << count) - 1)
            self.osr >>= count
            self.shifted = min(32, self.shifted + count)
            destination = instruction >> 5 & 7
            if destination == 0:
                assert count == 6 and not self.pins & CLK
                self.output((self.pins & ~63) | value)
            elif destination == 1:
                self.x = value
            elif destination == 2:
                self.y = value
            elif destination == 7:
                assert count == 16
                exec_word = value
            else:
                raise AssertionError(f"Unexpected OUT destination {destination}")
        elif opcode == 0:
            condition = instruction >> 5 & 7
            if condition == 1:
                take = self.x == 0
            elif condition == 2:
                take = self.x != 0
                self.x = (self.x - 1) & 0xffffffff
            elif condition == 4:
                take = self.y != 0
                self.y = (self.y - 1) & 0xffffffff
            else:
                raise AssertionError(f"Unexpected JMP condition {condition}")
            if take:
                branch = instruction & 31
        elif opcode == 7:
            assert injected, "Row SET must come from firmware-generated header"
            assert instruction >> 5 & 7 == 0, "Expected SET PINS"
            self.output((self.pins & ~(31 << 6)) | (instruction & 31) << 6)
        elif opcode == 5:
            assert instruction & 0xff == 0x42, "Expected NOP / MOV Y,Y"
        else:
            raise AssertionError(f"Unsupported instruction {instruction:04x}")
        self.cycles += 1 + delay
        if exec_word is not None:
            assert self.execute(exec_word, injected=True)
        if not injected:
            self.pc = branch if branch is not None else (self.pc + 1) % len(self.instructions)
        return True

    def run(self):
        while True:
            self.execute(self.instructions[self.pc])
            assert self.cycles < 4000000, "PIO did not finish in a bounded time"
            if self.pc == 0 and self.index == len(self.words):
                break
        assert self.pins & OE and not self.pins & (CLK | LAT)
        assert not self.pixels and self.on_start is None


@unittest.skipUnless(Path(PIOASM).is_file() and NATIVE.is_file(),
                     "Requires pioasm and built native tests")
class PioTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.scratch = tempfile.TemporaryDirectory()
        cls.directory = Path(cls.scratch.name)
        header = cls.directory / "hub75.pio.h"
        subprocess.run([PIOASM, "-o", "c-sdk", "-v", "0",
                        str(ROOT / "platform" / "common" / "hub75.pio"), str(header)], check=True)
        code = header.read_text()
        body = re.search(r"hub75_program_instructions\[\] = \{(.*?)\};", code, re.S)[1]
        cls.instructions = [int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]{4})", body)]
        assert len(cls.instructions) == 14
        assert int(re.search(r"#define hub75_wrap_target (\d+)", code)[1]) == 0
        assert int(re.search(r"#define hub75_wrap (\d+)", code)[1]) == 13

    @classmethod
    def tearDownClass(cls):
        cls.scratch.cleanup()

    def check_frame(self, percent, stall=False, repeat=1, geometry=None, pixel_format="RGB888"):
        scan = self.directory / "scan.bin"
        command = [str(NATIVE), "--scan", str(scan), str(percent)]
        if geometry:
            command += [str(v) for v in geometry] + [pixel_format]
        dimensions = subprocess.check_output(command, text=True)
        width, height = (int(value) for value in dimensions.strip().split(","))
        raw = scan.read_bytes()
        words = list(struct.unpack(f"<{len(raw) // 4}I", raw))
        self.assertEqual(len(words), height // 2 * 4 * (3 + (width + 4) // 5))
        if width == height == 64:
            self.assertEqual(len(raw), 8192)
        model = Playback(self.instructions, words * repeat, stall)
        model.run()
        ticks = height // 2 * (4 * (width * 10 + 10) + 400 * 15)
        self.assertEqual(model.cycles, ticks * repeat + (7 * len(words) * repeat if stall else 0))
        self.assertEqual(len(model.latched), height // 2 * 4 * repeat)
        expected_on = []
        for index, (address, pixels) in enumerate(model.latched):
            row = index // 4 % (height // 2)
            plane = index % 4
            self.assertEqual(address, row)
            self.assertEqual(len(pixels), width)
            for x, (pixel_row, color) in enumerate(pixels):
                self.assertEqual(pixel_row, row)
                expected = 0
                for half in range(2):
                    for channel in range(3):
                        pixel = (row + half * (height // 2)) * width + x
                        byte_index = pixel * 3 + channel if pixel_format == "RGB888" else pixel
                        byte = (byte_index * 73 + byte_index // 17) & 255
                        if pixel_format == "RGB888":
                            level = byte >> 4
                        else:
                            levels = (0, 2, 4, 6, 9, 11, 13, 15)
                            level = (levels[byte >> 5] if channel == 0 else
                                     levels[(byte >> 2) & 7] if channel == 1 else (byte & 3) * 5)
                        expected |= ((level >> plane) & 1) << (half * 3 + channel)
                self.assertEqual(color, expected)
            on = (400 << plane) * percent // 100
            if on:
                expected_on.append(on)
        self.assertEqual(model.on_times, expected_on)
        if not stall:
            for start in range(0, len(model.clock_times), width):
                times = model.clock_times[start:start + width]
                self.assertTrue(all(b - a == 10 for a, b in zip(times, times[1:])))

    def test_pixel_edges_latch_row_order_and_brightness_timing(self):
        for percent in (0, 1, 25, 50, 100):
            with self.subTest(percent=percent):
                self.check_frame(percent)

    def test_starvation_at_every_fifo_read_is_blank_and_resumes(self):
        self.check_frame(25, stall=True)

    def test_back_to_back_frames_preserve_stream_alignment(self):
        self.check_frame(100, repeat=2)

    def test_runtime_rgb332_geometry(self):
        for geometry in ((8,16), (64,64), (65,32), (256,64)):
            with self.subTest(geometry=geometry):
                self.check_frame(25, geometry=geometry, pixel_format="RGB332")
        self.check_frame(100, geometry=(65,32), pixel_format="RGB332", stall=True, repeat=2)


if __name__ == "__main__":
    unittest.main()
