import importlib.util
import hashlib
import json
import re
import struct
import tempfile
import unittest
import sys
import itertools
from pathlib import Path

ROOT = Path(__file__).parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
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
    lines = [
        "Water reflection: original-target camera/projection hook installed",
        "Water reflection: verified original-pixel-path vertex shaders created",
        "Water reflection: head-independent camera rendered into the original target",
        "Water reflection: original target projection matrix uploaded",
        "VRTEST water runner armed schema=21",
        "VRTEST lifecycle status=pass suppressed=1 recovered=1",
    ]
    for view in range(7):
        for step in (8, 18):
            for eye in ("left", "right"):
                replayed = 0 if eye == "left" else 4
                lines.append(
                    f"VRTEST water-image view={view} step={step} eye={eye} "
                    "yaw=0 image=1 matrix=1 fallback=0 scale=(3.00,1.50) "
                    f"storedDraws=4 replayedDraws={replayed} "
                    "capture=(-10.0,20.0,30.0)"
                )
                lines.append(
                    f"VRTEST water-matrix view={view} step={step} eye={eye} row=0 "
                    "live=0,0,0,0 capture=0.68,0.06,0.0,100.0"
                )
                for row in range(4):
                    lines.append(f"VRTEST water-world-projection view={view} step={step} eye={eye} "
                                 f"valid=1 row={row} value=(0.68,0.06,0.0,100.0)")
    lines.append(
        "VRTEST water-sweep status=pass views=7 imageMask=0FFFFFFF "
        "matrixMask=0FFFFFFF fixedCaptureMask=0FFFFFFF "
        "projectionScaleMask=0FFFFFFF eyeReuseMask=0FFFFFFF expected=0FFFFFFF"
    )
    return "\n".join(lines) + "\n"


def write_regions(root, rect=(0, .52, 1, .96)):
    document = {path.name: {"rect": rect, "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                for path in root.glob("OBVR-VRTest-water-view-*.bmp")}
    (root / "water-regions.json").write_text(json.dumps(document), encoding="utf-8")
    return document



class WaterVisualHarnessTest(unittest.TestCase):
    def test_registered_water_rejects_drift_black_and_insufficient_coverage(self):
        import numpy as np
        from PIL import Image
        from water_alignment_diagnostic import compare_water_pixels, register, camera_ray_maps
        image = np.full((40,80,3), 100, dtype=np.uint8)
        mask = np.ones((40,80), dtype=bool)
        self.assertEqual(compare_water_pixels(image,image,mask)['status'],'pass')
        self.assertEqual(compare_water_pixels(image,image+5,mask)['status'],'pass')
        self.assertEqual(compare_water_pixels(image,image+6,mask)['status'],'fail')
        self.assertEqual(compare_water_pixels(image,np.zeros_like(image),mask)['status'],'fail')
        black = np.zeros_like(image)
        self.assertEqual(compare_water_pixels(black,black,mask)['status'],'fail')
        self.assertEqual(compare_water_pixels(image,image,np.zeros_like(mask))['status'],'unavailable')
        stripe=image.copy();stripe[:,10:20]=220
        drift=np.roll(stripe,30,axis=1)
        self.assertEqual(compare_water_pixels(stripe,drift,mask)['status'],'fail')
        self.assertTrue(np.array_equal(np.asarray(register(Image.fromarray(stripe),np.eye(3),np.eye(3))),stripe))
        with self.assertRaises(ValueError): camera_ray_maps('',80,40)

    def test_alignment_end_to_end_and_refusal_flows(self):
        from water_alignment_diagnostic import analyze_alignment, camera_ray_maps
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            self.assertEqual(analyze_alignment(root)['status'],'unavailable')
            lines=[]
            mvp=('1,0,0,0','0,1,0,0','0,0,1,0','0,0,1,0')
            world=('1,0,0,0','0,1,0,0','0,0,1,0','0,0,0,1')
            for view in range(7):
                for step,stripe in ((8,28),(18,34)):
                    for eye,label in (('left','L'),('right','R')):
                        (root/f'OBVR-VRTest-water-view-{view}-step-{step}-{label}.bmp').write_bytes(bmp(stripe=stripe))
                        for row in range(4):
                            lines.append(f'VRTEST water-input view={view} step={step} eye={eye} row={row} mvp=({mvp[row]}) world=({world[row]})')
            text='\n'.join(lines)
            (root/'OBVR.log').write_text(text)
            document={'polygons':[[[0,0],[1,0],[1,1],[0,1]]], 'sha256':{
                path.name:hashlib.sha256(path.read_bytes()).hexdigest() for path in root.glob('*.bmp')}}
            annotation=root/'water-alignment-region.json'
            annotation.write_text(json.dumps(document))
            result=analyze_alignment(root)
            self.assertEqual(result['status'],'pass',result)
            self.assertEqual(result['waveMovingPairs'],14)
            self.assertEqual(harness.analyze_screenshots(root)['status'],'pass')
            with self.assertRaises(ValueError): camera_ray_maps(text+'\n'+lines[-1],80,40,'right',18)
            with self.assertRaises(ValueError): camera_ray_maps(text.replace('mvp=(1,0,0,0)','mvp=(nan,0,0,0)'),80,40)
            with self.assertRaises(ValueError): camera_ray_maps('\n'.join(lines[:-1]),80,40,'right',18)
            for polygon in ([[0,0],[1,1]], [[0,0],[2,0],[0,1]], [[0,0],[True,0],[0,1]]):
                annotation.write_text(json.dumps(dict(document,polygons=[polygon])))
                self.assertEqual(analyze_alignment(root)['status'],'unavailable')
            annotation.write_text(json.dumps(document))
            (root/'OBVR-VRTest-water-view-0-step-8-L.bmp').write_bytes(bmp(stripe=20))
            self.assertEqual(analyze_alignment(root)['status'],'unavailable')
            # Rebind after a deliberate frozen-wave negative control.
            for path in root.glob('*.bmp'): path.write_bytes(bmp(stripe=34))
            document['sha256']={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in root.glob('*.bmp')}
            annotation.write_text(json.dumps(document))
            self.assertEqual(analyze_alignment(root)['status'],'fail')
            path=root/'OBVR-VRTest-water-view-0-step-8-L.bmp'
            path.write_bytes(bmp(width=90))
            document['sha256'][path.name]=hashlib.sha256(path.read_bytes()).hexdigest()
            annotation.write_text(json.dumps(document))
            self.assertEqual(analyze_alignment(root)['status'],'unavailable')

    def test_world_projection_requires_complete_stable_grid(self):
        self.assertEqual(harness.analyze_world_projection(good_log())["status"], "pass")
        self.assertEqual(harness.analyze_world_projection('')["status"], "unavailable")
        for replacement in ('value=(0.70,0.06,0.0,100.0)', 'value=(0.68,0.06,0.0,110.0)'):
            drift = good_log().replace('value=(0.68,0.06,0.0,100.0)', replacement, 1)
            self.assertEqual(harness.analyze_world_projection(drift)["status"], "fail")
        self.assertEqual(harness.analyze_world_projection(good_log().replace('valid=1 row=0', 'valid=0 row=0', 1))["status"], "fail")
        self.assertEqual(harness.analyze_world_projection(good_log() + good_log())["status"], "fail")
        self.assertEqual(harness.analyze_world_projection(good_log().replace('value=(0.68', 'value=(NaN', 1))["status"], "unavailable")

    def test_contract_passes_for_current_source(self):
        result = harness.shader_contract(ROOT)
        self.assertEqual(result["status"], "pass")
        self.assertTrue(all(result["checks"].values()))
        self.assertTrue(result["checks"]["originalPixelShaderPreserved"])
        self.assertTrue(result["checks"]["originalReflectionTargetPreserved"])
        self.assertTrue(result["checks"]["automaticYawSweepCapturesWavePairs"])

    def test_contract_missing_source_is_explicit_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            result = harness.shader_contract(Path(directory))
            self.assertEqual(result["status"], "fail")
            self.assertIn("unreadable", result["reason"])

    def test_replay_phase_order_and_missing_requirements(self):
        phases = ['WriteAllText($pluginIni, $initialIni',
                  'Start-Sleep -Seconds $LoadWaitSec',
                  '$focusAfterSettling = Focus-Game',
                  'WriteAllText($pluginIni, $testIni']
        requirements = ["$settlingIni = [regex]::Replace", "'VRTestSuite=0'",
                        '$initialIni = if ($ArmBeforeLoad) { $testIni } else { $settlingIni }',
                        'if (-not $ArmBeforeLoad) {',
                        'if (-not $focusAfterSettling)',
                        'Focus-Game | Out-Null',
                        'if ($tail -match "^OBVR ready$")']
        for order in itertools.permutations(phases):
            with self.subTest(order=order):
                self.assertEqual(harness.replay_armed_after_settling(
                    '\n'.join(requirements + list(order))), list(order) == phases)
        tokens = requirements + phases
        for missing in range(len(tokens)):
            with self.subTest(missing=missing):
                self.assertFalse(harness.replay_armed_after_settling(
                    '\n'.join(t for i,t in enumerate(tokens) if i != missing)))
        self.assertFalse(harness.replay_armed_after_settling(''))

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

    def test_internal_yaw_grid_requires_reflections_and_moving_waves(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for view in range(7):
                for step, stripe in ((8, 28), (18, 34)):
                    for eye in ("L", "R"):
                        name = f"OBVR-VRTest-water-view-{view}-step-{step}-{eye}.bmp"
                        (root / name).write_bytes(bmp(stripe=stripe))
            self.assertEqual(harness.analyze_screenshots(root)["status"], "unavailable")
            write_regions(root)
            passed = harness.analyze_screenshots(root)
            self.assertEqual(passed["status"], "unavailable")
            self.assertEqual(passed["waterActivityStatus"], "pass")
            self.assertEqual(passed["worldAlignmentStatus"], "unavailable")
            self.assertEqual(passed["samples"], 28)
            self.assertGreaterEqual(
                passed["waveMovingPairs"], passed["waveRequiredPairs"]
            )
            for path in root.glob("*step-18-*.bmp"):
                path.write_bytes(bmp(stripe=28))
            self.assertEqual(harness.analyze_screenshots(root)["status"], "unavailable")
            write_regions(root)
            frozen = harness.analyze_screenshots(root)
            self.assertEqual(frozen["status"], "fail")
            self.assertLess(
                frozen["waveMovingPairs"], frozen["waveRequiredPairs"]
            )

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
            log = root / "OBVR.log"
            log.write_text(good_log(), encoding="utf-8")
            result = harness.analyze_runtime_log(log)
            self.assertEqual(result["status"], "pass")
            self.assertTrue(all(result["markers"].values()))
            self.assertFalse(result["fallbackObserved"])

            old = good_log().replace("capture=(-10.0,20.0,30.0)", "capture=(900,800,700)")
            log.write_text(old + good_log(), encoding="utf-8")
            result = harness.analyze_runtime_log(log)
            self.assertEqual(result["status"], "pass")
            self.assertEqual(result["captureSamples"], 28)
            log.write_text(good_log() + "VRTEST water runner armed schema=21\n", encoding="utf-8")
            self.assertEqual(harness.analyze_runtime_log(log)["status"], "unavailable")

            current_log = re.sub(r"^VRTEST water-matrix.*\n", "", good_log(), flags=re.M)
            log.write_text(current_log, encoding="utf-8")
            self.assertEqual(harness.analyze_runtime_log(log)["status"], "pass")
            for invalid in ("00000000", "07FFFFFF", "FFFFFFFF"):
                log.write_text(current_log.replace("fixedCaptureMask=0FFFFFFF",
                               f"fixedCaptureMask={invalid}"), encoding="utf-8")
                self.assertEqual(harness.analyze_runtime_log(log)["status"], "fail")
            log.write_text(current_log.replace("fixedCaptureMask=0FFFFFFF ", ""),
                           encoding="utf-8")
            self.assertEqual(harness.analyze_runtime_log(log)["status"], "fail")

            log.write_text(
                good_log().replace("scale=(3.00,1.50)", "scale=(1.00,1.00)", 1),
                encoding="utf-8",
            )
            self.assertEqual(harness.analyze_runtime_log(log)["status"], "fail")

            log.write_text(
                good_log().replace("capture=(-10.0,20.0,30.0)",
                                   "capture=(-10.0,22.0,30.0)", 1),
                encoding="utf-8",
            )
            self.assertEqual(harness.analyze_runtime_log(log)["status"], "fail")

            log.write_text(
                good_log().replace("fallback=0", "fallback=1", 1),
                encoding="utf-8",
            )
            self.assertEqual(harness.analyze_runtime_log(log)["status"], "fail")

            log.write_text(
                good_log().replace(
                    "Water reflection: head-independent camera rendered into the original target\n",
                    "",
                ),
                encoding="utf-8",
            )
            self.assertEqual(harness.analyze_runtime_log(log)["status"], "fail")

            log.write_text("OBVR ready\n", encoding="utf-8")
            self.assertEqual(harness.analyze_runtime_log(log)["status"], "unavailable")


    def test_run_manifest_requires_second_save_and_complete_captures(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.assertEqual(harness.analyze_run_manifest(root)["status"], "unavailable")
            files = [f"OBVR-VRTest-water-view-{i // 4}-step-{(i // 2) % 2}-{('left', 'right')[i % 2]}.bmp" for i in range(28)]
            manifest = {
                "schema": 1,
                "saveIndex": 1,
                "captureCount": 28,
                "sweep": "synthetic-hmd-yaw--60-to-60",
                "files": files,
            }
            result_path = root / "water-vr-result.json"
            result_path.write_text(json.dumps(manifest), encoding="utf-8")
            self.assertEqual(harness.analyze_run_manifest(root)["status"], "fail")
            manifest["schema"] = 2
            manifest["saveIndex"] = 0
            result_path.write_text(json.dumps(manifest), encoding="utf-8")
            self.assertEqual(harness.analyze_run_manifest(root)["status"], "fail")
            manifest["saveIndex"] = 1
            manifest["captureCount"] = 27
            result_path.write_text(json.dumps(manifest), encoding="utf-8")
            self.assertEqual(harness.analyze_run_manifest(root)["status"], "fail")
            manifest["captureCount"] = 28
            manifest["sweep"] = "physical"
            result_path.write_text(json.dumps(manifest), encoding="utf-8")
            self.assertEqual(harness.analyze_run_manifest(root)["status"], "fail")
            manifest["sweep"] = "synthetic-hmd-yaw--60-to-60"
            manifest["files"] = files[:-1]
            result_path.write_text(json.dumps(manifest), encoding="utf-8")
            self.assertEqual(harness.analyze_run_manifest(root)["status"], "fail")
            manifest["files"] = files
            result_path.write_text(json.dumps(manifest), encoding="utf-8")
            result = harness.analyze_run_manifest(root)
            self.assertEqual(result["status"], "pass")
            self.assertEqual(result["saveIndex"], 1)
            result_path.write_text(json.dumps(manifest), encoding="utf-8-sig")
            self.assertEqual(harness.analyze_run_manifest(root)["status"], "pass")
            result_path.write_text("\ufeff{invalid", encoding="utf-8")
            self.assertEqual(harness.analyze_run_manifest(root)["status"], "fail")

    def test_toggle_manifest_requires_restore_receipt_and_log_marker(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            files = [f"OBVR-VRTest-water-view-{i // 4}-step-{(i // 2) % 2}-{('left', 'right')[i % 2]}.bmp" for i in range(28)]
            manifest = {
                "schema": 2,
                "saveIndex": 1,
                "captureCount": 28,
                "sweep": "synthetic-hmd-yaw--60-to-60",
                "files": files,
                "toggleWaterReflections": True,
                "toggleEvidence": {"restored": True},
            }
            result_path = root / "water-vr-result.json"
            result_path.write_text(json.dumps(manifest), encoding="utf-8")
            self.assertEqual(harness.analyze_run_manifest(root)["status"], "fail")
            (root / "OBVR.log").write_text(
                "Water manager lifecycle: restored reflection resource 1234ABCD after Off -> On\n",
                encoding="utf-8",
            )
            self.assertEqual(harness.analyze_run_manifest(root)["status"], "pass")
            manifest["toggleEvidence"] = {"restored": False}
            result_path.write_text(json.dumps(manifest), encoding="utf-8")
            self.assertEqual(harness.analyze_run_manifest(root)["status"], "fail")

    def test_toggle_only_receipt_requires_runtime_restore_marker(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            receipt = {
                "schema": 1,
                "mode": "live-water-toggle-only",
                "toggleWaterReflections": True,
                "toggleEvidence": {"restored": True},
            }
            (root / "water-toggle-result.json").write_text(
                json.dumps(receipt), encoding="utf-8"
            )
            self.assertEqual(harness.analyze_toggle_manifest(root)["status"], "fail")
            (root / "OBVR.log").write_text(
                "Water manager lifecycle: restored reflection resource 1234ABCD after Off -> On\n",
                encoding="utf-8",
            )
            self.assertEqual(harness.analyze_toggle_manifest(root)["status"], "pass")
            receipt["toggleEvidence"] = {"restored": False}
            (root / "water-toggle-result.json").write_text(
                json.dumps(receipt), encoding="utf-8"
            )
            self.assertEqual(harness.analyze_toggle_manifest(root)["status"], "fail")

    def test_evaluate_requires_visual_evidence_when_artifact_is_given(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for index, x in enumerate((8, 16, 24, 32, 40, 48)):
                (root / f"ScreenShot{index + 1}.bmp").write_bytes(bmp(stripe=x))
            result = harness.evaluate(ROOT, root)
            self.assertEqual(result["status"], "fail")
            self.assertEqual(result["visual"]["status"], "fail")
            self.assertEqual(result["runtime"]["status"], "unavailable")

    def test_water_region_validation_and_ground_exclusion(self):
        sample = {"width": 10, "height": 10, "rows": [[(10, 10, 10)] * 10 for _ in range(10)]}
        changed = {"width": 10, "height": 10, "rows": [
            [(10, 10, 10) if y < 5 else (240, 240, 240)] * 10 for y in range(10)]}
        self.assertEqual(harness._water_difference(sample, changed, (0, 0, 1, .5)), 0)
        self.assertEqual(harness._water_metrics(sample, (0, 0, 1, .5))["blackFraction"], 1)
        self.assertEqual(harness._water_metrics(sample, (0, 0, .01, .01))["pixelCount"], 0)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / "OBVR-VRTest-water-view-0-step-8-L.bmp"
            path.write_bytes(bmp())
            internal = {(0, 8, "L"): path}
            for rect in ([0, 0, 1], [-1, 0, 1, 1], [0, 1, 1, 0], [0, 0, 2, 1],
                         [False, 0, 1, 1], [0, 0, float('nan'), 1], [0, 0, '1', 1]):
                write_regions(root, rect)
                self.assertIsNone(harness._load_water_regions(root, internal)[0])
            write_regions(root)
            self.assertIsNotNone(harness._load_water_regions(root, internal)[0])
            (root / "water-regions.json").write_text('{}', encoding="utf-8")
            self.assertIsNone(harness._load_water_regions(root, internal)[0])
            (root / "water-regions.json").write_text('{broken', encoding="utf-8")
            self.assertIsNone(harness._load_water_regions(root, internal)[0])


if __name__ == "__main__":
    unittest.main()
