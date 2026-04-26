#!/usr/bin/env python3
"""Convert an AGC rope word dump into an Arduino header.

Input can be:
- text containing octal words, one or many per line
- raw binary little-endian 16-bit words
- yaYUL .bin output, which stores big-endian 16-bit records with the
  15-bit AGC word shifted left by one parity bit

The output defines embedded_rope::kWords and embedded_rope::kImage for the
ESP32 sketch. This does not assemble AGC source by itself; run yaYUL first,
then convert the assembled rope dump with this helper.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path

WORDS_PER_BANK = 0o2000
WORD_MASK = 0o77777
MAX_BANKS = 36

# Default Block II yaYUL output swaps raw banks 0/1 with actual banks 2/3
# unless --hardware is used. The embedded core stores words by actual bank.
YAYUL_BANK_ORDER = [2, 3, 0, 1, *range(4, MAX_BANKS)]


def read_text_words(path: Path) -> list[int]:
    words: list[int] = []
    text = path.read_text(encoding="utf-8", errors="ignore")
    for token in re.findall(r"\b[0-7]{1,5}\b", text):
        words.append(int(token, 8) & WORD_MASK)
    return words


def read_binary_words(path: Path) -> list[int]:
    data = path.read_bytes()
    usable = len(data) - (len(data) % 2)
    return [
        int.from_bytes(data[i : i + 2], "little") & WORD_MASK
        for i in range(0, usable, 2)
    ]


def read_yayul_words(path: Path) -> list[int]:
    data = path.read_bytes()
    if len(data) % 2 != 0:
        raise SystemExit(f"{path} has an odd byte count; yaYUL .bin is 16-bit.")

    raw_words = [
        (int.from_bytes(data[i : i + 2], "big") >> 1) & WORD_MASK
        for i in range(0, len(data), 2)
    ]
    if len(raw_words) % WORDS_PER_BANK != 0:
        raise SystemExit(
            f"{path} has {len(raw_words)} words, not an exact number of banks."
        )

    bank_count = len(raw_words) // WORDS_PER_BANK
    if bank_count == MAX_BANKS:
        banks = [[0] * WORDS_PER_BANK for _ in range(MAX_BANKS)]
        for source_bank, actual_bank in enumerate(YAYUL_BANK_ORDER):
            start = source_bank * WORDS_PER_BANK
            banks[actual_bank] = raw_words[start : start + WORDS_PER_BANK]
        return [word for bank in banks for word in bank]

    return raw_words


def format_words(words: list[int]) -> str:
    lines: list[str] = []
    for start in range(0, len(words), 8):
        chunk = words[start : start + 8]
        lines.append("    " + ", ".join(f"0{oct(word)[2:]}" for word in chunk))
    return ",\n".join(lines)


def cpp_string(value: str) -> str:
    return value.replace("\\", "\\\\").replace('"', '\\"')


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--name", default="yaYUL rope")
    parser.add_argument(
        "--format", choices=("auto", "text", "binary", "yayul"), default="auto"
    )
    args = parser.parse_args()

    if args.format == "yayul":
        words = read_yayul_words(args.input)
    elif args.format == "binary":
        words = read_binary_words(args.input)
    elif args.format == "text":
        words = read_text_words(args.input)
    elif args.input.name.lower().endswith(".agc.bin"):
        words = read_yayul_words(args.input)
    else:
        words = read_text_words(args.input)
        if len(words) < WORDS_PER_BANK:
            words = read_binary_words(args.input)

    if not words:
        raise SystemExit("No AGC words found in input.")

    bank_count = (len(words) + WORDS_PER_BANK - 1) // WORDS_PER_BANK
    if bank_count > MAX_BANKS:
        raise SystemExit(
            f"Input has {bank_count} banks; this embedded core supports "
            f"{MAX_BANKS} fixed banks."
        )

    padded_count = bank_count * WORDS_PER_BANK
    words.extend([0] * (padded_count - len(words)))

    output = f"""#ifndef APOLLO11_EMBEDDED_ESP32_ROPE_IMAGE_H
#define APOLLO11_EMBEDDED_ESP32_ROPE_IMAGE_H

#include "agc_core.h"

namespace embedded_rope {{

constexpr uint16_t kWords[] = {{
{format_words(words)}
}};

constexpr agc::RopeImage kImage = {{
    kWords,
    {bank_count},
    "{cpp_string(args.name)}",
}};

}}  // namespace embedded_rope

#endif
"""

    args.output.write_text(output, encoding="utf-8")
    print(f"Wrote {bank_count} banks / {len(words)} words to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
