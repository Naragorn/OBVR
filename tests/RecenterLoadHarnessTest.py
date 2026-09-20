import json
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from recenter_load_harness import analyze_manifest  # noqa: E402


LOG = (
    "Camera: recentered on key 0x2E (frame 0, flat path)\n"
    "Render: on a world frame Oblivion draws into x=0..4036 y=0..2270 of the frame\n"
)


def receipt(**overrides):
    value = {
        "schema": 1,
        "mode": "live-recenter-load",
        "saveIndex": 1,
        "saveEntry": "Save 3 - Amy - Wilderness, Level 1, Playing Time 00.10.03.ess",
        "recenterEvidence": {
            "requested": True,
            "marker": "Camera: recentered on key 0x2E (frame 0, flat path)",
            "flatPath": True,
            "beforeWorldFrame": True,
        },
        "worldEvidence": {
            "loaded": True,
            "marker": "Render: on a world frame Oblivion draws",
        },
    }
    value.update(overrides)
    return value


class RecenterLoadHarnessTest(unittest.TestCase):
    def run_case(self, manifest, log=LOG):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "recenter-load-result.json").write_text(
                json.dumps(manifest), encoding="utf-8"
            )
            if log is not None:
                (root / "OBVR.log").write_text(log, encoding="utf-8")
            return analyze_manifest(root)

    def test_complete_ordered_receipt_passes(self):
        self.assertEqual(self.run_case(receipt())["status"], "pass")

    def test_missing_receipt_is_unavailable(self):
        with tempfile.TemporaryDirectory() as directory:
            self.assertEqual(analyze_manifest(Path(directory))["status"], "unavailable")

    def test_invalid_json_fails_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "recenter-load-result.json").write_text("{", encoding="utf-8")
            self.assertEqual(analyze_manifest(root)["status"], "fail")

    def test_wrong_schema_mode_and_requested_state_fail(self):
        self.assertEqual(self.run_case(receipt(schema=2))["status"], "fail")
        self.assertEqual(self.run_case(receipt(mode="other"))["status"], "fail")
        bad = receipt()
        bad["recenterEvidence"]["requested"] = False
        self.assertEqual(self.run_case(bad)["status"], "fail")

    def test_flat_and_world_receipts_are_required(self):
        bad = receipt()
        bad["recenterEvidence"]["flatPath"] = False
        self.assertEqual(self.run_case(bad)["status"], "fail")
        bad = receipt()
        bad["recenterEvidence"]["beforeWorldFrame"] = False
        self.assertEqual(self.run_case(bad)["status"], "fail")
        bad = receipt()
        bad["worldEvidence"]["loaded"] = False
        self.assertEqual(self.run_case(bad)["status"], "fail")

    def test_missing_runtime_log_or_markers_fail(self):
        self.assertEqual(self.run_case(receipt(), log=None)["status"], "fail")
        self.assertEqual(self.run_case(receipt(), log="world only\n")["status"], "fail")
        self.assertEqual(
            self.run_case(receipt(), log="Camera: recentered on key 0x2E (frame 0, flat path)\n")[
                "status"
            ],
            "fail",
        )

    def test_world_before_recenter_fails(self):
        self.assertEqual(self.run_case(receipt(), log=LOG)["status"], "pass")
        reversed_log = (
            "Render: on a world frame Oblivion draws\n"
            "Camera: recentered on key 0x2E (frame 0, flat path)\n"
        )
        self.assertEqual(self.run_case(receipt(), log=reversed_log)["status"], "fail")


if __name__ == "__main__":
    unittest.main()
