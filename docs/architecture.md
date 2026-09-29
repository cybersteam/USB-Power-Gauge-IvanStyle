# Architecture

The firmware is split so the math can be compiled by a host GCC and by avr-gcc from the same sources.

```
firmware/config.h            thresholds, periods, version
firmware/portable/           crc, format, measure, gamma, display, cal, protocol
firmware/avr/                pins, clocks, ADC, charlieplex, UART, main
firmware/tests/              host suite, linked against portable/
tools/                       size gate, calibration image, log-to-CSV
```

`portable/` includes no AVR header. `rom.h` maps to `PROGMEM` on the device and to plain `const` on the host, so a string or the gamma table cannot land in `.data` by accident. The image uses no `printf`, no `float`, no `malloc`, and no `__DATE__`.

## Boot

`early_init` is `naked` and placed in `.init3`. That section is concatenated into the startup code, not called. The fragment saves MCUSR into `g_reset_flags` (`.noinit`, so the later BSS clear leaves it alone), writes 0 to MCUSR, and disables the watchdog the way the datasheet requires (the timed sequence, with the zero register left intact). It has no `ret`. A return would skip the rest of the C runtime. The 8 MHz image places it at address `0x11a`, 24 bytes, falling through.

`main` then:

1. Powers down the USI (`PRR.PRUSI`). Nothing on this board uses it.
2. Reads the 16-byte EEPROM record and, for a valid factory record with a non-zero OSCCAL, writes OSCCAL before any timer starts.
3. Initializes lamps, ADC, UART, and the millisecond tick.
4. Arms the watchdog for 1 s.
5. Enables interrupts and prints the banner.
6. Walks the six lamps, 80 ms each.
7. Measures at 10 Hz, reports at 4 Hz, and renders on each measurement and on each blink edge.

The banner is a few short PROGMEM lines: version and `f_cpu`, a clock warning when the image was built for 16 MHz, `rst=` (`WDT`, `BOR`, `EXT`, `POR`, joined with `+`, or `none`), `cal=` (`factory`, `legacy`, `default`), bandgap, offset, gain, the live `osccal` byte, and the sentence legend. The watchdog is already running, and the blocking writer pets it.

## Clocks

Both supported clocks divide to the same two rates. `firmware/avr/clock.h` static-asserts them.

| | 8 MHz (production) | 16 MHz (already-burned fuses) |
|---|---|---|
| Timer0 | CTC, CK/8, OCR0A = 49 | CTC, CK/8, OCR0A = 99 |
| Lamp tick | 20 kHz | 20 kHz |
| Milliseconds | 20 ticks | 20 ticks |
| ADC clock | CK/64 = 125 kHz | CK/128 = 125 kHz |
| Timer1 UART | CK/8, top 103, period 104 | CK/64, top 25, period 26 |
| Baud | 9615 (+0.16%) | 9615 (+0.16%) |

Six lamps times 32 PWM steps at 20 kHz is about 104 Hz, above flicker. Brightness `n` in 0..32 means the LED is on while the PWM counter is below `n`. 32 is solid.

The UART ISR is Timer1 compare-match (vector 3). The lamp ISR is Timer0 compare-match (vector 10). Neither ISR calls the other, and neither wraps a long `cli()`.

## ADC

Two bursts per measurement, CPU awake.

VBUS uses ADMUX `0x0C` (bandgap input, VCC reference). Current uses ADC3 with the bandgap reference (`REFS1`). After a channel change the code waits about 300 µs, discards two conversions, then sums 16. `DIDR0.ADC3D` kills the digital input buffer on PB3.

ADC noise-reduction sleep would stop `clk_I/O` and stretch whatever UART bit was in flight, so conversions busy-wait. A stuck ADSC (a few tens of thousands of spins) returns a fault instead of wedging the watchdog. On fault the meter keeps the last good reading, sets `FLAG_ADC_FAULT`, and does not integrate energy. With no previous reading it reports zeros plus the fault flag.

Saturation is a raw sample at or above 1022 inside the current burst. The current filter snaps to that sample instead of slewing toward the rail.

## Measurement

All rounding is half-up. The identities the host suite locks:

```
Vbus_mV = round(vbg_mV × 1024 × n / bandgap_sum)
pin_mV  = round(adc_sum × vbg_mV / (1024 × n))
I_mA    = round(max(pin_mV − offset_mA, 0) × gain_q12 / 4096)
P_mW    = round(Vbus_mV × I_mA / 1000)
```

Golden checks: bandgap sum 3600 over 16 samples at 1100 mV → 5006 mV. Pin sum 1488 over 16 at 1100 mV → 100 mV, and 2976 → 200 mV. 5012 mV × 250 mA → 1253 mW.

The IIR moves one quarter of the remaining error per sample, and at least one millivolt or one milliamp when the error is non-zero, so a threshold cannot sit forever 1–3 counts away. Energy accumulates `P × dt` in a milliwatt-millisecond residue:

```
residue += min(P, 20000) × dt_ms
µWh     += residue / 3600
residue %= 3600
```

`dt` of 0 (the first sample) and `dt` above 2000 ms (a stall, a debug halt, a long ADC fault) add nothing. 1000 mW for 1000 ms yields 277 µWh and residue 2800; the next 1000 ms yields 555 µWh and residue 2000. Peak current and minimum VBUS follow the filtered reading and clear only on power-up.

Flags, with hysteresis so a noisy rail does not chatter:

| Bit | Name | Set | Clear |
|---|---|---|---|
| 0 | `VBUS_OK` | ≥ 4500 mV | < 4400 mV |
| 1 | `VBUS_IN_SPEC` | inside 4750–5250 | < 4700 or > 5300 |
| 2 | `I_SATURATED` | raw max ≥ 1022 | next unsaturated sample |
| 3 | `UNCALIBRATED` | EEPROM fell through to defaults | factory or legacy record |
| 4 | `OVERLOAD` | ≥ 5000 mW, or saturated | < 4800 mW and not saturated |
| 5 | `VBUS_LOW` | < 4000 mV | ≥ 4100 mV |
| 6 | `ADC_FAULT` | this sample failed | next good sample |

`VBUS_IN_SPEC` is telemetry only. It does not drive a lamp. While bit 6 is set, the other bits still describe the last good sample, and a consumer should drop the sample. See [protocol.md](protocol.md).

## Display

`gauge_render` writes six brightness bytes.

- ADC fault: bar off, green blinks on a 200 ms phase.
- VBUS ok and calibrated: green solid. VBUS ok and uncalibrated: green alternates full and dim (8) on a 500 ms phase. Below the ok threshold the green lamp is off.
- Otherwise each watt segment is full at or above its watt, and gamma-curved across the watt below it. The curve is `round((i / 31)² × 32)` for `i` in 0..31, endpoints 0 and 32. The bottom of the curve is intentionally dark.
- Overload: segments 1–4 solid, segment 5 alternates full and dim (8) on the 200 ms phase.
- Peak hold: the highest watt segment in the last 2 s is held at brightness 4 if the live bar is dimmer there.

At boot, before the first measurement, the six LEDs walk once at full brightness for 80 ms each. That is the lamp test, not a measurement.

## UART

TX only. Idle high, start low, 8 data bits LSB first, stop high. One producer (main) and one consumer (the Timer1 ISR) share a 64-byte ring. One slot stays empty, so 63 bytes are usable and a 60-byte frame fits. Main writes the bytes, then publishes the tail. The ISR never sees a torn chunk, and the copy does not need interrupts disabled. The blocking writer used by the banner pets the watchdog on every chunk, including the path that succeeds, and chunks 32 bytes.

## Memory

Measured with avr-gcc 7.3.0 (`-Os`, function sections, gc-sections, relax):

| Image | Flash (`.text` + `.data`) | SRAM (`.data` + `.bss` + `.noinit`) |
|---|---|---|
| 8 MHz | 6488 / 8192 (79.2%) | 170 / 512 (33.2%) |
| 16 MHz | 6552 / 8192 (80.0%) | 170 / 512 (33.2%) |

`tools/check_size.py` fails the build above 8192 B flash or 448 B SRAM. The 64 B gap under the device SRAM is stack. The transmit ring used to be 128 B; 64 B is what brought SRAM from 234 B down to 170 B while still holding a frame.

The largest functions in the 8 MHz map are `meter_apply_units` (1316 B), `main` (1220 B), `gauge_render` (474 B), and `protocol_format` (466 B). The UART vector is 158 B. The lamp tick is 126 B in the Timer0 vector plus 124 B in `leds_on_tick`. Those ISRs are short on purpose. They have not been cycle-counted against a bit time, which is the remaining timing risk if someone adds work to either of them.

## What stays out

There is no command parser, because there is no RX. There is no bootloader: EFUSE keeps `SELFPRGEN` off and programming is ISP. There is no runtime EEPROM write. A future change that adds either one has to re-budget the 8 KB and re-check that the lamp ISR still finishes well inside a UART bit.
