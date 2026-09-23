import json
import importlib.util
import tempfile
import unittest
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location(
    "analyze_performance", ROOT / "tools" / "analyze-performance.py")
analyze_performance = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(analyze_performance)


class AnalyzePerformanceTest(unittest.TestCase):
    def make_capture(self, root: Path) -> Path:
        capture = root / "capture"
        capture.mkdir()
        (capture / "manifest.json").write_text(json.dumps({
            "schema_version": 1, "session_id": 4, "qpc_frequency": 1000,
            "runtime_measurement": True, "status": "complete",
        }), encoding="utf-8")
        (capture / "cpu_events.csv").write_text(
            "event_id,event_type,present_id,scene_id,vr_frame_id,pass_index,eye,mode,start_tick,end_tick,status\n"
            "1,scene_pass,1,2,3,0,0,world_dual,100,120,0\n"
            "2,scene_pass,1,2,3,1,1,world_dual,120,160,0\n", encoding="utf-8")
        (capture / "frames.csv").write_text(
            "present_id,vr_frame_id,start_tick,end_tick,interval_tick,requested_mode,delivered_mode,pose_result,left_captured,right_captured,pass_count,setup,status,submit_left,submit_right\n"
            "1,3,100,160,50,world_dual,world_dual,0,1,1,2,0,0,0,0\n", encoding="utf-8")
        (capture / "gpu_samples.csv").write_text(
            "sample_id,present_id,vr_frame_id,event_type,pass_index,eye,status,start_tick,end_tick,frequency,age_presents\n"
            "8,1,3,scene_pass,0,0,ready,10,30,1000,0\n", encoding="utf-8")
        return capture

    def test_summary_and_quantiles(self):
        with tempfile.TemporaryDirectory(dir=str(ROOT)) as directory:
            summary = analyze_performance.analyze(self.make_capture(Path(directory)))
            self.assertEqual(summary["session_id"], 4)
            self.assertEqual(summary["cpu_wall_ms"]["scene_pass"]["count"], 2)
            self.assertEqual(summary["gpu_interval_ms"]["scene_pass"]["median_ms"], 20.0)
            self.assertEqual(summary["dual_pass_compare"]["paired_frames"], 1)
            self.assertEqual(summary["dual_pass_compare"]["pass_1_minus_pass_0_ms"]["mean_ms"], 20.0)
            self.assertEqual(summary["gpu_interval_by_mode_ms"]["world_dual"]["count"], 1)

    def test_duplicate_ids_are_rejected(self):
        with tempfile.TemporaryDirectory(dir=str(ROOT)) as directory:
            capture = self.make_capture(Path(directory))
            path = capture / "gpu_samples.csv"
            path.write_text(path.read_text(encoding="utf-8") +
                            "8,1,3,scene_pass,0,0,ready,10,30,1000,0\n", encoding="utf-8")
            with self.assertRaises(analyze_performance.CaptureError):
                analyze_performance.analyze(capture)


if __name__ == "__main__":
    unittest.main()
