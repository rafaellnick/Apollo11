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
- an `ESP32` AGC core shell that sends status to both the DSKY slave and the PC USB serial monitor
- ESP32 joystick input support for `VRX`, `VRY`, and `SW`
- an `ESP8266` DSKY slave for the first bench test
- a `Mega` starter that scans buttons, drives lamps, and mirrors state to an LCD

The ESP32 core currently runs a tiny AGC bring-up program that increments an erasable counter. The next layer is loading a real assembled rope image.

## Folder layout

- `shared/agc_core.h`: first AGC CPU/memory layer
- `shared/dsky_protocol.h`: protocol, key definitions, lamp bits, frame parsing
- `esp32_agc_core/esp32_agc_core.ino`: ESP32 AGC core shell
- `esp8266_dsky_slave/esp8266_dsky_slave.ino`: ESP8266 first-test DSKY slave
- `mega_dsky_panel/mega_dsky_panel.ino`: Mega-side panel controller
- `tests/agc_core_selftest.cpp`: desktop self-test for the core

Each Arduino sketch folder also contains local copies of the headers it needs. This is intentional: the Arduino IDE compiles a sketch folder as a standalone unit, so includes like `../shared/dsky_protocol.h` may fail when the sketch is opened directly.

## First Test Wiring: ESP32 + ESP8266

Use a dedicated UART between the boards.

The ESP32 uses hardware `Serial2`. The ESP8266 uses `SoftwareSerial` so its USB `Serial` remains available for typed key commands and local monitoring.

- `ESP32 TX2 GPIO17` -> `ESP8266 GPIO14 / NodeMCU D5`
- `ESP8266 GPIO12 / NodeMCU D6` -> `ESP32 RX2 GPIO16`
- `ESP32 GND` -> `ESP8266 GND`

Both boards are `3.3V`, so no level shifter is needed for this first test.

The board-to-board link runs at `38400` baud. The PC USB serial monitor on each board runs at `115200` baud.

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

## PC Monitoring

Open the ESP32 USB serial monitor at `115200`.

The ESP32 automatically prints:

- clean `AGC ...` human-readable status once per second
- `STATE,...` machine-readable frames to the DSKY slave every 250 ms
- `CORE ...` when you type `CORE`

By default, the PC USB serial uses clean output only. The DSKY UART still receives raw `STATE,...` frames.

Useful ESP32 USB commands:

- `HELP`
- `STATUS`
- `CORE`
- `STATE`
- `STEP`
- `HALT`
- `RUN`
- `RESET`
- `PEEK,<octal-address>`
- `POKE,<octal-address>,<octal-word>`
- `JOY`
- `JOYCAL`
- `USB,CLEAN`
- `USB,RAW`
- `USB,BOTH`
- `USB,QUIET`
- `KEY,VERB`
- `KEY,NOUN`
- `KEY,3`

## ESP8266 DSKY Test Commands

Open the ESP8266 USB serial monitor at `115200`.

The ESP8266 prints DSKY state received from the ESP32 and forwards key commands back to the ESP32.

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

Example:

```text
STATE,0,16,36,+00012,+00034,+00056,1202,1,96,1234
```

## Recommended ESP32 + ESP8266 upload order

1. Flash `esp8266_dsky_slave` to the ESP8266.
2. Flash `esp32_agc_core` to the ESP32.
3. Open USB serial on both boards at `115200`.
4. Power both boards with a shared ground and connect the UART link.
5. On the ESP8266 serial monitor, type `V`, `3`, `7`, `N`, `3`, `6`.
6. Watch the ESP32 serial monitor for clean `AGC ...` status updates.

## What this is not yet

This is not yet:

- a `yaYUL` build pipeline
- a rope-image loader
- a complete AGC CPU core
- simulated IMU/CDU/radar channels

Those are the next layers.

## Next milestones

1. Expand the AGC CPU instruction set and addressing model.
2. Add a host-side or build-time path for assembled rope images.
3. Move the Mega LCD from temporary debug display toward a more DSKY-like numeric display.
4. Expand the lamp set and key handling to track real `PINBALL` behavior more closely.
