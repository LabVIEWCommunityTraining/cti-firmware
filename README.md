# cti-firmware
Microcontroller firmware with simulation modes

[Documentation](./docs/readme.md)

[HUB75 display: Pico/Pico 2 wiring, SCPI uploads and smoke test](./docs/hub75.md)

[WS2811 LEDs: runtime PIO configuration and RGB uploads](./docs/ws2811.md)

The standard versioned Pico-family UF2 includes both services; select them
at runtime through SCPI. There are no separate display-specific firmware variants.

## Building

The CTI firmware is designed to handle a myriad of target devices and toolchains. As such, a build needs to be run for a specific platform and optionally for a specific board. Optionally, firmware for a specific operating mode can be built so look in the readme for a specific platform for available options and boards.