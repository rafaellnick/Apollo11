# ESP32 AGC + DSKY Slaves

This folder starts the split architecture that fits the hardware you have:

- `ESP32`: AGC core board
- `Arduino Mega 2560`: DSKY/panel board
- `ESP8266`: small first-test DSKY slave
- analog joystick board: optional first spacecraft hand-controller input

The intent is:

- the original AGC software eventually runs on the `ESP32` through an embedded AGC emulator core
- a DSKY slave handles keys, lamps, and display output
- the boards talk over a dedicated UART link
- the PC monitors the ESP32 over USB serial

## Current status

This is the first embedded AGC core layer, not yet a full AGC emulator.

What is implemented now:

- a shared DSKY serial protocol
- a reusable `agc::Core` with 15-bit words, erasable/fixed memory, register aliases, and a starter instruction loop
- fixed-bank rope image loading hooks for `yaYUL` output
- explicit Block II basic/extracode opcode decoding with quarter-code/peripheral-code dispatch
- Block II address decoding for unswitched/switched erasable, common fixed, and fixed-fixed windows
- read/write editing behavior for `CYR`, `SR`, `CYL`, and `EDOP`
- 9-bit AGC I/O channels, including the `L`/`Q` channel aliases and `SUPERBNK` bank-selection bit
- MCT-based instruction cycle accounting for core execution and scheduled peripheral timing
- initial interrupt, `DOWNRUPT`, keyrupt, uplink, and downlink plumbing in the AGC core
- a deterministic peripheral layer for keyrupt/uplink, downrupt, and downlink channel monitoring
- physics-inspired mission telemetry helpers for ascent, coast, orbit, descent, and reentry
- an `ESP32` AGC core shell that sends status to both the DSKY slave and the PC USB serial monitor
- ESP32 joystick input support for `VRX`, `VRY`, and `SW`
- an Apollo 11 launch/ascent monitor simulation for first DSKY mission tests
- an Apollo 11 mission command layer with DSKY commands for major nominal and abort situations
- a first PINBALL noun reference map for monitor/debug commands
- phase-label telemetry from ESP32 to the ESP8266 Web DSKY
- an `ESP8266` DSKY slave with a Wi-Fi browser interface for the first bench test
- a `Mega` starter that scans buttons, drives lamps, and mirrors state to an LCD

The ESP32 core currently runs a tiny AGC bring-up program that increments an erasable counter. The next layer is loading a real assembled rope image.

## Folder layout

- `shared/agc_core.h`: first AGC CPU/memory layer
- `shared/agc_peripherals.h`: deterministic keyrupt/downrupt/downlink peripheral model
- `shared/dsky_protocol.h`: protocol, key definitions, lamp bits, frame parsing
- `shared/mission_physics.h`: lightweight mission telemetry model
- `esp32_agc_core/esp32_agc_core.ino`: ESP32 AGC core shell
- `esp32_agc_core/rope_image.h`: generated or placeholder embedded rope image
- `APOLLO11_MISSION_PROGRAM.md`: command table for the Apollo 11 mission program layer
- `LAUNCH_SIMULATION_MANUAL.md`: launch simulation operating manual
- `ROPE_BUILD.md`: rope conversion and loading path
- `tools/rope_to_header.py`: converts rope word dumps into an ESP32 header
- `tools/build_rope_image.ps1`: validates `Comanche055`/`Luminary099`, runs `yaYUL` when available, and converts `MAIN.agc.bin`
- `tools/run_agc_trace_validation.ps1`: builds and runs the desktop AGC trace harness
- `tools/compare_agc_trace.ps1`: compares embedded-core trace CSVs against normalized yaAGC/VirtualAGC traces
- `shared/pinball_nouns.h`: first real PINBALL noun reference map
- `esp8266_dsky_slave/esp8266_dsky_slave.ino`: ESP8266 DSKY slave, serial bridge, and web server
- `esp8266_dsky_slave/web_dsky_page.h`: embedded browser DSKY page served by the ESP8266
- `esp8266_dsky_slave/wifi_config.h.example`: optional local Wi-Fi config template
- `mega_dsky_panel/mega_dsky_panel.ino`: Mega-side panel controller
- `tests/agc_core_selftest.cpp`: desktop self-test for the core
- `tests/agc_trace_runner.cpp`: deterministic trace emitter for core-vs-yaAGC validation
- `tests/agc_peripherals_selftest.cpp`: desktop self-test for keyrupt/downrupt/downlink behavior
- `tests/mission_physics_selftest.cpp`: desktop self-test for mission telemetry helpers

Each Arduino sketch folder also contains local copies of the headers it needs. This is intentional: the Arduino IDE compiles a sketch folder as a standalone unit, so includes like `../shared/dsky_protocol.h` may fail when the sketch is opened directly.

When changing `shared/agc_core.h` or `shared/agc_peripherals.h`, sync the matching copies in `esp32_agc_core/` before opening the sketch in the Arduino IDE.

## First Test Wiring: ESP32 + ESP8266

Use a dedicated UART between the boards.

The ESP32 uses hardware `Serial2`. The ESP8266 uses `SoftwareSerial` so its USB `Serial` remains available for typed key commands and local monitoring.

- `ESP32 TX2 GPIO17` -> `ESP8266 GPIO14 / NodeMCU D5`
- `ESP8266 GPIO12 / NodeMCU D6` -> `ESP32 RX2 GPIO16`
- `ESP32 GND` -> `ESP8266 GND`

Both boards are `3.3V`, so no level shifter is needed for this first test.

The board-to-board link runs at `38400` baud. The PC USB serial monitor on each board runs at `115200` baud.

## ESP8266 Web DSKY

The ESP8266 remains the DSKY slave. It receives `STATE,...` frames from the ESP32 over the board-to-board UART and sends `KEY,...` commands back to the ESP32. It also serves a small web DSKY, so your phone or PC browser can press DSKY keys without a physical button matrix.

By default, if no local Wi-Fi config exists, the ESP8266 starts its own access point:

- SSID: `AGC-DSKY`
- Password: `apollo11`
- Browser URL: `http://192.168.4.1`

To connect the ESP8266 to your existing Wi-Fi network, create `embedded/esp8266_dsky_slave/wifi_config.h` using this shape:

```cpp
#pragma once

#define DSKY_WIFI_SSID "YourWiFiName"
#define DSKY_WIFI_PASSWORD "YourWiFiPassword"

#define DSKY_AP_SSID "AGC-DSKY"
#define DSKY_AP_PASSWORD "apollo11"
```

The real `wifi_config.h` is ignored by git so your Wi-Fi password does not get committed. When station mode connects, the ESP8266 prints its browser URL on the USB serial monitor. It also tries to publish `http://agc-dsky.local` with mDNS.

## Joystick Wiring

Wire the analog joystick to the ESP32, not the ESP8266. The ESP8266 only has one analog input, while this joystick needs two.

- `Joystick VCC` -> `ESP32 3V3`
- `Joystick GND` -> `ESP32 GND`
- `Joystick VRX` -> `ESP32 GPIO34`
- `Joystick VRY` -> `ESP32 GPIO35`
- `Joystick SW` -> `ESP32 GPIO27`

Keep joystick `VCC` at `3.3V`. ESP32 ADC pins are not 5V tolerant.

The ESP32 samples the joystick every `50 ms`, normalizes `VRX` and `VRY` to about `-1000..+1000`, and stores the values in AGC erasable memory:

- `00110`: RHC X
- `00111`: RHC Y
- `00112`: RHC switch

Type `JOYCAL` on the ESP32 USB serial monitor while the stick is centered to recalibrate.

Set the DSKY noun to `99` to show joystick data:

```text
N
9
9
```

## Apollo 11 Launch Simulation

The ESP32 has a first launch monitor simulation for bench testing the DSKY flow. It is not yet the real Comanche rope running the Saturn V ascent. It is a mission sequencer that drives DSKY displays and serial status through major Apollo 11 ascent events: terminal count, liftoff, roll program, Max-Q, staging, S-IVB burn, and parking orbit insertion.

Full Apollo 11 mission command table: `APOLLO11_MISSION_PROGRAM.md`.

Full Portuguese operating manual: `LAUNCH_SIMULATION_MANUAL.md`.

Start it from the Web DSKY:

```text
VERB 37 NOUN 11 ENTR
```

Stop it from the Web DSKY:

```text
VERB 37 NOUN 00 ENTR
```

Start it in real time instead of accelerated time:

```text
VERB 37 NOUN 12 ENTR
```

During the launch simulation the display changes to `P11 V16 N62`:

- `R1`: mission time in seconds, negative during the final countdown
- `R2`: approximate altitude in kilometers
- `R3`: approximate velocity in meters per second

The default launch simulation speed is `x20`, so the 11 minute 45 second ascent to parking orbit runs in about 36 seconds. Use the ESP32 USB serial command `LAUNCH,SPEED,<1-100>` to change that speed.

## PC Monitoring

Open the ESP32 USB serial monitor at `115200`.

The ESP32 automatically prints:

- clean `AGC ...` human-readable status once per second
- `STATE,...` and `PHASE,...` machine-readable frames to the DSKY slave every 250 ms
- `CORE ...` when you type `CORE`

The `CYC` field is now the core's MCT-style cycle counter, not just an instruction counter.

By default, the PC USB serial uses clean output only. The DSKY UART still receives raw `STATE,...` frames.

The peripheral layer counts scheduled `KEYRUPT`/`DOWNRUPT` events and downlink changes by default. Use `PERIPH,IRQON` only when you want those scheduled peripheral events to request AGC core interrupts; the default keeps the bring-up loop stable while real interrupt handlers are still incomplete.

Useful ESP32 USB commands:

- `HELP`
- `STATUS`
- `CORE`
- `ROPE,INFO`
- `ROPE,LOAD`
- `PERIPH`
- `PERIPH,RESET`
- `PERIPH,IRQON`
- `PERIPH,IRQOFF`
- `DOWNLINK`
- `UPKEY,<key-name-or-octal-word>`
- `CHAN,<octal-channel>`
- `CHAN,<octal-channel>,<octal-word>`
- `IRQ,<0-7>`
- `PINBALL,<noun>`
- `PINBALL,LIST`
- `STATE`
- `STEP`
- `HALT`
- `RUN`
- `RESET`
- `PEEK,<octal-address>`
- `POKE,<octal-address>,<octal-word>`
- `JOY`
- `JOYCAL`
- `LAUNCH`
- `LAUNCH,STOP`
- `LAUNCH,STATUS`
- `LAUNCH,SPEED,<1-100>`
- `LAUNCH,REALTIME`
- `USB,CLEAN`
- `USB,RAW`
- `USB,BOTH`
- `USB,QUIET`
- `KEY,VERB`
- `KEY,NOUN`
- `KEY,3`

## ESP8266 DSKY Test Commands

Open the ESP8266 USB serial monitor at `115200`.

The ESP8266 prints DSKY state received from the ESP32, forwards USB key commands back to the ESP32, and serves the browser DSKY over Wi-Fi.

Useful shortcuts:

- `V`: `VERB`
- `N`: `NOUN`
- `0..9`: digit keys
- `E`: `ENTR`
- `C`: `CLR`
- `P`: `PRO`
- `R`: `RSET`

Example sequence:

```text
V
3
7
N
3
6
E
```

## Mega UART Wiring

The Mega panel is still available for later physical-button tests.

- `ESP32 TX2 pin 17` -> `Mega RX1 pin 19`
- `Mega TX1 pin 18` -> `ESP32 RX2 pin 16`
- `ESP32 GND` -> `Mega GND`

Important:

- the `Mega` is `5V`
- the `ESP32` is `3.3V`
- level shift the `Mega TX -> ESP32 RX` path
- a proper bidirectional level shifter is recommended for the UART link
- the board-to-board link runs at `38400` baud

## Mega starter pin map

### LCD

The panel starter keeps a temporary parallel `16x2` LCD so we can see useful state immediately:

- `RS = 12`
- `EN = 11`
- `D4 = 5`
- `D5 = 4`
- `D6 = 3`
- `D7 = 2`

### Buttons

The starter uses direct button wiring with `INPUT_PULLUP`.

Buttons to ground:

- `0..9` -> pins `22..31`
- `VERB` -> `32`
- `NOUN` -> `33`
- `CLR` -> `34`
- `PRO` -> `35`
- `KEY REL` -> `36`
- `ENTR` -> `37`
- `RSET` -> `38`
- `+` -> `39`
- `-` -> `40`

### Lamps

The starter exposes these lamp outputs:

- `COMP ACTY` -> `41`
- `UPLINK ACTY` -> `42`
- `TEMP` -> `43`
- `GIMBAL LOCK` -> `44`
- `PROG` -> `45`
- `KEY REL` -> `46`
- `OPR ERR` -> `47`
- `STBY` -> `48`
- `NO ATT` -> `49`
- `TRACKER` -> `50`

You do not have to wire all lamps immediately. Unused outputs can stay disconnected.

## Serial protocol

The protocol is line-based and human-readable on purpose.

From DSKY slaves to `ESP32`:

- `KEY,VERB`
- `KEY,NOUN`
- `KEY,0`
- `KEY,1`
- `KEY,ENTR`

From `ESP32` to DSKY slaves:

- `STATE,<program>,<verb>,<noun>,<r1>,<r2>,<r3>,<alarm>,<flash>,<lampMask>,<missionSeconds>`
- `PHASE,<label>,<r1Label>,<r2Label>,<r3Label>`

Example:

```text
STATE,0,16,36,+00012,+00034,+00056,1202,1,96,1234
```

## Recommended ESP32 + ESP8266 upload order

1. Flash `esp8266_dsky_slave` to the ESP8266.
2. Flash `esp32_agc_core` to the ESP32.
3. Open USB serial on both boards at `115200`.
4. Power both boards with a shared ground and connect the UART link.
5. Open the ESP8266 Web DSKY URL shown on its serial monitor.
6. Press `VERB`, `3`, `7`, `NOUN`, `3`, `6` in the web page.
7. Watch the ESP32 serial monitor for clean `AGC ...` status updates.

## What this is not yet

This is not yet:

- a bundled `yaYUL` executable
- a historically faithful Block II AGC CPU
- a complete `PINBALL` implementation
- high-fidelity IMU/CDU/radar/propulsion physics

Those are the next layers.

## Next milestones

1. Replace the bring-up program with a `yaYUL` rope image and test the exact opcodes it reaches.
2. Tighten Block II instruction semantics, interrupt timing, and I/O channel behavior against the original AGC docs.
3. Move the Mega LCD from temporary debug display toward a more DSKY-like numeric display.
4. Expand the lamp set and key handling to track real `PINBALL` behavior more closely.
