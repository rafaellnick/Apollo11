# Arduino Uno Port Start

## Current role in the project

The Uno path is now a constrained frontend/subset path, not the main native AGC core path.

The current high-fidelity direction is:

- `ESP32`: native AGC core, rope loader, timing model, serial monitor, joystick input
- `ESP8266`: first Web DSKY slave over Wi-Fi and UART
- `Arduino Mega`: larger physical DSKY/panel option
- `Arduino Uno`: compact LCD/serial terminal, host-driven display, or very small AGC-like subset

This folder remains useful for a minimal `16x2 LCD + serial` build, but the Uno cannot hold a faithful Block II AGC core plus a Comanche/Luminary rope image. If the goal is historical validation, use the ESP32/desktop harness documented in `embedded/README.md` and `embedded/ROPE_BUILD.md`.

## What this repository contains

This repository is not a modern C or C++ codebase. It is the original Apollo Guidance Computer assembly source, split into include files for two Apollo 11 flight programs:

- `Comanche055`: Command Module software
- `Luminary099`: Lunar Module software

The code is still organized like the original monolithic build. `MAIN.agc` is just an include list, not a linker-driven project.

## What matters for an Arduino Uno port

The files that matter most for a usable `LCD + serial` port are these:

- `EXECUTIVE.agc`: job scheduling, priorities, sleep/wake
- `WAITLIST.agc`: deferred work
- `INTERPRETER.agc`: the interpretive virtual machine used by a large part of the guidance code
- `PINBALL_GAME_BUTTONS_AND_LIGHTS.agc`: keyboard/display logic, verbs, nouns, monitors
- `DISPLAY_INTERFACE_ROUTINES.agc`: display requests, priorities, flashing displays, operator interaction
- `KEYRUPT_UPRUPT.agc`: keyboard and uplink interrupt entry points
- `T4RUPT_PROGRAM.agc`: periodic display refresh and sampled input handling
- `DOWN-TELEMETRY_PROGRAM.agc` and `DOWNLINK_LISTS.agc`: serializable state and downlink formatting

In other words: the AGC is not "just guidance math". It is a complete real-time system with its own scheduler, interrupt model, display subsystem, telemetry subsystem, and banked memory model.

## Feasibility on an Uno

A faithful full native port is not realistic on an Arduino Uno.

Why:

- A Block II AGC has `36,864` words of fixed memory. If represented as `uint16_t`, that is about `72 KB` before any emulator overhead.
- A Block II AGC has `2,048` words of erasable memory. If represented as `uint16_t`, that is about `4 KB` of RAM before stack, buffers, LCD state, and serial I/O.
- An Arduino Uno has only `32 KB` flash and `2 KB` SRAM.

That means even a compact whole-machine emulator runs out of Uno memory before we add:

- the AGC scheduler
- bank switching state
- DSKY/LCD formatting
- serial telemetry buffers
- command parsing

So the Uno project should be framed as one of these three paths:

1. `Uno frontend + external AGC core`
   The Uno becomes a DSKY/telemetry terminal. A PC or larger MCU runs the AGC logic.
2. `Uno-native AGC subset`
   Recreate a narrow, useful slice of AGC behavior: a few verbs, nouns, monitor pages, alarms, and selected mission math.
3. `ESP32 native core, Uno panel`
   The ESP32 runs the AGC core while the Uno drives a small LCD or status panel.

For the Uno specifically, path `1` or `2` is the right place to start.

## Recommended starting architecture

### Phase 1: lock down the operator interface

Use the Uno as a front-end first:

- `16x2 LCD`: condensed state only
- `Serial`: fuller telemetry stream and control channel

That gets the physical interface working before we decide how much AGC logic will actually live on the Uno.

### Phase 2: port the DSKY-facing concepts, not the whole machine

Start with concepts from `PINBALL`, `DISPLAY_INTERFACE_ROUTINES`, `T4RUPT`, and `DOWN-TELEMETRY_PROGRAM`:

- major mode / program number
- verb / noun display
- R1 / R2 / R3 register presentation
- alarm codes
- periodic monitor updates
- operator commands over serial

This is a good fit for the user's target: basic data on the LCD, everything else on serial.

### Phase 3: choose the execution model

After the interface is stable, pick one:

- host-driven mode: a PC script or future emulator sends state to the Uno
- subset mode: the Uno computes a reduced local model

The host-driven model is the shortest path to seeing real AGC-like behavior on hardware.

## Suggested LCD mapping

The `16x2` display is too small to mirror the DSKY directly, so the practical mapping is:

- line 1: `Pxx Vxx Nxx`
- line 2: rotating detail page

Suggested detail pages:

- page 0: alarm + mission clock
- page 1: `R1`
- page 2: `R2`
- page 3: `R3`
- page 4: flags / status

The serial stream can carry the wider state:

- current page data
- all three registers
- alarm flags
- flash state
- key-release state
- raw telemetry snapshots later

## Baseline recommendation

Use `Comanche055` as the initial source reference for the porting work.

Reason:

- the codebase inspection for this start was centered on `Comanche055`
- the UI / executive / interrupt structure is representative
- the DSKY-facing code is already clear there

If the goal shifts toward landing-specific behavior, we can switch the mission layer over to `Luminary099` while keeping the same Uno frontend.

## Current starter in this folder

`agc_uno_frontend/agc_uno_frontend.ino` is a first frontend skeleton for the Uno. It does not pretend to run the full AGC. It gives us:

- a stable `LiquidCrystal` output layer
- a compact LCD page model
- a serial command protocol
- periodic serial telemetry dumps
- a demo mode so the hardware can be tested before the AGC core exists

## Serial protocol in the starter sketch

Supported commands:

- `P=nn`
- `V=nn`
- `N=nn`
- `A=nnnn`
- `T=seconds`
- `R1=text`
- `R2=text`
- `R3=text`
- `FLASH=0|1`
- `KEYREL=0|1`
- `AUTO=0|1`
- `PAGE=0..4`
- `DEMO=0|1`
- `DUMP`
- `HELP`

This makes the Uno immediately usable as a hardware display target for later scripts, parsers, or a reduced AGC runtime.

## Next recommended steps

1. Build and test the Uno frontend on real hardware.
2. Add a small host-side tool that pushes `P/V/N/R1/R2/R3` updates over serial.
3. Extract a minimal set of verbs/nouns from `PINBALL` and `ASSEMBLY_AND_OPERATION_INFORMATION`.
4. Decide whether the next milestone is:
   - a host-driven AGC viewer, or
   - a standalone Uno-native subset
5. If full instruction-level fidelity is required, keep the execution core off the Uno and use the ESP32 or desktop trace harness.

## Relationship to the native AGC core

The embedded ESP32 core now has:

- yaYUL rope-image loading for Comanche/Luminary builds
- Block II memory, opcode, channel, interrupt, editing-register, and timing models
- first-pass downrupt/uplink/radar/hand-controller/restart behavior
- yaAGC trace-validation tooling with CPU-only and faithful hardware-timing windows

That work should not be duplicated on the Uno. The recommended Uno milestone is to consume status frames from a host or ESP32 and present them on the LCD/serial interface.
