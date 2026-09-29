#!/usr/bin/env python3
"""Lock the host tools to the firmware's CRC, calibration image, and frame."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import calibrate
import gauge_log

ROOT = Path(__file__).resolve().parents[1]
SAMPLE = "$UPG,1,5012,250,1253,10000,03,400,4900*9E\r\n"


class ToolTests(unittest.TestCase):
    def test_crc_vector(self):
        self.assertEqual(calibrate.crc8(b"123456789"), 0xF4)
        self.assertEqual(gauge_log.crc8(b"123456789"), 0xF4)

    def test_reference_math(self):
        self.assertEqual(calibrate.vbg_from_reference(1100, 4980, 5012), 1107)
        self.assertEqual(calibrate.gain_from_load(230, 250), 4452)

    def test_frame_round_trip(self):
        sample = gauge_log.parse_frame(SAMPLE)
        self.assertIsNotNone(sample)
        self.assertEqual(sample["vbus_mV"], 5012)
        self.assertEqual(sample["i_mA"], 250)
        self.assertEqual(sample["p_mW"], 1253)
        self.assertEqual(sample["energy_uWh"], 10000)
        self.assertEqual(sample["flags"], 0x03)
        self.assertEqual(sample["vbus_ok"], 1)
        self.assertEqual(sample["in_spec"], 1)
        self.assertEqual(sample["adc_fault"], 0)
        self.assertIsNone(gauge_log.parse_frame(SAMPLE.replace("5012", "5013")))
        csv_text, skipped, kept = gauge_log.frames_to_csv(
            ["# banner\n", SAMPLE, "not a frame\n"]
        )
        self.assertEqual(skipped, 1)
        self.assertEqual(kept, 1)
        self.assertIn("5012,250,1253,10000,3,400,4900", csv_text)

    def test_c_artifacts(self):
        image_path = ROOT / "build" / "host" / "sample.eep"
        frame_path = ROOT / "build" / "host" / "sample.frame"
        self.assertTrue(image_path.is_file(), "run make test-c first")
        blob = image_path.read_bytes()
        encoded = calibrate.encode_image(1102, -4, 4100, 0x91)
        self.assertEqual(blob, encoded)
        decoded = calibrate.decode_image(blob)
        self.assertEqual(decoded["source"], "factory")
        self.assertEqual(decoded["vbg_mV"], 1102)
        self.assertEqual(decoded["offset_mA"], -4)
        frame = frame_path.read_bytes().decode("ascii")
        self.assertEqual(frame, SAMPLE)
        legacy = calibrate.decode_image(bytes([0x04, 0x4C]) + bytes(14))
        self.assertEqual(legacy["source"], "legacy")
        self.assertEqual(legacy["vbg_mV"], 1100)


if __name__ == "__main__":
    unittest.main()
