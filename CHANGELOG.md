# Changelog

## 2.0.0

Production firmware for the existing Adafruit USB Power Gauge PCB and its
ATtiny85V-10S. The original sketch is archived under `legacy/` and is no
longer the image this repository builds.

- Integer measurement core with round-half-up scaling, a minimum-step IIR,
  hysteresis on every lamp threshold, and energy integration that ignores
  gaps longer than 2 s. Host tests lock the golden values.
- Factory EEPROM record: magic, version, OSCCAL override, bandgap, current
  offset, Q12 gain, CRC-8. A failed record falls back to the original
  big-endian bandgap word. Blank EEPROM uses the 1.100 V assumption and
  pulses the green lamp.
- Telemetry sentence `$UPG` at 4 Hz, CRC-8/SMBUS, 9600 8N1 TTL on PB1.
  Reports carry flags, peak current, and minimum VBUS since boot.
- Charlieplex PWM on Timer0 at 20 kHz. The lamp ISR leaves PB1 and PB3
  alone and tri-states the lamp pins before the next pair.
- Timer1 bit-bang UART. Divider error is +0.16% (9615 baud) at 8 MHz and
  at 16 MHz. The transmit ring holds one 60-byte frame.
- Watchdog at 1 s, brown-out fuse at 2.7 V, reset cause printed at boot.
  The early init that captures MCUSR is a naked `.init3` fragment and
  falls through into the C runtime.
- Production clock is the internal 8 MHz RC, inside the ATtiny85V-10S
  10 MHz rating. `F_CPU=16000000` remains a compile option for boards
  whose fuses were already burned that way, and that image says so.
- `make test`, a flash-and-SRAM size gate, and a guarded `avrdude` path
  (`CONFIRM=yes`). Fuses are documented and are never written by a plain
  `make`.
