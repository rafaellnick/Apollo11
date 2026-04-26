# Rope Image Build Path

This project has an embedded rope-image hook for the ESP32 AGC core and a local helper for the Virtual AGC source layout in this repository.

The local Apollo 11 source directories are already in yaYUL's expected shape:

- `Comanche055/MAIN.agc` includes the Command Module source files.
- `Luminary099/MAIN.agc` includes the Lunar Module source files.

The workspace does not currently contain a working local `yaYUL` executable, so the helper validates the source tree, searches for `yaYUL`, and prints the exact next command when the assembler is missing.

## One-command path

From the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File embedded\tools\build_rope_image.ps1 -Program Comanche055
```

For the Lunar Module rope:

```powershell
powershell -ExecutionPolicy Bypass -File embedded\tools\build_rope_image.ps1 -Program Luminary099
```

If `yaYUL.exe` is not on `PATH`, place or build it locally and pass the path explicitly:

```powershell
powershell -ExecutionPolicy Bypass -File embedded\tools\build_rope_image.ps1 -Program Comanche055 -YaYulPath .\yaYUL.exe
```

The helper copies the selected source tree to a temporary build directory before assembly, so `MAIN.agc.bin`, the symbol table, and the listing are not written into `Comanche055/` or `Luminary099/`. It also creates temporary include aliases from each file's `# Filename:` metadata; this handles the local `Luminary099` checkout where two filenames differ from the names included by `MAIN.agc`. When assembly succeeds, it converts yaYUL's `MAIN.agc.bin` into `embedded/esp32_agc_core/rope_image.h`.

The equivalent raw yaYUL command, if you are assembling manually from a source directory, is:

```powershell
..\yaYUL\yaYUL MAIN.agc > Comanche055.lst
```

yaYUL writes `MAIN.agc.bin` next to `MAIN.agc`. That `.bin` file is big-endian, stores each 15-bit AGC word shifted left by one parity bit, and orders banks as `2,3,0,1,4...`. The converter's `--format yayul` mode normalizes that into the ESP32 core's bank order.

## Convert a rope dump

From the repository root:

```powershell
py embedded\tools\rope_to_header.py path\to\rope_dump.txt embedded\esp32_agc_core\rope_image.h --format text --name "Comanche055"
```

For yaYUL output:

```powershell
py embedded\tools\rope_to_header.py Comanche055\MAIN.agc.bin embedded\esp32_agc_core\rope_image.h --format yayul --name "Comanche055"
```

For raw binary 16-bit little-endian words:

```powershell
py embedded\tools\rope_to_header.py path\to\rope_dump.bin embedded\esp32_agc_core\rope_image.h --format binary --name "Comanche055"
```

`--format auto` treats `*.agc.bin` as yaYUL output. Other files are parsed as text first and fall back to raw little-endian binary if too few octal words are found.

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
