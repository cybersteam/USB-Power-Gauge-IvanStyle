# Calibration

The gauge has no receive pin, so calibration is an EEPROM image written over ISP. The firmware only reads it. Runtime commands cannot change it, and a chip erase preserves it only after HFUSE has EESAVE programmed (`0xD5`). Program fuses, then flash, then EEPROM. See [hardware.md](hardware.md).

## Record

16 bytes, little-endian, at EEPROM address 0. `firmware/portable/cal.c` and `tools/calibrate.py` implement the same layout.

| Offset | Field | |
|---|---|---|
| 0 | `'U'` | |
| 1 | `'P'` | |
| 2 | version | `1` |
| 3 | OSCCAL | `0` keeps the value loaded at reset |
| 4 | `vbg_mV` | uint16 |
| 6 | `offset_mA` | int16, subtracted from the pin reading |
| 8 | `gain_q12` | uint16, 4096 means 1.000 |
| 10 | CRC-8/SMBUS of bytes 0..9 | |
| 11..15 | reserved | written `0` |

Accepted range, rejected otherwise: bandgap 800–1300 mV, offset −500–500 mA, gain 2048–8192 (0.5× to 2×).

Decode order:

1. Magic `UP`, version 1, CRC match, fields inside range → factory record. A non-zero OSCCAL is written before the timers start. `FLAG_UNCALIBRATED` stays clear and the green lamp is solid when VBUS is ok.
2. Else a big-endian bandgap at bytes 0..1 inside 800–1300, which is the original sketch's record → legacy. Offset 0, gain 1.000, OSCCAL left alone. The green lamp treats this as voltage-calibrated. Current is not.
3. Else defaults: 1100 mV, offset 0, gain 4096. The green lamp pulses. Blank EEPROM (`0xFF`) lands here. Magic `UP` is 0x5550, which is outside the legacy window, so a broken factory CRC cannot be mistaken for a legacy bandgap.

## Solving a record

Voltage, with the assumed bandgap that produced the reading (1100 mV on a default image):

```
vbg = round(assumed_mV × meter_mV / reported_mV)
```

Golden: assumed 1100, reported 4980, meter 5012 → 1107.

Current gain, after the offset has been removed, from a known load:

```
gain_q12 = round(true_mA × 4096 / reported_mA)
```

Golden: true 250 mA, reported 230 mA → 4452.

The offset is the no-load current reported at unity gain. Sign is "what the gauge said". The firmware computes `max(pin_mV − offset, 0)` and then applies gain. Do this with the downstream connector empty. The number then includes the gauge's own supply current and the INA169 offset, which is what a user means by zero.

## Fixture tool

```sh
python3 tools/calibrate.py solve \
    --reported-mv 4980 --meter-mv 5012 \
    --zero-ma -4 \
    --reported-load-ma 230 --true-ma 250 \
    --osccal 0 \
    -o cal.bin

python3 tools/calibrate.py decode cal.bin
make eeprom-write IMAGE=cal.bin PROGRAMMER=usbtiny CONFIRM=yes
```

`encode` writes a record from raw fields when the solver is not what you want:

```sh
python3 tools/calibrate.py encode --vbg 1102 --offset -4 --gain 4100 --osccal 0x91 -o cal.bin
```

`--osccal` accepts `0x91` or `145`. Leave it at 0 unless a frequency measurement says the factory trim is off. The banner prints the live byte either way, so the log shows which trim ran.

A sample image used by the tests (bandgap 1102, offset −4, gain 4100, OSCCAL `0x91`) is regenerated at `build/host/sample.eep` by `make test`. `cal.bin` is gitignored on purpose: it is a per-board artifact.

## What a record does not fix

The stored gain and offset are one operating point. They do not track the 1% resistors over temperature, the INA169's ±2% max output error, or bandgap drift as VBUS moves. Recalibrate when the shunt or the load resistor is replaced. Confirm the shunt's wattage before using the top of the analog range as a working current; the math will happily scale a resistor that is already too hot.
