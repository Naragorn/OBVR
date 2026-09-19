import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image


def phase_name(path: Path) -> str:
    return path.stem.rsplit("-", 1)[0]


def analyze(directory: Path, box: tuple[int, int, int, int]) -> dict:
    files = sorted(directory.glob("*.jpg"))
    if len(files) < 2:
        raise ValueError(f"need at least two JPEG frames in {directory}")
    frames = [
        np.asarray(Image.open(path).convert("L").crop(box), dtype=np.float32)
        for path in files
    ]
    changes = [float(np.mean(np.abs(frames[i] - frames[i - 1])))
               for i in range(1, len(frames))]
    phases: dict[str, list[float]] = {}
    for index, change in enumerate(changes, 1):
        phases.setdefault(phase_name(files[index]), []).append(change)
    return {
        "directory": str(directory),
        "box": list(box),
        "frameCount": len(files),
        "phases": {
            name: {
                "samples": len(values),
                "meanAbsoluteChange": sum(values) / len(values),
                "maxAbsoluteChange": max(values),
            }
            for name, values in phases.items()
        },
        "largestTransitions": [
            {"frame": files[index].name, "meanAbsoluteChange": changes[index - 1]}
            for index in sorted(range(1, len(files)),
                                key=lambda item: changes[item - 1], reverse=True)[:10]
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=Path)
    parser.add_argument("--box", required=True,
                        help="left,top,right,bottom water rectangle")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    box = tuple(int(value) for value in args.box.split(","))
    if len(box) != 4 or box[2] <= box[0] or box[3] <= box[1]:
        raise SystemExit("--box must be left,top,right,bottom with positive area")
    result = analyze(args.directory, box)
    text = json.dumps(result, indent=2)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text + "\n", encoding="utf-8")
    print(text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
