"""Configure HUB75 and upload RGB332/RGB888 patterns or repeat frames over USB CDC."""

import argparse
from pathlib import Path
import time

WIDTH = HEIGHT = 64
FRAME_BYTES = WIDTH * HEIGHT * 3
CHUNK_BYTES = 2000


def frame_bytes(width, height, pixel_format="RGB888"):
    if not 8 <= width <= 256 or height not in (16, 32, 64):
        raise ValueError("Geometry requires width 8..256 and height 16, 32 or 64")
    if pixel_format not in ("RGB888", "RGB332"):
        raise ValueError("Format must be RGB888 or RGB332")
    return width * height * (3 if pixel_format == "RGB888" else 1)


def pattern(name, width=WIDTH, height=HEIGHT, pixel_format="RGB888"):
    frame_bytes(width, height, pixel_format)
    colors = (
        (255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255),
        (0, 255, 255), (255, 0, 255), (255, 255, 0), (0, 0, 0),
    )
    pixels = bytearray()
    for y in range(height):
        for x in range(width):
            if name == "bars":
                color = colors[x * 8 // width]
                if y >= height // 2:
                    color = tuple(c // 2 for c in color)
            elif name == "checker":
                color = (255, 255, 255) if (x // 8 + y // 8) % 2 == 0 else (0, 0, 0)
            elif name == "ramp":
                color = (x * 255 // (width - 1), y * 255 // (height - 1), 255 - x * 255 // (width - 1))
            else:
                raise ValueError(f"Unknown pattern: {name}")
            # Distinct corner markers identify orientation and top/bottom mapping.
            if x < 3 and y < 3:
                color = (255, 0, 0)
            elif x >= width - 3 and y < 3:
                color = (0, 255, 0)
            elif x < 3 and y >= height - 3:
                color = (0, 0, 255)
            elif x >= width - 3 and y >= height - 3:
                color = (255, 255, 255)
            if pixel_format == "RGB888":
                pixels.extend(color)
            else:
                r, g, b = color
                pixels.append((r & 0xE0) | ((g >> 3) & 0x1C) | (b >> 6))
    return bytes(pixels)


def data_commands(image, width=WIDTH, height=HEIGHT, pixel_format="RGB888"):
    expected = frame_bytes(width, height, pixel_format)
    if len(image) != expected:
        raise ValueError(f"Expected {expected} {pixel_format} bytes")
    for offset in range(0, expected, CHUNK_BYTES):
        chunk = image[offset:offset + CHUNK_BYTES]
        length = str(len(chunk)).encode("ascii")
        yield (
            f"DISP:HUB75:DATA {offset},".encode("ascii")
            + b"#" + str(len(length)).encode("ascii") + length + chunk + b"\n"
        )


def validate_pins(pins):
    if pins is None:
        return
    if (len(pins) != 14 or len(set(pins)) != 14
            or any(p not in (*range(23), 26, 27, 28) for p in pins)
            or list(pins[:6]) != list(range(pins[0], pins[0] + 6))
            or list(pins[6:11]) != list(range(pins[6], pins[6] + 5))
            or list(pins[11:]) != list(range(pins[11], pins[11] + 3))):
        raise ValueError("Pins require 14 distinct header GPIOs in consecutive RGB(6), address(5), control(3) groups")


def configuration_commands(width, height, pixel_format, pins=None):
    frame_bytes(width, height, pixel_format)
    validate_pins(pins)
    commands = [
        b"DISP:HUB75:STOP\n",
        f"DISP:HUB75:DIM {width},{height}\n".encode("ascii"),
        f"DISP:HUB75:FORM {pixel_format}\n".encode("ascii"),
    ]
    if pins is not None:
        commands.append(("DISP:HUB75:PINS " + ",".join(str(p) for p in pins) + "\n").encode("ascii"))
    return commands


def scpi_stream(image, brightness, width=WIDTH, height=HEIGHT, pixel_format="RGB888", pins=None):
    if not 0 <= brightness <= 100:
        raise ValueError("Brightness must be 0..100")
    # Offline streams cannot wait for brightness swaps; live mode can.
    if brightness != 25:
        raise ValueError("Offline SCPI streams require --brightness 25; use live mode for other levels")
    return (b"".join(configuration_commands(width, height, pixel_format, pins))
            + b"DISP:HUB75:START\n"
            + b"".join(data_commands(image, width, height, pixel_format)) + b"DISP:HUB75:SHOW\n")


class Session:
    def __init__(self, link):
        self.link = link

    def write(self, command):
        if self.link.write(command) != len(command):
            raise OSError("Short serial write")

    def query(self, command, timeout=2, prefix=None):
        self.write(command.encode("ascii") + b"\n")
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            line = self.link.readline()
            if not line:
                continue
            text = line.decode("ascii").strip()
            if text and (prefix is None or text.startswith(prefix)):
                return text
        raise TimeoutError(f"No response to {command}")

    def check_error(self):
        response = self.query("SYST:ERR?")
        if int(response.split(",", 1)[0]) != 0:
            raise RuntimeError(f"Firmware error: {response}")

    def checked(self, command):
        self.write(command)
        self.check_error()

    def status(self):
        values = tuple(int(v) for v in self.query("DISP:HUB75:STAT?").split(","))
        if len(values) != 5:
            raise RuntimeError(f"Invalid HUB75 status: {values}")
        return values

    def wait_swap(self):
        deadline = time.monotonic() + 1
        while time.monotonic() < deadline:
            status = self.status()
            if not status[0]:
                raise RuntimeError("Display stopped unexpectedly")
            if not status[2]:
                return status
            time.sleep(0.01)
        raise TimeoutError("HUB75 frame swap did not complete")


def upload(link, image, brightness, width=WIDTH, height=HEIGHT, pixel_format="RGB888",
           pins=None, frames=1, fps=30):
    if not 0 <= brightness <= 100:
        raise ValueError("Brightness must be 0..100")
    if frames < 1 or not 0 < fps <= 120:
        raise ValueError("Frames must be positive; fps must be >0 and <=120")
    # Validate everything before changing the device.
    commands = configuration_commands(width, height, pixel_format, pins)
    payload = b"".join(data_commands(image, width, height, pixel_format))
    expected = frame_bytes(width, height, pixel_format)
    session = Session(link)
    print(session.query("*IDN?", timeout=8, prefix="CTI,"))
    session.checked(commands[0])
    try:
        for command in commands[1:]:
            session.checked(command)
        geometry = tuple(int(v) for v in session.query("DISP:HUB75:GEOM?").split(","))
        if geometry != (width, height, expected, 4):
            raise RuntimeError(f"Firmware geometry {geometry} does not match requested {width}x{height}")
        if session.query("DISP:HUB75:FORM?") != pixel_format:
            raise RuntimeError("Firmware did not accept the requested image format")
        session.checked(b"DISP:HUB75:START\n")
        session.checked(f"DISP:HUB75:BRIG {brightness}\n".encode("ascii"))
        session.wait_swap()
        started = time.monotonic()
        for index in range(frames):
            if index:
                time.sleep(max(0, started + index / fps - time.monotonic()))
            # One USB write, no per-chunk round trips; staging is independent of scan DMA.
            session.write(payload)
            session.check_error()
            status = session.wait_swap()
            if status[1] != expected:
                raise RuntimeError("Firmware did not stage a complete image")
            session.checked(b"DISP:HUB75:SHOW\n")
        first = session.wait_swap()
        elapsed = time.monotonic() - started
        time.sleep(0.1)
        last = session.status()
        session.check_error()
        if not last[0] or last[4] == first[4]:
            raise RuntimeError("Refresh frame counter is not advancing")
        print(f"Sent {frames} frame(s) in {elapsed:.3f}s: {frames / elapsed:.2f} frames/s, "
              f"{frames * expected * 8 / elapsed / 1e6:.3f} Mbps image payload")
        print(f"Running: status={last}; image persists after this tool exits")
    except (OSError, ValueError, RuntimeError, TimeoutError) as error:
        try:
            session.write(b"DISP:HUB75:STOP\n")
        except OSError as cleanup_error:
            error.add_note(f"Could not send STOP during cleanup: {cleanup_error}")
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    destination = parser.add_mutually_exclusive_group(required=True)
    destination.add_argument("--port", help="USB CDC port, e.g. COM7")
    destination.add_argument("--output", type=Path, help="Write binary SCPI stream without hardware (25% brightness)")
    parser.add_argument("--pattern", choices=("bars", "checker", "ramp"), default="bars")
    parser.add_argument("--brightness", type=int, choices=range(101), default=25, metavar="0..100")
    parser.add_argument("--width", type=int, default=WIDTH, help="Runtime display width (default: 64)")
    parser.add_argument("--height", type=int, choices=(16, 32, 64), default=HEIGHT,
                        help="Runtime display height (default: 64)")
    parser.add_argument("--format", choices=("RGB332", "RGB888"), default="RGB332")
    parser.add_argument("--pins", type=int, nargs=14, metavar="GPIO", help="R0 G0 B0 R1 G1 B1 A B C D E CLK LAT OE")
    parser.add_argument("--frames", type=int, default=1, help="Repeat the pattern to measure transfer rate")
    parser.add_argument("--fps", type=float, default=30, help="Requested frame rate when repeating (default: 30)")
    args = parser.parse_args()
    image = pattern(args.pattern, args.width, args.height, args.format)
    if args.output:
        if args.frames != 1:
            parser.error("--frames applies to live uploads only")
        args.output.write_bytes(scpi_stream(image, args.brightness, args.width, args.height, args.format, args.pins))
        print(f"Wrote {args.output} ({len(image)} RGB bytes)")
        return
    import serial  # Optional for offline generation; install pyserial for live USB.
    with serial.Serial(args.port, 115200, timeout=0.25, write_timeout=2) as link:
        link.reset_input_buffer()
        upload(link, image, args.brightness, args.width, args.height, args.format, args.pins, args.frames, args.fps)


if __name__ == "__main__":
    main()
