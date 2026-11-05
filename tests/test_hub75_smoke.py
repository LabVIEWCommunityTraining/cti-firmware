import contextlib
import importlib.util
import io
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("hub75_smoke", ROOT / "tools" / "hub75_smoke.py")
smoke = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(smoke)
NATIVE = ROOT / "build" / "host-tests" / "cti_tests.exe"
if not NATIVE.exists():
    NATIVE = ROOT / "build" / "host-tests" / "cti_tests"


class FakeSerial:
    def __init__(self, width=64, height=64):
        self.responses = []
        self.writes = []
        self.received = 0
        self.brightness = 25
        self.frames = 0
        self.running = False
        self.width = width
        self.height = height
        self.expected = smoke.frame_bytes(width, height)
        self.pixel_format = "RGB888"

    def write(self, data):
        self.writes.append(data)
        if data == b"*IDN?\n":
            self.responses.extend((b" == Initializing Firmware ==\n", b"CTI,RP2350-Visa,123,0.9.4\n"))
        elif data == b"SYST:ERR?\n":
            self.responses.append(b"0, No error\n")
        elif data == b"DISP:HUB75:GEOM?\n":
            self.responses.append(f"{self.width},{self.height},{self.expected},4\n".encode())
        elif data == b"DISP:HUB75:STOP\n":
            self.running = False
            self.received = 0
        elif data in (b"DISP:HUB75:INIT\n", b"DISP:HUB75:START\n"):
            self.running = True
        elif data.startswith(b"DISP:HUB75:DIM "):
            self.width, self.height = map(int, data.split()[-1].split(b","))
            self.expected = smoke.frame_bytes(self.width, self.height, self.pixel_format)
        elif data.startswith(b"DISP:HUB75:FORM "):
            self.pixel_format = data.split()[-1].decode()
            self.expected = smoke.frame_bytes(self.width, self.height, self.pixel_format)
        elif data == b"DISP:HUB75:FORM?\n":
            self.responses.append((self.pixel_format + "\n").encode())
        elif data.startswith(b"DISP:HUB75:PINS "):
            smoke.validate_pins(list(map(int, data.split()[-1].split(b","))))
        elif data.startswith(b"DISP:HUB75:DATA "):
            remaining = data
            while remaining:
                match = re.match(rb"DISP:HUB75:DATA (\d+),#(\d)", remaining)
                assert match and int(match[1]) == self.received
                digits = int(match[2])
                length = int(remaining[match.end():match.end() + digits])
                end = match.end() + digits + length
                assert 0 < length <= smoke.CHUNK_BYTES and remaining[end:end+1] == b"\n"
                self.received += length
                remaining = remaining[end+1:]
        elif data == b"DISP:HUB75:SHOW\n":
            assert self.received == self.expected
            self.received = 0
        elif data.startswith(b"DISP:HUB75:BRIG "):
            self.brightness = int(data.split()[-1])
        elif data == b"DISP:HUB75:STAT?\n":
            self.frames += 1
            self.responses.append(
                f"{int(self.running)},{self.received},0,{self.brightness},{self.frames}\n".encode()
            )
        else:
            raise AssertionError(f"Unexpected write: {data[:80]!r}")
        return len(data)

    def readline(self):
        return self.responses.pop(0) if self.responses else b""


class SmokeTests(unittest.TestCase):
    def test_patterns_and_corners(self):
        for name in ("bars", "checker", "ramp"):
            image = smoke.pattern(name)
            self.assertEqual(len(image), 12288)
            self.assertEqual(image[:3], bytes((255, 0, 0)))
            self.assertEqual(image[63 * 3:64 * 3], bytes((0, 255, 0)))
            self.assertEqual(image[-64 * 3:-63 * 3], bytes((0, 0, 255)))
            self.assertEqual(image[-3:], bytes((255, 255, 255)))
        with self.assertRaises(ValueError):
            smoke.pattern("invalid")

    def test_chunk_headers_and_payload(self):
        image = smoke.pattern("ramp")
        commands = list(smoke.data_commands(image))
        self.assertEqual(len(commands), 7)
        recovered = bytearray()
        for offset, command in enumerate(commands):
            length = min(smoke.CHUNK_BYTES, len(image) - offset * smoke.CHUNK_BYTES)
            digits = str(length)
            prefix = f"DISP:HUB75:DATA {offset * smoke.CHUNK_BYTES},#{len(digits)}{digits}".encode()
            self.assertTrue(command.startswith(prefix))
            long_form_crlf = command.replace(b"DISP:", b"DISPlay:", 1)[:-1] + b"\r\n"
            self.assertLessEqual(len(long_form_crlf), 2047)
            self.assertEqual(command[-1:], b"\n")
            recovered.extend(command[len(prefix):-1])
        self.assertEqual(recovered, image)
        with self.assertRaises(ValueError):
            list(smoke.data_commands(b"short"))

    def test_live_upload_and_refresh_check(self):
        link = FakeSerial()
        with contextlib.redirect_stdout(io.StringIO()):
            smoke.upload(link, smoke.pattern("bars"), 10)
        self.assertTrue(link.running)
        self.assertEqual(link.brightness, 10)
        self.assertGreater(link.frames, 1)

    def test_alternative_geometries(self):
        for width, height in ((32, 16), (64, 32), (65, 64), (128, 64), (256, 64)):
            image = smoke.pattern("ramp", width, height)
            self.assertEqual(len(image), width * height * 3)
            commands = list(smoke.data_commands(image, width, height))
            self.assertEqual(len(commands), (len(image) + smoke.CHUNK_BYTES - 1) // smoke.CHUNK_BYTES)
            link = FakeSerial(width, height)
            with contextlib.redirect_stdout(io.StringIO()):
                smoke.upload(link, image, 25, width, height)
            self.assertTrue(link.running)
        for width, height in ((0, 64), (257, 64), (64, 48)):
            with self.assertRaises(ValueError):
                smoke.pattern("bars", width, height)

    def test_runtime_reconfiguration_and_rgb332_streaming(self):
        link = FakeSerial(32, 32)
        image = smoke.pattern("bars", 64, 64, "RGB332")
        self.assertEqual(len(image), 4096)
        self.assertEqual(image[0], 0xE0)
        self.assertEqual(image[63], 0x1C)
        self.assertEqual(image[-64], 0x03)
        self.assertEqual(image[-1], 0xFF)
        pins = [14,15,16,17,18,19,0,1,2,3,4,26,27,28]
        with contextlib.redirect_stdout(io.StringIO()):
            smoke.upload(link, image, 25, 64, 64, "RGB332", pins, frames=3, fps=120)
        self.assertEqual((link.width, link.height, link.pixel_format), (64, 64, "RGB332"))
        self.assertEqual(sum(b"DISP:HUB75:SHOW\n" == w for w in link.writes), 3)
        # All chunks for each frame are written in one batch, not acknowledged one-by-one.
        self.assertEqual(sum(w.startswith(b"DISP:HUB75:DATA ") for w in link.writes), 3)

    def test_invalid_configuration_does_not_touch_device(self):
        for pins in ([0] * 14, list(range(13)), list(range(14,28))):
            link = FakeSerial()
            with self.assertRaises(ValueError):
                smoke.upload(link, smoke.pattern("bars"), 25, pins=pins)
            self.assertEqual(link.writes, [])
        link = FakeSerial()
        with self.assertRaises(ValueError):
            smoke.upload(link, b"wrong", 25, pixel_format="RGB332")
        self.assertEqual(link.writes, [])

    def test_error_is_reported(self):
        class ErrorSerial(FakeSerial):
            def readline(self):
                result = super().readline()
                return b"-221, GPIO busy\n" if result == b"0, No error\n" else result
        with self.assertRaisesRegex(RuntimeError, "GPIO busy"):
            smoke.Session(ErrorSerial()).check_error()

    def test_short_write_is_reported(self):
        class ShortSerial:
            def write(self, data):
                return len(data) - 1
        with self.assertRaisesRegex(OSError, "Short serial write"):
            smoke.Session(ShortSerial()).write(b"DISP:HUB75:STOP\n")

    @unittest.skipUnless(NATIVE.exists(), "Build native C++ tests first")
    def test_offline_stream_with_actual_firmware_handlers(self):
        with tempfile.TemporaryDirectory() as directory:
            stream = Path(directory) / "bars.scpi"
            stream.write_bytes(smoke.scpi_stream(smoke.pattern("bars"), 25))
            subprocess.run([str(NATIVE), str(stream)], check=True)
            for width, height in ((8,16), (64,64), (65,32), (256,64)):
                stream.write_bytes(smoke.scpi_stream(
                    smoke.pattern("ramp", width, height, "RGB332"), 25, width, height, "RGB332"))
                subprocess.run([str(NATIVE), str(stream)], check=True)

    @unittest.skipUnless(NATIVE.exists(), "Build native C++ tests first")
    def test_actual_firmware_registration_capacity(self):
        sources = list((ROOT / "source" / "visa").glob("*.cpp"))
        sources += [ROOT / "platform" / "rp2350" / "platform" / name
                    for name in ("pi_pico_visa.cpp", "hub75.cpp")]
        names = []
        for source in sources:
            text = re.sub(r"//[^\n]*|/\*.*?\*/", "", source.read_text(), flags=re.S)
            names += re.findall(r'addCommand\("([^"]+)"', text)
        self.assertGreater(len(names), 40)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "commands.txt"
            path.write_text("\n".join(names), encoding="ascii")
            subprocess.run([str(NATIVE), "--register", str(path)], check=True)


if __name__ == "__main__":
    unittest.main()
