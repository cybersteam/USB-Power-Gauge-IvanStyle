# Hardware

The PCB is the Adafruit USB Power Gauge, product 1549. This firmware does not move a trace. Photographs are `1549-02.jpg`, `1549-03.jpg`, and `assets/image.jpg`. The schematic is `usbpowergauge sch.png`. The original bill of materials is `legacy/partslist.txt`.

## Microcontroller

ATTINY85V-10S. 8 KB flash, 512 B SRAM, 512 B EEPROM, internal RC oscillator, no crystal on this board. The V-10 ordering code is 10 MHz from 1.8 V to 5.5 V. The production clock is 8 MHz, which stays inside that rating when VBUS sags down to the 2.7 V brown-out.

| Pin | Net | Firmware use |
|---|---|---|
| PB0 | charlieplex | lamp anode/cathode |
| PB1 | TX, ISP MISO | TTL serial out, idle high. The lamp ISR never drives this pin |
| PB2 | charlieplex | lamp anode/cathode |
| PB3 | INA169 OUT | ADC3, current. Digital input buffer disabled |
| PB4 | charlieplex | lamp anode/cathode |
| PB5 | RESET, ISP | left as reset so ISP still works |

USB D+ and D− are not connected to the microcontroller. The gauge is in series with VBUS and ground only, so the USB data rate is the cable's, not the firmware's.

PB1 is shared with ISP. Disconnect the serial adapter before programming, or the adapter's TX/RX pair will fight the programmer on MISO.

## Current and voltage

The INA169 is a high-side shunt monitor. Transfer is:

```
Vout = Iload × Rs × RL × gm
```

`gm` is 1000 µA/V. With Rs = 0.1 Ω and RL = 10 kΩ (R2 on the parts list), Vout is 1 V per amp. The ADC measures that pin against the internal bandgap, so full scale is one bandgap voltage, about 1.1 V, about 1.1 A. Counts at or above 1022 raise `FLAG_I_SATURATED` and the overload lamp pattern.

VBUS is the chip's own supply. The firmware measures the bandgap with VCC as the ADC reference (ADMUX `0x0C`) and computes VCC from the calibrated bandgap voltage. One stored bandgap constant scales both voltage and current.

From the INA169 datasheet, at 25 °C class conditions: transconductance 990–1010 µA/V over 10–150 mV sense, total output error ±0.5% typical and ±2% max at 100 mV sense, offset ±0.2 mV typical and ±1 mV max referred to the input. On a 0.1 Ω shunt, 1 mV is 10 mA. R2 is listed as 1%. The shunt is listed as `0.1W 0.1/1%`, which is ambiguous. I²R in 0.1 Ω is 0.1 W at 1 A and 0.4 W at 2 A. Read the resistor that is actually fitted before running a load past 1 A. The original product page said a 2 A load was acceptable because the lamps max out. That describes the display, not the shunt's thermal rating.

The ATtiny bandgap is typically 1.0–1.2 V. Uncalibrated readings use 1.100 V and light the green lamp as uncalibrated. A fixture measurement replaces that constant. See [calibration.md](calibration.md).

Zeroing the current offset with no downstream load folds in the gauge's own draw (the MCU, the INA169, and the lamps) and the amplifier offset. That is the right zero for "what is the device drawing", and the wrong zero for "what is the shunt current including the gauge".

## Lamps

Six LEDs on three pins. The drive pairs match the original sketch so the silkscreen (OK, 1 W … 5 W) stays honest:

| Index | Silkscreen | DDR high | PORT high |
|---|---|---|---|
| 0 | green OK | PB2, PB4 | PB4 |
| 1 | 1 W | PB2, PB4 | PB2 |
| 2 | 2 W | PB0, PB4 | PB0 |
| 3 | 3 W | PB0, PB4 | PB4 |
| 4 | 4 W | PB0, PB2 | PB0 |
| 5 | 5 W | PB0, PB2 | PB2 |

The ISR returns all three pins to Hi-Z before it drives the next pair. Driving a new pair on top of the old one shorts two LEDs through the pin drivers.

## Fuses

Documented in `firmware/fuses.mk`. Nothing in `make` or `make all` writes them.

| Fuse | Value | Meaning |
|---|---|---|
| LFUSE | `0xE2` | Internal 8 MHz, CKDIV8 off, SUT slowly rising (64 ms), CKOUT off |
| HFUSE | `0xD5` | SPIEN, EESAVE, BODLEVEL 2.7 V, reset enabled, watchdog not forced on |
| EFUSE | `0xFF` | SELFPRGEN off. No bootloader writes flash |

A virgin ATtiny85 usually has CKDIV8 programmed and runs at 1 MHz. Baud, the lamp tick, and the ADC clock are all wrong until LFUSE is `0xE2`. Program fuses before trusting a sentence.

EESAVE is what keeps a calibration across the chip erase that `avrdude` performs on a later flash. Set it before the calibration image is stored.

Brown-out at 2.7 V resets the part rather than letting it execute at a VCC where the 8 MHz RC and the ADC are both outside comfortable range. The V-part is still specified at that voltage.

## Programming order

USBtiny is the default programmer. Any avrdude `-c` name works as `PROGRAMMER=`.

```sh
make fuses PROGRAMMER=usbtiny CONFIRM=yes
make flash PROGRAMMER=usbtiny CONFIRM=yes
make eeprom-write IMAGE=cal.bin PROGRAMMER=usbtiny CONFIRM=yes
```

Each target exits before avrdude unless `CONFIRM=yes` is on the command line. Flash uses `build/fw-$(F_CPU)/usb-power-gauge.hex`. The default `F_CPU` is 8000000.

`make F_CPU=16000000 flash` is only for a board whose fuses were already burned for 16 MHz (the old Trinket "burn bootloader" flow). Do not copy a 16 MHz fuse set onto this V-10 part. The datasheet does not guarantee operation above the ordering-code frequency.

## Serial

9600 8N1, idle high, 0 V to VBUS. PB1. There is no RX. Commands cannot be sent to a running gauge; statistics clear on power cycle, and calibration changes by rewriting EEPROM.

The bit clock is 9615 baud (+0.16% from the integer divider) before RC error. Microchip's electrical table lists factory OSCCAL as ±10% at 3 V, 25 °C. The clock chapter of the same family datasheet says the calibration byte loaded at reset is within ±1% at that point. Those sentences disagree, so a production log should record the `osccal=` field from the banner, and a fixture that cares about baud over temperature should measure the bit and store an override. User calibration in the electrical table is ±1% at a fixed voltage and temperature.
