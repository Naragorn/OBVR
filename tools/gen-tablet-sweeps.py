"""Hand-script lines for the tablet scenarios' smooth sweeps (the harness has no
glide command: a smooth turn is many small steps). tablet-sword.txt: updown;
tablet-info.txt: columns and row; tablet-on-hand.txt: updown-left, turn and
curl; tablet-stats-info.txt: rests with its angles. Prints to stdout; paste in
place of a sweep.
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


def turn_left_hand(left, right, step, every):
    """The left hand, palm to the eyes, turned about the upright: to the left
    until the page's face is seen nearly edge on (the page then reaches away
    from the eyes), back through ahead a little to the right (it reaches
    towards them), and back to ahead."""
    yaws = (list(range(0, left + 1, step)) + list(range(left, -right - 1, -step)) + list(range(-right, 1, step)))
    out = []
    for yaw in yaws:
        out.append(f"left rot 90 {yaw} 90")
        out.append(f"wait {every}")
    return out


def curl_left_hand(start, end, steps, every):
    """The left hand's fingers closed or opened in steps."""
    out = []
    for i in range(1, steps + 1):
        out.append(f"left curl {start + (end - start) * i / steps:.2f}")
        out.append(f"wait {every}")
    return out


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


def beam_rests(yaw, top, bottom, step, rest):
    """The beam stepped down a column, resting on each step: a hovered row's
    info box slides out whole and stays before the next."""
    lines = []
    p = top
    while p >= bottom:
        lines.append(f"right rot {p} {yaw} 0")
        lines.append(f"wait {rest}")
        p -= step
    return lines


if __name__ == "__main__":
    which = sys.argv[1]
    if which == "updown":
        print("\n".join(up_down()))
    elif which == "columns":
        print("\n".join(beam_columns([25, 40, 55], 80, 24, 0.06)))
    elif which == "rests":
        print("\n".join(beam_rests(40, 75, 30, 3, 1.2)))
    elif which == "row":
        print("\n".join(beam_row(52, 15, 65, 0.05)))
    elif which == "updown-left":
        hand = [("left", 0, 90)]
        # down to 30 degrees only: the page stands before the hand and would swing into the face
        print("\n".join(pitch_sweep(hand, 90, 30, 2, 0.05) + pitch_sweep(hand, 30, 90, 2, 0.05)))
    elif which == "turn":
        print("\n".join(turn_left_hand(76, 36, 2, 0.05)))
    elif which == "close":
        # steps of 0.05: each is past the step the hand's reach is read again at (vr/HandReach.h)
        print("\n".join(curl_left_hand(0.0, 1.0, 20, 0.07)))
    elif which == "open":
        print("\n".join(curl_left_hand(1.0, 0.0, 20, 0.07)))
    elif which == "rests-at":
        # rests-at YAW TOP BOTTOM STEP REST
        yaw, top, bottom, step = (int(v) for v in sys.argv[2:6])
        print("\n".join(beam_rests(yaw, top, bottom, step, float(sys.argv[6]))))
