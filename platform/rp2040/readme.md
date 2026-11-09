# RP2040 - Raspberry Pi Pico and derivatives

## HUB75 display

Pico and Pico W builds include the same optional
[HUB75 PIO/DMA BCM service](../../docs/hub75.md) as Pico 2. Configure dimensions,
GPIO groups and RGB332/RGB888 uploads through SCPI, then START; no display
resources are claimed at boot. The normal CTI VISA/peripheral implementation
remains RP2040-specific.

From the repository root with the pinned Pico SDK/tool bundle available:

```powershell
cmake -S . -B build\hub75-pico -G Ninja -DPICO_BOARD=pico -DCMAKE_BUILD_TYPE=Release
cmake --build build\hub75-pico
```

Flash `uf2\cti_visa_rp2040_pico_0.9.4.uf2`. For Pico W use a separate build directory
and `-DPICO_BOARD=pico_w`, producing `uf2\cti_visa_rp2040_pico_w_0.9.4.uf2`.
Do not flash the Pico 2 firmware onto either RP2040 board.

The same standard firmware includes the runtime
[WS2811 strip service](../../docs/ws2811.md). No display-specific UF2 is needed.

## Legacy building instructions

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

With a successful build you will have a `cti_visa-udaq_pi_pico_#.#.#.uf2` file that you can deploy to a rp2040 target.