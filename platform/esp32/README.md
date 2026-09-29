# ESP32 board support

The ESP32 target is organized as a family-level platform plus per-board
configuration packages under `platform/esp32/boards/`. Each board keeps its own
pin map, LED init, and board identity definition. That lets CTI add a new ESP32
variant by creating a new board directory instead of rewriting the shared ESP32
platform code.

This repo currently supports the common Heltec/HTIT-style 915 MHz ESP32 LoRa
layout via the `htit-wb32la` board definition, and it also retains the original
`wishio-lora-915` target for compatibility with earlier board files.

## Supported board definitions

- `htit-wb32la` - HTIT-WB32LA / Heltec-style ESP32 LoRa + OLED board
- `wishio-lora-915` - original WIshioT 915 MHz board definition

The shared pin map for these boards follows the common ESP32 LoRa/OLED layout:
OLED I2C uses SDA GPIO 21 and SCL GPIO 22, the radio uses SPI MOSI 27, MISO 19,
SCK 5, CS 18, reset 14, and DIO0 26, and the status LED is GPIO 25. Verify the
silkscreen against the purchased board before connecting external hardware
because board listings can vary by revision.

## Build examples

```powershell
idf.py -C platform/esp32 -B build\esp32-htit-wb32la set-target esp32
idf.py -C platform/esp32 -B build\esp32-htit-wb32la build
```

```powershell
idf.py -C platform/esp32 -B build\esp32-wishio-lora-915 set-target esp32
idf.py -C platform/esp32 -B build\esp32-wishio-lora-915 build
```

To add another ESP32 board, create a new directory under
`platform/esp32/boards/<board-name>/` with:

- `cti_board.h` for the board identity and GPIO map
- `cti_board.cmake` to register the board-specific source file
- `<board>_board.cpp` for LED and other board setup

The ESP32 platform code itself stays board-agnostic; only the board definition and
setup code change when a new module is introduced.
