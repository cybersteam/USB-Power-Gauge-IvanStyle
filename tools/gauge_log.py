#!/usr/bin/env python3
"""Read USB Power Gauge telemetry and write CSV.

Accepts a capture file, stdin, or a serial port. pyserial is imported only
when --port is used. Lines that are not valid $UPG frames are skipped.
"""

import argparse
import sys

FLAG_NAMES = (
    (0x01, "vbus_ok"),
    (0x02, "in_spec"),
    (0x04, "i_saturated"),
    (0x08, "uncalibrated"),
    (0x10, "overload"),
    (0x20, "vbus_low"),
    (0x40, "adc_fault"),
)


def crc8(data: bytes) -> int:
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ 0x07) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc


def parse_frame(line: str):
    text = line.strip()
    if not text.startswith("$") or "*" not in text:
        return None
    body, _, tail = text[1:].partition("*")
    if len(tail) < 2:
        return None
    try:
        got = int(tail[:2], 16)
    except ValueError:
        return None
    try:
        raw = body.encode("ascii")
    except UnicodeEncodeError:
        return None
    if crc8(raw) != got:
        return None
    fields = body.split(",")
    if len(fields) != 9 or fields[0] != "UPG" or fields[1] != "1":
        return None
    try:
        flags = int(fields[6], 16)
        values = {
            "vbus_mV": int(fields[2]),
            "i_mA": int(fields[3]),
            "p_mW": int(fields[4]),
            "energy_uWh": int(fields[5]),
            "i_peak_mA": int(fields[7]),
            "v_min_mV": int(fields[8]),
        }
    except ValueError:
        return None
    values["flags"] = flags
    for bit, name in FLAG_NAMES:
        values[name] = 1 if flags & bit else 0
    return values


def csv_header():
    base = ["vbus_mV", "i_mA", "p_mW", "energy_uWh", "flags", "i_peak_mA", "v_min_mV"]
    return base + [name for _, name in FLAG_NAMES]


def to_csv_row(sample):
    return ",".join(str(sample[name]) for name in csv_header())


def frames_to_csv(lines):
    rows = [",".join(csv_header())]
    skipped = 0
    for line in lines:
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        sample = parse_frame(line)
        if sample is None:
            skipped += 1
            continue
        rows.append(to_csv_row(sample))
    return "\n".join(rows) + "\n", skipped, len(rows) - 1


def _open_port(name, baud):
    try:
        import serial
    except ImportError:
        print("pyserial is not installed. Use --file, or pip install pyserial.", file=sys.stderr)
        return None
    return serial.Serial(name, baud, timeout=1)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--file", help="text capture instead of a serial port")
    parser.add_argument("--port", help="serial device, for example /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=9600)
    parser.add_argument("--csv", help="CSV output path, default stdout")
    parser.add_argument("--once", action="store_true", help="stop after one valid frame on a port")
    args = parser.parse_args(argv)

    if args.file and args.port:
        print("pass only one of --file and --port", file=sys.stderr)
        return 1

    if args.port:
        port = _open_port(args.port, args.baud)
        if port is None:
            return 1
        lines = []
        try:
            while True:
                raw = port.readline()
                if not raw:
                    continue
                line = raw.decode("ascii", errors="replace")
                if parse_frame(line) is not None:
                    lines.append(line)
                    if args.once:
                        break
        except KeyboardInterrupt:
            pass
        finally:
            port.close()
    elif args.file:
        with open(args.file, "r", encoding="utf-8") as fh:
            lines = fh.readlines()
    else:
        lines = sys.stdin.readlines()

    csv_text, skipped, kept = frames_to_csv(lines)
    if args.csv:
        with open(args.csv, "w", encoding="utf-8") as fh:
            fh.write(csv_text)
    else:
        sys.stdout.write(csv_text)
    print(f"# kept {kept} skipped {skipped}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
