"""Check actual assembled WS2811 PIO-v0 waveforms; this is not a hardware bench test."""

from pathlib import Path
import re
import struct
import subprocess
import tempfile
import unittest

from test_hub75_pio import PIOASM, NATIVE


class Playback:
    def __init__(self, instructions, words):
        self.instructions = instructions
        self.words = iter(words)
        self.osr = self.x = self.pc = self.cycles = 0
        self.shifted = 32
        self.pin = 0
        self.edges = []

    def step(self):
        instruction = self.instructions[self.pc]
        side = (instruction >> 12) & 1
        delay = (instruction >> 8) & 15
        if side != self.pin:
            self.edges.append((self.cycles, side))
            self.pin = side
        opcode = instruction >> 13
        branch = None
        if opcode == 3:
            assert instruction & 255 == 0x21  # OUT X,1
            if self.shifted >= 24:
                try:
                    self.osr = next(self.words)
                except StopIteration:
                    assert self.pin == 0  # Autopull starvation is low, not a held-high bit.
                    return False
                self.shifted = 0
            self.x = self.osr >> 31
            self.osr = self.osr << 1 & 0xFFFFFFFF
            self.shifted += 1
        elif opcode == 0:
            condition = instruction >> 5 & 7
            assert condition in (0, 1)
            if condition == 0 or self.x == 0:
                branch = instruction & 31
        else:
            assert opcode == 5 and instruction & 255 == 0x42
        self.cycles += 1 + delay
        self.pc = branch if branch is not None else (self.pc + 1) % len(self.instructions)
        return True


@unittest.skipUnless(Path(PIOASM).is_file(), "Requires pioasm")
class Ws2811PioTests(unittest.TestCase):
    def assemble(self, directory):
        source = Path(__file__).resolve().parents[1] / "platform" / "common" / "ws2811.pio"
        header = Path(directory) / "ws2811.pio.h"
        subprocess.run([PIOASM, "-o", "c-sdk", "-v", "0", str(source), str(header)], check=True)
        body = re.search(r"ws2811_program_instructions\[\] = \{(.*?)\};", header.read_text(), re.S)[1]
        instructions = [int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]{4})", body)]
        self.assertEqual(len(instructions), 9)
        return instructions

    def check_waveform(self, instructions, zero_high, zero_low, one_high, one_low):
        words = [value * 0x01010100 for value in range(256)]
        bits = [word >> shift & 1 for word in words for shift in range(31, 7, -1)]
        model = Playback(instructions, words)
        total = sum(one_high + one_low if bit else zero_high + zero_low for bit in bits)
        while model.step():
            self.assertLessEqual(model.cycles, total)
        self.assertEqual(model.cycles, total)
        self.assertEqual(model.pin, 0)
        self.assertEqual(len(model.edges), len(bits) * 2)
        at = 1
        for index, bit in enumerate(bits):
            rising, falling = model.edges[2*index:2*index+2]
            high, low = (one_high, one_low) if bit else (zero_high, zero_low)
            self.assertEqual(rising, (at, 1))
            self.assertEqual(falling, (at + high, 0))
            at += high + low

    def test_default_assembled_waveform(self):
        with tempfile.TemporaryDirectory() as directory:
            self.check_waveform(self.assemble(directory), 4, 16, 10, 10)

    @unittest.skipUnless(NATIVE.is_file(), "Requires native timing patch helper")
    def test_actual_cpp_timing_patches_all_bytes_and_independent_periods(self):
        with tempfile.TemporaryDirectory() as directory:
            instructions = self.assemble(directory)
            source, target = Path(directory) / "source.bin", Path(directory) / "patched.bin"
            source.write_bytes(struct.pack("<9H", *instructions))
            for ns in (
                (250, 1000, 625, 625), (500, 2000, 1250, 1250),
                (500, 2000, 1200, 1300), (300, 950, 700, 600),
                (63, 250, 125, 250), (1000, 3062, 2000, 3062),
                (250, 2000, 625, 250), (250, 250, 625, 2000),
            ):
                with self.subTest(ns=ns):
                    subprocess.run([str(NATIVE), "--ws-timing", str(source), str(target),
                                    *map(str, ns), "500"], check=True)
                    patched = struct.unpack("<9H", target.read_bytes())
                    for original, changed in zip(instructions, patched):
                        self.assertEqual(original & ~0x0F00, changed & ~0x0F00)
                    cycles = [(value * 2 + 62) // 125 for value in ns]
                    self.check_waveform(patched, *cycles)


if __name__ == "__main__":
    unittest.main()
