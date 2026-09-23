#!/usr/bin/env python3
"""Validate and summarize one OBVR performance capture.

The tool is intentionally offline: it reads the four files emitted by the
profiler and never touches the game, OpenVR, or the graphics device.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import statistics
from pathlib import Path
from typing import Any, Iterable

SCHEMA_VERSION = 1
EVENT_COLUMNS = {
    "event_id", "event_type", "present_id", "scene_id", "vr_frame_id", "pass_index",
    "eye", "mode", "start_tick", "end_tick", "status",
}
FRAME_COLUMNS = {
    "present_id", "vr_frame_id", "start_tick", "end_tick", "interval_tick",
    "requested_mode", "delivered_mode", "pose_result", "left_captured", "right_captured",
    "pass_count", "setup", "status", "submit_left", "submit_right",
}
GPU_COLUMNS = {
    "sample_id", "present_id", "vr_frame_id", "event_type", "pass_index", "eye",
    "status", "start_tick", "end_tick", "frequency", "age_presents",
}


class CaptureError(ValueError):
    pass


def _finite_number(value: str, field: str) -> float:
    try:
        number = float(value)
    except (TypeError, ValueError) as exc:
        raise CaptureError(f"invalid {field}: {value!r}") from exc
    if not math.isfinite(number):
        raise CaptureError(f"non-finite {field}")
    return number


def _uint(value: str, field: str) -> int:
    number = _finite_number(value, field)
    if number < 0 or number != int(number):
        raise CaptureError(f"invalid unsigned {field}: {value!r}")
    return int(number)


def _read_csv(path: Path, required: set[str]) -> list[dict[str, str]]:
    try:
        with path.open(newline="", encoding="utf-8") as handle:
            reader = csv.DictReader(handle)
            fields = set(reader.fieldnames or [])
            missing = required - fields
            if missing:
                raise CaptureError(f"{path.name}: missing columns {sorted(missing)}")
            return list(reader)
    except OSError as exc:
        raise CaptureError(f"cannot read {path}: {exc}") from exc


def _unique(rows: Iterable[dict[str, str]], field: str, source: str) -> None:
    seen: set[int] = set()
    for row in rows:
        value = _uint(row[field], f"{source}.{field}")
        if value in seen:
            raise CaptureError(f"duplicate {source} id {value}")
        seen.add(value)


def _nearest_rank(values: list[float], percentile: float) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    rank = max(1, math.ceil(percentile / 100.0 * len(ordered)))
    return ordered[rank - 1]


def _stats(values: list[float]) -> dict[str, Any]:
    if not values:
        return {"count": 0, "mean_ms": None, "median_ms": None,
                "p95_ms": None, "p99_ms": None, "max_ms": None}
    return {
        "count": len(values),
        "mean_ms": statistics.fmean(values),
        "median_ms": statistics.median(values),
        "p95_ms": _nearest_rank(values, 95.0),
        "p99_ms": _nearest_rank(values, 99.0),
        "max_ms": max(values),
    }


def _duration(row: dict[str, str], frequency: int, start: str = "start_tick",
              end: str = "end_tick") -> float | None:
    if not row[start] or not row[end] or row.get("status", "") not in ("0", "ready", "complete"):
        return None
    first = _uint(row[start], start)
    last = _uint(row[end], end)
    if last < first or frequency <= 0:
        return None
    return (last - first) * 1000.0 / frequency


def analyze(capture: Path) -> dict[str, Any]:
    try:
        manifest = json.loads((capture / "manifest.json").read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise CaptureError(f"invalid manifest.json: {exc}") from exc
    if manifest.get("schema_version") != SCHEMA_VERSION:
        raise CaptureError(f"unsupported schema_version {manifest.get('schema_version')!r}")
    if manifest.get("status", "complete") not in ("complete", "truncated_capacity"):
        raise CaptureError(f"capture is not complete: {manifest.get('status')!r}")
    session_id = _uint(str(manifest.get("session_id", "")), "manifest.session_id")
    frequency = _uint(str(manifest.get("qpc_frequency", "")), "manifest.qpc_frequency")
    if frequency == 0:
        raise CaptureError("manifest.qpc_frequency must be positive")

    events = _read_csv(capture / "cpu_events.csv", EVENT_COLUMNS)
    frames = _read_csv(capture / "frames.csv", FRAME_COLUMNS)
    gpu = _read_csv(capture / "gpu_samples.csv", GPU_COLUMNS)
    _unique(events, "event_id", "event")
    _unique(gpu, "sample_id", "gpu")

    for row in events + frames + gpu:
        for key in ("present_id", "vr_frame_id"):
            _uint(row[key], f"{key}")

    cpu_by_type: dict[str, list[float]] = {}
    cpu_by_mode: dict[str, list[float]] = {}
    dual_passes: dict[tuple[int, int], dict[int, list[float]]] = {}
    for row in events:
        value = _duration(row, frequency)
        if value is None:
            continue
        cpu_by_type.setdefault(row["event_type"], []).append(value)
        cpu_by_mode.setdefault(row["mode"], []).append(value)
        if row["event_type"] == "scene_pass" and row["mode"] == "world_dual" and row["pass_index"] in ("0", "1"):
            key = (_uint(row["present_id"], "present_id"), _uint(row["scene_id"], "scene_id"))
            dual_passes.setdefault(key, {}).setdefault(int(row["pass_index"]), []).append(value)

    frame_intervals = []
    for row in frames:
        if row["interval_tick"]:
            ticks = _uint(row["interval_tick"], "interval_tick")
            frame_intervals.append(ticks * 1000.0 / frequency)

    gpu_by_type: dict[str, list[float]] = {}
    frame_modes = {
        _uint(row["present_id"], "present_id"): row["delivered_mode"]
        for row in frames
        if row["delivered_mode"]
    }
    gpu_by_mode: dict[str, list[float]] = {}
    for row in gpu:
        if row["status"] != "ready":
            continue
        gpu_frequency = _uint(row["frequency"], "gpu.frequency")
        if gpu_frequency == 0:
            continue
        value = _duration(row, gpu_frequency)
        if value is not None:
            gpu_by_type.setdefault(row["event_type"], []).append(value)
            gpu_by_mode.setdefault(frame_modes.get(_uint(row["present_id"], "present_id"), "unknown"), []).append(value)

    paired = [passes for passes in dual_passes.values()
              if len(passes.get(0, [])) == 1 and len(passes.get(1, [])) == 1]
    pass0 = [passes[0][0] for passes in paired]
    pass1 = [passes[1][0] for passes in paired]
    deltas = [right - left for left, right in zip(pass0, pass1)]

    return {
        "schema_version": SCHEMA_VERSION,
        "session_id": session_id,
        "source": str(capture),
        "runtime_measurement": bool(manifest.get("runtime_measurement", False)),
        "frames": {"count": len(frames), "present_interval": _stats(frame_intervals)},
        "cpu_wall_ms": {key: _stats(value) for key, value in sorted(cpu_by_type.items())},
        "cpu_wall_by_mode_ms": {key: _stats(value) for key, value in sorted(cpu_by_mode.items())},
        "gpu_interval_ms": {key: _stats(value) for key, value in sorted(gpu_by_type.items())},
        "gpu_interval_by_mode_ms": {key: _stats(value) for key, value in sorted(gpu_by_mode.items())},
        "dual_pass_compare": {
            "paired_frames": len(paired),
            "pass_0_ms": _stats(pass0),
            "pass_1_ms": _stats(pass1),
            "pass_1_minus_pass_0_ms": _stats(deltas),
        },
        "gpu_status_counts": {
            status: sum(1 for row in gpu if row["status"] == status)
            for status in sorted({row["status"] for row in gpu})
        },
        "diagnosis": {
            "classification": "observation_only",
            "text": "Synthetische oder unvollständige Messungen beweisen keinen CPU- oder GPU-Bottleneck.",
        },
    }


def _markdown(summary: dict[str, Any]) -> str:
    lines = [
        "# OBVR Performance Report",
        "",
        f"Session `{summary['session_id']}`; runtime_measurement=`{summary['runtime_measurement']}`.",
        "",
        "Die Werte sind Wall-Zeit beziehungsweise GPU-Intervalle. Verschachtelte CPU-Spannen "
        "werden nicht addiert. Die Diagnose bleibt Beobachtung → Hypothese → Gegenprüfung.",
        "",
        "## CPU-Wall-Spannen",
        "",
        "| Ereignis | n | Mittel | Median | p95 | p99 | Max |",
        "|---|---:|---:|---:|---:|---:|---:|",
    ]
    for name, stats in summary["cpu_wall_ms"].items():
        lines.append("| %s | %s | %s | %s | %s | %s | %s |" % (
            name, stats["count"], *_fmt_stats(stats)))
    lines += ["", "## GPU-Intervalle", "", "| Marker | n | Mittel | Median | p95 | p99 | Max |",
              "|---|---:|---:|---:|---:|---:|---:|"]
    for name, stats in summary["gpu_interval_ms"].items():
        lines.append("| %s | %s | %s | %s | %s | %s | %s |" % (
            name, stats["count"], *_fmt_stats(stats)))
    lines += ["", "## Aussage", "", summary["diagnosis"]["text"], ""]
    compare = summary["dual_pass_compare"]
    lines += ["## Gepaarter Dual-Pass-Vergleich", "",
              f"Gültige Paare: `{compare['paired_frames']}`. Nur gleiche Present-/Scene-IDs "
              "mit genau einem Pass 0 und Pass 1 werden verglichen.", ""]
    return "\n".join(lines)


def _fmt_stats(stats: dict[str, Any]) -> tuple[str, str, str, str, str]:
    def fmt(value: Any) -> str:
        return "—" if value is None else f"{value:.3f} ms"
    return tuple(fmt(stats[key]) for key in ("mean_ms", "median_ms", "p95_ms", "p99_ms", "max_ms"))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        summary = analyze(args.capture)
    except CaptureError as exc:
        parser.error(str(exc))
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    (args.output / "report.md").write_text(_markdown(summary), encoding="utf-8")
    print(args.output / "report.md")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
