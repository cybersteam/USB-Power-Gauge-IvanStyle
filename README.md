# USB Power Gauge

Production firmware for the [Adafruit USB Power Gauge](https://www.adafruit.com/product/1549) (product 1549). The board is unchanged: an ATtiny85V-10S, an INA169 on a 0.1 Ω shunt, six charlieplexed LEDs, and a TTL transmit pin. USB D+ and D− pass straight through. Firmware 2.0 replaces the original Arduino sketch with an image that can be built, tested, calibrated, and programmed without a host-side framework.

<a href="https://www.adafruit.com/product/1549"><img src="assets/image.jpg" width="500" alt="Adafruit USB Power Gauge, product photo"></a>

The gauge sits between a USB host and a device and shows bus power on the lamp bar. One blue segment is about one watt. The green lamp tracks whether VBUS is still above 4.5 V. A 9600 baud TTL sentence on PB1 carries millivolts, milliamps, milliwatts, accumulated energy, and status flags.

| | |
|---|---|
| MCU | ATtiny85V-10S, 8 KB flash, 512 B SRAM, 512 B EEPROM |
| Clock | Internal 8 MHz RC. 16 MHz is a compile option for boards already fused that way |
| Sense | INA169, Rs = 0.1 Ω, RL = 10 kΩ, about 1 V/A. Analog full scale is about 1.1 A |
| Lamps | Green OK, then 1 W through 5 W. PWM brightness 0..32 |
| Telemetry | 9600 8N1, TTL 0–5 V, TX only. Sentence `$UPG`, CRC-8/SMBUS, 4 Hz |
| Image | 6488 B flash (79.2%), 170 B SRAM (33.2%) at 8 MHz |

## What the rewrite is for

The original sketch reads the shunt, lights the bar, and prints a line. This image keeps that product behavior and adds the pieces a small production firmware is judged on:

- Measurement, display, calibration, and framing are plain C with no AVR headers. The same objects link into the firmware and into a host test suite. Golden values (5006 mV from a known bandgap sum, 1253 mW from 5012 mV × 250 mA, the exact telemetry sentence below) fail the build if the arithmetic moves.
- Every multiply that can exceed 65535 is widened before the multiply. On this MCU, `int` is 16 bits. The host compiler will not catch a missing cast, so the tests and a comment in `firmware/portable/measure.c` exist to keep that rule visible.
- Factory calibration is a 16-byte EEPROM record with a CRC. The chip has no receive pin, so the fixture writes the record with `avrdude`. A legacy bandgap word from the original sketch still loads. Blank EEPROM is an explicit uncalibrated state, not a silent 1.100 V.
- The lamp ISR and the UART ISR share one core. The lamp code tri-states its pins before driving the next pair, never touches PB1 or PB3, and never holds `cli()` across a byte. ADC noise-reduction sleep is avoided because it stops the UART bit clock.
- Reset cause, calibration source, bandgap, offset, gain, and the live OSCCAL are printed once at boot. A 1 s watchdog is armed after the peripherals and before interrupts. Fuses (8 MHz, EEPROM preserved, brown-out at 2.7 V, reset still an ISP pin) are a documented, confirmed step. A plain `make` does not touch a chip.
- Flash and SRAM are gated. The budget is 8192 B of flash and 448 B of SRAM, leaving headroom in the 512 B device. The 8 MHz image is 6488 / 170. The 16 MHz image is 6552 / 170.

## Build

Host tests need GCC and Python 3. The firmware needs avr-gcc, avr-libc, and avr-binutils. Debian and Ubuntu package those as `gcc-avr`, `avr-libc`, and `binutils-avr`. If the system compiler is absent, `tools/fetch-toolchain.sh` installs the Arduino x86_64 toolchain under `~/.local/avr-sdk`, which the Makefile finds on its own.

```sh
make test                  # host C suite, then the Python tool checks
make                       # 8 MHz hex + size gate
make F_CPU=16000000 all    # boards whose fuses are already 16 MHz
```

`make clean` removes `build/`. Outputs land in `build/fw-8000000/` or `build/fw-16000000/`: `usb-power-gauge.elf` and `.hex`.

## Program a board

Read [docs/hardware.md](docs/hardware.md) before the first ISP session. The production fuse word enables EEPROM preservation, so the order is fuses, then flash, then the calibration image. Each of these refuses to run until `CONFIRM=yes` is present.

```sh
make fuses  PROGRAMMER=usbtiny CONFIRM=yes
make flash  PROGRAMMER=usbtiny CONFIRM=yes
make eeprom-write IMAGE=cal.bin PROGRAMMER=usbtiny CONFIRM=yes
```

`F_CPU=16000000` compiles. It does not change the fuse Makefile. Burning a 16 MHz clock into an ATtiny85V-10S takes the part outside its 10 MHz ordering code, and that image prints a warning over the UART.

## Read a log

PB1 is idle-high TTL at 9600 8N1. It is the ISP MISO pin as well. Use a 3.3 V or 5 V serial adapter, not an RS-232 port. A golden sentence, locked by the host test, is:

```
$UPG,1,5012,250,1253,10000,03,400,4900*9E
```

```sh
python3 tools/gauge_log.py --file capture.txt --csv readings.csv
python3 tools/gauge_log.py --port /dev/ttyUSB0 --once
```

`--port` imports pyserial when it is installed. Lines that fail the CRC are skipped. The field layout, flag bits, and CRC are in [docs/protocol.md](docs/protocol.md).

## Accuracy

Treat the numbers as a guide for charge rate and port sag. The analog chain is a 1% shunt, a 1% load resistor, and an INA169 whose datasheet total output error is ±2% max at 100 mV sense, with an input offset up to ±1 mV (about ±10 mA on this shunt). The ATtiny bandgap spans roughly 1.0 V to 1.2 V until it is calibrated. Factory calibration removes the board's own offset and gain. It does not remove resistor tempco, the INA169's remaining error, or RC-oscillator drift.

The parts list writes the shunt as `0.1W 0.1/1%`, which does not cleanly say resistance, tolerance, and wattage. Dissipation in 0.1 Ω is 0.1 W at 1 A and 0.4 W at 2 A. Confirm the resistor fitted on the board before the load exceeds 1 A. The converter saturates near 1.1 A either way, and the bar then shows overload.

## Documentation

- [docs/hardware.md](docs/hardware.md) — pins, sensing, fuses, programming order
- [docs/architecture.md](docs/architecture.md) — clocks, ADC, ISRs, boot, memory
- [docs/protocol.md](docs/protocol.md) — `$UPG` sentence, CRC, flags
- [docs/calibration.md](docs/calibration.md) — EEPROM record and the fixture tool
- [legacy/README.md](legacy/README.md) — the original sketch, unmodified

## License

Adafruit designed the board and wrote the original sketch (Limor Fried / Ladyada). That design, the photos, the schematic (`usbpowergauge sch.png`), and `legacy/` stay under the notices in [NOTICE](NOTICE) and [license.txt](license.txt).

The firmware, tools, and docs added here are BSD 3-Clause, [LICENSE-FIRMWARE](LICENSE-FIRMWARE). Copyright 2026, USB Power Gauge contributors. IvanStyle is the name of this repository variant.

Adafruit invests time and resources providing this open source design, please support Adafruit and open-source hardware by purchasing products from Adafruit.

Designed by Adafruit Industries.
Creative Commons Attribution, Share-Alike license, check license.txt for more information.
All text above must be included in any redistribution.
