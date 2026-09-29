# Telemetry protocol

PB1 transmits 9600 8N1, idle high, TTL levels between 0 V and VBUS. The electrical interface is not RS-232. There is no receive pin. Sentences start after the boot banner and the lamp test, four times a second. Measurements run at 10 Hz, so a sentence is every second or third sample, carrying the filtered values.

## Sentence

```
$UPG,<proto>,<mV>,<mA>,<mW>,<uWh>,<flags>,<iPeak>,<vMin>*<CRC>\r\n
```

| Field | Type | Meaning |
|---|---|---|
| `proto` | decimal | `1` for this document |
| `mV` | uint16 | filtered VBUS |
| `mA` | uint16 | filtered load current |
| `mW` | uint32 | `round(mV × mA / 1000)` |
| `uWh` | uint32 | energy since boot |
| `flags` | 2 hex digits, uppercase | status bits below |
| `iPeak` | uint16 | highest filtered current since boot |
| `vMin` | uint16 | lowest filtered VBUS since boot |
| `CRC` | 2 hex digits, uppercase | CRC-8/SMBUS |

The golden sentence locked by `firmware/tests/test_protocol.c` and `tools/test_tools.py`:

```
$UPG,1,5012,250,1253,10000,03,400,4900*9E\r\n
```

The longest legal sentence is 60 bytes, also locked by the test:

```
$UPG,1,65535,65535,4294967295,4294967295,FF,65535,65535*FF\r\n
```

The formatter refuses to write a sentence if the buffer is shorter than the result plus a NUL. The firmware passes a 64-byte buffer. The transmit ring holds 63 payload bytes, and `uart_write` queues a sentence only when the free space can hold all of it. A sentence that does not fit is dropped whole, so the measurement loop never blocks on the UART. The banner uses the blocking writer, and it finishes before the loop starts. At 9600 baud a 60-byte sentence takes about 63 ms, and sentences are 250 ms apart, so a drop means the bit clock stalled, not that the link is merely slow.

## CRC

CRC-8/SMBUS. Polynomial `0x07`, initial value 0, no reflection, xor-out 0. The covered bytes are everything after `$` and before `*`.

```
crc8("123456789") = 0xF4
crc8("A")         = 0xC0
```

`protocol_crc_ok` and `tools/gauge_log.py` both require a matching CRC, the tag `UPG`, and protocol `1`. A line that fails is not a sample.

## Flags

Bits are numbered from the least significant bit of the two hex digits.

| Mask | Name | Meaning |
|---|---|---|
| `0x01` | `vbus_ok` | VBUS is at or above the 4.5 V lamp threshold (4.40 V to turn back off) |
| `0x02` | `in_spec` | VBUS is inside the USB window, with hysteresis. Log only; no lamp |
| `0x04` | `i_saturated` | A current sample in this burst was at or above 1022 counts |
| `0x08` | `uncalibrated` | EEPROM did not contain a factory or legacy record |
| `0x10` | `overload` | Power is at or above 5 W, or the current ADC is saturated |
| `0x20` | `vbus_low` | VBUS is under 4.0 V (4.10 V to turn back off) |
| `0x40` | `adc_fault` | This conversion failed. Drop the sentence |

When `adc_fault` is set, the numeric fields are the last good sample, or zeros if the ADC has never succeeded. Other flags describe that held sample. Energy does not advance on a fault. `in_spec` uses entry 4.75–5.25 V and exit outside 4.70–5.30 V.

`03` in the golden sentence is `vbus_ok` plus `in_spec`.

## Banner

Before the first `$UPG` line the firmware prints comments. A logger should ignore any line whose first non-space character is `#`.

```
# usb-power-gauge 2.0.0 hw=attiny85v-10 f_cpu=8000000
# rst=POR cal=factory vbg=1102 off=-4 g=4100
# osccal=0x91
# $UPG,1,mV,mA,mW,uWh,fl,iPk,vMin*crc
```

`rst` is one or more of `WDT`, `BOR`, `EXT`, `POR`, joined by `+`, or `none` when MCUSR was clear. A 16 MHz build inserts a warning line after the identity line. `cal` is `factory`, `legacy`, or `default`. `osccal` is the register value after the optional EEPROM override, printed as two uppercase hex digits.

## Host parser

```sh
python3 tools/gauge_log.py --file capture.txt --csv readings.csv
python3 tools/gauge_log.py --port /dev/ttyUSB0 --baud 9600 --once
```

CSV columns are `vbus_mV,i_mA,p_mW,energy_uWh,flags,i_peak_mA,v_min_mV`, then one column per flag. `--port` is the only path that imports pyserial. Comment lines are ignored. Other non-frames count as skipped and the count is printed to stderr.
