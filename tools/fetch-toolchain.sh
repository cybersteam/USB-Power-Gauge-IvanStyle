#!/bin/sh
# Download a user-space avr-gcc. Useful when the system package is not installed.
set -eu
if [ "$(uname -m)" != "x86_64" ]; then
    echo "This URL is the x86_64 toolchain. Install gcc-avr from your distro instead." >&2
    exit 1
fi
dest="${PREFIX:-$HOME/.local/avr-sdk}"
url="https://downloads.arduino.cc/tools/avr-gcc-7.3.0-atmel3.6.1-arduino7-x86_64-pc-linux-gnu.tar.bz2"
mkdir -p "$dest"
tmp="$(mktemp)"
curl -fL --retry 3 -o "$tmp" "$url"
tar -xjf "$tmp" -C "$dest"
rm -f "$tmp"
echo "$dest/avr/bin/avr-gcc"
"$dest/avr/bin/avr-gcc" --version | head -1
