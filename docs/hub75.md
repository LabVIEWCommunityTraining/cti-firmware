# HUB75 display (RP2040 / RP2350 Pico boards)

The default build supports one conventional **64x64, 1/32-scan HUB75 panel**, with
ABCDE row addressing and no special initialization registers. Dimensions, pin
groups and RGB332/RGB888 image format are selected by SCPI while stopped.
The normal VISA engine remains in control; HUB75 allocates RAM and loads its
bundled PIO program only on START, not at boot. It is not a
generic panel mapper, arbitrary PIO loader, or chained-panel implementation.
Pico, Pico W, Pico 2 and Pico 2 W builds bundle the same PIO program and
register the same display commands. AVR builds do not register display
commands. No host-supplied machine code is executed. The shared engine and
PIO source are `platform\common\hub75.cpp` and `platform\common\hub75.pio`;
the RP2040/RP2350 platform implementations remain separate.

## Wiring

| Panel signal | Pico / Pico 2 GPIO |
|---|---|
| R0, G0, B0 | GP0, GP1, GP2 |
| R1, G1, B1 | GP3, GP4, GP5 |
| A, B, C, D, E | GP6, GP7, GP8, GP9, GP10 |
| CLK | GP11 |
| LAT | GP12 |
| /OE (active low) | GP13 |

Use the panel's input connector and a common ground. Panel power is external:
do not power a panel from a GPIO. Verify that the panel accepts 3.3 V signals
or use suitable level translation. An external /OE pull-up is recommended to
keep the panel blank during power-up/reset, before firmware takes ownership.

Row address `r` selects image rows `r` (R0/G0/B0) and `r+height/2` (R1/G1/B1).
Columns shift left-to-right, x=0 first. Some physical panels reverse column
order or use different internal scan mappings; these need bench confirmation.

## Commands

Send one command per LF-terminated message. CRLF is also accepted. Long and
short SCPI spellings below are equivalent.

| Command | Behavior |
|---|---|
| `DISPlay:HUB75:DIMensions <width>,<height>` / `DIMensions?` | Set/query runtime dimensions. Width 8..256; height 16, 32 or 64. Setter requires STOP. |
| `DISPlay:HUB75:PINS <R0>,<G0>,<B0>,<R1>,<G1>,<B1>,<A>,<B>,<C>,<D>,<E>,<CLK>,<LAT>,<OE>` / `PINS?` | Set/query fourteen GPIO numbers in exactly this order. Setter requires STOP; grouped-pin constraints below. |
| `DISPlay:HUB75:FORMat RGB332\|RGB888` / `FORMat?` | Set/query raw image format while stopped. Firmware boots in RGB888 for compatibility; select RGB332 for one byte per pixel. |
| `DISPlay:HUB75:START` | Claim configured pins, one PIO state machine, fourteen instructions, one DMA channel and a shared DMA IRQ0 handler. Refresh black at 25% brightness. Repeated START while running is a no-op. |
| `DISPlay:HUB75:INIT` | Backward-compatible alias for START, using the current configuration. |
| `DISPlay:HUB75:DATA <offset>,#<n><length><bytes>` | Stage 1..2000 raw bytes in the selected format. Offset is a decimal byte offset, not a pixel index. |
| `DISPlay:HUB75:SHOW` | Queue a complete staged image for the next DMA frame boundary; does not wait for that boundary. |
| `DISPlay:HUB75:BRIGhtness <percent>` | Integer 0..100. Queue a brightness-only frame-boundary change without disturbing a partial upload. Zero blanks the image while refresh continues. |
| `DISPlay:HUB75:STATus?` | Return `running,received,pending,brightness,frames`, five comma-separated integers followed by LF. |
| `DISPlay:HUB75:GEOMetry?` | Return `width,height,frameBytes,colorBits`, e.g. `64,64,4096,4` in RGB332. Last field is scan depth per channel, not upload bits/pixel. Available while stopped or running. |
| `DISPlay:HUB75:STOP` | Blank /OE high; stop refresh; free RAM, DMA, state machine, instruction memory and this IRQ handler. Repeated STOP is safe. |
| `*RST` | Also stops and blanks HUB75; otherwise retains the existing firmware's reset behavior. |
| `SYSTem:ERRor?` | Pop the standard SCPI error queue; `0, No error` means empty. |

`received` is the number of contiguous staged bytes, 0..frameBytes. `pending` is
one while a queued image or brightness change awaits a boundary.
`brightness` is the most recently accepted setting. `frames` counts DMA
frame completions since START, not measured optical refreshes. Allow the
last few FIFO words to drain after a boundary before interpreting the panel
optically. STOP sets running/received/pending to zero; brightness and frames
retain their last values until the next START. Dimensions, format and pin
configuration survive STOP and `*RST`, but are not stored across reboot.

Images are row-major, top-to-bottom and left-to-right, with no file header,
padding, alpha, compression or gamma correction:

| Format | Layout | 64x64 image |
|---|---|---|
| RGB332 | One byte per pixel: bits 7..5 red, 4..2 green, 1..0 blue. Red/green levels 0..7 map to BCM levels `0,2,4,6,9,11,13,15`; blue levels map to `0,5,10,15`. Black=0x00, red=0xE0, green=0x1C, blue=0x03, white=0xFF. | 4096 bytes |
| RGB888 | Three unsigned bytes per pixel, R then G then B; upper four bits of each byte drive the BCM planes (16 levels/channel). Original default format. | 12288 bytes |

Start each new upload at offset 0. Continue with exactly the next offset:
0, 2000, 4000 for 64x64 RGB332 (2000, 2000, 96-byte chunks). A valid chunk at offset 0
restarts staging; other gaps, overlaps, empty or oversized chunks and
out-of-range offsets fail without modifying staging. Chunk lengths need
not be multiples of three. SHOW rejects incomplete images and resets
received to zero on acceptance. No active scan RAM is modified by DATA.
DATA can stage the next image while a swap is pending. SHOW and brightness
changes are rejected while pending; poll STATUS? until pending=0 before
retrying. START/INIT/STOP accept no parameters. Configuration setters validate
the entire proposed configuration before committing it and reserve nothing.
They are rejected while running or while failed shutdown retains resources.

The SCPI block header is ASCII `#`, a single digit giving the number of
length digits, then that many decimal length digits, followed by exactly
that number of binary bytes. For example, a 1024-byte chunk at offset 0 is:

```text
DISP:HUB75:DATA 0,#41024<exactly 1024 raw bytes><LF>
```

Embedded NUL, LF, CR, `#`, comma and 0xFF are payload bytes, not delimiters.
Do not append an extra terminator inside the declared length. `#0`
indefinite-length blocks are unsupported. The shared parameter buffer is
2048 bytes: **2047 parameter bytes plus LF**, with a separately allocated NUL
sentinel. The command header is parsed separately, so a complete 2047-byte
SCPI message is supported, not limited by the former 8-bit parameter cursor.
For CRLF, CR consumes one parameter-buffer byte. HUB75's 2000-byte payload cap
leaves room for offsets, block framing and terminators; existing 1024-byte
chunks remain valid. Oversized definite blocks are drained by their declared length
and rejected rather than being interpreted as further commands. Leave no
gap of two seconds or more within a command/block: an incomplete message
is discarded on that bounded idle timeout. After a timeout, do not send
the old block's remainder; start a fresh command.

Check `SYST:ERR?` after configuration, START, DATA and SHOW. For throughput,
batch the three DATA commands into one USB write, then check errors/staged
byte count once per complete frame. Errors include -221
(stopped, incomplete image, pending swap or pin conflict), -222 (range or
chunk/order error), -225 (RAM/PIO/DMA/IRQ unavailable), -161/-223/-363
(framing/size errors), and -200 (refresh or shutdown failure). A DMA abort
is bounded to 1 ms; on failure the output remains blank and resources/RAM
stay reserved until safe cleanup can complete. The service loop retries
cleanup; do not assume STOP succeeded if the error queue reports failure.

## Known-pattern smoke test

Flash the UF2 matching your board via BOOTSEL, let USB enumerate, and select its CDC COM
port. Firmware has an approximately five-second startup LED sequence.
With Python and pyserial available:

```powershell
python -m pip install pyserial
python tools\hub75_smoke.py --port COM7 --pattern bars --brightness 25
python tools\hub75_smoke.py --port COM7 --pattern checker --brightness 10
python tools\hub75_smoke.py --port COM7 --pattern ramp --brightness 25
python tools\hub75_smoke.py --port COM7 --pattern bars --frames 300 --fps 30
```

The CLI defaults to RGB332; use `--format RGB888` for the older transfer format.
It sends STOP, DIM, FORM, optional PINS, and START before uploading.
`--pins` takes fourteen GPIO numbers in signal order. Omit it to preserve the
device's selected pin layout (which defaults to GP0..13 after reboot).
The tool checks identity, accepted geometry/format, errors, complete staging, swaps and an advancing
frame counter. It leaves the image refreshing after disconnecting. Bars
are R/G/B/white/cyan/magenta/yellow/black; the lower half is half intensity.
Three-pixel corner markers are red (top left), green (top right), blue
(bottom left), white (bottom right). Check colors, orientation, row mapping,
ghosting and flicker. Then verify that uploading a new image does not show
partial rows, brightness zero blanks the panel, STOP blanks it, and
START/upload/SHOW works again. Repeating a pattern reports elapsed time, achieved
frame rate and image-payload Mbps; this is a transfer measurement, not an
animated pattern or an optical-refresh measurement. Stop using `DISP:HUB75:STOP` over the same CDC
connection; query `SYST:ERR?` afterward.

Offline generation needs no serial package:

```powershell
python tools\hub75_smoke.py --output build\hub75-bars.scpi --pattern bars
```

This produces a binary SCPI stream (not a text file) with STOP, DIM, FORM,
START, three DATA blocks and SHOW at 64x64 RGB332, using START's default 25%
brightness. Live uploads can set any brightness; the offline stream requires 25%.

The equivalent manual setup is:

```text
DISP:HUB75:STOP
DISP:HUB75:DIM 64,64
DISP:HUB75:PINS 0,1,2,3,4,5,6,7,8,9,10,11,12,13
DISP:HUB75:FORM RGB332
DISP:HUB75:START
DISP:HUB75:GEOM?
```

GEOM returns `64,64,4096,4`. Then send four binary DATA messages at offsets
0,1024,2048,3072, followed by `DISP:HUB75:SHOW` and `SYST:ERR?`.
These binary messages must be sent as bytes, not pasted as text containing
`<bytes>` placeholders.

At **1 Mbps effective image-payload throughput**, 4096 bytes takes 32.768 ms,
or a theoretical 30.52 new images/s before command/host overhead. This is
independent of the nominal 72.67 Hz PIO panel refresh. USB full-speed signaling
is 12 Mbps; neither that ceiling nor the CDC nominal 115200 baud setting
guarantees an application's sustained transfer rate. No real-device USB
throughput measurement has been made here.

## Display dimensions

Geometry is **runtime configuration** using `DISP:HUB75:DIM width,height`
after STOP. The same PIO program uses a column count in each row header;
it has no hardcoded 64-column loop. Packing, allocation sizes and upload
bounds follow the selected dimensions without rebuilding.

Supported settings are width **8..256** and height **16, 32 or 64**. These assume
conventional two-row parallel panels with respectively 1/8, 1/16 or 1/32 scan.
All five address GPIOs remain reserved; unused upper address lines stay low.
Unusual internal mappings, 1/4-scan variants, and special panel initialization
are not supported merely by changing dimensions.

For example, use the same firmware with a conventional 64x32 panel:

```powershell
python tools\hub75_smoke.py --port COM7 --width 64 --height 32 --pattern bars
```

Optional CMake settings change only the **boot defaults** and artifact name:

```powershell
cmake -S . -B build\hub75-64x32 -G Ninja -DPICO_BOARD=pico2 -DCTI_HUB75_WIDTH=64 -DCTI_HUB75_HEIGHT=32 -DCMAKE_BUILD_TYPE=Release
cmake --build build\hub75-64x32
```

Its named output is `uf2\cti_hub75_pico2_64x32.uf2`; runtime DIM can still change
it. For offline generation, pass `--width` and `--height`; that stream configures
the device but cannot query errors.

## Runtime pin mapping

The fourteen signals can move, but the compact PIO engine requires three
ascending consecutive groups: six RGB pins, five address pins, three control
pins (CLK/LAT/OE). Groups must not overlap. Only exposed header GPIOs
0..22 and 26..28 are allowed on all four supported Pico boards;
onboard power/LED/wireless pins are excluded.
Arbitrary per-signal permutations are not supported.

For example, RGB=GP14..19, address=GP0..4, control=GP26..28:

```text
DISP:HUB75:STOP
DISP:HUB75:PINS 14,15,16,17,18,19,0,1,2,3,4,26,27,28
DISP:HUB75:START
```

GPIO26..28 cannot simultaneously serve ADC inputs when owned by HUB75.
All five address signals remain part of the pin configuration even for
shorter panels.

## Resource and timing budget

PIO generates CLK, LAT and /OE directly using three side-set pins. It counts
columns rather than reading a separate RAM word for every clock edge.
Each row/plane stream consists of:

| Words | Contents |
|---|---|
| 1 header | Low 16 bits: firmware-generated `SET PINS, row` instruction with blanking side-set. High 16 bits: column count minus one. SET uses the configured address group. |
| `ceil(width/5)` pixel words | Five 6-bit pixel pairs per 32-bit word, least-significant pair first; each pair is R0/G0/B0/R1/G1/B1. Two high bits and any unused final pairs are padding. |
| 1 on-time word | Illuminated cycle count minus one; zero selects the fully blank path. Nonzero brightness yields at least four on cycles with the current timing constants. |
| 1 blank-time word | Remaining dwell delay, maintaining the same frame duration at every brightness. |

Headers are generated by trusted firmware, not received as executable SCPI
payload. PIO's `OUT EXEC` applies only that fixed-form row-setting instruction.
The default mapping is GP0..5 for pixel OUTs, GP6..10 for row SETs, and GP11..13
for side-set; START sets these bases from the configured groups.
Explicit blocking pulls use a 30-bit empty threshold for the
five packed pixels; unconditional header/dwell pulls discard padding safely.
Autopull is disabled. A FIFO wait always holds CLK/LAT low and /OE high.

At 64x64, each row/plane needs 13 pixel words plus three control words:
**2048 words / 8192 bytes per scan**, compared with the previous expanded
67584-byte scan. Two scan buffers use 16384 bytes; staging uses 4096 bytes
in RGB332 or 12288 bytes in RGB888: **20480 / 28672 bytes of image/scan data**,
plus 20 bytes of buffer-object bookkeeping on either chip and allocator overhead.
General image/scan storage is
`width*height*bytesPerPixel + 2*(height/2)*4*(ceil(width/5)+3)*4`.
At maximum 256x64, that is 72704 bytes in RGB332 or 105472 bytes in RGB888,
plus bookkeeping. Native tests may have a larger `size_t`.
RAM is dynamically allocated only while started and released on STOP.
Both RP2040 and RP2350 builds disable the SDK's default malloc panic so failed display
allocations can be cleaned up and reported through SCPI (-225), rather than
halting the firmware. A 64x64 RGB332 display's data buffers need about 20 KiB
of RP2040's 264 KiB SRAM. Available memory also depends on the normal firmware.
The parser, command tree, USB buffers, stack, and ordinary firmware heap
are additional to this display budget.

At the normal 125 MHz Pico / 150 MHz Pico 2 system clocks, the PIO divider
is configured for an average 20 MHz (6.25 / 7.5 respectively). Fractional
division produces system-clock-scale timing variation; it does not change
the nominal refresh budget. The pixel
loop takes ten cycles per column, giving a nominal **2 MHz shift clock**.
LAT is high for one cycle after shifting, and drops one cycle before
illumination. One row/plane takes `10*width + 10 + dwellBudget` cycles,
including the extra cycle for the row SET executed by OUT EXEC.
Color intensity uses **binary-coded modulation (BCM)**, not a separate PWM
counter per pixel: each bitplane's illumination time is weighted 1:2:4:8.
Plane dwell budgets are 400, 800, 1600, 3200 cycles. Brightness divides the
illuminated and blank dwell without changing total frame duration. A complete
frame takes `(height/2) * (4*(10*width+10) + 6000)` cycles:
275200 cycles at 64x64, **13.760 ms / 72.67 Hz nominal**. FIFO starvation and
the short DMA rearm interrupt add blank time, so real refresh is slightly
lower. SHIFT and LAT are always blanked, /OE goes high before each blocking
FIFO pull, and the next row address is set only while blanked. Frame time
and memory usage scale with the selected geometry. A full-white pixel's
maximum duty at 64x64 is about 2.18%, before the brightness multiplier.
Brightness is linear duty control, not perceptual/gamma control.

PIO and DMA perform all shifts and dwells; the CPU handles only one short
DMA rearm interrupt per frame. Swaps select a fully packed inactive buffer
at a complete RAM-stream boundary; FIFO order preserves the panel frame.
USB/SCPI never waits for a swap. The main loop detects DMA error flags or
a missing frame completion for 100 ms and stops/blanks the display with
a queued error. Existing unrelated blocking peripheral transfers can delay
that software health check, but the PIO's blank-on-stall behavior remains.

While active (including failed shutdown cleanup), mutating digital, PWM,
ADC-enable and UART/I2C/SPI INIT commands cannot take configured display GPIOs.
START rejects existing
peripheral muxes, digital outputs or pulled SIO inputs on those pins.
Release a prior peripheral/digital assignment with
`DIG:PIN<n>:DIR IN` (and `DIG:PIN<n>:PULL NONE` if necessary) first.
STOP returns all selected pins except OE to disconnected inputs and leaves
the selected OE as SIO output high for safety. Previous OE pins can remain
high across pin-layout changes until reused; other commands may use these
pins after a successful STOP.
PIO programs are firmware-bundled and claimed dynamically, never uploaded.
RP2040 searches its two PIO blocks; RP2350 searches its three. Both require
one available state machine, fourteen instruction slots and one DMA channel.

## Build and regression tests

Use the tool bundle described in [RP2350 setup](../platform/rp2350/readme.md):

```powershell
cmake -S . -B build\hub75-pico2 -G Ninja -DPICO_BOARD=pico2 -DCMAKE_BUILD_TYPE=Release
cmake --build build\hub75-pico2
```

The ordinary output is `uf2\cti_visa_rp2350_pico2_0.9.4.uf2`. The build also
copies `uf2\cti_hub75_pico2_64x64.uf2` to distinguish it from older builds.
For RP2040 Pico, build the same service with:

```powershell
cmake -S . -B build\hub75-pico -G Ninja -DPICO_BOARD=pico -DCMAKE_BUILD_TYPE=Release
cmake --build build\hub75-pico
```

Its named output is `uf2\cti_hub75_pico_64x64.uf2`. Use `-DPICO_BOARD=pico_w`
in another build directory for `uf2\cti_hub75_pico_w_64x64.uf2`. Never flash a
Pico 2 UF2 onto RP2040 or vice versa. The SCPI commands, GP2..GP15 example
mapping, upload formats and host tool are identical on both chips.

Native tests require a host C++17 compiler, not the Arm cross-compiler:

```powershell
cmake -S tests -B build\host-tests -G Ninja
cmake --build build\host-tests
ctest --test-dir build\host-tests --output-on-failure
python -m unittest discover -s tests -p "test_*.py"
```

Tests exercise the real parser, packing/upload code and display lifecycle
with a mock SDK, including all 256 RGB332 values, runtime geometry changes,
remapped pin ownership, incomplete frames, resource exhaustion, pin conflicts,
deferred swaps, DMA errors and abort timeouts. CTest runs the shared driver
for both chip configurations, omitting PIO2 from the RP2040 mock and preserving
its existing ADC channel 3 support. With `pioasm` available, Python tests
also assemble the actual `.pio` source and run an instruction-level model
against C++-packed frames, checking every pixel clock edge, row/latch
ordering, exact dwell times, consecutive frames, padding and blanking when
every FIFO read is deliberately stalled. Set `PIOASM_EXE` if it is not on
PATH or in the pinned Pico tool bundle. `HUB75_TEST_BINARY` selects an
alternative-geometry native test executable, or `cti_tests_rp2040`, for those
PIO tests. Assembly targets PIO version 0 for RP2040 compatibility.
They do not emulate either chip's bus,
prove optical timing, or validate a physical panel. A logic analyzer and
panel bench test are still required before treating this as hardware-
validated firmware. Check CLK/LAT/ABCDE setup/hold, /OE blanking and levels,
actual refresh, USB responsiveness, and long-duration behavior.
