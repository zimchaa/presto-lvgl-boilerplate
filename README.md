# Presto + LVGL boilerplate

Minimal, commented starting point for running **[LVGL 9](https://lvgl.io)** on
the **[Pimoroni Presto](https://shop.pimoroni.com/products/presto)** (RP2350B,
480×480 capacitive touchscreen) with the **pico-sdk in C/C++** — display and
touch working. Two display modes: 240×240 pixel-doubled (default, entirely
from on-chip SRAM) or native 480×480 (`-DPRESTO_FULL_RES=ON`, front buffer
in PSRAM).

Flash it and you get a ~60fps demo UI: a counter button, a spinner, and a
live touch readout. Delete the demo, keep the port, build your own interface.

## Why 240×240 by default?

A double-buffered 480×480 RGB565 framebuffer needs 900KB; the RP2350 has 520KB
of SRAM. But Pimoroni's ST7701 driver has a half-resolution mode where the
**hardware pixel-doubles** — each line is scanned out twice, each pixel held
two dot clocks — so a 240×240 logical UI fills the physical 480×480 panel with
crisp integer scaling. The entire display stack then fits in SRAM:

| Buffer                                   | Size      |
|------------------------------------------|-----------|
| Front buffer (LVGL composits into this)  | 112.5 KB  |
| Back buffer (scanout DMA target)         | 112.5 KB  |
| 2 × LVGL stripe draw buffers (240×60)    | 56.3 KB   |
| LVGL heap (`LV_MEM_SIZE`)                | 48 KB     |
| **Total**                                | **~330 KB** |

## Full-resolution mode (480×480)

```bash
cmake -S . -B build-fullres -G Ninja -DCMAKE_BUILD_TYPE=Release -DPRESTO_FULL_RES=ON
cmake --build build-fullres
```

`PRESTO_FULL_RES` drives the panel at native 480×480 RGB565 — noticeably
crisper text and images. Since one 480×480 buffer is 450KB, the memory story
changes: the Presto's 8MB QMI PSRAM is brought up at boot (pico-sdk
`hardware_psram`, CS on GPIO 47) and everything that can tolerate PSRAM
latency moves there:

| Buffer                                | Location | Size    |
|---------------------------------------|----------|---------|
| Back buffer (scanout DMA target)      | SRAM     | 450 KB  |
| Front buffer (LVGL composits here)    | PSRAM    | 450 KB  |
| 2 × LVGL stripe draw buffers (480×20) | SRAM     | 37.5 KB |
| LVGL heap (`LV_MEM_POOL_ALLOC` hook)  | PSRAM    | 64 KB   |

The scanout buffer **must** stay in SRAM — the core-1 PIO/DMA scanout can't
tolerate QMI bus latency — and it consumes nearly all of it: at 24-line
stripes the link already fails. Only core 0 ever touches PSRAM.

Trade-offs vs. the default mode:

- LVGL software-renders 4× the pixels on the same CPU, so full-screen
  redraws are proportionally slower; typical partial-update UIs are fine.
- `presto->update()` copies the 450KB front buffer out of PSRAM each frame
  (~10ms), which bounds full-frame animation.
- SRAM headroom for your application's statics is a few KB — put big data
  in PSRAM (`__uninitialized_psram("yourgroup") type name[...]`), which has
  ~7MB free.

The mode is selected in `src/display_config.hpp` / `lv_conf.h`; the touch
driver scales coordinates automatically.

## Architecture

```
core 1                              core 0
┌───────────────────────────┐       ┌────────────────────────────────┐
│ ST7701 driver              │       │ LVGL (+ your application)       │
│  PIO1 timing + parallel SM │       │  renders dirty areas into       │
│  2× DMA channels, ISRs     │       │  240×60 stripe buffers          │
│         ▲                  │       │        │ flush_cb               │
│    back buffer ◄───────────┼───────┼── byte-swap + copy into front,  │
│    (DMA source)            │update()│  then presto->update(&gfx)     │
└───────────────────────────┘       └────────────────────────────────┘
```

- **Core 1 owns the display.** The ST7701 scanout is driven by interrupts that
  must be serviced continuously; running `init()` on core 1 keeps them away
  from your application code (this mirrors the shipping MicroPython firmware).
- **LVGL renders partial** into two small stripe buffers; the flush callback
  byte-swaps (the panel wants big-endian RGB565 — `lv_draw_sw_rgb565_swap`)
  and composites into the front buffer; `presto->update()` then copies
  front→back while racing the beam, so updates are tear-free with no vsync
  wait.
- **Touch** is the FT6236 on I²C1, ported from Pimoroni's MicroPython
  `touch.py` and fed to LVGL as a pointer device. Polling is gated on the
  touch controller's INT line, so the I²C bus is only used when a finger is
  (or just was) on the glass. Coordinates arrive in 480-space and are scaled
  to the logical resolution.

## Building

Prerequisites (Debian/Ubuntu):

```bash
sudo apt install cmake ninja-build build-essential gcc-arm-none-eabi \
  libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib git
```

Clone with pinned dependencies (pico-sdk, pimoroni-pico, presto, lvgl):

```bash
git clone https://github.com/zimchaa/presto-lvgl-boilerplate
cd presto-lvgl-boilerplate
git submodule update --init                          # top-level deps
git -C lib/pico-sdk submodule update --init lib/tinyusb   # USB serial (printf)
```

Build and flash:

```bash
./build.sh          # -> build/presto-lvgl.uf2
./flash.sh          # then put the Presto into BOOTSEL (hold BOOT, tap RESET)
```

`printf` debug output appears on USB serial (`/dev/ttyACM*`).

## Where to customise

- `src/main.cpp` — `build_ui()` is the demo; replace it with your screens.
- `lv_conf.h` — LVGL options (fonts, log level, heap size). Anything not set
  here falls back to LVGL's defaults.
- `src/lvgl_port.cpp` — the display/touch glue. You shouldn't need to touch
  it, but it's short and commented if you do.

## Note on the ST7701 driver pin

The `lib/presto` submodule currently points at a fork branch carrying a
one-function fix for a driver bug that freezes the display core at the first
frame boundary in pico-sdk builds (upstream issue
[pimoroni/presto#112](https://github.com/pimoroni/presto/issues/112), fix PR
[#113](https://github.com/pimoroni/presto/pull/113)). Once the PR is merged,
the submodule will move back to `pimoroni/presto` main.

## Credits

- **Pimoroni** for the Presto and its excellent drivers — this repo is glue,
  not driver code.
- **DrJonEA**, whose LVGL port on the Pimoroni Interstate 75 (same RP2350
  family) inspired this one.
- Extracted from the [skyfiscreen](https://github.com/zimchaa/skyfiscreen)
  project (a drone ground-station control panel), where you can see the same
  port grown into a full application with WiFi + a REST client.

MIT licensed — see [LICENSE](LICENSE).
