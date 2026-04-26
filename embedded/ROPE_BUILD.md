# Rope Image Build Path

This project has an embedded rope-image hook for the ESP32 AGC core and a local helper for the Virtual AGC source layout in this repository.

The local Apollo 11 source directories are already in yaYUL's expected shape:

- `Comanche055/MAIN.agc` includes the Command Module source files.
- `Luminary099/MAIN.agc` includes the Lunar Module source files.

The helper validates the source tree, searches for `yaYUL`, and prints the exact next command when the assembler is missing. In this workspace the normal path uses the local `yaYUL.exe` at the repository root.

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

The GitHub Apollo 11 source layout has a few lines whose yaYUL-significant indentation differs from the maintained VirtualAGC checkout. The helper normalizes those local numeric labels and applies two Comanche055 compatibility fixes only inside the temporary build copy; the historical source files in `Comanche055/` and `Luminary099/` are not rewritten.

After conversion, the helper also writes `embedded/esp32_agc_core/rope_image.manifest.txt` with the selected program, source path, generated word count, bank count, and SHA-256 hashes of both `MAIN.agc.bin` and `rope_image.h`.

To validate the source tree without requiring yaYUL yet:

```powershell
powershell -ExecutionPolicy Bypass -File embedded\tools\build_rope_image.ps1 -Program Comanche055 -ValidateOnly
```

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

## Trace validation path

The repository now includes a deterministic desktop trace harness for validating the embedded core against a yaAGC/VirtualAGC reference trace.

Generate the yaAGC reference trace:

```powershell
powershell -ExecutionPolicy Bypass -File embedded\tools\run_yaagc_reference_trace.ps1
```

Build and emit the current embedded-core trace:

```powershell
powershell -ExecutionPolicy Bypass -File embedded\tools\run_agc_trace_validation.ps1
```

Compare it with a reference CSV:

```powershell
powershell -ExecutionPolicy Bypass -File embedded\tools\run_agc_trace_validation.ps1 -ReferenceTrace path\to\yaagc_trace.csv
```

The candidate trace is written to `embedded/tests/agc_trace_candidate.csv`. The trace schema is:

```text
step,pc,instr,extended,a,l,q,z,eb,fb,bb,cyc,mct,irq,ch010,ch015
```

`embedded/tests/agc_trace_runner.cpp` deliberately exercises the layers that need historical validation first: Block II opcode/quarter decoding, editing-safe register reads, 9-bit I/O channel operations, branch timing, and the core MCT counter. A trace generated from yaAGC/VirtualAGC should be normalized to the same CSV columns before using `embedded/tools/compare_agc_trace.ps1`.

The first checked-in yaAGC reference trace is `embedded/tests/agc_trace_reference_yaagc.csv`. It is produced by compiling `embedded/tests/yaagc_reference_trace_driver.c` against VirtualAGC's `libyaAGC.a`; the helper searches for a sibling `..\VirtualAGC\yaAGC` checkout by default and accepts `-VirtualAgcRoot` or `-YaAgcDir` when the checkout lives elsewhere.

To run the full local comparison:

```powershell
powershell -ExecutionPolicy Bypass -File embedded\tools\run_yaagc_reference_trace.ps1
powershell -ExecutionPolicy Bypass -File embedded\tools\run_agc_trace_validation.ps1 -ReferenceTrace embedded\tests\agc_trace_reference_yaagc.csv
```

## Real Comanche trace path

The synthetic trace keeps the desktop harness deterministic. The next validation layer runs the real checked-in Comanche rope image through the embedded core and runs a freshly assembled yaYUL `MAIN.agc.bin` through yaAGC:

```powershell
powershell -ExecutionPolicy Bypass -File embedded\tools\run_real_rope_trace_validation.ps1 -Steps 64
```

That command:

- assembles a temporary Comanche055 rope with `yaYUL`
- generates `embedded/tests/agc_trace_real_yaagc.csv` from `yaAGC`
- generates an ignored local `embedded/tests/agc_trace_real_candidate.csv` from the embedded core's `embedded/esp32_agc_core/rope_image.h`
- compares both traces and prints the first mismatching columns without failing the whole run

The default real-rope comparison is intentionally faithful to yaAGC's hardware timing. It enables the embedded machine-timing layer for scaler steals, `TIME1..TIME6` counter pulses, interrupt-entry rows, and the channel-10 DSKY output latch model. The checked-in faithful reference currently validates 8192 real Comanche055 trace rows with all columns matching.

For opcode and CPU-state validation without scaler/downrupt interference, run CPU-only mode:

```powershell
powershell -ExecutionPolicy Bypass -File embedded\tools\run_real_rope_trace_validation.ps1 -Steps 4096 -CpuOnly
```

CPU-only mode freezes yaAGC's asynchronous scaler/downrupt scheduling and writes `embedded/tests/agc_trace_real_cpu_yaagc.csv`. The checked-in CPU-only reference currently validates the embedded core for 4096 real Comanche instructions with all trace columns matching.

You can also run each side manually:

```powershell
powershell -ExecutionPolicy Bypass -File embedded\tools\run_yaagc_reference_trace.ps1 -RomImage path\to\MAIN.agc.bin -Output embedded\tests\agc_trace_real_yaagc.csv -Steps 64
powershell -ExecutionPolicy Bypass -File embedded\tools\run_agc_trace_validation.ps1 -Mode Rope -Steps 64 -CandidateTrace embedded\tests\agc_trace_real_candidate.csv -AllowRunnerFailure -HardwareTiming
powershell -ExecutionPolicy Bypass -File embedded\tools\compare_agc_trace.ps1 -CandidateTrace embedded\tests\agc_trace_real_candidate.csv -ReferenceTrace embedded\tests\agc_trace_real_yaagc.csv -AllowMismatch -MaxMismatches 20
```

At this stage mismatches beyond the checked validation windows are still expected: the embedded core now has executable first-pass opcode, edit-register, scaler, interrupt-entry, downrupt, and channel behavior, while yaAGC remains the historical reference. The useful artifact is the first mismatch location; it tells us exactly which semantic layer to tighten next.

## Current emulator status

The core now has:

- fixed-bank loading through `agc::RopeImage`
- Block II address decoding for unswitched/switched erasable, common fixed through `FBANK`, and fixed-fixed banks 2/3
- physical-format `EBANK`, `FBANK`, and `BBANK` mirroring compatible with yaAGC traces
- explicit Block II instruction-map dispatch for basic opcodes, extracodes, quarter-code groups, and peripheral-code channel operations
- I/O channel storage using the AGC 9-bit channel address, including `L`/`Q` channel aliases, `SUPERBNK` bank selection, channel `030..033` reset defaults, and channel `033` CPU-write latch behavior
- channel `010` DSKY output-row latching compatible with yaAGC's `OutputChannel10[16]`
- Block II interrupt vectors for `T6RUPT`, `T5RUPT`, `T3RUPT`, `T4RUPT`, `KEYRUPT1/2`, `UPRUPT`, `DOWNRUPT`, `RADAR`, and `HANDRUPT`
- trace-visible interrupt-entry rows that save `ZRUPT/BRUPT` before executing the vector
- MCT-driven counter pulses for `TIME1..TIME6`, uplink shifting, keyrupt, and scheduled downrupt/downlink tests
- yaAGC-style machine timing for scaler overflows, pre-instruction steals, in-instruction extra-delay folding, and `TIME1..TIME6` pulses
- read-side and write-side editing behavior for `CYR`, `SR`, `CYL`, and `EDOP`
- ones-complement `INDEX` instruction addition, including the `-0` case
- expanded basic/extracode execution for `DAS`, `LXCH`, `INCR`, `ADS`, `DXCH`, `TS`, `XCH`, `TC Q`, `BZF`, `BZMF`, `MSU`, `QXCH`, `AUG`, `DIM`, `DCA`, `DCS`, `SU`, `MP`, and the channel logic instructions
- a yaAGC reference trace path that passes 4096 CPU-only Comanche055 instructions against the embedded core
- a yaAGC faithful hardware-timing trace path that passes 8192 Comanche055 rows against the embedded core

This is still not enough to claim a complete native AGC. The CPU-only opcode/channel path has a 4096-instruction yaAGC validation window and the faithful hardware-timing path has an 8192-row window, but longer traces still need systematic expansion around downrupt/uplink scheduling, restart-watchdog behavior, radar/hand controller traps, and richer peripheral interleaving.
