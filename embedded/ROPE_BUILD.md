# Rope Image Build Path

This project now has the first embedded rope-image hook for the ESP32 AGC core.

It is not a full `yaYUL` automation yet, because the workspace does not contain a working local `yaYUL` executable in this environment. The path is ready for it:

1. Assemble the AGC source with `yaYUL`.
2. Export or save the assembled rope as octal text words or little-endian 16-bit binary words.
3. Convert that rope dump into `embedded/esp32_agc_core/rope_image.h`.
4. Flash `embedded/esp32_agc_core` to the ESP32.
5. Use `ROPE,LOAD` on the ESP32 serial monitor.

## Convert a rope dump

From the repository root:

```powershell
python embedded/tools/rope_to_header.py path\to\rope_dump.txt embedded\esp32_agc_core\rope_image.h --format text --name "Comanche055"
```

For binary 16-bit little-endian words:

```powershell
python embedded/tools/rope_to_header.py path\to\rope_dump.bin embedded\esp32_agc_core\rope_image.h --format binary --name "Comanche055"
```

`--format auto` tries text first and falls back to binary if too few octal words are found.

## ESP32 commands

```text
ROPE,INFO
ROPE,LOAD
CORE
```

`ROPE,INFO` prints the embedded image name, bank count, and word count.

`ROPE,LOAD` copies the generated fixed-memory banks into the AGC core and starts execution at the boot address.

## Current emulator status

The core now has:

- fixed-bank loading through `agc::RopeImage`
- fixed-bank window selection through `FBANK`
- I/O channel storage with `READ/WRITE` style extended operations
- interrupt request/pending plumbing
- basic `ZERO`, `CYR`, `SR`, and `CYL` special-register behavior
- initial extended opcode handling for divide, branch-zero, subtract, channel read/write/and/xor

This is still not enough to run the full original Comanche software correctly. The next hard pieces are exact Block II opcode decoding, complete editing-register behavior, downrupt/rupt timing, and faithful channel behavior.
