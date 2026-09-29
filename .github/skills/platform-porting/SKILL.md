---
name: platform-porting
description: Add a new MCU family or board while preserving existing CTI firmware targets, build selection, and SCPI behavior.
---

# CTI platform-porting skill

Use this guidance when adding an MCU family or board (for example ESP32) to CTI firmware. The goal is a separate, validated target—not a broad refactor of the working RP2040, RP2350, or AVR paths.

## Non-negotiable invariants

- Add a sibling platform implementation under `platform/<family>/`; do not rewrite an existing family to make it generic.
- Keep shared SCPI, VISA, command parsing, and application logic in `source/` and `include/`. Platform code owns hardware and toolchain boundaries.
- Keep board-specific pin maps, LEDs, boot behavior, and networking in `platform/<family>/boards/<board>/`.
- Make top-level build changes only for platform dispatch, target registration, or shared integration that is required by the new target.
- Preserve existing target names, artifact names, protocol bytes, and behavior. A new target must not change what an existing target builds.
- Never assume a familiar peripheral has the same channel numbering, clock, interrupt, reset, or electrical behavior on another MCU.

These rules reflect the RP2350 port: it was added beside RP2040 because PWM clocks, ADC temperature selection, board identity, and peripheral details were not interchangeable. They also reflect the Uno port: constrained memory and the Optiboot/DTR reset window required target-local buffering and host timing guidance rather than changes to the shared protocol.

## Before editing

1. Identify the exact MCU, SDK/toolchain, board/module revisions, bootloader, and intended artifact (`UF2`, `HEX`, `BIN`, etc.).
2. Read `include/cti/platform.h` and related platform headers to inventory the contract: communication, GPIO, ADC, PWM, UART, I2C, SPI, timers, unique ID, status LED, and board identity.
3. Trace how the top-level `CMakeLists.txt` selects a platform and how an existing sibling registers sources, compile definitions, SDK libraries, and board files.
4. Choose the nearest working reference by behavior, not by brand. RP2040/RP2350 are useful for Pico SDK structure; AVR is useful for small-memory and bootloader constraints.
5. Record the baseline build commands and artifact names for at least one existing target before changing files.

## Recommended layout

```text
platform/<family>/
  CMakeLists.txt                 # only if the family needs one
  <family>.inc.cmake             # family/toolchain integration when applicable
  platform/                      # family-level drivers and platform contract
  boards/<board>/                # board pin map, init, LED, optional network files
```

Keep one source file per hardware concern when that matches the existing family. Prefer a narrow family implementation over preprocessor branches scattered through shared code. Add a new board directory when pinout, LED wiring, boot mode, or module support differs materially.

## Port in layers

Implement and test in this order:

1. Build selection and minimal startup.
2. Host transport and serial initialization.
3. Board identity (`*IDN?`), reset behavior, and status LED.
4. GPIO and digital I/O.
5. ADC and temperature reporting.
6. PWM, deriving frequency from the actual system/peripheral clock.
7. UART, I2C, and SPI with the MCU's real peripheral and pin-mux rules.
8. Timers, unique ID, optional network support, and packaging/install output.

Do not claim parity because a firmware image links. Every implemented command must either work on the target or be explicitly unsupported using the repository's normal error behavior.

## Protocol and host behavior

Treat the Pico response format and SCPI framing as the compatibility contract:

- Preserve line endings, binary block layout, byte order, and response sequencing.
- Test exact command bytes, including `*IDN?\n`, not only interactive terminal input.
- Exercise unknown headers, blank frames, repeated commands, and binary payloads containing `0x0A` so the parser cannot desynchronize.
- Check startup output. A bootloader must not emit bytes that look like a response or error.
- Document reset-on-open behavior. Uno/Optiboot resets when DTR changes, so hosts should keep the port open and wait at least two seconds after opening, or avoid toggling DTR. Treat this as a transport/boot constraint, not a reason to alter SCPI parsing.
- For USB-native or Wi-Fi boards, define when the transport is ready and make the host-visible behavior explicit.

## Build isolation

- Add only the new platform/board to the build graph; do not change existing platform source lists unless shared registration requires it.
- Keep separate build directories per target (`build\\pico`, `build\\pico2`, `build\\<new-board>`). Never use a stale cache to infer support.
- Capture exact configure and build commands in the family README, including required SDK/toolchain versions and expected output paths.
- Keep generated firmware in the repository's established artifact location; do not commit large intermediate build trees unless the project already does so.

## Validation gate

Before merging a new family or board:

- Clean-configure and build every new board variant.
- Regression-build at least one RP2040 and one RP2350 target; include AVR when the change touches shared protocol or build dispatch.
- Verify identity, startup/reset timing, GPIO, ADC/temperature, PWM, serial buses, timers, status LED, and unique ID on hardware where available.
- Compare representative SCPI responses byte-for-byte with the canonical implementation, including binary responses.
- Inspect the diff for accidental edits to existing platform directories and confirm old artifact names are unchanged.
- Record hardware-only limitations (bootloader delay, DTR reset, pin conflicts, ADC restrictions, wireless LED control) beside the build instructions.

## ESP32 starting point

For ESP32, first choose the exact family and framework (ESP-IDF or the repository's supported Arduino integration), then add `platform/esp32/` and one board directory per module. Resolve GPIO matrix/pin-strap restrictions, ADC attenuation and calibration, LEDC PWM clock behavior, UART/I2C/SPI controller assignment, flash/partition layout, and reset/log output before attempting feature parity. Keep Wi-Fi/BLE and FreeRTOS integration board-local until the core CTI transport and command behavior pass the validation gate.

### ESP32 porting lessons

Apply these checks before writing platform code:

- Start from the exact board/module pinout or schematic, not the product name or a similar board. Heltec-style ESP32 LoRa boards commonly differ from other ESP32 LoRa boards: for the validated HTIT-WB32LA/Heltec-style layout, OLED uses SDA `GPIO4`, SCL `GPIO15`, and reset `GPIO16`; LoRa uses MOSI `GPIO27`, MISO `GPIO19`, SCK `GPIO5`, CS `GPIO18`, reset `GPIO14`, and DIO0 `GPIO26`; the status LED is `GPIO25`.
- Put every board-specific pin in `boards/<board>/cti_board.h`. Do not leave OLED, LoRa, LED, or ADC pins hardcoded in family-level files.
- Treat `LVBlock` capability responses as a wire-format contract. The declared `len`, the payload byte count, and every embedded count must agree. A mismatch causes SCPI binary-block desynchronization or trailing bytes. Before building, count each descriptor explicitly for GPIO, ADC, PWM, UART, I2C, and SPI, then compare the decoded response with a known-good Pico response.
- ESP32 ADC resources require two mappings: the advertised physical GPIO and the ESP-IDF ADC channel. Do not pass a logical resource index directly to `adc_oneshot_*`. On classic ESP32, exposed ADC1 GPIOs 36-39 map to ADC1 channels 0-3; avoid ADC2 when Wi-Fi may be enabled.
- Exclude GPIO6-11 from general-purpose, PWM, and UART capability lists on classic ESP32 modules because they are normally connected to SPI flash. Also exclude board-reserved OLED/LoRa/LED pins from generic GPIO/PWM lists unless the protocol intentionally exposes shared-function pins.
- Use ESP32 SPI2/SPI3 hosts for user SPI. SPI1 is normally reserved for flash and must not be selected for application transactions. Advertise the actual MOSI/MISO/SCK pins from the board definition, and configure the same host in `spi_bus_initialize` and `spi_bus_add_device`.
- UART0 is the USB/UART console. A direct `uart_read_bytes(UART_NUM_0, ...)` timeout reader requires a UART driver to be installed; initialize it once and tolerate `ESP_ERR_INVALID_STATE` if the console already owns the driver. Do not advertise UART2 on GPIO16/17 when those pins are used by OLED reset or other board functions.
- ESP-IDF `GPIO_NUM_MAX` and `CONFIG_SOC_GPIO_IN_RANGE_MAX` are useful bounds, not a board capability list. Build the advertised GPIO list from the module pinout and flash/strap restrictions. A binary list containing nonexistent GPIO50+ values is invalid even if the descriptor length is correct.
- Keep board selection dynamic in the ESP-IDF component CMake file. Resolve `boards/${CTI_BOARD}`, include its directory, compile its `.cpp` files, and derive `_CTI_BOARD` from the selected board. Never hardcode a legacy board source or board macro in the component registration.
- Use a clean, board-specific build directory after changing CMake, board selection, or descriptor structure. Verify `*IDN?`, every `*:AVAIL?` response, and representative read/write commands on hardware; a successful link or flash is not resource validation.

## Arduino porting lessons

For Arduino-compatible targets, distinguish the MCU, bootloader, USB bridge, and Arduino core. A board name alone does not define the serial reset behavior, pin numbering, timer layout, ADC reference, or upload protocol.

- Treat the Arduino pin namespace as part of the public contract. Document whether a resource uses raw MCU GPIO numbers or Arduino pin numbers, and keep the conversion in the board definition. Do not mix `A0`/`D13` names with raw GPIO values in capability descriptors.
- Reserve the control UART before advertising other UART resources. On an Arduino Uno, D0/D1 are owned by the ATmega16U2 USB bridge and SCPI transport, so the firmware must not expose them as a second user UART.
- Account for bootloader timing and DTR behavior. Optiboot normally resets when the virtual COM port opens and owns the UART briefly; hosts should wait at least one second, preferably two, before sending the first command. A partial command received during reset cannot be repaired by the application.
- Keep startup output intentional. If the canonical target is silent during reset, do not add debug text that can be mistaken for a SCPI response. Preserve LF bytes exactly; never translate binary payload bytes to CRLF.
- Derive PWM availability from actual timers and pins, not from a generic Arduino list. On the Uno, D5/D6 share Timer0 with the firmware timebase, D9/D10 share Timer1, and D3/D11 share Timer2. Document shared-timer side effects and reject unsupported phase-correct, top, divider, or frequency operations explicitly.
- Derive ADC availability from the real ADC mux and reference. Document resolution, voltage reference, attenuation, and whether an analog pin is also advertised as digital I/O. Do not advertise internal temperature or DAC resources unless the MCU provides a dependable implementation.
- Keep capability descriptors exact on small-memory targets. Check `LVBlock.len`, payload bytes, embedded counts, and the SCPI block header together. Use bounded static buffers and avoid heap-backed parser or transfer structures when the MCU's SRAM is constrained.
- Keep I2C, SPI, and UART transfers bounded by the board's memory profile. Verify mode, word size, clock-divider quantization, pin ownership, and transaction length against the MCU datasheet; do not assume the ESP32 or Pico implementation can be copied to AVR.
- Pin the Arduino toolchain and upload protocol in the board README. Record compiler/core version, `avrdude` programmer, baud rate, MCU part, reset behavior, artifact type, and the exact flash command. Build and upload from separate target directories.
- Validate with one persistent serial session: open the port, wait through reset, send exactly `*IDN?\n`, read through LF, then repeat without reopening. Also test every `*:AVAIL?` response byte-for-byte and verify that binary payloads containing `0x0A` do not desynchronize the parser.

## Completion checklist

- [ ] New family is a sibling platform; existing targets are not generalized or rewritten.
- [ ] Board files contain pin, LED, boot, identity, and module-specific behavior.
- [ ] All platform contract surfaces are implemented or explicitly unsupported.
- [ ] Exact SCPI framing and response bytes match the compatibility contract.
- [ ] New and regression builds are clean from separate build directories.
- [ ] Hardware caveats and exact artifact names are documented.
