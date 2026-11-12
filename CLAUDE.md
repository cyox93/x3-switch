# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Bare-metal firmware (`x3-switch`) for the **MindMotion MM32F0144C6P** (Cortex-M0,
64KB Flash / 8KB SRAM) inside the **Tuya X3 Knob Switch** (model `X3_PPR_V3.0`).
The original Tuya MCU firmware was mass-erased during the first J-Link unlock
pass and is unrecoverable; this codebase is a clean rewrite.

**Scope**: in the X3 Thermostat project this MCU is an **I/O sub-controller for
an ESP32-S3 main-board**, not the brain. It owns buttons K1~K4, the bi-color
button LEDs (+ rail enable and PWM brightness) and relays L1~L4, and exposes
them over USART2 as a classic AT command **slave** — it reports key DOWN/UP and
executes commands, and holds no policy of its own. The LCD, rotary encoder and
LED ring were removed from this firmware: the panel FFC moves from CON1 to
`x3_uart_board` in the ZS3L socket, so those pins reach nothing.

Read `docs/` before changing behaviour:

| Document | Contents |
|---|---|
| `docs/architecture.md` | Role split, board wiring, data flow |
| `docs/hardware.md` | The pin map this firmware owns, and the measured traps |
| `docs/at-protocol.md` | The AT command contract — the spec the firmware implements |

`README.md` has toolchain, wiring (SWD → J2), RTT usage and an AC-mains safety
warning. System-level design and the hardware reverse engineering live outside
this repository; keep references to them out of committed files, since they
do not resolve for anyone reading the repository. Note that the
reverse-engineered LQFP48 pin map was **inferred from trace routing, not
measured** — `BTN_K4 = PD0` was doubted when the first board reset on K4, and
has since been confirmed on a second board (see "K4 reset the first board" below).

## Build and flash

```sh
# Configure (once, or after CMakeLists changes)
cmake -S . -B build -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake \
      -DCMAKE_BUILD_TYPE=Debug

cmake --build build

# Flash via J-Link Commander. The device name MUST be MM32F0144C6P
# (with the trailing P) — MM32F0144C6 triggers an unknown-device popup
# that requires manual GUI dismissal.
JLinkExe -device MM32F0144C6P -if SWD -speed 4000 \
         -autoconnect 1 -CommanderScript scripts/flash.jlink
```

VSCode equivalents (local `.vscode/`, not tracked): tasks `Configure` / `Build` /
`Flash`, and **F5** launches Cortex-Debug.

There are no tests — this is firmware that runs on hardware. Back behavioural
claims with evidence from the bench, not with reasoning alone.

## Debug and RTT logging

SEGGER RTT (channel 0) is the only logging facility. Use the `RTT_LOG{E,W,I,D,V}(tag, fmt, ...)`
macros from `components/include/log.h`; do not use `printf`. Set
`-DLOG_LEVEL_MAX=LOG_LEVEL_INFO` to compile-time strip the noisier levels.

**Critical gotcha**: `JLinkRTTClient` does **not** speak to a J-Link probe
directly. It needs a separate process bridging RTT — either a VSCode F5
(Cortex-Debug) session, or a standalone `JLinkGDBServerCL` you've started
yourself. If `JLinkRTTClient` shows "RTT not found" or stops receiving lines,
the bridge died.

```sh
# Terminal 1: flash, then leave probe to the GDB server
JLinkExe -device MM32F0144C6P -if SWD -speed 4000 -CommanderScript scripts/flash.jlink
# Terminal 2: GDB server keeps probe alive and exposes RTT bridge
JLinkGDBServerCL -device MM32F0144C6P -if SWD -speed 4000 -port 2331 -nohalt -noir -nogui
# Terminal 3: connects to localhost:19021 automatically
JLinkRTTClient
```

Re-flashing while `JLinkGDBServerCL` is up requires killing the GDB server first
(`pkill -f JLinkGDBServerCL`); `JLinkExe` will fight it for probe ownership.

### Prefer live register reads over a debugger session

Most questions about running firmware are answered faster by reading memory
while it runs, with no halt and no GDB:

```sh
JLinkExe -device MM32F0144C6P -if SWD -speed 4000 -autoconnect 1 -NoGui 1 \
         -CommanderScript probe.jlink     # mem32 <addr> <count>, w4 <addr> <val>, sleep <ms>
```

`sleep` between reads samples a value over time — that is how the 21× slow
delay loop and the LEDEN/LEDBRI behaviour were pinned down. `arm-none-eabi-nm`
gives the address of any static variable to watch. Writing registers with `w4`
can also reconfigure hardware without reflashing, which is how the hi-Z LED
behaviour was measured.

## Architecture

**ESP-IDF-inspired component pattern over plain CMake.** Each component is a
static lib (`add_library(<name> STATIC ...)`) registered via `add_subdirectory()`
from the top-level `CMakeLists.txt`. The executable links those component libs
plus the vendor SDK sources baked directly into the `add_executable(...)` call.

Layers (`components/include/` is on the global include path, so any C file can
`#include "log.h"` etc):

- **`components/core/`** — facades (`log.c` over SEGGER RTT, `clock.c` for the
  48 MHz PLL, `tick.c` for the 1 ms SysTick time base every driver times off).
- **`components/device/`** — per-peripheral drivers (`dev_led.c`, `dev_uart.c`, …).
  Each driver owns one piece of hardware and exposes a small API.
  `device.c::device_init()` brings them all up; `device.h` is an aggregate header
  re-exporting the individual `dev_*.h` files.
- **`components/include/board.h`** — single include point that pulls in the vendor
  HAL (`hal_conf.h`) and defines X3-specific pin macros (`BTN_K1_PIN`, `LED_EN_PIN`,
  `RELAY_L1_PIN`, `DBG_UART_*`, …). Driver `.c` files should `#include "board.h"`
  and use these macros, not raw GPIO addresses. (clangd may warn `board.h` is an
  unused include — false positive; it is the project's HAL entry point.)
- **`components/at/`** — the AT interface. `at_io.c` owns line framing and the
  reply/notification helpers; `at_cmd.c` owns the command table and argument
  parsing. `docs/at-protocol.md` is the contract both implement.
- **`main/main.c`** — the application: one cooperative polling loop over
  `at_poll()`, `dev_key_poll()` and `dev_led_tick()`. SysTick is the only
  interrupt, so no driver state needs guarding.
- **`vendor/`** — git submodules (`vendor/mm32f0140` is the MindMotion SDK;
  `vendor/segger_rtt` is SEGGER RTT). Treat as read-only; if you need a peripheral
  that isn't built yet, add its `hal_<peri>.c` to the `add_executable(...)` list in
  the top-level `CMakeLists.txt` rather than vendoring more code.

### Hardware quirks worth knowing before you write any peripheral code

1. **GPIO is STM32F1-style, not STM32F0xx.** The MM32F0140 inherits STM32F1's
   4-bit-per-pin `CRL`/`CRH` config registers and puts `BSRR` at offset `0x10`.
   STM32F0xx register macros (`MODER`, `OTYPER`, `BSRR@0x18`) silently target the
   wrong addresses. Always go through `hal_gpio.c` / `hal_rcc.c`.

2. **Mode and alternate function are separate registers.** `CRL`/`CRH` set the
   mode; `AFRL`/`AFRH` select the function. `GPIO_Mode_AF_PP` without a matching
   `GPIO_PinAFConfig()` leaves the pad on its default function and the peripheral
   is silently dead. Same family of trap as the MM32 SPI `TXEN`/`RXEN` quirk.

3. **`SystemInit()` is intentionally a no-op.** The SDK's stock
   `Source/system_mm32f0140.c` boots a 72 MHz HSI+PLL using a code path that reads
   `RCC_PLLCFGR_PLLSRC` as HSE — but **X3 has no HSE crystal populated**, so the
   stock SystemInit deadlocks the MCU hard enough that even SWD can't attach
   (recovery: NRST-asserted connect-under-reset + mass erase). The override lives
   in `main/system_mm32f0140.c`. Clocks come up from the application via
   `clock_init_pll48mhz()` (HSI ×6 → 48 MHz, PCLK1 24 MHz); don't touch SystemInit.

4. **Never use NOP-count delays.** Cortex-M0 has no hardware divider, so a
   busy-wait whose loop condition divides (`i < SystemCoreClock / 8000`) calls
   `__aeabi_uidiv` every iteration and runs **~21× slow** (measured). Derive all
   timing from a SysTick 1 ms tick. Timing observations recorded before
   2026-09-22 were taken with that delay and are ~20× off.

5. **Button LEDs have no per-channel off.** A hi-Z pin leaves a DC path from the
   rail through the orange LED and its 120 Ω into the 4.7 kΩ base resistor, so the
   NPN turns on and the white LED lights. Channels are white or orange only;
   darkness comes from the rail (`LED_EN`, PB9), which is also `TIM17_CH1` and
   therefore a 1 kHz PWM dimmer.

6. **K4 reset the first board.** On the first unit K4's net reached NRST: all
   four LEDs went white while held and the MCU booted on release, and a repair
   attempt did not fix it. A second board does not reset on K4 (SysTick counter
   sampled live for 54 s across 5 presses, never restarted) and reads K4 on PD0.
   Treat it as a defect of that one unit; see `docs/hardware.md` §2.1.1.

7. **SPI1 remap is reserved for the on-board W25Q64 flash** (PA15/PB3/PB4/PB5).
   That remap is all-or-nothing, so any future peripheral that wants SPI1's
   default mapping would lock out the flash. This firmware touches neither.

## Conventions

- **Adding a device driver**: create `components/device/dev_<name>.c` +
  `components/include/dev_<name>.h`, add the `.c` to
  `components/device/CMakeLists.txt`, include the `.h` in
  `components/include/device.h`, and call its init from
  `components/device/device.c::device_init()`.
- **Adding a HAL peripheral**: append `${HAL_DIR}/Src/hal_<peri>.c` to the
  `add_executable(...)` source list in the top-level `CMakeLists.txt`.
- **Changing the AT interface**: `docs/at-protocol.md` is the contract, not a
  description of the code. Update it in the same change, and keep the recorded
  measurements (they are evidence, not prose).
- **Commit messages**: structured subject line, present tense, no trailing period.
  - Subject format: `{type} : {scope} : {sub-scope} : {summary}`
    - `type`: `fix` | `feat` | `docs` (and similar conventional types)
    - `scope`: top-level component, typically `core` | `device` | `main` (use the directory under `components/`, or `main`)
    - `sub-scope`: specific file/driver — e.g. `dev_led`, `dev_uart`, `log`, `clock`. Omit if the change is genuinely scope-wide.
    - `summary`: short imperative phrase
  - Body lines: indent **2 spaces** and explain *why* / verification evidence. Blank line between subject and body.
  - Trailer: the `Co-Authored-By:` line from the session's attribution instructions.
  - Example:
    ```
    feat : device : dev_relay : Add L1~L4 relay driver

      NPN low-side drivers, MCU HIGH = coil energised. All four channels are
      exposed because the front PCB populates every driver; only the back-PCB
      relays differ per room, and MM32 cannot tell which are fitted.

    Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
    ```
- **Comments**: explain *why*, not *what* — the existing code follows this
  consistently (see `components/core/clock.c`'s PLL ramp comment for the
  canonical example).
