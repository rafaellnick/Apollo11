# Apollo 11 Mission Program Commands

This document describes the ESP32 `Apollo 11 Mission Program` bench layer and its automated mission-procedure sequencer.

It does not replace real Comanche or Luminary execution. Its job is to provide DSKY-selectable mission situations and automatic checklist-inspired phase procedures for the ESP32/ESP8266 hardware stack while the native AGC core, rope loader, timing model, and peripheral models keep moving toward historical validation.

Use this layer for DSKY, serial, joystick, Web DSKY, and panel testing. Use the rope-validation tools in `ROPE_BUILD.md` when the goal is instruction-level comparison against yaAGC/VirtualAGC.

## Run From The Web DSKY

Use this format:

```text
VERB 37 NOUN xx ENTR
```

`xx` is the mission-situation noun.

Accelerated launch example:

```text
VERB 37 NOUN 11 ENTR
```

Lunar landing surface-state example:

```text
VERB 37 NOUN 28 ENTR
```

## Run From ESP32 Serial

Open the ESP32 USB serial monitor at `115200`.

Supported commands:

```text
APOLLO11,LIST
APOLLO11,STATUS
APOLLO11,STOP
APOLLO11,FULL
APOLLO11,28
MISSION,28
```

`APOLLO11,<noun>` and `MISSION,<noun>` are equivalent.

The automated procedure speed can be adjusted without restarting:

```text
MISSION,SPEED,10
MISSION,REALTIME
APOLLO11,SPEED,20
```

## DSKY Readout

For automated mission procedures, the DSKY shows the currently active phase step:

- `PROG`: approximate AGC program for that phase
- `VERB`: normally `16`, monitor mode
- `NOUN`: selected mission noun, updated as the automatic sequence advances
- `R1`: GET in minutes, or a phase-specific field when noted
- `R2`: altitude, range, burn time, or phase-specific value
- `R3`: velocity, delta-v, or status code

For launch `N11` or `N12`, the display first changes to `P11 V16 N62`:

- `R1`: mission time in seconds from liftoff, negative before `T+00:00`
- `R2`: approximate altitude in kilometers
- `R3`: approximate velocity in meters per second

After parking orbit insertion, `N11` and `N12` automatically continue into the mission-procedure sequence at `N20`, then proceed through TLI, transposition/docking, translunar coast, LOI, lunar orbit, descent, landing, EVA, ascent, rendezvous, TEI, reentry, splashdown, and recovery.

## DSKY Command Table

| Command | Situation | Expected display | R1 | R2 | R3 |
| --- | --- | --- | --- | --- | --- |
| `V37 N00 ENTR` | Stop mission program | `P00 V16 N36` | AGC counter | A | Z |
| `V37 N11 ENTR` | Full mission from launch, accelerated `x20` | `P11 V16 N62`, then auto sequence | `T+ sec` | `ALT_KM` | `VEL_MS` |
| `V37 N12 ENTR` | Full mission from launch, real time `x1` | `P11 V16 N62`, then auto sequence | `T+ sec` | `ALT_KM` | `VEL_MS` |
| `V37 N20 ENTR` | Auto mission from Earth parking orbit | `P11 V16 N20` | `GET_MIN` | `ALT_KM` | `VEL_MS` |
| `V37 N21 ENTR` | Auto mission from translunar injection | `P15 V16 N21` | `GET_MIN` | `BURN_SEC` | `DV_MPS` |
| `V37 N22 ENTR` | CSM/LM transposition and docking | `P17 V16 N22` | `GET_MIN` | `RANGE_M` | `DOCKED` |
| `V37 N23 ENTR` | Translunar coast | `P23 V16 N23` | `GET_MIN` | `DIST_KKM` | `MCC` |
| `V37 N24 ENTR` | Lunar orbit insertion | `P40 V16 N24` | `GET_MIN` | `BURN_SEC` | `DV_MPS` |
| `V37 N25 ENTR` | Lunar orbit | `P20 V16 N25` | `GET_MIN` | `ALT_KM` | `ORBIT` |
| `V37 N26 ENTR` | Powered descent | `P63 V16 N26` | `GET_MIN` | `ALT_KM` | `VEL_MS` |
| `V37 N27 ENTR` | Final landing phase | `P66 V16 N27` | `GET_MIN` | `ALT_M` | `VEL_MS` |
| `V37 N28 ENTR` | Landed on lunar surface | `P68 V16 N28` | `GET_MIN` | `ALT_M` | `LANDED` |
| `V37 N29 ENTR` | Lunar EVA | `P00 V16 N29` | `GET_MIN` | `EVA_MIN` | `SAMPLE_KG` |
| `V37 N30 ENTR` | LM ascent | `P12 V16 N30` | `GET_MIN` | `BURN_SEC` | `VEL_MS` |
| `V37 N31 ENTR` | Rendezvous and docking | `P20 V16 N31` | `GET_MIN` | `RANGE_KM` | `DOCKED` |
| `V37 N32 ENTR` | Transearth injection | `P40 V16 N32` | `GET_MIN` | `BURN_SEC` | `DV_MPS` |
| `V37 N33 ENTR` | Transearth coast | `P23 V16 N33` | `GET_MIN` | `DIST_KKM` | `MCC` |
| `V37 N34 ENTR` | Reentry interface | `P61 V16 N34` | `GET_MIN` | `ALT_KM` | `VEL_MS` |
| `V37 N35 ENTR` | Splashdown and recovery procedure | `P67 V16 N35` | `GET_MIN` | `ALT_KM` | `RECOVERY` |
| `V37 N40 ENTR` | Launch abort Mode I | `P70 V16 N40` | `GET_MIN` | `MODE` | `STATUS` |
| `V37 N41 ENTR` | Earth-orbit abort | `P37 V16 N41` | `GET_MIN` | `DV_MPS` | `STATUS` |
| `V37 N42 ENTR` | Translunar abort / free return | `P37 V16 N42` | `GET_MIN` | `MCC` | `STATUS` |
| `V37 N43 ENTR` | Lunar-descent abort | `P71 V16 N43` | `GET_MIN` | `ALT_KM` | `STATUS` |
| `V37 N44 ENTR` | Program alarm `1202` demo | `P63 V16 N44` | `ALARM` | `RECYCLE` | `STATUS` |
| `V37 N45 ENTR` | Program alarm `1201` demo | `P63 V16 N45` | `ALARM` | `RECYCLE` | `STATUS` |
| `V37 N46 ENTR` | Loss of communication | `P00 V16 N46` | `GET_MIN` | `UPLINK` | `STATUS` |
| `V37 N47 ENTR` | IMU realignment | `P52 V16 N47` | `GET_MIN` | `STAR` | `STATUS` |
| `V37 N48 ENTR` | Manual attitude / RHC | `P00 V16 N48` | `GET_MIN` | `RHC` | `STATUS` |

## Expected Serial Output

The older static status format is still used as a fallback for nouns without a procedure:

```text
APOLLO11 N28 P68 V16 LANDED SURFACE | GET_MIN 6155 | ALT_M 0 | LANDED 1 | ALM 0000
```

Selecting an automated procedure:

```text
APOLLO11 AUTO N21 STEP 1/62 P15 V16 P15 TLI ENABLE | GET_MIN 164 | BURN_SEC 348 | DV_MPS 3200 | ALM 0000 | ELAPSED 0/10 | x10
```

The clean ESP32 status line also includes the `MSN` block:

```text
AGC P68 V16 N28 | R1 +06155 R2 +00000 R3 +00001 | ALM 0000 | JOY +0000,+0000,0 | Z 4000 A 21521 | CYC 123456 | RUN | MSN N28 LANDED SURFACE | GET_MIN 6155 | ALT_M 0 | LANDED 1
```

Selecting an alarm demo:

```text
VERB 37 NOUN 44 ENTR
```

Expected output:

```text
APOLLO11 N44 P63 V16 PROGRAM ALARM 1202 | ALARM 1202 | RECYCLE 1 | STATUS 0 | ALM 1202
```

## Recommended Test Commands

Full mission path:

```text
V37 N11 ENTR
```

Phase jump tests:

```text
V37 N20 ENTR
V37 N21 ENTR
V37 N24 ENTR
V37 N26 ENTR
V37 N28 ENTR
V37 N30 ENTR
V37 N32 ENTR
V37 N35 ENTR
```

In the current implementation, selecting `N11` already runs that full path automatically after ascent. Selecting any nominal mission noun from `N20` through `N35` starts at that phase and continues to recovery. Selecting abort/demo nouns from `N40` through `N48` runs only that abnormal procedure.

Abnormal-situation path:

```text
V37 N40 ENTR
V37 N43 ENTR
V37 N44 ENTR
V37 N45 ENTR
V37 N46 ENTR
```

Return to the basic monitor display:

```text
V37 N00 ENTR
```

## Current Fidelity Boundary

This mission layer is intentionally operational, not historically exact:

- it does not run the full Comanche or Luminary job tree for each mission phase
- it models major crew/AGC procedure steps, not every switch throw, callout, checklist line, or spacecraft-system transient
- it does not calculate a high-fidelity Saturn V, CSM, or LM trajectory
- it does not yet model IMU, rendezvous radar, landing radar, SPS, DPS, APS, RCS, or telemetry electronics as physical devices
- it is not a substitute for the real Apollo 11 crew checklists

Its current value is hardware coverage. It lets the ESP32 core, ESP8266 Web DSKY, optional Mega panel, joystick input, serial monitor, and display protocol exercise all major mission modes while the native AGC core is validated against yaAGC/VirtualAGC.

## Native AGC Core Status

Implemented in the native core and validation path:

- yaYUL rope-image build and conversion for `Comanche055` and `Luminary099`
- ESP32 `ROPE,INFO` / `ROPE,LOAD` loader commands
- Block II memory mapping for direct erasable, switched erasable, common fixed, and fixed-fixed windows
- explicit Block II basic, extracode, quarter-code, and peripheral-code instruction dispatch
- editing-register behavior for `CYR`, `SR`, `CYL`, and `EDOP`
- 9-bit AGC I/O channels, including `L`/`Q` aliases and `SUPERBNK`
- interrupt vectors for `T6RUPT`, `T5RUPT`, `T3RUPT`, `T4RUPT`, `KEYRUPT1/2`, `UPRUPT`, `DOWNRUPT`, `RADAR`, and `HANDRUPT`
- MCT cycle accounting for instructions, scaler steals, counter pulses, interrupt entry rows, and peripheral timing
- deterministic peripheral models for key input, downrupt/downlink, uplink, radar source words, hand-controller traps, and restart-monitor latches
- bit-level uplink with parity conversion and channel-77 parity-fail restart behavior
- downlink frame capture for channel `034/035` pairs
- real rope trace validation against yaAGC, including CPU-only and faithful hardware-timing windows
- automated Apollo 11 procedure steps for launch-to-recovery, TLI, LOI, descent, surface operations, ascent, rendezvous, TEI, entry, recovery, and abnormal cases

Still required before claiming a complete historical AGC replica:

- continuous mission-length Comanche and Luminary validation runs
- broader trace sampling around every major mission phase
- faithful spacecraft electrical and sensor data-source models
- full IMU, optics, radar, propulsion, RCS, telemetry, and uplink/downlink environment simulation
