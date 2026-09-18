import importlib.util
import struct
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).parents[1]
spec = importlib.util.spec_from_file_location("water_visual_harness", ROOT / "tools" / "water_visual_harness.py")
harness = importlib.util.module_from_spec(spec)
spec.loader.exec_module(harness)


def bmp(width=80, height=40, stripe=None):
    """Create a small bottom-up 24-bit BMP with an optional cyan stripe."""
    row_bytes = width * 3
    stride = (row_bytes + 3) & ~3
    payload = bytearray(stride * height)
    for y in range(height):
        for x in range(width):
            # Dark blue background plus an unmistakable cyan highlight.
            rgb = (18, 32, 48)
            if stripe is not None and stripe <= x < stripe + 5 and int(height * .08) <= y < int(height * .28):
                rgb = (24, 190, 236)
            offset = y * stride + x * 3
            payload[offset:offset + 3] = bytes((rgb[2], rgb[1], rgb[0]))
    header = bytearray(54)
    header[:2] = b"BM"
    header[2:6] = struct.pack("<I", 54 + len(payload))
    header[10:14] = struct.pack("<I", 54)
    header[14:18] = struct.pack("<I", 40)
    header[18:22] = struct.pack("<i", width)
    header[22:26] = struct.pack("<i", height)
    header[26:28] = struct.pack("<H", 1)
    header[28:30] = struct.pack("<H", 24)
    return bytes(header + payload)


def good_log():
    lines = []
    for index in range(4):
        lines.append(
            "VRTEST water-image view=0 step=10 eye=left image=1 matrix=1 "
            "capture=(-10.0,20.0,30.0) fallback=0"
        )
        lines.append(
            "VRTEST water-image view=0 step=10 eye=right image=1 matrix=1 "
            "capture=(-10.0,20.0,30.0) fallback=0"
        )
        lines.append(
            "VRTEST water-matrix view=0 step=10 eye=left row=0 live=0,0,0,0 "
            "capture=0.68,0.06,0.0,100.0"
        )
        lines.append(
            "VRTEST water-matrix view=0 step=10 eye=right row=0 live=0,0,0,0 "
            "capture=0.68,0.06,0.0,100.0"
        )
    return "\n".join(lines) + "\n"


class WaterVisualHarnessTest(unittest.TestCase):
    def test_contract_passes_for_current_source(self):
        result = harness.shader_contract(ROOT)
        self.assertEqual(result["status"], "pass")
        self.assertTrue(all(result["checks"].values()))
        self.assertTrue(result["checks"]["centerCaptureIsSoleReflectionSample"])
        self.assertTrue(result["checks"]["storedMatricesFullMvp"])
        self.assertTrue(result["checks"]["reuseIgnoresLaterWorldMat"])

    def test_contract_missing_source_is_explicit_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            result = harness.shader_contract(Path(directory))
            self.assertEqual(result["status"], "fail")
            self.assertIn("unreadable", result["reason"])

    def test_moving_band_fails_and_static_band_passes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for index, x in enumerate((8, 16, 24, 32, 40, 48)):
                (root / f"ScreenShot{index + 1}.bmp").write_bytes(bmp(stripe=x))
            moving = harness.analyze_screenshots(root)
            self.assertEqual(moving["status"], "fail")
            self.assertGreater(moving["best"]["driftPixels"], moving["driftThresholdPixels"])
            for path in root.glob("ScreenShot*.bmp"):
                path.write_bytes(bmp(stripe=32))
            static = harness.analyze_screenshots(root)
            self.assertEqual(static["status"], "pass")
            self.assertLess(static["best"]["driftPixels"], static["driftThresholdPixels"])

    def test_unavailable_and_invalid_image_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.assertEqual(harness.analyze_screenshots(root)["status"], "unavailable")
            for index in range(4):
                (root / f"ScreenShot{index + 1}.bmp").write_bytes(b"not a bitmap")
            self.assertEqual(harness.analyze_screenshots(root)["status"], "fail")

    def test_runtime_log_stability_and_failure_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "OBVR.log").write_text(good_log(), encoding="utf-8")
            self.assertEqual(harness.analyze_runtime_log(root / "OBVR.log")["status"], "pass")
            queued = good_log().replace(
                "fallback=0", "fallback=0 storedDraws=4 replayedDraws=4"
            )
            (root / "OBVR.log").write_text(queued, encoding="utf-8")
            self.assertEqual(harness.analyze_runtime_log(root / "OBVR.log")["status"], "pass")
            empty_queue = queued.replace("storedDraws=4 replayedDraws=4",
                                          "storedDraws=0 replayedDraws=0", 1)
            (root / "OBVR.log").write_text(empty_queue, encoding="utf-8")
            self.assertEqual(harness.analyze_runtime_log(root / "OBVR.log")["status"], "fail")
            bad = good_log().replace("capture=(-10.0,20.0,30.0)", "capture=(-10.0,22.0,30.0)", 1)
            (root / "OBVR.log").write_text(bad, encoding="utf-8")
            self.assertEqual(harness.analyze_runtime_log(root / "OBVR.log")["status"], "fail")
            (root / "OBVR.log").write_text("", encoding="utf-8")
            self.assertEqual(harness.analyze_runtime_log(root / "OBVR.log")["status"], "unavailable")

    def test_evaluate_requires_visual_evidence_when_artifact_is_given(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for index, x in enumerate((8, 16, 24, 32, 40, 48)):
                (root / f"ScreenShot{index + 1}.bmp").write_bytes(bmp(stripe=x))
            result = harness.evaluate(ROOT, root)
            self.assertEqual(result["status"], "fail")
            self.assertEqual(result["visual"]["status"], "fail")
            self.assertEqual(result["runtime"]["status"], "unavailable")


if __name__ == "__main__":
    unittest.main()
