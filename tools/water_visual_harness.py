"""Automatic water-reflection stability harness.

The harness has no dependency on Direct3D or a running game.  It checks the
stable shader contract, the capture/matrix evidence emitted by VRTestRuntime,
and (when a screenshot sequence is present) measures horizontal movement of a
water-reflection colour band.  A screenshot sequence is evidence about pixels;
missing or unusable evidence is reported explicitly instead of being treated
as a pass.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import struct
import statistics
import sys
from pathlib import Path
from typing import Iterable, Optional, Sequence

_NUMBER = r"[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?"
_CAPTURE_LINE = re.compile(
    rf"VRTEST water-image .*?eye=(left|right).*?"
    rf"capture=\(({_NUMBER}),({_NUMBER}),({_NUMBER})\)"
)
_MATRIX_ROW0 = re.compile(
    rf"VRTEST water-matrix .*?eye=(left|right) row=0 .*?"
    rf"capture=({_NUMBER}),({_NUMBER}),({_NUMBER}),({_NUMBER})"
)

_QUEUE_LINE = re.compile(
    r"VRTEST water-image .*?storedDraws=(\d+) replayedDraws=(\d+)"
)
_SCALE_LINE = re.compile(
    rf"VRTEST water-image .*?scale=\(({_NUMBER}),({_NUMBER})\)"
)
_SWEEP_PASS_LINE = re.compile(r"VRTEST water-sweep status=pass")
_FIXED_CAPTURE_MASK = re.compile(
    r"VRTEST water-sweep status=pass .*?fixedCaptureMask=([0-9A-Fa-f]+) "
    r".*?expected=([0-9A-Fa-f]+)"
)
_WORLD_PROJECTION = re.compile(
    rf"VRTEST water-world-projection view=(\d+) step=(\d+) eye=(left|right) "
    rf"valid=(\d+) row=(\d+) value=\(({_NUMBER}),({_NUMBER}),({_NUMBER}),({_NUMBER})\)"
)


def analyze_world_projection(text):
    rows = {}
    for match in _WORLD_PROJECTION.finditer(text):
        view, step, eye, valid, row = match.groups()[:5]
        key = (int(view), int(step), eye, int(row))
        if valid != "1" or key in rows:
            return {"status": "fail", "reason": "invalid or duplicate world projection row"}
        rows[key] = [float(value) for value in match.groups()[5:]]
    expected = {(view, step, eye, row) for view in range(7) for step in (8, 18)
                for eye in ("left", "right") for row in range(4)}
    if set(rows) != expected:
        return {"status": "unavailable", "reason": "complete world projection grid required", "rows": len(rows)}
    ranges = {}
    for eye in ("left", "right"):
        ranges[eye] = [[max(v[column] for k, v in rows.items() if k[2:] == (eye, row))
                        - min(v[column] for k, v in rows.items() if k[2:] == (eye, row))
                        for column in range(4)] for row in range(4)]
    stable = all(math.isfinite(value) and value <= (0.1 if col == 3 else .002)
                 for eye in ranges.values() for row in eye for col, value in enumerate(row))
    return {"status": "pass" if stable else "fail", "ranges": ranges,
            "reason": "world projection stable" if stable else "shader world projection changes during HMD sweep"}


def _read_bmp(path: Path, max_width: int = 320, max_height: int = 180):
    """Read a small nearest-neighbour sample from a 24/32-bit BMP.

    Only the sampled pixels are retained, so the 4192x2358 game screenshots
    stay well below the memory cost of a full decoded frame.
    """
    try:
        data = path.read_bytes()
        if len(data) < 54 or data[:2] != b"BM":
            return None
        offset = int.from_bytes(data[10:14], "little")
        signed_width = int.from_bytes(data[18:22], "little", signed=True)
        signed_height = int.from_bytes(data[22:26], "little", signed=True)
        bits = int.from_bytes(data[28:30], "little")
        width, height = abs(signed_width), abs(signed_height)
        if offset < 54 or width < 1 or height < 1 or bits not in (24, 32):
            return None
        bpp = bits // 8
        stride = ((width * bits + 31) // 32) * 4
        if offset + stride * height > len(data):
            return None
        sample_width = min(width, max_width)
        sample_height = min(height, max_height)
        rows = []
        for sy in range(sample_height):
            source_y = min(height - 1, int((sy + 0.5) * height / sample_height))
            if signed_height > 0:
                source_y = height - 1 - source_y
            base = offset + source_y * stride
            row = []
            for sx in range(sample_width):
                source_x = min(width - 1, int((sx + 0.5) * width / sample_width))
                index = base + source_x * bpp
                # BMP stores B,G,R,(A); the feature code uses RGB order.
                row.append((data[index + 2], data[index + 1], data[index]))
            rows.append(row)
        return {"width": sample_width, "height": sample_height, "rows": rows}
    except (OSError, IndexError, ValueError):
        return None


def _blue_reflection_score(rgb: tuple[int, int, int]) -> float:
    """Score cyan/blue water highlights while rejecting green/brown ground."""
    red, green, blue = rgb
    return max(0.0, ((green + blue) * 0.5) - red - 18.0)


def _centroid(sample, top: float, bottom: float) -> Optional[float]:
    if sample is None:
        return None
    width, height, rows = sample["width"], sample["height"], sample["rows"]
    first = max(0, min(height - 1, int(height * top)))
    last = max(first + 1, min(height, int(height * bottom)))
    columns = [0.0] * width
    total = 0.0
    for row in rows[first:last]:
        for x, rgb in enumerate(row):
            score = _blue_reflection_score(rgb)
            columns[x] += score
            total += score
    if total <= max(12.0, width * (last - first) * 0.02):
        return None
    return sum(x * value for x, value in enumerate(columns)) / total


def _fit(values: Sequence[Optional[float]]):
    points = [(index, value) for index, value in enumerate(values) if value is not None]
    if len(points) < 3:
        return {"valid": len(points), "slope": None, "r2": None, "driftPixels": None}
    mean_x = sum(x for x, _ in points) / len(points)
    mean_y = sum(y for _, y in points) / len(points)
    denominator = sum((x - mean_x) ** 2 for x, _ in points)
    if denominator <= 0:
        return {"valid": len(points), "slope": 0.0, "r2": 0.0, "driftPixels": 0.0}
    slope = sum((x - mean_x) * (y - mean_y) for x, y in points) / denominator
    intercept = mean_y - slope * mean_x
    residual = sum((y - (slope * x + intercept)) ** 2 for x, y in points)
    total = sum((y - mean_y) ** 2 for _, y in points)
    r2 = 1.0 if total <= 1.0e-9 and abs(slope) > 0 else (0.0 if total <= 1.0e-9 else max(0.0, 1.0 - residual / total))
    return {
        "valid": len(points),
        "slope": slope,
        "r2": r2,
        "driftPixels": abs(slope) * (len(values) - 1),
    }


def _screenshot_paths(root: Path) -> list[Path]:
    numbered = []
    for path in root.glob("ScreenShot*.bmp"):
        match = re.search(r"ScreenShot(\d+)\.bmp$", path.name, re.IGNORECASE)
        numbered.append((int(match.group(1)) if match else 10**9, path.name, path))
    return [path for _, _, path in sorted(numbered)]


def _analyze_legacy_screenshots(root: Path) -> dict:
    paths = _screenshot_paths(root)
    if len(paths) < 4:
        return {"status": "unavailable", "reason": "at least four ordered ScreenShot*.bmp files are required", "samples": len(paths)}
    samples = [_read_bmp(path) for path in paths]
    if any(sample is None for sample in samples):
        bad = [path.name for path, sample in zip(paths, samples) if sample is None]
        return {"status": "fail", "reason": "invalid BMP evidence", "invalid": bad, "samples": len(paths)}
    windows = ((0.55, 0.72), (0.72, 0.95), (0.55, 0.95))
    measurements = []
    for top, bottom in windows:
        series = [_centroid(sample, top, bottom) for sample in samples]
        fit = _fit(series)
        fit.update({"top": top, "bottom": bottom, "centroids": series})
        measurements.append(fit)
    usable = [item for item in measurements if item["valid"] >= 4 and item["slope"] is not None]
    if not usable:
        return {"status": "unavailable", "reason": "no measurable blue/cyan reflection band", "samples": len(paths), "windows": measurements}
    best = max(usable, key=lambda item: (item["driftPixels"] or 0.0) * (item["r2"] or 0.0))
    width = samples[0]["width"]
    drift_threshold = max(10.0, width * 0.08)
    moving = (best["driftPixels"] or 0.0) >= drift_threshold and (best["r2"] or 0.0) >= 0.45
    return {
        "status": "fail" if moving else "pass",
        "reason": "ordered reflection band drifts with the screenshot sequence" if moving else "no significant ordered horizontal reflection drift",
        "samples": len(paths),
        "width": width,
        "driftThresholdPixels": drift_threshold,
        "best": best,
        "windows": measurements,
        "files": [path.name for path in paths],
    }


def _region_pixels(sample, region):
    left, top, right, bottom = region
    width, height = sample["width"], sample["height"]
    return [(x, y) for y in range(int(height * top), int(height * bottom))
            for x in range(int(width * left), int(width * right))]


def _load_water_regions(root, internal):
    """Use reviewed water rectangles bound to the exact captured image bytes.

    Colour selection would silently omit black/missing reflections. A rectangle
    includes every pixel, including failures, and must be reviewed per capture.
    """
    try:
        document = json.loads((root / "water-regions.json").read_text(encoding="utf-8-sig"))
        regions = {}
        for key, path in internal.items():
            entry = document[path.name]
            region = entry["rect"]
            if (len(region) != 4 or any(isinstance(v, bool) or not isinstance(v, (int, float))
                                      or not math.isfinite(v) for v in region)
                    or not (0 <= region[0] < region[2] <= 1
                            and 0 <= region[1] < region[3] <= 1)):
                raise ValueError("invalid normalized rectangle")
            if hashlib.sha256(path.read_bytes()).hexdigest() != entry["sha256"]:
                raise ValueError("water region belongs to a different image")
            regions[key] = region
        return regions, None
    except (OSError, ValueError, TypeError, KeyError) as error:
        return None, str(error)


def _water_metrics(sample, region) -> dict:
    width, height, rows = sample["width"], sample["height"], sample["rows"]
    coordinates = _region_pixels(sample, region)
    pixels = [rows[y][x] for x, y in coordinates]
    luminance = [sum(pixel) / 3.0 for pixel in pixels]
    horizontal_edges = [
        abs(sum(rows[y][x]) / 3.0 - sum(rows[y][x - 1]) / 3.0)
        for x, y in coordinates if x > int(width * region[0])
    ]
    black = sum(1 for pixel in pixels if max(pixel) < 16) / max(1, len(luminance))
    return {
        "pixelCount": len(pixels),
        "blackFraction": black,
        "contrast": statistics.pstdev(luminance) if len(luminance) > 1 else 0.0,
        "edgeEnergy": statistics.fmean(horizontal_edges) if horizontal_edges else 0.0,
    }


def _water_difference(first, second, region) -> float:
    height = min(first["height"], second["height"])
    width = min(first["width"], second["width"])
    differences = []
    for x, y in _region_pixels({"width": width, "height": height}, region):
        a, b = first["rows"][y][x], second["rows"][y][x]
        differences.append(sum(abs(a[channel] - b[channel]) for channel in range(3)) / 3.0)
    return statistics.fmean(differences) if differences else 0.0


def _internal_water_paths(root: Path):
    pattern = re.compile(
        r"OBVR-VRTest-water-view-(\d+)-step-(\d+)-([LR])\.bmp$",
        re.IGNORECASE,
    )
    result = {}
    for path in root.glob("OBVR-VRTest-water-view-*.bmp"):
        match = pattern.match(path.name)
        if match:
            result[(int(match.group(1)), int(match.group(2)), match.group(3).upper())] = path
    return result


def analyze_screenshots(root: Path) -> dict:
    if (root / "water-alignment-region.json").exists():
        from water_alignment_diagnostic import analyze_alignment
        return analyze_alignment(root)
    internal = _internal_water_paths(root)
    if not internal:
        return _analyze_legacy_screenshots(root)
    views = sorted({key[0] for key in internal})
    eyes = ("L", "R")
    if len(views) < 5:
        return {
            "status": "fail",
            "reason": "fewer than five synthetic HMD yaw views were captured",
            "samples": len(internal),
            "views": views,
        }
    missing = []
    decoded = {}
    for view in views:
        steps = sorted({key[1] for key in internal if key[0] == view})
        if len(steps) != 2:
            missing.append({"view": view, "reason": "expected exactly two wave-time samples"})
            continue
        for step in steps:
            for eye in eyes:
                key = (view, step, eye)
                if key not in internal:
                    missing.append({"view": view, "step": step, "eye": eye})
                    continue
                decoded[key] = _read_bmp(internal[key])
                if decoded[key] is None:
                    missing.append({"view": view, "step": step, "eye": eye, "reason": "invalid BMP"})
    if missing:
        return {"status": "fail", "reason": "incomplete synthetic HMD screenshot grid", "missing": missing}

    regions, region_error = _load_water_regions(root, internal)
    if regions is None:
        return {"status": "unavailable", "reason": "reviewed water regions required",
                "regionError": region_error, "samples": len(decoded)}
    metrics = {key: _water_metrics(sample, regions[key]) for key, sample in decoded.items()}
    black_failures = [key for key, value in metrics.items() if value["blackFraction"] >= 0.20]
    detail_failures = [
        key for key, value in metrics.items()
        if value["pixelCount"] < 32 or value["contrast"] < 4.0 or value["edgeEnergy"] < 0.35
    ]
    wave_differences = {}
    for view in views:
        steps = sorted({key[1] for key in decoded if key[0] == view})
        for eye in eyes:
            first_key, second_key = (view, steps[0], eye), (view, steps[1], eye)
            if regions[first_key] != regions[second_key]:
                return {"status": "fail", "reason": "wave pair water regions differ"}
            if (decoded[first_key]["width"], decoded[first_key]["height"]) != (
                    decoded[second_key]["width"], decoded[second_key]["height"]):
                return {"status": "fail", "reason": "wave pair image dimensions differ"}
            wave_differences[(view, eye)] = _water_difference(
                decoded[first_key], decoded[second_key], regions[first_key]
            )
    moving_pairs = sum(value >= 0.75 for value in wave_differences.values())
    required_moving_pairs = math.ceil(len(wave_differences) * 0.75)
    passed = (
        not black_failures
        and not detail_failures
        and moving_pairs >= required_moving_pairs
    )
    return {
        # Moving texture and contrast do not prove world-locked reflections.
        "status": "unavailable" if passed else "fail",
        "waterActivityStatus": "pass" if passed else "fail",
        "worldAlignmentStatus": "unavailable",
        "reason": (
            "water activity measured; cross-view world alignment remains unverified"
            if passed
            else "black coverage, missing reflected detail, or frozen wave pixels were detected"
        ),
        "samples": len(decoded),
        "views": views,
        "blackFailures": black_failures,
        "detailFailures": detail_failures,
        "waveMovingPairs": moving_pairs,
        "waveRequiredPairs": required_moving_pairs,
        "waveDifferences": {f"{view}-{eye}": value for (view, eye), value in wave_differences.items()},
        "metrics": {f"{view}-{step}-{eye}": value for (view, step, eye), value in metrics.items()},
        "files": [path.name for path in internal.values()],
    }


def analyze_runtime_log(log_path: Path) -> dict:
    try:
        text = log_path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return {"status": "unavailable", "reason": "OBVR.log is missing"}
    startup_text = text
    # An attached process can contain several sweeps at different player poses.
    # Never combine their camera ranges or accept a preceding sweep's verdict.
    starts = list(re.finditer(r"VRTEST water runner armed schema=21", text))
    if starts:
        text = text[starts[-1].start():]
    captures = {"left": [], "right": []}
    for match in _CAPTURE_LINE.finditer(text):
        captures[match.group(1)].append(tuple(float(match.group(i)) for i in range(2, 5)))
    rows = {"left": [], "right": []}
    for match in _MATRIX_ROW0.finditer(text):
        rows[match.group(1)].append(tuple(float(match.group(i)) for i in range(2, 6)))
    queue_counts = [
        (int(match.group(1)), int(match.group(2)))
        for match in _QUEUE_LINE.finditer(text)
    ]
    scales = [
        (float(match.group(1)), float(match.group(2)))
        for match in _SCALE_LINE.finditer(text)
    ]
    fallback = bool(re.search(r"VRTEST water-image .*?fallback=[1-9]", text))
    markers = {
        "originalTargetHook": "original-target camera/projection hook installed" in startup_text,
        "originalPixelPath": "verified original-pixel-path vertex shaders created" in startup_text,
        "stableCaptureRendered": "head-independent camera rendered into the original target" in startup_text,
        "projectionUploaded": "original target projection matrix uploaded" in startup_text,
        "toggleLifecycleRecovered": "VRTEST lifecycle status=pass suppressed=1 recovered=1" in text,
        "sweepPassed": bool(_SWEEP_PASS_LINE.search(text)),
    }
    capture_samples = sum(len(values) for values in captures.values())
    if capture_samples < 20:
        return {
            "status": "unavailable",
            "reason": "complete synthetic HMD water sweep evidence is missing",
            "captureSamples": capture_samples,
            "markers": markers,
        }
    capture_ranges = {}
    for eye, values in captures.items():
        if values:
            capture_ranges[eye] = [
                max(point[i] for point in values) - min(point[i] for point in values)
                for i in range(3)
            ]
    rotation_ranges = {}
    for eye, values in rows.items():
        if values:
            rotation_ranges[eye] = [
                max(point[i] for point in values) - min(point[i] for point in values)
                for i in range(3)
            ]
    stable_capture = bool(capture_ranges) and all(
        max(ranges) <= 1.5 for ranges in capture_ranges.values()
    )
    # The current runtime compares all nine camera-rotation components at
    # every eye/time/view sample. Older logs additionally contain matrix rows.
    # Missing rows are not a parser failure; require the complete runtime mask.
    masks = list(_FIXED_CAPTURE_MASK.finditer(text))
    rotation_mask_complete = bool(masks) and all(
        int(match.group(1), 16) == int(match.group(2), 16) == 0x0FFFFFFF
        for match in masks
    )
    stable_rotation = rotation_mask_complete and all(
        max(ranges) <= 0.02 for ranges in rotation_ranges.values()
    )
    queues_valid = bool(queue_counts) and all(
        stored > 0 or replayed > 0 for stored, replayed in queue_counts
    )
    scales_valid = bool(scales) and all(
        abs(horizontal - 3.0) <= 0.01 and abs(vertical - 1.5) <= 0.01
        for horizontal, vertical in scales
    )
    status = "pass" if (
        all(markers.values()) and stable_capture and stable_rotation and
        queues_valid and scales_valid and not fallback
    ) else "fail"
    world_projection = analyze_world_projection(text)
    if world_projection["status"] != "pass":
        status = "fail"
    return {
        "status": status,
        "reason": (
            "original target, fixed capture camera, widened projection, and stereo reuse passed"
            if status == "pass"
            else "runtime evidence contradicts or does not complete the stable original-target path"
        ),
        "captureSamples": capture_samples,
        "worldProjection": world_projection,
        "captureRanges": capture_ranges,
        "rotationRanges": rotation_ranges,
        "rotationMaskComplete": rotation_mask_complete,
        "fallbackObserved": fallback,
        "queueCounts": queue_counts,
        "scales": scales,
        "markers": markers,
    }


def replay_armed_after_settling(runner: str) -> bool:
    """Source-order regression guard; runtime loading still requires a live test."""
    phases = [runner.find(token) for token in (
        'WriteAllText($pluginIni, $initialIni',
        'Start-Sleep -Seconds $LoadWaitSec',
        'WriteAllText($pluginIni, $testIni',
    )]
    return (all(position >= 0 for position in phases)
            and phases[0] < phases[1] < phases[2]
            and "$settlingIni = [regex]::Replace" in runner
            and '$initialIni = if ($ArmBeforeLoad) { $testIni } else { $settlingIni }' in runner
            and 'if (-not $ArmBeforeLoad) {' in runner
            and "'VRTestSuite=0'" in runner
            and 'if ($tail -match "^OBVR ready$")' in runner)


def shader_contract(source_root: Path) -> dict:
    hook_path = source_root / "src" / "render" / "WaterReflectionHook.cpp"
    reprojection_path = source_root / "src" / "render" / "WaterReprojection.cpp"
    vertex_path = source_root / "src" / "render" / "WaterReprojection.h"
    interface_path = source_root / "src" / "render" / "InterfaceRenderHook.cpp"
    runtime_path = source_root / "src" / "test" / "WaterVRTestRuntime.cpp"
    plan_path = source_root / "src" / "test" / "WaterVRTestPlan.h"
    runner_path = source_root / "tools" / "water-vr-run.ps1"
    try:
        hook = hook_path.read_text(encoding="utf-8")
        reprojection = reprojection_path.read_text(encoding="utf-8")
        vertex = vertex_path.read_text(encoding="utf-8")
        interface = interface_path.read_text(encoding="utf-8")
        runtime = runtime_path.read_text(encoding="utf-8")
        plan = plan_path.read_text(encoding="utf-8")
        runner = runner_path.read_text(encoding="utf-8")
    except OSError as error:
        return {"status": "fail", "reason": f"source contract unreadable: {error}"}
    checks = {
        "originalPixelShaderPreserved": (
            "return g_originalSetPixelShader(self, shader);" in interface
            and "SelectWaterReflectionPixelShader(self" not in interface
        ),
        "originalReflectionTargetPreserved": (
            "head-independent camera rendered into the original target" in hook
            and "StoreWaterReflectionTarget" not in hook
            # The target probe observes/AddRefs the native surface; it does
            # not replace the target. Saving a copy for substitution is distinct.
        ),
        "singleEngineCapture": (
            "BuildWaterCameraLocalFromParent(captureParent, renderWorld, captureLocal)" in hook
            and "WaterRenderCameraFromBody(stableWorld)" in hook
            and "RestoreCameraTransforms restore(camera, captureLocal);" in hook
            and "for (unsigned slot" not in hook
        ),
        "wideCaptureProjectionPaired": (
            "kHorizontalCaptureScale = 3.0f" in hook
            and "kVerticalCaptureScale = 1.5f" in hook
            and "ScaleWaterProjectionRows(projected[slot]" in reprojection
        ),
        "failClosedShaderSelection": (
            "ShouldSelectStableWaterVertexShader(" in reprojection
            and "return requested;" in reprojection
        ),
        "nativeVertexUsesCaptureRows": all(
            token in vertex for token in
            ("0xA0E4000D", "0xA0E4000E", "0xA0E4000F", "0xA0E40010")
        ),
        "fullCaptureMvpBuilderUsed": (
            "BuildStableWaterCaptureMvp(current, world, live, g_capture[slot]" in reprojection
        ),
        "stereoProjectionQueueBounded": (
            "kMaxStoredWaterDraws = 2048" in reprojection
            and "CanStoreWaterCaptureMvp" in reprojection
            and "CanReplayWaterCaptureMvp" in reprojection
        ),
        "automaticYawSweepCapturesWavePairs": (
            "kWaterTestViews = 7" in plan
            and "kWaterTestCaptureFrames[2]" in plan
            and "OBVR-VRTest-water-view-%u-step-%u-%s.bmp" in runtime
        ),
        "manualStartAttachCanArmRunner": (
            "ChooseWaterTestEnableAction(requested, g_enabled, g_active)" in runtime
            and "$startedMarker = $false" in runner
            and 'VRTEST water runner armed schema=21' in runner
            and 'if ($Attach) {' in runner
            and 'if (-not $AlreadyInWorld) {' in runner
            and 'Using the already-loaded world; no menu input will be sent.' in runner
            and 'Waiting for the in-world config reload to arm the water runner' in runner
            and '$statusMatch = [regex]::Match' in runner
            and '$statusMatch.Groups[1].Value' in runner
            and 'Oblivion water reflections are disabled:' in runner
            and 'bUseWaterReflectionsTrees' in runner
            and 'bUseWaterReflectionsStatics' in runner
            and runner.index('Selecting Load') < runner.index(
                'Waiting for the in-world config reload to arm the water runner'
            )
        ),
        "replayArmedAfterSettling": replay_armed_after_settling(runner),
        "loaderRedirectRefusesLauncherFallback": (
            "OBSE redirected to OblivionLauncher; refusing launcher fallback." in runner
            and "if (Click-LauncherPlay $launcher)" not in runner
            and "click Play manually" not in runner
        ),
    }
    files = [str(path) for path in (
        hook_path, reprojection_path, vertex_path, interface_path, runtime_path,
        plan_path, runner_path
    )]
    return {"status": "pass" if all(checks.values()) else "fail", "checks": checks, "files": files}


def analyze_run_manifest(root: Path) -> dict:
    path = root / "water-vr-result.json"
    try:
        manifest = json.loads(path.read_text(encoding="utf-8-sig"))
    except OSError:
        return {"status": "unavailable", "reason": "water-vr-result.json is missing"}
    except (TypeError, ValueError) as error:
        return {"status": "fail", "reason": f"invalid water-vr-result.json: {error}"}
    schema = manifest.get("schema")
    save_index = manifest.get("saveIndex")
    capture_count = manifest.get("captureCount")
    sweep = manifest.get("sweep")
    files = manifest.get("files")
    if schema != 2:
        return {
            "status": "fail",
            "reason": "the run manifest is not the synthetic water-sweep schema",
            "schema": schema,
        }
    if save_index != 1:
        return {
            "status": "fail",
            "reason": "the run did not select the second save entry",
            "saveIndex": save_index,
        }
    if capture_count != 28:
        return {
            "status": "fail",
            "reason": "the run did not capture the complete 7-view, 2-step, 2-eye grid",
            "captureCount": capture_count,
        }
    if sweep != "synthetic-hmd-yaw--60-to-60":
        return {
            "status": "fail",
            "reason": "the run did not use the required synthetic HMD yaw sweep",
            "sweep": sweep,
        }
    if not isinstance(files, list) or len(files) != capture_count:
        return {
            "status": "fail",
            "reason": "capture manifest does not match captureCount",
            "captureCount": capture_count,
            "files": files,
        }
    return {
        "status": "pass",
        "reason": "second-save water run manifest is complete",
        "saveIndex": save_index,
        "captureCount": capture_count,
        "saveEntry": manifest.get("saveEntry"),
    }


def evaluate(source_root: Path, artifact_dir: Optional[Path] = None) -> dict:
    contract = shader_contract(source_root)
    result = {"status": contract["status"], "contract": contract}
    if artifact_dir is not None:
        manifest = analyze_run_manifest(artifact_dir)
        visual = analyze_screenshots(artifact_dir)
        runtime = analyze_runtime_log(artifact_dir / "OBVR.log")
        result["manifest"] = manifest
        result["visual"] = visual
        result["runtime"] = runtime
        if (
            manifest["status"] != "pass"
            or visual["status"] != "pass"
            or runtime["status"] != "pass"
        ):
            result["status"] = "fail"
    return result


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--artifact-dir", type=Path)
    parser.add_argument("--json", type=Path, help="also write the result to this JSON file")
    args = parser.parse_args(argv)
    result = evaluate(args.source_root, args.artifact_dir)
    encoded = json.dumps(result, indent=2, sort_keys=True)
    print(encoded)
    if args.json:
        args.json.write_text(encoded + "\n", encoding="utf-8")
    return 0 if result["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
