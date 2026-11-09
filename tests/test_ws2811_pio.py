"""Check actual assembled WS2811 PIO-v0 waveforms; this is not a hardware bench test."""

from pathlib import Path
import re
import subprocess
import tempfile
import unittest

from test_hub75_pio import PIOASM


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
    def test_all_byte_values_and_word_boundaries_at_both_rates(self):
        source = Path(__file__).resolve().parents[1] / "platform" / "common" / "ws2811.pio"
        with tempfile.TemporaryDirectory() as directory:
            header = Path(directory) / "ws2811.pio.h"
            subprocess.run([PIOASM, "-o", "c-sdk", "-v", "0", str(source), str(header)], check=True)
            body = re.search(r"ws2811_program_instructions\[\] = \{(.*?)\};", header.read_text(), re.S)[1]
            instructions = [int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]{4})", body)]
        self.assertEqual(len(instructions), 4)
        words = [value * 0x01010100 for value in range(256)]
        bits = [word >> shift & 1 for word in words for shift in range(31, 7, -1)]
        model = Playback(instructions, words)
        while model.step():
            self.assertLessEqual(model.cycles, len(bits) * 20)
        self.assertEqual(model.cycles, len(bits) * 20)
        self.assertEqual(model.pin, 0)
        self.assertEqual(len(model.edges), len(bits) * 2)
        for index, bit in enumerate(bits):
            rising, falling = model.edges[2*index:2*index+2]
            self.assertEqual(rising, (index * 20 + 10, 1))
            self.assertEqual(falling, (index * 20 + 10 + (10 if bit else 4), 0))
        for rate in (400000, 800000):
            tick_us = 1000000 / (rate * 20)
            self.assertAlmostEqual(20 * tick_us, 2.5 if rate == 400000 else 1.25)
            self.assertAlmostEqual(4 * tick_us, 0.5 if rate == 400000 else 0.25)
            self.assertAlmostEqual(10 * tick_us, 1.25 if rate == 400000 else 0.625)


if __name__ == "__main__":
    unittest.main()
