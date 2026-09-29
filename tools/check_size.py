#!/usr/bin/env python3
"""Fail the build when the ATtiny85 image does not fit, with stack headroom."""

import sys

FLASH_LIMIT = 8192
RAM_LIMIT = 448
RAM_DEVICE = 512


def field(lines, name):
    for line in lines:
        parts = line.split()
        if len(parts) >= 2 and parts[0] == name:
            return int(parts[1])
    return 0


def main():
    text = sys.stdin.read().splitlines()
    flash = field(text, ".text") + field(text, ".data")
    ram = field(text, ".data") + field(text, ".bss") + field(text, ".noinit")
    print(f"flash {flash} / {FLASH_LIMIT} bytes ({100.0 * flash / FLASH_LIMIT:.1f}%)")
    print(f"sram  {ram} / {RAM_DEVICE} bytes ({100.0 * ram / RAM_DEVICE:.1f}%), limit {RAM_LIMIT}")
    if flash > FLASH_LIMIT or ram > RAM_LIMIT:
        print("memory budget exceeded", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
