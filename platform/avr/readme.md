# Arduino Uno / ATmega328P

The AVR target builds first-class CTI VISA/SCPI firmware for the classic
Arduino Uno at 16 MHz. The Uno's ATmega16U2 USB bridge exposes the ATmega328P
hardware UART as a virtual serial port.

## Toolchain and build

The validated Windows toolchain is the Arduino AVR Boards 1.8.6 bundle:

- AVR GCC `7.3.0-atmel3.6.1-arduino7`
- avrdude `6.3.0-arduino17`
- Ninja `1.13.2`
- CMake 3.20 or newer

The build automatically searches the standard Arduino15 package directory,
but specifying the pinned compiler path makes the selected toolchain
unambiguous:

```powershell
$avr = "$env:LOCALAPPDATA\Arduino15\packages\arduino\tools\avr-gcc\7.3.0-atmel3.6.1-arduino7"
$ninja = "$env:USERPROFILE\.pico-sdk\ninja\v1.13.2\ninja.exe"
cmake -S . -B build-uno -G Ninja -DCMAKE_MAKE_PROGRAM="$ninja" -DCTI_PLATFORM=avr -DCTI_BOARD=arduino-uno -DAVR_TOOLCHAIN_ROOT="$avr" -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build-uno
```

The build emits ELF, map, listing, and Intel HEX files in `build-uno` and
copies the flashable image to:

```text
hex/cti_visa_avr_arduino-uno_0.9.4.hex
```

The post-build `avr-size` report is authoritative for flash and static SRAM
usage. The AVR configuration uses bounded static parser tables, a 64-byte
SCPI parameter buffer, 16-byte peripheral transfer buffers, and a 32-byte
serial receive buffer; it does not allocate the SCPI command tree from the
heap.

## Upload

Replace `COM3` with the Uno serial port. This is the standard Uno Optiboot
protocol (`arduino`, 115200 baud, ATmega328P):

```powershell
$avrdude = "$env:LOCALAPPDATA\Arduino15\packages\arduino\tools\avrdude\6.3.0-arduino17"
& "$avrdude\bin\avrdude.exe" -C "$avrdude\etc\avrdude.conf" -v -p atmega328p -c arduino -P COM3 -b 115200 -D -U "flash:w:hex\cti_visa_avr_arduino-uno_0.9.4.hex:i"
```

After reset, connect at **115200 baud, 8 data bits, no parity, one stop bit**.
Commands are newline terminated. `*IDN?` reports vendor `CTI`, model
`Arduino-Uno-ATmega328P`, serial `UNAVAILABLE`, and the firmware version.
The AVR command channel is silent during boot and reset. The first received
bytes are therefore the response to the first command sent after the device is
ready; the firmware does not issue `*IDN?` or send an IDN response
automatically.

Opening the Uno virtual COM port normally pulses DTR and resets the ATmega328P.
The Optiboot bootloader owns UART0 for roughly 0.5 seconds before the firmware
and SCPI parser exist. A query transmitted during that interval can be consumed
or truncated by the bootloader and cannot be recovered by application
firmware. After opening the port, wait at least one second before the first
query (two seconds is a conservative cross-host value), or configure the host
not to toggle DTR when the serial API permits it. Do not retry by appending to a
partial line: send a newline to discard it, then resend the complete query.

A byte-level identity check should use one persistent serial session:

1. Open at 115200 8N1 and wait two seconds (or open without toggling DTR).
2. Discard any host-side bytes retained from an older session. Current firmware
   emits no reset text.
3. Write exactly `2A 49 44 4E 3F 0A` (`*IDN?\n`).
4. Read through LF. The response must be exactly
   `CTI,Arduino-Uno-ATmega328P,UNAVAILABLE,<version>\n`.
5. Repeat steps 3-4 without reopening the port; every query must return one IDN
   line. `*IDN?\r\n` is accepted as well.

If step 3 is intentionally performed immediately after a DTR reset, no response
is guaranteed: Optiboot, not this firmware, owns UART0. If only a suffix such as
`IDN?\n` reaches the application, it is correctly rejected as unknown. Wait for
application startup and resend the complete six-byte query.

Digital availability accepts either the SCPI short form `DIG:AVAIL?` or full
form `DIGital:AVAILable?`. The response is an IEEE-style arbitrary block, not a
text line: `#222`, followed by 22 binary bytes
`00 00 00 12 02 03 ... 13`, followed by LF. The first four binary bytes
encode an 18-element list, and the remaining bytes are Arduino pins D2-D19.

## Supported interfaces

| Surface | Support and constraints |
|---|---|
| VISA/SCPI stdio | UART0 through the Uno USB serial bridge at 115200 baud |
| Status LED | Built-in LED on D13 |
| Digital GPIO | D2-D13 and A0-A5 (Arduino pin numbers 14-19); D0/D1 are reserved for SCPI |
| Pull resistors | Internal pull-up or none; the ATmega328P has no pull-down |
| ADC | A0-A5, 10-bit, AVcc reference |
| PWM | D3, D5, D6, D9, D10, D11 |
| I2C/TWI | Bus 0 on SCL=A5/D19 and SDA=A4/D18 |
| SPI | Bus 0, modes 0-3, 8-bit words, MOSI=D11, MISO=D12, SCK=D13 |
| Timers | Timer0 provides `millis`/`micros` and timeout behavior; Timer1/2 back PWM |

PWM resources follow the ATmega328P hardware:

- D5/D6 share Timer0 with the firmware timebase. Duty control is supported at
  the fixed fast-PWM frequency (approximately 976.56 Hz); phase-correct mode,
  frequency, top, and divider changes are rejected.
- D9/D10 share 16-bit Timer1. Frequency, top, divider, enable, and duty are
  supported; changing timer configuration affects both pins.
- D3/D11 share 8-bit Timer2. Duty and prescaler-quantized frequency/divider are
  supported; programmable top is not.

I2C and SPI transactions are blocking. Their SCPI binary payloads are limited
to 16 bytes by the Uno memory profile. D13 is shared by the status LED and SPI
SCK, so the LED follows clock activity while SPI owns the pin.

`ANAlog:AVAILable?` returns `#216`, followed by the 16-byte payload
`00 00 00 06 00 0E 01 0F 02 10 03 11 04 12 05 13`, followed by LF. This
matches the Pico schema: a big-endian U32 channel count and one
`(ADC channel, GPIO pin)` byte pair per channel.

AVR serial output does not translate LF bytes to CRLF. This matches Pico and is
required for binary blocks: a payload byte `0A` must remain one byte and cannot
be expanded without invalidating the advertised block length.

## Explicitly unsupported

- A separate VISA UART bus is not advertised. The ATmega328P has one hardware
  UART and it is reserved for the SCPI control plane on D0/D1.
- Internal temperature is not exposed. The ATmega328P interface does not
  provide a dependable factory-calibrated board temperature reading.
- A unit-unique serial number is not available from the ATmega328P. Returning a
  fabricated or board-generic identifier would be misleading, so `*IDN?`
  reports `UNAVAILABLE`.
- Analog output/DAC and internal pull-down resistors are not present.

Compilation, linking, HEX generation, and memory sizing can be validated
without hardware. Upload, serial enumeration, electrical behavior, ADC
accuracy, bus interoperability, and timer waveforms require an actual Uno.
