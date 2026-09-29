import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def lvblock(source, name):
    text = (ROOT / source).read_text(encoding="utf-8")
    match = re.search(
        rf"LVBlock\s+{name}\s*=\s*\{{\s*(\d+)\s*,\s*\{{(.*?)\}}\s*\}};",
        text,
        re.DOTALL,
    )
    if not match:
        raise AssertionError(f"LVBlock {name} not found in {source}")
    body = re.sub(r"//.*", "", match.group(2))
    payload = bytes(int(token, 0) for token in re.findall(r"0x[0-9A-Fa-f]+|\d+", body))
    declared_length = int(match.group(1))
    if declared_length != len(payload):
        raise AssertionError(
            f"{name} declares {declared_length} bytes but initializes {len(payload)}"
        )
    return payload


def avr_block(payload):
    length = str(len(payload)).encode("ascii")
    return b"#" + str(len(length)).encode("ascii") + length + payload + b"\n"


def command_patterns():
    patterns = {}
    for source in (ROOT / "source" / "visa").glob("*.cpp"):
        text = source.read_text(encoding="utf-8")
        for pattern, command, query in re.findall(
            r'addCommand\("([^"]+)",\s*([^,]+),\s*([^)]+)\)', text
        ):
            patterns[pattern] = (command.strip(), query.strip())
    return patterns


def match_segment(pattern, candidate):
    has_number = pattern.endswith("#")
    if has_number:
        pattern = pattern[:-1]

    text = candidate
    number = -1
    if has_number:
        match = re.search(r"(\d+)$", candidate)
        if not match or int(match.group(1)) > 127:
            return None
        text = candidate[: match.start()]
        number = int(match.group(1))

    required = 0
    while required < len(pattern) and not pattern[required].islower():
        required += 1
    if len(text) not in (required, len(pattern)):
        return None
    if text.upper() != pattern[: len(text)].upper():
        return None
    return number


def matches(pattern, candidate):
    pattern_parts = pattern.removeprefix(":").split(":")
    candidate_parts = candidate.removeprefix(":").split(":")
    if len(pattern_parts) != len(candidate_parts):
        return False
    return all(
        match_segment(pattern_part, candidate_part) is not None
        for pattern_part, candidate_part in zip(pattern_parts, candidate_parts)
    )


class FlatRegistryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.patterns = command_patterns()

    def handlers_for(self, command):
        return [
            handlers
            for pattern, handlers in self.patterns.items()
            if matches(pattern, command)
        ]

    def test_digital_available_short_form_selects_only_digital_handler(self):
        self.assertEqual(
            self.handlers_for("DIG:AVAIL"),
            [("nullptr", "digital_available")],
        )

    def test_digital_available_long_form_selects_only_digital_handler(self):
        self.assertEqual(
            self.handlers_for("DIGITAL:AVAILABLE"),
            [("nullptr", "digital_available")],
        )

    def test_idn_selects_only_idn_handler(self):
        self.assertEqual(
            self.handlers_for("*IDN"),
            [("nullptr", "SCPI_CoreIdnQ")],
        )

    def test_unknown_query_has_no_default_handler(self):
        self.assertEqual(self.handlers_for("DIG:INVALID"), [])
        self.assertEqual(self.handlers_for("UNKNOWN"), [])

    def test_every_registered_short_and_long_form_is_unambiguous(self):
        for pattern in self.patterns:
            segments = pattern.split(":")
            short = ":".join(
                segment.rstrip("#")[: next(
                    (
                        index
                        for index, character in enumerate(segment.rstrip("#"))
                        if character.islower()
                    ),
                    len(segment.rstrip("#")),
                )]
                + ("0" if segment.endswith("#") else "")
                for segment in segments
            )
            long = pattern.replace("#", "0")
            with self.subTest(pattern=pattern, spelling="short"):
                self.assertEqual(len(self.handlers_for(short)), 1)
            with self.subTest(pattern=pattern, spelling="long"):
                self.assertEqual(len(self.handlers_for(long)), 1)

    def test_avr_registry_capacity_matches_registered_command_count(self):
        cmake = (ROOT / "platform/avr/avr.inc.cmake").read_text(encoding="utf-8")
        capacity = int(re.search(r"SCPI_MAX_COMMANDS=(\d+)", cmake).group(1))
        self.assertEqual(len(self.patterns), 36)
        self.assertEqual(capacity, len(self.patterns))


class UnoGoldenWireTests(unittest.TestCase):
    def test_digital_availability(self):
        payload = lvblock("platform/avr/platform/avr_io.cpp", "availableGPIOs")
        self.assertEqual(payload, bytes([0, 0, 0, 18, *range(2, 20)]))
        self.assertEqual(
            avr_block(payload),
            bytes.fromhex(
                "23 32 32 32 00 00 00 12 02 03 04 05 06 07 08 09 "
                "0A 0B 0C 0D 0E 0F 10 11 12 13 0A"
            ),
        )

    def test_analog_availability(self):
        payload = lvblock("platform/avr/platform/avr_io.cpp", "availableAnalogInputs")
        self.assertEqual(
            payload,
            bytes.fromhex("00 00 00 06 00 0E 01 0F 02 10 03 11 04 12 05 13"),
        )
        self.assertEqual(
            avr_block(payload),
            bytes.fromhex(
                "23 32 31 36 00 00 00 06 00 0E 01 0F 02 10 "
                "03 11 04 12 05 13 0A"
            ),
        )

    def test_pwm_availability(self):
        payload = lvblock("platform/avr/platform/avr_pwm.cpp", "availablePwms")
        self.assertEqual(payload, bytes.fromhex("00 00 00 06 03 05 06 09 0A 0B"))
        self.assertEqual(
            avr_block(payload),
            bytes.fromhex("23 32 31 30 00 00 00 06 03 05 06 09 0A 0B 0A"),
        )

    def test_uart_availability_is_empty_count(self):
        payload = lvblock("platform/avr/platform/avr_comms.cpp", "availableUarts")
        self.assertEqual(payload, bytes.fromhex("00 00 00 00"))
        self.assertEqual(avr_block(payload), b"#14\x00\x00\x00\x00\n")

    def test_i2c_availability(self):
        payload = lvblock("platform/avr/platform/avr_comms.cpp", "availableI2c")
        self.assertEqual(
            payload,
            bytes.fromhex("00 00 00 01 00 00 00 00 01 13 00 00 00 01 12"),
        )
        self.assertEqual(
            avr_block(payload),
            bytes.fromhex(
                "23 32 31 35 00 00 00 01 00 00 00 00 01 13 "
                "00 00 00 01 12 0A"
            ),
        )

    def test_spi_availability(self):
        payload = lvblock("platform/avr/platform/avr_comms.cpp", "availableSpi")
        self.assertEqual(
            payload,
            bytes.fromhex(
                "00 00 00 01 00 00 00 00 01 0B "
                "00 00 00 01 0C 00 00 00 01 0D"
            ),
        )
        self.assertEqual(
            avr_block(payload),
            bytes.fromhex(
                "23 32 32 30 00 00 00 01 00 00 00 00 01 0B "
                "00 00 00 01 0C 00 00 00 01 0D 0A"
            ),
        )

    def test_avr_stdio_does_not_translate_binary_lf(self):
        source = (ROOT / "platform/avr/src/avr_serial.cpp").read_text(encoding="utf-8")
        self.assertNotIn("value == '\\n'", source)
        self.assertNotIn("uartPutchar('\\r'", source)

    def test_pwm_real_format_matches_pico_precision(self):
        source = (ROOT / "source/visa/pwm.cpp").read_text(encoding="utf-8")
        self.assertIn('"%s%lu.%06lu\\n"', source)
        self.assertIn('"%f\\n"', source)

    def test_shared_line_and_block_framing(self):
        digital = (ROOT / "source/visa/digital.cpp").read_text(encoding="utf-8")
        visa = (ROOT / "source/visa/visa_core.cpp").read_text(encoding="utf-8")
        self.assertIn('"1\\n"', digital)
        self.assertIn('"0\\n"', digital)
        self.assertIn('"OUT\\n"', digital)
        self.assertIn('"IN\\n"', digital)
        self.assertIn('"NONE\\n"', digital)
        self.assertIn('"UP\\n"', digital)
        self.assertIn('"DOWN\\n"', digital)
        self.assertIn('"BOTH\\n"', digital)
        self.assertIn('"#%u%u"', visa)
        self.assertIn('"%d, %s\\n"', visa)

    def test_uart_ring_wrap_preserves_an_idn_query(self):
        source = (ROOT / "platform/avr/src/avr_serial.cpp").read_text(encoding="utf-8")
        capacity = int(re.search(r"RxBufferSize = (\d+)", source).group(1))
        self.assertEqual(capacity, 32)

        storage = bytearray(capacity)
        head = 29
        tail = 29
        query = b"*IDN?\n"
        for byte in query:
            next_head = (head + 1) % capacity
            self.assertNotEqual(next_head, tail)
            storage[head] = byte
            head = next_head

        received = bytearray()
        while head != tail:
            received.append(storage[tail])
            tail = (tail + 1) % capacity
        self.assertEqual(received, query)


if __name__ == "__main__":
    unittest.main()
