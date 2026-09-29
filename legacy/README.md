# Original firmware

These files are the Adafruit USB Power Gauge sketch and its notes, kept so the rewrite can be compared with what the board shipped with. They are not the image `make` builds.

| File | What it is |
|---|---|
| `adafruit_usbpowergauge.ino` | Arduino sketch. Trinket 16 MHz target, SoftwareSerial on pin 1, EEPROM bandgap word |
| `analog.cpp` | VCC and current reads used by that sketch |
| `charlie.cpp` | The lamp map the new firmware copies |
| `partslist.txt` | The handwritten bill of materials, including the ambiguous shunt line |
| `ORIGINAL_README.md` | The product README this repository used before firmware 2.0 |

The sketch header says the code is BSD and then points at `license.txt`. That file, one level up, is the Creative Commons Attribution-ShareAlike 3.0 legal code. The schematic title block names CC BY-SA 2.5. None of those notices were edited. [NOTICE](../NOTICE) quotes the header and the product attribution in full.

Build and program the firmware in the repository root. The sketch's "burn bootloader at 16 MHz" instruction does not apply to the production image: the ATtiny85V-10S on this PCB is built for the internal 8 MHz oscillator. See [docs/hardware.md](../docs/hardware.md).
