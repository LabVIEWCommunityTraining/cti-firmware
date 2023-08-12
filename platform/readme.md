## CTI Platform Directory

This directory handles the differences between different targets and toolchains. For 2 examples: AVR chips (older Arduinos using the avr-gcc toolchain) and RP2040 which is the Raspberry Pi Pico chip using the pico-sdk. Each platform's folder will include different configuration and source files that accomodate different board styles, such as the myriad of boards based around the RP2040 chip like SFPMicro, Pi Pico, WizFi360, etc. Ideally anything in this platform directory will only be toolchain/chip/board specific and anything common will reside in the root source folder.

The platform directory also contains all build tooling whether common or for a specific toolchain.

Current supported platforms/boards:

* RP2040 (pico-sdk toolchain)
  * Pi Pico (USB only)
  * WizFi360-EVB-Pico (WiFi + USB)

Planned support, in no particular order:
* AVR (Older Arduinos, avr-gcc toolchain)
  * ATmega328p (Arduino Uno)
  * ATmega1280/2560 (Arduino Mega)
* ESP32