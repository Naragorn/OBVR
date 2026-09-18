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
import json
import math
import re
import struct
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
_CACHE_REUSE_LINE = re.compile(
    r"Water reflection: cached captures reused while body camera is unchanged"
)


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


def analyze_screenshots(root: Path) -> dict:
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


def analyze_runtime_log(log_path: Path) -> dict:
    try:
        text = log_path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return {"status": "unavailable", "reason": "OBVR.log is missing"}
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
    cache_reuse_count = len(_CACHE_REUSE_LINE.findall(text))
    fallback = bool(re.search(r"VRTEST water-image .*?fallback=[1-9]", text))
    capture_samples = sum(len(values) for values in captures.values())
    if capture_samples < 4:
        if cache_reuse_count > 0 and not fallback:
            return {
                "status": "pass",
                "reason": "runtime reported cached captures reused with no fallback",
                "captureSamples": capture_samples,
                "cacheReuseCount": cache_reuse_count,
                "fallbackObserved": fallback,
                "queueCounts": queue_counts,
            }
        return {
            "status": "unavailable",
            "reason": "VRTEST water-image capture evidence is missing",
            "captureSamples": capture_samples,
            "cacheReuseCount": cache_reuse_count,
        }
    capture_ranges = {}
    for eye, values in captures.items():
        if values:
            capture_ranges[eye] = [max(point[i] for point in values) - min(point[i] for point in values) for i in range(3)]
    rotation_ranges = {}
    for eye, values in rows.items():
        if values:
            rotation_ranges[eye] = [max(point[i] for point in values) - min(point[i] for point in values) for i in range(3)]
    stable_capture = all(max(ranges) <= 1.5 for ranges in capture_ranges.values())
    stable_rotation = all(max(ranges) <= 0.02 for ranges in rotation_ranges.values()) if rotation_ranges else False
    queue_invalid = bool(queue_counts) and any(
        stored <= 0 and replayed <= 0 for stored, replayed in queue_counts
    )
    status = "pass" if stable_capture and stable_rotation and not fallback and not queue_invalid else "fail"
    return {
        "status": status,
        "reason": "capture transform and camera-only matrix rotation stay stable" if status == "pass" else "capture transform, matrix rotation, or fallback evidence is unstable",
        "captureSamples": capture_samples,
        "cacheReuseCount": cache_reuse_count,
        "captureRanges": capture_ranges,
        "rotationRanges": rotation_ranges,
        "fallbackObserved": fallback,
        "queueCounts": queue_counts,
        "queueEvidenceInvalid": queue_invalid,
    }


def shader_contract(source_root: Path) -> dict:
    shader_path = source_root / "src" / "render" / "WaterReflectionBlendShader.inc"
    reprojection_path = source_root / "src" / "render" / "WaterReprojection.cpp"
    vertex_path = source_root / "src" / "render" / "WaterReprojection.h"
    blend_path = source_root / "src" / "render" / "WaterReflectionBlend.cpp"
    try:
        shader = shader_path.read_text(encoding="utf-8")
        reprojection = reprojection_path.read_text(encoding="utf-8")
        vertex = vertex_path.read_text(encoding="utf-8")
        blend = blend_path.read_text(encoding="utf-8")
    except OSError as error:
        return {"status": "fail", "reason": f"source contract unreadable: {error}"}
    checks = {
        "worldInterpolantDeclared": "dcl_centroid t1.xyz" in shader,
        "localPositionInterpolantDeclared": "dcl_centroid t0.xyz" in shader,
        "worldInterpolantConsumed": "add r1.xyz, -t1, c1\\n" in shader,
        "localPositionInterpolantConsumed": "mov r1.xyz, t0\\n" in shader,
        "diagnosticLocalPositionConsumed": "mov r1.xyz, t0\\n" in blend,
        "fullCaptureProjectionConstants": all(f"c{register}" in shader for register in range(13, 25)),
        "legacyReflectionInterpolantsAbsent": not re.search(r"\b(?:t[2-5]|oT[2-5])(?:\.|\b)", shader),
        "centerCaptureIsSoleReflectionSample": (
            "texldp r3, r6, s2\\n" in shader
            and "mov r0.xyz, r3\\n" in shader
            and "texldp r0, r5, s0\\n" not in shader
            and "texldp r2, r7, s1\\n" not in shader
        ),
        "fullCaptureMvpBuilderUsed": "BuildStableWaterCaptureMvp(current, world, live, g_capture[slot]" in reprojection,
        "storedMatricesFullMvp": "g_captureWaterMvp[g_captureWaterMvpCount][slot] = projected[slot]" in reprojection,
        "reuseIgnoresLaterWorldMat": "g_captureWaterMvp[g_captureWaterMvpReplay][slot]" in reprojection and "CopyStoredWaterCaptureMvp" in reprojection,
        "boundedPerDrawReplayQueue": "kMaxStoredWaterDraws = 2048" in reprojection and "CanStoreWaterCaptureMvp" in reprojection and "CanReplayWaterCaptureMvp" in reprojection,
        "nativeVertexKeepsLiveW": vertex.count("const UInt32 replacements[6] = {") == 2 and "0x00000001,0xE00F0000,0x90E40000,0,0" in vertex and "0x90E40000};" in vertex,
    }
    return {"status": "pass" if all(checks.values()) else "fail", "checks": checks, "files": [str(shader_path), str(reprojection_path), str(vertex_path)]}


def analyze_run_manifest(root: Path) -> dict:
    path = root / "water-vr-result.json"
    try:
        manifest = json.loads(path.read_text(encoding="utf-8"))
    except OSError:
        return {"status": "unavailable", "reason": "water-vr-result.json is missing"}
    except (TypeError, ValueError) as error:
        return {"status": "fail", "reason": f"invalid water-vr-result.json: {error}"}
    save_index = manifest.get("saveIndex")
    capture_count = manifest.get("captureCount")
    files = manifest.get("files")
    if save_index != 1:
        return {
            "status": "fail",
            "reason": "the run did not select the second save entry",
            "saveIndex": save_index,
        }
    if not isinstance(capture_count, int) or capture_count < 4:
        return {
            "status": "fail",
            "reason": "the run captured fewer than four ordered views",
            "captureCount": capture_count,
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
            or runtime["status"] not in ("pass", "unavailable")
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
