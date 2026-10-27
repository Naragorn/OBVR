import importlib.util
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("perf_samples", ROOT / "tools" / "perf-samples.py")
perf_samples = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(perf_samples)


def row(eip, in_pass="1", *chain):
    r = {"tick": "0", "in_pass": in_pass, "scene_call": "1", "eip": eip}
    for k in range(8):
        r["w%d" % k] = chain[k] if k < len(chain) else ""
    return r


def row_outside(eip, outside, *chain):
    """A sample of the 2026-10-27 format: x0..x3, the return addresses in
    other modules, innermost first."""
    r = row(eip, "1", *chain)
    for k in range(4):
        r["x%d" % k] = outside[k] if k < len(outside) else ""
    return r


class PerfSamplesTest(unittest.TestCase):
    def test_arguments(self):
        s, r, marks, opts = perf_samples.parse_args(["s.csv", "run", "a", "b", "--bin", "64", "--in-pass", "only", "c"])
        self.assertEqual((s, r, marks), ("s.csv", "run", ["a", "b", "c"]))
        self.assertEqual((opts["bin"], opts["in_pass"], opts["top"]), (64, "only", 25))
        self.assertEqual(perf_samples.parse_args(["s.csv"])[0], None)

    def test_marks_are_read_from_the_log(self):
        log = ["HandScript: mark ready (t=12.00 s, line 30, scene call 0, tick 100)",
               "HandScript: mark old (t=12.00 s, line 30)"]
        self.assertEqual(perf_samples.mark_ticks(log), {"ready": 100})

    def test_exclusive_bins_and_inclusive_frames(self):
        samples = [
            row("00767530", "1", "007694C2", "00500010"),
            row("00767540", "1", "007694C2", "00500010"),
            row("79D00000", "0", "007694C2"),
            # the same return address twice on one stack counts once
            row("00400010", "1", "00500010", "00500010"),
        ]
        agg = perf_samples.aggregate(samples, 256)
        self.assertEqual((agg["count"], agg["in_pass"]), (4, 3))
        # the samples outside the engine's code (the DLL eip, and the one in
        # the exe's header) name the engine site they were called from
        self.assertEqual(agg["callers_out"], {0x7694C2: 1, 0x500010: 1})
        self.assertEqual(agg["exclusive"][0x767500], 2)
        self.assertEqual(agg["exclusive"][0x79D00000], 1)
        self.assertEqual(agg["inclusive"][0x7694C2], 3)
        self.assertEqual(agg["inclusive"][0x500010], 3)

    def test_outside_frames(self):
        samples = [
            # a stall in ntdll under vrclient under D3D9 under the engine
            row_outside("77D2861C", ["50BA9296", "7994A0F0"], "0057A123"),
            row_outside("77D2861C", ["50BA9296"], "0057A123"),
            row_outside("00767530", [], "007694C2"),
            row("00767540", "1", "007694C2"),  # an old file without x columns
        ]
        agg = perf_samples.aggregate(samples, 256)
        # the innermost return address outside the exe, once per sample
        self.assertEqual(agg["outside_first"], {0x50BA9296: 2})
        # every outside return address, once per sample
        self.assertEqual(agg["outside_inclusive"], {0x50BA9296: 2, 0x7994A0F0: 1})

    def test_in_pass_filter(self):
        self.assertTrue(perf_samples.wanted(row("0", "1"), "only"))
        self.assertFalse(perf_samples.wanted(row("0", "0"), "only"))
        self.assertTrue(perf_samples.wanted(row("0", "0"), "out"))
        self.assertFalse(perf_samples.wanted(row("0", "1"), "out"))
        self.assertTrue(perf_samples.wanted(row("0", "0"), "all"))

    def test_module_of(self):
        self.assertEqual(perf_samples.module_of(0x00767530), "Oblivion.exe")
        self.assertEqual(perf_samples.module_of(0x00A88EA4), "Oblivion.exe data")
        self.assertEqual(perf_samples.module_of(0x79D04808), "outside the exe")

    def test_modules_name_the_pages_outside_the_engine(self):
        table = perf_samples.module_table([
            {"page": "798C", "module": "d3d9.dll", "base": "79800000"},
            {"page": "F7B7", "module": "?", "base": "00000000"},
            {"page": "bad", "module": "x", "base": "y"},
        ])
        self.assertEqual(perf_samples.module_of(0x798C9200, table), "d3d9.dll")
        self.assertEqual(perf_samples.module_of(0xF7B77E00, table), "outside the exe")
        self.assertEqual(perf_samples.module_of(0x7BEFD400, table), "outside the exe")
        self.assertEqual(perf_samples.name_address(0x798C9200, table), "d3d9.dll+C9200")
        self.assertEqual(perf_samples.name_address(0xF7B77E00, table), "F7B77E00")
        self.assertEqual(perf_samples.name_address(0x00767530, None), "00767530")


if __name__ == "__main__":
    unittest.main()
