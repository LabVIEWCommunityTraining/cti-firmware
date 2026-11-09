import contextlib
import io
import re
import unittest
from tools import ws2811_smoke as smoke


class FakeSerial:
    def __init__(self):
        self.responses = []
        self.writes = []
        self.count = 64
        self.received = 0
        self.brightness = 25
        self.frames = 0
        self.running = False

    def write(self, data):
        self.writes.append(data)
        if data == b"*IDN?\n":
            self.responses.append(b"CTI,RP2040-Visa,123,0.9.4\n")
        elif data == b"SYST:ERR?\n":
            self.responses.append(b"0, No error\n")
        elif data == b"DISP:WS2811:STOP\n":
            self.running = False
            self.received = 0
        elif data.startswith(b"DISP:WS2811:COUNT "):
            self.count = int(data.split()[-1])
        elif data.startswith((b"DISP:WS2811:PIN ", b"DISP:WS2811:RATE ", b"DISP:WS2811:ORD ")):
            pass
        elif data == b"DISP:WS2811:START\n":
            self.running = True
            self.frames += 1
        elif data.startswith(b"DISP:WS2811:BRIG "):
            self.brightness = int(data.split()[-1])
            self.frames += 1
        elif data.startswith(b"DISP:WS2811:DATA "):
            while data:
                match = re.match(rb"DISP:WS2811:DATA (\d+),#(\d)", data)
                assert match and int(match[1]) == self.received
                digits = int(match[2])
                length = int(data[match.end():match.end()+digits])
                end = match.end() + digits + length
                assert 1 <= length <= 2000 and data[end:end+1] == b"\n"
                self.received += length
                data = data[end+1:]
        elif data == b"DISP:WS2811:SHOW\n":
            assert self.received == self.count * 3
            self.received = 0
            self.frames += 1
        elif data == b"DISP:WS2811:STAT?\n":
            self.responses.append(f"{int(self.running)},{self.received},0,{self.brightness},{self.frames}\n".encode())
        else:
            raise AssertionError(data)
        return len(self.writes[-1])

    def readline(self):
        return self.responses.pop(0) if self.responses else b""


class Ws2811SmokeTests(unittest.TestCase):
    def test_patterns_chunks_and_live_workflow(self):
        for count in (1, 4, 64, 4096):
            image = smoke.pattern(count)
            self.assertEqual(len(image), count * 3)
            self.assertEqual(image[:3], b"\xff\0\0")
            commands = list(smoke.data_commands(image, count))
            self.assertEqual(len(commands), (len(image) + 1999) // 2000)
            self.assertTrue(all(len(command) <= 2047 for command in commands))
            link = FakeSerial()
            with contextlib.redirect_stdout(io.StringIO()):
                smoke.upload(link, image, count, 20, 400000, "GRB", 10)
            self.assertTrue(link.running)
            self.assertEqual(link.brightness, 10)
            self.assertEqual(link.received, 0)
            self.assertEqual(link.frames, 3)

    def test_invalid_input_leaves_device_untouched(self):
        for args in ((0, 2, 800000, "RGB", 25), (4097, 2, 800000, "RGB", 25),
                     (4, 23, 800000, "RGB", 25), (4, 2, 1000000, "RGB", 25),
                     (4, 2, 800000, "RGBW", 25), (4, 2, 800000, "RGB", 101)):
            link = FakeSerial()
            with self.assertRaises(ValueError):
                smoke.upload(link, b"", *args)
            self.assertEqual(link.writes, [])
        link = FakeSerial()
        with self.assertRaises(ValueError):
            smoke.upload(link, b"wrong")
        self.assertEqual(link.writes, [])


if __name__ == "__main__":
    unittest.main()
