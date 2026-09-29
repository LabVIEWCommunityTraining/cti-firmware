# cti-firmware
Microcontroller firmware with simulation modes

[Documentation](./docs/readme.md)

## Building

The CTI firmware is designed to handle a myriad of target devices and toolchains. As such, a build needs to be run for a specific platform and optionally for a specific board. Optionally, firmware for a specific operating mode can be built so look in the readme for a specific platform for available options and boards.

### Arduino Uno

Configure from the repository root with the Arduino AVR GCC toolchain:

```powershell
$avr = "$env:LOCALAPPDATA\Arduino15\packages\arduino\tools\avr-gcc\7.3.0-atmel3.6.1-arduino7"
$ninja = "$env:USERPROFILE\.pico-sdk\ninja\v1.13.2\ninja.exe"
cmake -S . -B build-uno -G Ninja -DCMAKE_MAKE_PROGRAM="$ninja" -DCTI_PLATFORM=avr -DCTI_BOARD=arduino-uno -DAVR_TOOLCHAIN_ROOT="$avr" -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build-uno
```

The flashable artifact is copied to `hex/cti_visa_avr_arduino-uno_0.9.4.hex`.
See [the Arduino Uno platform documentation](platform/avr/readme.md) for
supported interfaces, memory limits, and the exact `avrdude` upload command.