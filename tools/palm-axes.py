#!/usr/bin/env python3
"""Which axis of the Bip01 hand bones faces the palm, read from the game's
skeleton and the hand-to-hand idle (the fists); and where each bone axis
lands in the controller's frame under the hand calibrations of the live
INI (EulerToMatrix = Z(yaw) * Y(pitch) * X(roll), the calibration turning
bone axes into controller axes: bone_rot = controller_rot * calibration).

Run with ~/obvr-venv/bin/python -I (pyffi 2.2.4.dev4).
"""
import math
import sys

from pyffi.formats.nif import NifFormat


def mat_of(m33):
    return [list(r) for r in m33.as_list()]


def mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]


def apply(m, v):
    return [sum(m[i][k] * v[k] for k in range(3)) for i in range(3)]


def transpose(m):
    return [[m[j][i] for j in range(3)] for i in range(3)]


def add(a, b):
    return [a[i] + b[i] for i in range(3)]


def quat_to_mat(w, x, y, z):
    # The usual rotation matrix of a unit quaternion (column convention).
    return [
        [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
        [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
        [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)],
    ]


def fmt(v):
    return '(%6.2f %6.2f %6.2f)' % tuple(v)


def load(path):
    data = NifFormat.Data()
    with open(path, 'rb') as f:
        data.read(f)
    return data


def find_nodes(root, out):
    if root is None:
        return
    if isinstance(root, NifFormat.NiNode):
        out[root.name.decode('latin-1')] = root
        for c in root.children:
            find_nodes(c, out)


def bind_local(node):
    return mat_of(node.rotation), [node.translation.x, node.translation.y, node.translation.z]


def chain_in_hand(nodes, names, rots, use_transpose):
    """Position of the last bone's origin in the first bone's frame, with the
    bones' local rotations `rots` (name -> 3x3) and the nif translations."""
    pos = [0.0, 0.0, 0.0]
    rot = [[1, 0, 0], [0, 1, 0], [0, 0, 1]]
    for name in names[1:]:
        r_parent = rots[names[names.index(name) - 1]]
        r = transpose(r_parent) if use_transpose else r_parent
        t = bind_local(nodes[name])[1]
        pos = add(pos, apply(rot, t)) if False else None
        break
    # Straightforward chaining: world_i = world_{i-1} * local_i.
    pos = [0.0, 0.0, 0.0]
    rot = [[1, 0, 0], [0, 1, 0], [0, 0, 1]]
    for i, name in enumerate(names):
        if i == 0:
            continue
        r_local = rots[name]
        r_local = transpose(r_local) if use_transpose else r_local
        t_local = bind_local(nodes[name])[1]
        # The parent's accumulated rotation turns the child's translation.
        pos = add(pos, apply(rot, t_local))
        rot = mul(rot, r_local)
    return pos


def main():
    skel = load(sys.argv[1])
    kf = load(sys.argv[2])
    nodes = {}
    for r in skel.roots:
        find_nodes(r, nodes)

    # The fists' local rotations from the kf (first key, or the constant).
    seq = kf.roots[0]
    fist = {}
    for block in seq.controlled_blocks:
        name = block.get_node_name()
        if isinstance(name, bytes):
            name = name.decode('latin-1')
        interp = block.interpolator
        q = None
        if interp is not None and interp.data is not None and interp.data.num_rotation_keys > 0:
            k = interp.data.quaternion_keys[0].value
            q = (k.w, k.x, k.y, k.z)
        elif interp is not None:
            q = (interp.rotation.w, interp.rotation.x, interp.rotation.y, interp.rotation.z)
        if q is not None and not any(math.isnan(c) for c in q):
            fist[name] = quat_to_mat(*q)

    for side in ('R', 'L'):
        hand = 'Bip01 %s Hand' % side
        print('==', hand)
        for child in nodes[hand].children:
            if child is None:
                continue
            r, t = bind_local(child)
            print('  child %-22s at %s' % (child.name.decode('latin-1'), fmt(t)))
        for finger in ('0', '1', '2', '3', '4'):
            names = [hand, 'Bip01 %s Finger%s' % (side, finger), 'Bip01 %s Finger%s1' % (side, finger),
                     'Bip01 %s Finger%s2' % (side, finger)]
            names = [n for n in names if n in nodes]
            if len(names) < 3:
                continue
            bind = {n: bind_local(nodes[n])[0] for n in names}
            for use_t in (False, True):
                tip_bind = chain_in_hand(nodes, names, bind, use_t)
                rots = dict(bind)
                for n in names[1:]:
                    if n in fist:
                        rots[n] = fist[n]
                tip_fist = chain_in_hand(nodes, names, rots, use_t)
                moved = [tip_fist[i] - tip_bind[i] for i in range(3)]
                print('  finger %s %-9s bind tip %s  fist tip %s  moved %s' % (
                    finger, 'transposed' if use_t else 'direct', fmt(tip_bind), fmt(tip_fist), fmt(moved)))

    # The calibrations: the live INI's and the defaults.
    def euler(roll, pitch, yaw):
        a, b, c = (math.radians(v) for v in (roll, pitch, yaw))
        rx = [[1, 0, 0], [0, math.cos(a), -math.sin(a)], [0, math.sin(a), math.cos(a)]]
        ry = [[math.cos(b), 0, math.sin(b)], [0, 1, 0], [-math.sin(b), 0, math.cos(b)]]
        rz = [[math.cos(c), -math.sin(c), 0], [math.sin(c), math.cos(c), 0], [0, 0, 1]]
        return mul(rz, mul(ry, rx))

    print('== calibrations: each bone axis in the controller frame (x right, y forward, z up)')
    for label, right, left in (
        ('defaults', (0.0, 0.0, 90.0), (180.0, 0.0, 90.0)),
        ('live INI', (-10.2130, 53.7376, 86.1560), (157.6445, 71.0317, 65.8923)),
    ):
        for side, angles in (('right', right), ('left', left)):
            cal = euler(*angles)
            for axis, name in enumerate(('bone x (fingers)', 'bone y', 'bone z')):
                col = [cal[i][axis] for i in range(3)]
                print('  %-8s %-5s %-16s -> controller %s' % (label, side, name, fmt(col)))


if __name__ == '__main__':
    main()
