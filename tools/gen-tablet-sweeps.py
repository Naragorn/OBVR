"""Hand-script lines for the tablet scenarios' smooth sweeps (the harness has no
glide command: a smooth turn is many small steps). tablet-sword.txt: updown;
tablet-info.txt: columns and row. Prints to stdout; paste in place of a sweep.
"""
import sys


def pitch_sweep(hands, start, end, step, every):
    """hands: list of (side, yaw, roll); pitch from start to end."""
    out = []
    sign = 1 if end >= start else -1
    p = start
    while (p - end) * sign <= 0:
        for side, yaw, roll in hands:
            out.append(f"{side} rot {p} {yaw} {roll}")
        out.append(f"wait {every}")
        p += sign * step
    return out


def up_down():
    hands = [("left", 0, 90), ("right", 0, -90)]
    lines = pitch_sweep(hands, 90, -40, 2, 0.05) + pitch_sweep(hands, -40, 90, 2, 0.05)
    return lines


def beam_columns(yaws, top, bottom, every):
    lines = []
    for yaw in yaws:
        lines.append(f"right rot {top} {yaw} 0")
        lines.append("wait 0.4")
        lines += pitch_sweep([("right", yaw, 0)], top, bottom, 1, every)
    return lines


def beam_row(pitch, left, right, every):
    lines = []
    sign = 1 if right >= left else -1
    y = left
    while (y - right) * sign <= 0:
        lines.append(f"right rot {pitch} {y} 0")
        lines.append(f"wait {every}")
        y += sign * 1
    return lines


if __name__ == "__main__":
    which = sys.argv[1]
    if which == "updown":
        print("\n".join(up_down()))
    elif which == "columns":
        print("\n".join(beam_columns([25, 40, 55], 80, 24, 0.06)))
    elif which == "row":
        print("\n".join(beam_row(52, 15, 65, 0.05)))
