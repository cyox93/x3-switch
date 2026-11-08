# x3-switch — MM32 firmware for the X3 Knob Switch

Bare-metal firmware for the **MindMotion MM32F0144C6P** (Cortex-M0, 64KB Flash /
8KB SRAM) inside the **Tuya X3 Knob Switch** (model `X3_PPR_V3.0`).

In the **X3 Thermostat** project this MCU is an **I/O sub-controller**, not the
brain. It owns the buttons, the button LEDs and the relays, and talks to an
ESP32-S3 main-board over USART2 with AT commands. The LCD, rotary encoder and
LED ring belong to the ESP32-S3 — the panel FFC is unplugged from CON1 and goes
to `x3_uart_board` in the ZS3L socket instead.

| Owned by this firmware | Moved to the ESP32-S3 |
|---|---|
| Buttons K1~K4 | 1.28" round LCD (GC9A01A) |
| Button LEDs K1~K4 + rail enable/brightness | Rotary encoder |
| Relays L1~L4 | LED ring |
| USART2 AT interface | WiFi/MQTT, PLC, temperature sensing |

> ⚠️ The original Tuya MCU firmware was **mass-erased** during the first J-Link
> RDP-unsecure pass and is unrecoverable. The external W25Q64JV SPI flash (UI
> resources) and the ZS3L Zigbee module firmware are untouched and out of scope.

## Documentation

| Document | Contents |
|---|---|
| [docs/architecture.md](docs/architecture.md) | Where this MCU sits, board wiring, responsibility split |
| [docs/hardware.md](docs/hardware.md) | The pin map this firmware owns, and the traps that cost time |
| [docs/at-protocol.md](docs/at-protocol.md) | ESP32-S3 ↔ MM32 AT command spec |

## Toolchain

| | Version / Source |
|---|---|
| Compiler | arm-none-eabi-gcc 15.2.1 (`brew install --cask gcc-arm-embedded`) |
| Build | CMake ≥ 3.20 / Ninja |
| Debug probe | SEGGER J-Link V9.x + JLinkGDBServer / JLinkExe (`brew install --cask segger-jlink`) |
| Editor | VSCode + Cortex-Debug + CMake Tools |
| Vendor SDK | [MindMotion MM32F0140](https://github.com/mm32/MM32F0140) (git submodule) |
| RTT | [SEGGER RTT](https://github.com/SEGGERMicro/RTT) (git submodule) |

## Getting started

```sh
git clone --recursive <repo> x3-switch && cd x3-switch
# (or, if already cloned without --recursive)
git submodule update --init --recursive

cmake -S . -B build -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake \
      -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Output: `build/x3-switch.elf` (+ `.bin` / `.hex` / `.map`).

## Flash

```sh
JLinkExe -device MM32F0144C6P -if SWD -speed 4000 -autoconnect 1 \
         -nogui 1 -CommanderScript scripts/flash.jlink
```

The device name must be **`MM32F0144C6P`** with the trailing `P` — `MM32F0144C6`
raises an unknown-device dialog that needs a GUI click to dismiss.

VSCode equivalents: tasks **Configure** / **Build** / **Flash**, and **F5**
launches Cortex-Debug.

## Debug and logging

SEGGER RTT (channel 0) is the only logging facility. Use
`RTT_LOG{E,W,I,D,V}(tag, fmt, ...)` from `components/include/log.h`; never
`printf`. `-DLOG_LEVEL_MAX=LOG_LEVEL_INFO` strips the noisier levels at compile
time.

```c
#include "log.h"
static const char *TAG = "key";

log_init();                              // once at boot
RTT_LOGI(TAG, "K%u down", channel);      // I (key): K3 down
```

**`JLinkRTTClient` does not talk to the probe directly.** It needs a separate
process bridging RTT — either a VSCode F5 session or a `JLinkGDBServerCL` you
started yourself. "RTT not found" means that bridge died.

```sh
# Terminal 1 — flash, then hand the probe to the GDB server
JLinkExe -device MM32F0144C6P -if SWD -speed 4000 -CommanderScript scripts/flash.jlink
# Terminal 2 — keeps the probe alive and exposes the RTT bridge
JLinkGDBServerCL -device MM32F0144C6P -if SWD -speed 4000 -port 2331 -nohalt -noir -nogui
# Terminal 3
JLinkRTTClient
```

Re-flashing while the GDB server runs needs `pkill -f JLinkGDBServerCL` first,
or `JLinkExe` fights it for the probe.

### Talking AT over a USB-serial adapter

```sh
screen /dev/cu.usbserial-XXXX 115200      # or any 115200 8N1 terminal
```

`+READY:x3-switch,<ver>` on the wire right after a reset confirms the link.
**Both directions have to be wired**: on 2026-09-22 the bench adapter only had
its RX hooked up, so every response looked correct while no command was ever
received. If `AT` gets no `OK`, sample `GPIOA IDR` (`0x48000008`) over SWD while
streaming bytes — PA3 staying HIGH means nothing is reaching the pin.

Reading target state without stopping the firmware is often faster than a
debugger session: `JLinkExe -autoconnect 1 -CommanderScript` with `mem32` reads
live registers and RAM, and `sleep` between reads samples a value over time.
Several findings below were established that way.

## Wiring (SWD, J-Link 20-pin → X3 J2 header)

| J-Link | Signal | X3 J2 |
|---:|---|---:|
| 1 | VTref | 4 (3V3) |
| 4 | GND | 3 |
| 7 | SWDIO | 2 (PA13) |
| 9 | SWCLK | 1 (PA14) |

⚠️ **Never connect AC mains** while J-Link is attached — X3's back PCB is a
non-isolated SMPS, so GND couples to the AC line and can destroy the host PC or
the probe. Power the front PCB only from J1's +5V pin with an isolated supply.

## Project layout

ESP-IDF-inspired component pattern over plain CMake (`add_library(STATIC)` +
`add_subdirectory()`). `components/include/` is on the global include path, so
any C file can `#include "log.h"` etc.

```
.
├── CMakeLists.txt                top-level: vendor sources, executable, components
├── cmake/arm-none-eabi.cmake     toolchain selection
├── linker/mm32f0144c6p_flash.ld  Flash 64KB @ 0x08000000, SRAM 8KB @ 0x20000000
├── startup/startup_mm32f0140.s   GCC vector table + Reset_Handler
├── docs/                         architecture, hardware, AT protocol
├── main/
│   ├── main.c                    application: one cooperative polling loop
│   └── system_mm32f0140.c        SystemInit override + SystemCoreClock
├── components/
│   ├── include/                  public headers, auto-included project-wide
│   │   ├── board.h               HAL umbrella + X3 pin map
│   │   ├── at.h  clock.h  tick.h  version.h  log.h
│   │   └── device.h  dev_key.h  dev_led.h  dev_relay.h  dev_uart.h
│   ├── core/                     log.c (RTT facade), clock.c (PLL), tick.c (SysTick)
│   ├── device/                   device.c aggregator + dev_*.c drivers
│   └── at/                       at_io.c (line framing), at_cmd.c (command table)
├── scripts/flash.jlink           J-Link Commander flash script
└── vendor/                       MM32F0140 SDK + SEGGER RTT (git submodules)
```

**Adding a device driver**: create `components/device/dev_<name>.c` +
`components/include/dev_<name>.h`, add the `.c` to
`components/device/CMakeLists.txt`, include the `.h` from
`components/include/device.h`, and call its init from `device_init()`.

**Adding a HAL peripheral**: append `${HAL_DIR}/Src/hal_<peri>.c` to the
`add_executable(...)` list in the top-level CMakeLists.

**Adding or changing an AT command**: update `docs/at-protocol.md` first — it
is the contract — then the table in `components/at/at_cmd.c`.

There are no tests — this is firmware that runs on hardware. Claims about
behaviour are backed by register reads or observation on the bench.

## Hardware notes

Details and evidence are in [docs/hardware.md](docs/hardware.md); these are the
ones that will bite you first.

### GPIO is STM32F1-style, not STM32F0xx

Despite being a Cortex-M0 part, the MM32F0140 GPIO peripheral inherits the
**STM32F1xx layout** — 4-bit-per-pin `CRL`/`CRH` instead of `MODER`/`OTYPER`,
and `BSRR` at offset **`0x10`** (not `0x18`). STM32F0xx register macros silently
target the wrong addresses. Always go through `hal_gpio.c` / `hal_rcc.c`.

Alternate functions are a separate matter: the mode bits live in `CRL`/`CRH`
but the function select lives in `AFRL`/`AFRH`. Setting `GPIO_Mode_AF_PP`
without `GPIO_PinAFConfig()` leaves the pad on its default function, and the
peripheral is silently dead.

### `SystemInit()` is intentionally a no-op

The SDK's stock `Source/system_mm32f0140.c` boots a 72 MHz HSI+PLL through a
path that reads `RCC_PLLCFGR_PLLSRC` as HSE — and **X3 has no HSE crystal
populated**, so it deadlocks the MCU hard enough that SWD cannot attach
(recovery: NRST-asserted connect-under-reset + mass erase). The override lives
in `main/system_mm32f0140.c`.

Clocks come up from the application instead: `clock_init_pll48mhz()` runs the
PLL off HSI ×6 for SYSCLK/HCLK/PCLK2 = 48 MHz, PCLK1 = 24 MHz. The PWR interface
clock must be enabled before the `PLLCFGR` writes or `PLLRDY` never rises.

### Never use NOP-count delays

Cortex-M0 has no hardware divider, so a busy-wait whose loop condition divides
(`i < SystemCoreClock / 8000`) pays an `__aeabi_uidiv` call every iteration and
runs **~21× slow** — measured, not estimated. Derive all timing from a SysTick
1 ms tick.

Timing observations made before 2026-09-22 used that delay, so the numbers in
the LCD bring-up history are ~20× longer than the code claims.

### Button LEDs cannot be turned off per channel

A hi-Z pin leaves a DC path from the rail through the orange LED and its 120 Ω
into the 4.7 kΩ base resistor, which turns the NPN on and lights the **white**
LED. Channels are white or orange; darkness only comes from the rail
(`LED_EN`, PB9), which is also `TIM17_CH1` and therefore a 1 kHz PWM dimmer.

### K4 reset the MCU on the first board

On the first unit, pressing K4 held the MM32 in reset — its net reached NRST.
All four LEDs went white while held, and the MCU booted on release. A second
board does not show this and reads K4 on PD0 as `board.h` defines, so this is a
defect of that one unit. If K4 resets your board, see
[docs/hardware.md §2.1.1](docs/hardware.md) for the diagnosis and the repair
checklist.

### SPI1 remap is reserved for the on-board W25Q64

The remap (PA15/PB3/PB4/PB5) is all-or-nothing, so any peripheral wanting SPI1's
default mapping would lock out the flash. This firmware does not touch either.
