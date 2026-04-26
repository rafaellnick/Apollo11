# Apollo 11 Launch Simulation Manual

This manual explains how to run the launch simulation with:

- `ESP32`: AGC core board and ascent monitor
- `ESP8266`: DSKY slave with the browser interface
- PC or phone browser: DSKY buttons and displays
- ESP32 USB serial: clean mission monitoring

Important: this simulation is not yet the real Comanche rope flying a complete Saturn V ascent. It is a `Launch Monitor`, a mission-layer test mode that drives the DSKY through the major Apollo 11 ascent events while the embedded AGC core continues to run underneath. After parking orbit insertion, the launch monitor now chains into the automated Apollo 11 mission procedure sequencer.

For the wider mission command table beyond launch, see `APOLLO11_MISSION_PROGRAM.md`.

## Files Used

- `embedded/esp32_agc_core/esp32_agc_core.ino`: ESP32 AGC core and launch monitor
- `embedded/esp8266_dsky_slave/esp8266_dsky_slave.ino`: ESP8266 DSKY slave
- `embedded/esp8266_dsky_slave/web_dsky_page.h`: browser DSKY page served by the ESP8266
- `embedded/esp8266_dsky_slave/wifi_config.h.example`: optional Wi-Fi template for joining your local network

## ESP32 To ESP8266 Link

Use a dedicated UART between the boards.

```text
ESP32 GPIO17 / TX2  ->  ESP8266 GPIO14 / NodeMCU D5
ESP32 GPIO16 / RX2  <-  ESP8266 GPIO12 / NodeMCU D6
ESP32 GND           ->  ESP8266 GND
```

Both boards use `3.3V` logic. Do not drive the ESP8266 UART with a `5V` signal.

Serial speeds:

- board-to-board UART: `38400`
- ESP32 USB serial: `115200`
- ESP8266 USB serial: `115200`

## Board Preparation

1. Open `embedded/esp8266_dsky_slave/esp8266_dsky_slave.ino` in the Arduino IDE.
2. Select your ESP8266 board and upload the sketch.
3. Open `embedded/esp32_agc_core/esp32_agc_core.ino` in the Arduino IDE.
4. Select your ESP32 board and upload the sketch.
5. Wire the board-to-board UART using the pinout above.
6. Open the ESP32 serial monitor at `115200`.
7. Open the ESP8266 serial monitor at `115200`.

## DSKY Wi-Fi

With no extra configuration, the ESP8266 starts its own access point:

```text
SSID: AGC-DSKY
Password: apollo11
URL: http://192.168.4.1
```

Connect your PC or phone to that network and open `http://192.168.4.1`.

To connect the ESP8266 to your own Wi-Fi network, create a local file named `embedded/esp8266_dsky_slave/wifi_config.h`:

```cpp
#pragma once

#define DSKY_WIFI_SSID "YourWiFiName"
#define DSKY_WIFI_PASSWORD "YourWiFiPassword"

#define DSKY_AP_SSID "AGC-DSKY"
#define DSKY_AP_PASSWORD "apollo11"
```

That file is ignored by git, so your password will not be committed. When station mode connects, the ESP8266 prints its IP address on USB serial and also tries to publish `http://agc-dsky.local` through mDNS.

## Quick Start

On the Web DSKY, press:

```text
VERB 37 NOUN 11 ENTR
```

Button-by-button sequence:

```text
VERB
3
7
NOUN
1
1
ENTR
```

The DSKY should change to:

```text
P11 V16 N62
```

During the simulation:

- `R1`: mission time in seconds
- `R2`: approximate altitude in kilometers
- `R3`: approximate velocity in meters per second

Negative `R1` values represent the final countdown. For example, `-00010` means `T-10`.

## Stop The Simulation

On the Web DSKY:

```text
VERB 37 NOUN 00 ENTR
```

Button-by-button sequence:

```text
VERB
3
7
NOUN
0
0
ENTR
```

## Run In Real Time

By default, the simulation runs at `x20`, so ascent to parking orbit takes about 36 seconds.

To run in real time from the DSKY:

```text
VERB 37 NOUN 12 ENTR
```

Button-by-button sequence:

```text
VERB
3
7
NOUN
1
2
ENTR
```

## ESP32 Serial Commands

In the ESP32 serial monitor at `115200`, you can use:

```text
LAUNCH
LAUNCH,STOP
LAUNCH,STATUS
LAUNCH,SPEED,20
LAUNCH,REALTIME
APOLLO11,FULL
MISSION,SPEED,10
MISSION,REALTIME
```

Meaning:

- `LAUNCH`: starts the simulation using the current accelerated-time setting
- `LAUNCH,STOP`: stops the simulation and returns the panel to `P00 V16 N36`
- `LAUNCH,STATUS`: prints the current simulation state
- `LAUNCH,SPEED,20`: sets the time scale from `1` to `100`
- `LAUNCH,REALTIME`: sets the time scale to `x1`
- `APOLLO11,FULL`: starts the complete launch-to-recovery sequence
- `MISSION,SPEED,10`: sets the automated post-ascent procedure speed
- `MISSION,REALTIME`: sets automated post-ascent procedure steps to `x1`

## Expected ESP32 Serial Output

With the simulation running, the ESP32 clean output includes an `ASC` block:

```text
AGC P11 V16 N62 | R1 +00013 R2 +00002 R3 +00081 | ALM 0000 | JOY +0000,+0000,0 | Z 4000 A 21521 | CYC 123456 | RUN | ASC T+00:13 ROLL PROGRAM ALT 2km VEL 81m/s x20
```

Major events also appear as `LAUNCH` lines:

```text
LAUNCH T+00:00 LIFTOFF | P11 V16 N62 | R1 T+0 R2 ALT_KM 0 R3 VEL_MS 0 | x20
LAUNCH T+01:23 MAX-Q | P11 V16 N62 | R1 T+83 R2 ALT_KM 14 R3 VEL_MS 520 | x20
LAUNCH T+11:45 PARKING ORBIT | P11 V16 N62 | R1 T+705 R2 ALT_KM 185 R3 VEL_MS 7800 | x20 COMPLETE
APOLLO11 AUTO N20 STEP 1/66 P11 V16 ORBIT INSERTION CHECK | GET_MIN 12 | ALT_KM 185 | VEL_MS 7800 | ALM 0000 | ELAPSED 0/12 | x10
```

## Simulated Timeline

These are the events currently encoded in the ESP32 launch monitor:

| Time | Serial event |
| --- | --- |
| `T-00:10` | `TERMINAL COUNT` |
| `T-00:08` | `F-1 IGNITION` |
| `T+00:00` | `LIFTOFF` |
| `T+00:13` | `ROLL PROGRAM` |
| `T+00:34` | `ROLL COMPLETE` |
| `T+01:23` | `MAX-Q` |
| `T+02:17` | `S-IC INBOARD CUTOFF` |
| `T+02:44` | `S-IC/S-II STAGING` |
| `T+03:13` | `INTERSTAGE SEP` |
| `T+03:17` | `LES JETTISON` |
| `T+05:27` | `S-IVB TO COI` |
| `T+07:42` | `S-II INBOARD CUTOFF` |
| `T+08:22` | `MIXTURE SHIFT` |
| `T+09:00` | `ABORT MODE IV` |
| `T+09:15` | `S-II/S-IVB STAGING` |
| `T+11:45` | `PARKING ORBIT` |

Altitudes and velocities are approximations for DSKY visualization. They are not a complete Saturn V flight-dynamics simulation.

## What To Watch On The Web DSKY

At start:

- `PROG` should become `11`
- `VERB` should become `16`
- `NOUN` should become `62`
- `R1` should start near `-00010`
- `R2` and `R3` should start near `+00000`

During ascent:

- `R1` increases to `+00705`
- `R2` rises to about `+00185`
- `R3` rises to about `+07800`
- some lamps may light to show program activity, uplink/telemetry, and tracking state

At completion:

- USB serial prints `PARKING ORBIT`
- the launch display briefly reaches `P11 V16 N62`
- the automated mission sequencer starts at `N20`
- the DSKY phase label advances through orbit checks, TLI, docking, coast, LOI, descent, landing, surface operations, ascent, rendezvous, TEI, entry, splashdown, and recovery

## Troubleshooting

If the Web DSKY opens but does not change state:

- confirm that the ESP32 and ESP8266 share `GND`
- confirm `ESP32 GPIO17 -> ESP8266 D5/GPIO14`
- confirm `ESP8266 D6/GPIO12 -> ESP32 GPIO16`
- confirm both sketches were uploaded after the latest changes
- open the ESP8266 serial monitor and look for `DSKY LINK=1`

If the Web DSKY does not open:

- connect to the `AGC-DSKY` Wi-Fi network
- open `http://192.168.4.1`
- if using your own Wi-Fi network, read the IP address printed by the ESP8266 serial monitor
- try `http://agc-dsky.local` if your system supports mDNS

If the ESP32 does not print `LAUNCH` lines:

- open the ESP32 serial monitor at `115200`
- send `LAUNCH,STATUS`
- send `LAUNCH`
- confirm USB output is not quiet by sending `USB,CLEAN`

If numeric DSKY input triggers an alarm:

- press `VERB` or `NOUN` first
- then enter exactly two digits
- to start the launch simulation, use `VERB 37 NOUN 11 ENTR`

## Historical References

- [Apollo 11 Flight Journal, Day 1: Launch](https://www.apollojournals.org/afj/ap11fj/01launch.html)
- [NASA Apollo 11 Mission Overview](https://www.nasa.gov/history/apollo-11-mission-overview/)
