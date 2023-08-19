# RP2040 - Raspberry Pi Pico and derivatives

## Building

Building is accomplished in the terminal and not within VS Code, at least until someone gets the VS Code configuration straightened out.

1. With the cti-firmware folder open in VS Code, open a terminal
> ***`CTRL + ~`***
2. Navigate to the rp2040 platform
> ``cd platform\rp2040``
3. Create the build folder
> ``mkdir build``
4. Navigate to the build folder
> ``cd build``
5. Configure the build with cmake
> ``cmake -G "MinGW Makefiles" ..``
6. Run the build
> ``make``