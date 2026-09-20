"""Fail-closed validator for a live main-menu recenter -> save-load run."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Optional, Sequence


RECENTER = re.compile(r"Camera: recentered on key 0x2E \(frame \d+, flat path\)")
WORLD = re.compile(r"Render: on a world frame Oblivion draws")


def analyze_manifest(root: Path) -> dict:
    """Validate receipt fields and their ordering in the copied runtime log."""

    path = root / "recenter-load-result.json"
    try:
        manifest = json.loads(path.read_text(encoding="utf-8-sig"))
    except OSError:
        return {"status": "unavailable", "reason": "recenter-load-result.json is missing"}
    except (TypeError, ValueError) as error:
        return {"status": "fail", "reason": f"invalid recenter receipt: {error}"}

    if manifest.get("schema") != 1 or manifest.get("mode") != "live-recenter-load":
        return {"status": "fail", "reason": "recenter receipt has the wrong schema or mode"}
    recenter = manifest.get("recenterEvidence")
    world = manifest.get("worldEvidence")
    if not isinstance(recenter, dict) or recenter.get("requested") is not True:
        return {"status": "fail", "reason": "recenter receipt has no requested-key evidence"}
    if recenter.get("flatPath") is not True or recenter.get("beforeWorldFrame") is not True:
        return {"status": "fail", "reason": "recenter receipt does not prove a pre-world flat recenter"}
    if not isinstance(world, dict) or world.get("loaded") is not True:
        return {"status": "fail", "reason": "recenter receipt has no loaded-world evidence"}

    try:
        log = (root / "OBVR.log").read_text(encoding="utf-8", errors="replace")
    except OSError:
        return {"status": "fail", "reason": "recenter receipt is missing OBVR.log"}
    recenter_match = RECENTER.search(log)
    world_match = WORLD.search(log)
    if not recenter_match:
        return {"status": "fail", "reason": "runtime log has no flat recenter marker"}
    if not world_match:
        return {"status": "fail", "reason": "runtime log has no post-load world marker"}
    if recenter_match.start() >= world_match.start():
        return {"status": "fail", "reason": "runtime recenter marker occurs after the world marker"}
    return {
        "status": "pass",
        "reason": "live main-menu recenter preceded the loaded world frame",
        "saveIndex": manifest.get("saveIndex"),
        "saveEntry": manifest.get("saveEntry"),
    }


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact-dir", type=Path, required=True)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args(argv)
    result = analyze_manifest(args.artifact_dir)
    encoded = json.dumps(result, indent=2, sort_keys=True)
    print(encoded)
    if args.json:
        args.json.write_text(encoded + "\n", encoding="utf-8")
    return 0 if result["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
