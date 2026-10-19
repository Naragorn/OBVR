import importlib.util
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("perf_phases", ROOT / "tools" / "perf-phases.py")
perf_phases = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(perf_phases)


class MarkCallsTest(unittest.TestCase):
    """Where each mark sits in the log's scene calls - the capture's clock."""

    def test_marks_bracketed_by_the_dual_heartbeat(self):
        # A log from before the marks carried their scene call: a mark is
        # placed between the dual heartbeat lines around it.
        log = [
            "Dual draws at scene call 120: first pass 300, between the passes 0, second pass 300",
            "HandScript: mark ready (t=12.00 s, line 30)",
            "Dual draws at scene call 240: first pass 300, between the passes 0, second pass 300",
            "Performance: capture 1 stopped (truncated_capacity), export queued",
            "HandScript: mark done (t=90.00 s, line 40)",
        ]
        marks, call_at_stop, exact = perf_phases.mark_calls(log)
        self.assertEqual(marks["ready"], (120, 240))
        self.assertEqual(marks["done"], (240, 240))
        self.assertEqual(call_at_stop, 240)
        self.assertFalse(exact)

    def test_marks_carrying_their_scene_call_are_placed_exactly(self):
        # Since the mark line carries the scene call there is nothing to
        # bracket: the mark is at that call, in any stereo mode.
        log = [
            "HandScript: mark ready (t=12.00 s, line 30, scene call 1234)",
            "HandScript: mark people (t=70.00 s, line 36, scene call 5000)",
        ]
        marks, call_at_stop, exact = perf_phases.mark_calls(log)
        self.assertEqual(marks["ready"], (1234, 1234))
        self.assertEqual(marks["people"], (5000, 5000))
        self.assertIsNone(call_at_stop)
        self.assertTrue(exact)

    def test_alternate_eyes_log_has_no_heartbeat_and_still_places_marks(self):
        # Under Stereo=aer the "Dual ..." lines never appear; the marks'
        # own scene calls are the only clock, and the capture stop is
        # placed at the last mark before it.
        log = [
            "Config: Render.Stereo=aer",
            "HandScript: mark placed (t=34.00 s, line 32, scene call 2000)",
            "HandScript: mark people (t=70.00 s, line 36, scene call 4700)",
            "Performance: capture 1 stopped (duration), export queued",
            "HandScript: mark done (t=160.00 s, line 44, scene call 11000)",
        ]
        marks, call_at_stop, exact = perf_phases.mark_calls(log)
        self.assertEqual(marks["placed"], (2000, 2000))
        self.assertEqual(marks["people"], (4700, 4700))
        self.assertEqual(marks["done"], (11000, 11000))
        self.assertEqual(call_at_stop, 4700)
        self.assertTrue(exact)

    def test_a_mark_with_its_scene_call_closes_an_older_bracket(self):
        # Mixed logs: an old-style mark pending from before is closed by the
        # next scene call seen, whichever line carries it.
        log = [
            "Dual draws at scene call 120: first pass 300, between the passes 0, second pass 300",
            "HandScript: mark ready (t=12.00 s, line 30)",
            "HandScript: mark placed (t=34.00 s, line 32, scene call 200)",
        ]
        marks, _, exact = perf_phases.mark_calls(log)
        self.assertEqual(marks["ready"], (120, 200))
        self.assertEqual(marks["placed"], (200, 200))
        self.assertFalse(exact)  # one mark had to be bracketed

    def test_marks_carrying_a_tick_are_read(self):
        log = [
            "HandScript: mark ready (t=12.00 s, line 30, scene call 0, tick 153659593659)",
            "HandScript: mark people (t=70.00 s, line 36, scene call 5000, tick 153660200000)",
            "HandScript: mark old (t=90.00 s, line 40, scene call 6000)",
        ]
        ticks = perf_phases.mark_ticks(log)
        self.assertEqual(ticks, {"ready": 153659593659, "people": 153660200000})

    def test_frames_are_cut_by_tick(self):
        # The capture's frames keep the same counter: a phase is the frames
        # that start at or after its mark and before the next one.
        frames = [
            {"present_id": "3", "start_tick": "300"},
            {"present_id": "1", "start_tick": "100"},
            {"present_id": "2", "start_tick": "200"},
            {"present_id": "4", "start_tick": "400"},
        ]
        self.assertEqual(perf_phases.presents_between_ticks(frames, 200, 400), [2, 3])
        self.assertEqual(perf_phases.presents_between_ticks(frames, 401, 900), [])

    def test_compositor_phase_sums_what_the_compositor_saw(self):
        rows = [
            # 4 is the async flag every frame carries; not a reprojection.
            {"dropped_frames": "0", "mis_presented": "0", "reprojection_flags": "4",
             "pre_submit_gpu_ms": "2.0", "new_frame_ready_ms": "-1.0"},
            # 6 = async + GPU reason: reprojected.
            {"dropped_frames": "1", "mis_presented": "1", "reprojection_flags": "6",
             "pre_submit_gpu_ms": "4.0", "new_frame_ready_ms": "3.0"},
        ]
        s = perf_phases.compositor_phase(rows)
        self.assertEqual((s["frames"], s["dropped"], s["mis_presented"], s["reprojected"]), (2, 1, 1, 1))
        self.assertEqual((s["gpu_mean"], s["gpu_p95"]), (3.0, 4.0))
        self.assertEqual((s["submit_mean"], s["submit_p95"]), (1.0, 3.0))
        self.assertEqual(perf_phases.compositor_cells(s), ("1 / 1 / 1", "3.00 / 4.00", "1.00 / 3.00"))

    def test_compositor_phase_without_rows_is_missing(self):
        # A capture from before compositor.csv, or a compositor that answered
        # nothing: the cells say so instead of inventing zeros.
        self.assertIsNone(perf_phases.compositor_phase([]))
        self.assertEqual(perf_phases.compositor_cells(None), ("-", "-", "-"))

    def test_scale_is_one_when_the_marks_are_exact(self):
        self.assertEqual(perf_phases.call_scale(8280, 8293, True), 1.0)
        self.assertAlmostEqual(perf_phases.call_scale(8280, 8293, False), 8280 / 8293.0)
        self.assertEqual(perf_phases.call_scale(100, 0, False), 1.0)


if __name__ == "__main__":
    unittest.main()
