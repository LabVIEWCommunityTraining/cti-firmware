# RP2350 - Raspberry Pi Pico 2 and Pico 2 W

This platform supports the RP2350A-based Raspberry Pi Pico 2 and Pico 2 W
boards with USB stdio/VISA/SCPI, GPIO, ADC and temperature measurement, PWM,
UART, I2C, SPI, timers, unique ID, and the board status LED.

## Prerequisites

The repository is pinned to Pico SDK 2.3.1, Arm GNU Toolchain 15.2.Rel1,
picotool 2.3.1, CMake 4.3.4, and Ninja 1.13.2. The official Raspberry Pi Pico
VS Code extension can install and select this coherent tool bundle.

## Raspberry Pi Pico 2

From the repository root:

```powershell
cmake -S . -B build\pico2 -G Ninja -DPICO_BOARD=pico2
cmake --build build\pico2
```

The versioned UF2 is generated in the build directory and copied to:

```text
uf2\cti_visa_rp2350_pico2_0.9.4.uf2
```

## Raspberry Pi Pico 2 W

From the repository root:

```powershell
cmake -S . -B build\pico2_w -G Ninja -DPICO_BOARD=pico2_w
cmake --build build\pico2_w
```

The versioned UF2 is generated in the build directory and copied to:

```text
uf2\cti_visa_rp2350_pico2_w_0.9.4.uf2
```

Hold **BOOTSEL** while connecting the board, then copy the matching UF2 to the
mounted `RP2350` mass-storage volume. The Pico 2 W status LED is controlled
through the CYW43 wireless chip; all other CTI functionality uses the
Pico-compatible header pinout.
