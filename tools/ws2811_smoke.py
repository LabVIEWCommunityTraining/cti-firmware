"""Configure and upload RGB test pixels to WS2811 using the standard CTI firmware."""

import argparse

if __package__:
    from .hub75_smoke import Session
else:
    from hub75_smoke import Session


def validate(count, pin, rate, order, brightness):
    if not 1 <= count <= 4096:
        raise ValueError("Count must be 1..4096 addressable WS2811 controllers")
    if pin not in (*range(23), 26, 27, 28):
        raise ValueError("Data pin must be an exposed header GPIO")
    if rate not in (400000, 800000) or order not in ("RGB", "RBG", "GRB", "GBR", "BRG", "BGR"):
        raise ValueError("Select 400000/800000 signaling and a three-channel color order")
    if not 0 <= brightness <= 100:
        raise ValueError("Brightness must be 0..100")


def pattern(count):
    if not 1 <= count <= 4096:
        raise ValueError("Count must be 1..4096")
    colors = ((255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255))
    return bytes(channel for pixel in range(count) for channel in colors[pixel % len(colors)])


def data_commands(image, count):
    if not 1 <= count <= 4096 or len(image) != count * 3:
        raise ValueError("Image must contain count*3 RGB888 bytes")
    for offset in range(0, len(image), 2000):
        chunk = image[offset:offset + 2000]
        length = str(len(chunk))
        yield (f"DISP:WS2811:DATA {offset},#{len(length)}{length}".encode("ascii") + chunk + b"\n")


def upload(link, image, count=64, pin=2, rate=800000, order="RGB", brightness=25):
    validate(count, pin, rate, order, brightness)
    payload = b"".join(data_commands(image, count))
    session = Session(link, "WS2811")
    print(session.query("*IDN?", timeout=8, prefix="CTI,"))
    session.checked(b"DISP:WS2811:STOP\n")
    try:
        for setting in (f"COUNT {count}", f"PIN {pin}", f"RATE {rate}", f"ORD {order}", "START"):
            session.checked(f"DISP:WS2811:{setting}\n".encode("ascii"))
        session.wait_swap()  # START transmits black after the initial reset-low interval.
        session.checked(f"DISP:WS2811:BRIG {brightness}\n".encode("ascii"))
        session.wait_swap()
        session.write(payload)
        session.check_error()
        if session.status()[1] != len(image):
            raise RuntimeError("Firmware did not stage a complete WS2811 frame")
        session.checked(b"DISP:WS2811:SHOW\n")
        status = session.wait_swap()
        session.check_error()
        print(f"Latched {count} RGB pixels: status={status}; LEDs retain the image without host traffic")
    except (OSError, ValueError, RuntimeError) as error:
        try:
            session.write(b"DISP:WS2811:STOP\n")
        except OSError as cleanup_error:
            error.add_note(f"Could not send STOP during cleanup: {cleanup_error}")
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="USB CDC port, e.g. COM7")
    parser.add_argument("--count", type=int, default=64)
    parser.add_argument("--pin", type=int, default=2)
    parser.add_argument("--rate", type=int, choices=(400000, 800000), default=800000)
    parser.add_argument("--order", choices=("RGB", "RBG", "GRB", "GBR", "BRG", "BGR"), default="RGB")
    parser.add_argument("--brightness", type=int, default=25)
    args = parser.parse_args()
    validate(args.count, args.pin, args.rate, args.order, args.brightness)
    import serial
    with serial.Serial(args.port, 115200, timeout=0.25, write_timeout=2) as link:
        link.reset_input_buffer()
        upload(link, pattern(args.count), args.count, args.pin, args.rate, args.order, args.brightness)


if __name__ == "__main__":
    main()
