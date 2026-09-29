#!/usr/bin/env python3
"""Build and inspect the 16-byte calibration image programmed over ISP.

The gauge has no serial RX pin. Factory calibration is an EEPROM image,
not a runtime command. Byte layout matches firmware/portable/cal.c.
"""

import argparse
import struct
import sys

CAL_LEN = 16
VBG_MIN, VBG_MAX = 800, 1300
OFFSET_MIN, OFFSET_MAX = -500, 500
GAIN_MIN, GAIN_MAX = 2048, 8192
GAIN_UNITY = 4096


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


def _defaults():
    return {
        "source": "default",
        "vbg_mV": 1100,
        "offset_mA": 0,
        "gain_q12": GAIN_UNITY,
        "osccal": 0,
    }


def _ranges_ok(vbg, offset, gain):
    return VBG_MIN <= vbg <= VBG_MAX and OFFSET_MIN <= offset <= OFFSET_MAX and GAIN_MIN <= gain <= GAIN_MAX


def encode_image(vbg_mV, offset_mA, gain_q12, osccal=0) -> bytes:
    if not _ranges_ok(vbg_mV, offset_mA, gain_q12):
        raise ValueError("calibration fields are outside the accepted range")
    if not 0 <= osccal <= 255:
        raise ValueError("osccal must be 0..255")
    body = bytes([ord("U"), ord("P"), 1, osccal]) + struct.pack("<HhH", vbg_mV, offset_mA, gain_q12)
    return body + bytes([crc8(body)]) + bytes(5)


def decode_image(blob: bytes) -> dict:
    out = _defaults()
    if len(blob) >= CAL_LEN and blob[0:3] == b"UP\x01" and crc8(blob[:10]) == blob[10]:
        vbg, offset, gain = struct.unpack_from("<HhH", blob, 4)
        if _ranges_ok(vbg, offset, gain):
            out.update(
                source="factory",
                vbg_mV=vbg,
                offset_mA=offset,
                gain_q12=gain,
                osccal=blob[3],
            )
            return out
    if len(blob) >= 2:
        legacy = (blob[0] << 8) | blob[1]
        if VBG_MIN <= legacy <= VBG_MAX:
            out.update(source="legacy", vbg_mV=legacy)
    return out


def vbg_from_reference(assumed_vbg, reported_mV, meter_mV) -> int:
    if assumed_vbg <= 0 or reported_mV <= 0 or meter_mV <= 0:
        raise ValueError("voltages must be positive")
    solved = (assumed_vbg * meter_mV + reported_mV // 2) // reported_mV
    if not VBG_MIN <= solved <= VBG_MAX:
        raise ValueError(f"solved bandgap {solved} mV is outside {VBG_MIN}..{VBG_MAX}")
    return solved


def gain_from_load(reported_mA, true_mA) -> int:
    if reported_mA <= 0:
        raise ValueError("reported load current must be positive")
    gain = (true_mA * GAIN_UNITY + reported_mA // 2) // reported_mA
    if not GAIN_MIN <= gain <= GAIN_MAX:
        raise ValueError(f"solved gain {gain} is outside {GAIN_MIN}..{GAIN_MAX}")
    return gain


def _print_cal(cal):
    print(
        f"source={cal['source']} vbg={cal['vbg_mV']} mV offset={cal['offset_mA']} mA "
        f"gain_q12={cal['gain_q12']} osccal={cal['osccal']}"
    )


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="cmd", required=True)

    enc = sub.add_parser("encode", help="write a factory image")
    enc.add_argument("--vbg", type=int, required=True)
    enc.add_argument("--offset", type=int, default=0)
    enc.add_argument("--gain", type=int, default=GAIN_UNITY)
    enc.add_argument("--osccal", type=lambda s: int(s, 0), default=0)
    enc.add_argument("-o", "--output", required=True)

    dec = sub.add_parser("decode", help="print an image")
    dec.add_argument("image")

    sol = sub.add_parser("solve", help="compute an image from meter readings")
    sol.add_argument("--assumed-vbg", type=int, default=1100)
    sol.add_argument("--reported-mv", type=int, required=True)
    sol.add_argument("--meter-mv", type=int, required=True)
    sol.add_argument("--zero-ma", type=int, default=0, help="no-load current at unity gain")
    sol.add_argument("--true-ma", type=int)
    sol.add_argument("--reported-load-ma", type=int)
    sol.add_argument("--osccal", type=lambda s: int(s, 0), default=0)
    sol.add_argument("-o", "--output", required=True)

    args = parser.parse_args(argv)
    try:
        if args.cmd == "encode":
            blob = encode_image(args.vbg, args.offset, args.gain, args.osccal)
        elif args.cmd == "decode":
            blob = open(args.image, "rb").read()
            _print_cal(decode_image(blob))
            return 0
        else:
            vbg = vbg_from_reference(args.assumed_vbg, args.reported_mv, args.meter_mv)
            gain = GAIN_UNITY
            if args.true_ma is not None or args.reported_load_ma is not None:
                if args.true_ma is None or args.reported_load_ma is None:
                    raise ValueError("pass both --true-ma and --reported-load-ma")
                gain = gain_from_load(args.reported_load_ma, args.true_ma)
            blob = encode_image(vbg, args.zero_ma, gain, args.osccal)
        with open(args.output, "wb") as fh:
            fh.write(blob)
        _print_cal(decode_image(blob))
        print(f"wrote {args.output} ({len(blob)} bytes)")
    except (OSError, ValueError) as exc:
        print(exc, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
