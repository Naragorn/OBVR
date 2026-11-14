#!/usr/bin/env python3
"""Mirrors an Oblivion .kf animation left to right, for a skeleton.

The engine plays one stagger file per stance, and it leans one way. A mirrored
copy lets OBVR pick the side from the blow (OBNG knowledge/hit-reactions.md,
"Which stagger animation plays").

How: each bone b takes the track of its partner s (L <-> R, a centre bone is
its own partner) and maps it so that b's world pose is the mirror image of s's:

    world'(b) = M world(s) M C(b),     C(b) = (M bind(s) M)^T bind(b)

M reflects the lateral axis of the skeleton's root space; C(b) turns the
reflected frame back into b's own bind frame (the biped's left and right
frames differ by more than the reflection). In local terms that is

    local_rot'(b)   = C(parent b)^T  M local_rot(s) M  C(b)
    local_trans'(b) = C(parent b)^T  M local_trans(s)

Both are linear in a key's components, so B-spline control points map one by
one and the spline stays a spline. In the bind pose the map gives every bone
its own bind back. Attachment nodes (Weapon, Torch, quivers) keep their own
tracks: the weapon stays in the right hand. Footstep text keys swap sides.

Usage: kf-mirror.py <skeleton.nif> <in.kf> <out.kf>
Vanilla files come out of the BSA with tools/bsa-extract.py.
Run with ~/obvr-venv/bin/python -I (pyffi 2.2.4.dev4); the maths needs no pyffi
(tools/kf-mirror-test.py).
"""
import math
import sys
from collections import namedtuple

ATTACHMENTS = {"Weapon", "Torch", "BackWeapon", "SideWeapon", "Quiver", "magicNode"}

Link = namedtuple("Link", "source c_parent c_self axis ref", defaults=((1.0, 0.0, 0.0, 0.0),))


# --- names -----------------------------------------------------------------
def partner(name):
    if " L " in name:
        return name.replace(" L ", " R ", 1)
    if " R " in name:
        return name.replace(" R ", " L ", 1)
    return name


def swap_text_key(value):
    if value == "Enum: Left":
        return "Enum: Right"
    if value == "Enum: Right":
        return "Enum: Left"
    return value


# --- 3x3 maths (column vectors: world = parent * local) ---------------------
def mat_mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]


def mat_t(a):
    return [[a[j][i] for j in range(3)] for i in range(3)]


def mat_vec(a, v):
    return tuple(sum(a[i][k] * v[k] for k in range(3)) for i in range(3))


def vec_add(a, b):
    return tuple(x + y for x, y in zip(a, b))


def vec_sub(a, b):
    return tuple(x - y for x, y in zip(a, b))


def det(a):
    return (a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1])
            - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0])
            + a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]))


def reflection(axis):
    m = [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]]
    m[axis][axis] = -1.0
    return m


# --- quaternions (w, x, y, z), Hamilton product, R(a*b) = R(a) R(b) ---------
def quat_mul(a, b):
    aw, ax, ay, az = a
    bw, bx, by, bz = b
    return (aw * bw - ax * bx - ay * by - az * bz,
            aw * bx + ax * bw + ay * bz - az * by,
            aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw)


def quat_conj(q):
    return (q[0], -q[1], -q[2], -q[3])


def quat_from_axis_angle(axis, angle):
    n = math.sqrt(sum(c * c for c in axis))
    s = math.sin(angle / 2.0) / n
    return (math.cos(angle / 2.0), axis[0] * s, axis[1] * s, axis[2] * s)


def mat_from_quat(q):
    n = math.sqrt(sum(c * c for c in q))
    w, x, y, z = (c / n for c in q)
    return [[1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y)],
            [2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x)],
            [2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y)]]


def quat_from_mat(m):
    t = m[0][0] + m[1][1] + m[2][2]
    if t > 0.0:
        s = math.sqrt(t + 1.0) * 2.0
        return (0.25 * s, (m[2][1] - m[1][2]) / s, (m[0][2] - m[2][0]) / s, (m[1][0] - m[0][1]) / s)
    if m[0][0] > m[1][1] and m[0][0] > m[2][2]:
        s = math.sqrt(1.0 + m[0][0] - m[1][1] - m[2][2]) * 2.0
        return ((m[2][1] - m[1][2]) / s, 0.25 * s, (m[0][1] + m[1][0]) / s, (m[0][2] + m[2][0]) / s)
    if m[1][1] > m[2][2]:
        s = math.sqrt(1.0 + m[1][1] - m[0][0] - m[2][2]) * 2.0
        return ((m[0][2] - m[2][0]) / s, (m[0][1] + m[1][0]) / s, 0.25 * s, (m[1][2] + m[2][1]) / s)
    s = math.sqrt(1.0 + m[2][2] - m[0][0] - m[1][1]) * 2.0
    return ((m[1][0] - m[0][1]) / s, (m[0][2] + m[2][0]) / s, (m[1][2] + m[2][1]) / s, 0.25 * s)


def mirror_quat(q, axis):
    """The quaternion of M R(q) M, M the reflection of `axis`: the vector part
    keeps its component along the axis and flips the other two."""
    w, v = q[0], list(q[1:])
    return (w,) + tuple(v[i] if i == axis else -v[i] for i in range(3))


# --- the skeleton ------------------------------------------------------------
def forward_kinematics(parents, local):
    """World (rotation, position) of every bone in `local`, parents first."""
    world = {}

    def solve(bone):
        if bone in world:
            return world[bone]
        r, t = local[bone]
        parent = parents.get(bone)
        if parent is None or parent not in local:
            world[bone] = (r, tuple(t))
        else:
            pr, pp = solve(parent)
            world[bone] = (mat_mul(pr, r), vec_add(pp, mat_vec(pr, t)))
        return world[bone]

    for bone in local:
        solve(bone)
    return world


def local_from_world(parents, world):
    local = {}
    for bone, (r, p) in world.items():
        parent = parents.get(bone)
        if parent is None or parent not in world:
            local[bone] = (r, p)
        else:
            pr, pp = world[parent]
            local[bone] = (mat_mul(mat_t(pr), r), mat_vec(mat_t(pr), vec_sub(p, pp)))
    return local


def lateral_axis(world, left, right):
    """The root-space axis along which a left/right bone pair differs most."""
    d = vec_sub(world[left][1], world[right][1])
    return max(range(3), key=lambda i: abs(d[i]))


def bone_maps(parents, world_bind, axis):
    """The Link of every bone that has a partner in the skeleton."""
    m = reflection(axis)
    local_bind = local_from_world(parents, world_bind)
    identity = [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]]

    def c_of(bone):
        if bone is None or bone not in world_bind:
            return identity
        src = partner(bone)
        reflected = mat_mul(mat_mul(m, world_bind[src][0]), m)
        c = mat_mul(mat_t(reflected), world_bind[bone][0])
        if det(c) < 0.5:
            raise ValueError("%s and %s are not mirror images up to a turn" % (bone, src))
        return c

    maps = {}
    for bone in world_bind:
        if bone in ATTACHMENTS or partner(bone) not in world_bind:
            continue
        maps[bone] = Link(partner(bone), quat_from_mat(c_of(parents.get(bone))), quat_from_mat(c_of(bone)), axis,
                          quat_from_mat(local_bind[bone][0]))
    return maps


def align_track(quats, ref):
    """A mapped track's keys turned into the hemisphere of `ref` (the bone's
    bind rotation in its parent), all of them by the first key's side: the
    map gives the same rotations, but may give -q for q, and the engine's
    blend adds quaternions without a sign check - a pelvis at -q blended half
    way with the last animation's q is no rotation at all (the stagger mirror's
    Bip01 Pelvis, 2026-11-14: the body crumpled through the blend)."""
    quats = [tuple(q) for q in quats]
    if not quats:
        return quats
    dot = sum(a * b for a, b in zip(quats[0], ref))
    if dot < 0.0:
        return [tuple(-x for x in q) for q in quats]
    return quats


def map_rotation(q, link):
    return quat_mul(quat_mul(quat_conj(link.c_parent), mirror_quat(q, link.axis)), link.c_self)


def map_translation(t, link):
    reflected = tuple(-c if i == link.axis else c for i, c in enumerate(t))
    return mat_vec(mat_t(mat_from_quat(link.c_parent)), reflected)


# --- the file ----------------------------------------------------------------
def nif_matrix(rot):
    """A NiNode rotation as a column-vector matrix (NIF stores it transposed)."""
    return [[rot.m_11, rot.m_21, rot.m_31], [rot.m_12, rot.m_22, rot.m_32], [rot.m_13, rot.m_23, rot.m_33]]


def read_skeleton(path, nif):
    data = nif.Data()
    with open(path, "rb") as f:
        data.read(f)
    parents, local = {}, {}

    def walk(node, parent):
        if not isinstance(node, nif.NiNode):
            return
        name = node.name.decode()
        parents[name] = parent
        t = node.translation
        local[name] = (nif_matrix(node.rotation), (t.x, t.y, t.z))
        for child in node.children:
            if child:
                walk(child, name)

    for root in data.roots:
        walk(root, None)
    return parents, forward_kinematics(parents, local)


# The animation's accumulation root. skeleton.nif's bind turns it a quarter
# (-90 degrees about z) while the game's animations key it unturned, so the
# body the animation moves stands a quarter turn from the bind's: mirrored in
# the bind's root space, the reflection crossed the body front to back
# instead of left to right and the mirrored staggers turned the body round
# (the hips 145 to 161 degrees off the heading against the game's file's 19
# to 47, stagger-sides.txt 2026-11-11; the reference's own heading did not
# move). So the bones' frames (C) are read off the bind with the root at the
# animation's own turn (rebase_root); the reflection's plane stays the
# actor's, the lateral axis of the bind as the actor shows it.
ACCUMULATION_ROOT = "Bip01"


def rebase_root(parents, world_bind, root, root_rot):
    """The bind's world poses with `root` turned `root_rot` in its parent, as
    the animation holds it; the rest keep their bind locals."""
    if root not in world_bind:
        return world_bind
    local = local_from_world(parents, world_bind)
    local[root] = (root_rot, local[root][1])
    return forward_kinematics(parents, local)


def animation_root_rotation(nif, data, root):
    """The animation's own turn of `root`: its interpolator's static rotation,
    else its first key; None when the file does not move it."""
    for seq in data.roots:
        if not isinstance(seq, nif.NiControllerSequence):
            continue
        for block in seq.controlled_blocks:
            if block.get_node_name().decode() != root or block.interpolator is None:
                continue
            it = block.interpolator
            r = it.rotation
            if _has(r.x):
                return mat_from_quat((r.w, r.x, r.y, r.z))
            if isinstance(it, nif.NiTransformInterpolator) and it.data and it.data.num_rotation_keys:
                v = it.data.quaternion_keys[0].value
                return mat_from_quat((v.w, v.x, v.y, v.z))
    return None


def mirror_file(skeleton_path, in_path, out_path):
    from pyffi.formats.nif import NifFormat as nif

    data = nif.Data()
    with open(in_path, "rb") as f:
        data.read(f)
    parents, world_bind = read_skeleton(skeleton_path, nif)
    # The reflection's plane is the actor's: read off the bind as it stands
    # (the body the actor shows, its arms along the actor's right).
    axis = lateral_axis(world_bind, "Bip01 L UpperArm", "Bip01 R UpperArm")
    root_rot = animation_root_rotation(nif, data, ACCUMULATION_ROOT)
    if root_rot is not None:
        world_bind = rebase_root(parents, world_bind, ACCUMULATION_ROOT, root_rot)
    maps = bone_maps(parents, world_bind, axis)
    for seq in data.roots:
        if not isinstance(seq, nif.NiControllerSequence):
            continue
        blocks = {b.get_node_name().decode(): b for b in seq.controlled_blocks}
        source_interp = {name: b.interpolator for name, b in blocks.items()}
        missing = [n for n in blocks if n in maps and maps[n].source not in source_interp]
        if missing:
            raise SystemExit("no partner track for: %s" % ", ".join(missing))
        for name, block in blocks.items():
            if name not in maps:
                continue
            # The hemisphere the game's own animations keep this bone in: its
            # first rotation in the source file (the bind's quaternion from a
            # matrix has an arbitrary sign; the pelvis's came out opposite).
            own = first_rotation(nif, source_interp[name])
            link = maps[name]._replace(ref=own) if own is not None else maps[name]
            block.interpolator = mirrored_interpolator(nif, source_interp[link.source], link)
        if seq.text_keys:
            for key in seq.text_keys.text_keys:
                key.value = swap_text_key(key.value.decode()).encode()
    with open(out_path, "wb") as f:
        data.write(f)
    return axis, len(maps)


def first_rotation(nif, it):
    """An interpolator's first rotation (w, x, y, z), or None."""
    if it is None:
        return None
    if isinstance(it, nif.NiBSplineCompTransformInterpolator) and it.rotation_offset != 65535:
        return tuple(next(iter(it.get_rotations())))
    if isinstance(it, nif.NiTransformInterpolator) and it.data and it.data.num_rotation_keys:
        v = it.data.quaternion_keys[0].value
        return (v.w, v.x, v.y, v.z)
    r = it.rotation
    return (r.w, r.x, r.y, r.z) if _has(r.w) else None


def _set_quat(dst, q):
    dst.w, dst.x, dst.y, dst.z = q


def _set_vec(dst, v):
    dst.x, dst.y, dst.z = v


def _has(v):
    return abs(v) < 3.0e38  # the NIF's "no value" is -FLT_MAX


def mirrored_interpolator(nif, src, link):
    if src is None:
        return None
    if isinstance(src, nif.NiBSplineCompTransformInterpolator):
        out = nif.NiBSplineCompTransformInterpolator()
        for field in ("start_time", "stop_time", "basis_data", "scale", "scale_offset",
                      "scale_bias", "scale_multiplier"):
            setattr(out, field, getattr(src, field))
        out.spline_data = src.spline_data
        _copy_static(out, src, link)
        out.translation_offset = out.rotation_offset = 65535
        out.translation_bias = src.translation_bias
        out.translation_multiplier = src.translation_multiplier
        out.rotation_bias = src.rotation_bias
        out.rotation_multiplier = src.rotation_multiplier
        if src.translation_offset != 65535:
            pts = [map_translation(p, link) for p in src.get_translations()]
            out.translation_offset, out.translation_bias, out.translation_multiplier = \
                src.spline_data.append_comp_data(pts)
        if src.rotation_offset != 65535:
            pts = align_track([map_rotation(p, link) for p in src.get_rotations()], link.ref)
            out.rotation_offset, out.rotation_bias, out.rotation_multiplier = \
                src.spline_data.append_comp_data(pts)
        return out
    if isinstance(src, nif.NiTransformInterpolator):
        out = nif.NiTransformInterpolator()
        out.scale = src.scale
        _copy_static(out, src, link)
        if src.data:
            out.data = mirrored_transform_data(nif, src.data, link)
        return out
    raise SystemExit("unhandled interpolator %s" % type(src).__name__)


def _copy_static(out, src, link):
    t = src.translation
    if _has(t.x):
        _set_vec(out.translation, map_translation((t.x, t.y, t.z), link))
    else:
        _set_vec(out.translation, (t.x, t.y, t.z))
    r = src.rotation
    if _has(r.w):
        _set_quat(out.rotation, align_track([map_rotation((r.w, r.x, r.y, r.z), link)], link.ref)[0])
    else:
        _set_quat(out.rotation, (r.w, r.x, r.y, r.z))


def mirrored_transform_data(nif, src, link):
    if src.rotation_type == 4:
        raise SystemExit("XYZ rotation keys are not handled")
    out = nif.NiTransformData()
    out.num_rotation_keys = src.num_rotation_keys
    out.rotation_type = src.rotation_type
    out.quaternion_keys.update_size()
    mapped = align_track([map_rotation((k.value.w, k.value.x, k.value.y, k.value.z), link)
                          for k in src.quaternion_keys], link.ref)
    for k_out, k_in, q in zip(out.quaternion_keys, src.quaternion_keys, mapped):
        k_out.time = k_in.time
        _set_quat(k_out.value, q)
        if src.rotation_type == 3:
            k_out.tbc.t, k_out.tbc.b, k_out.tbc.c = k_in.tbc.t, k_in.tbc.b, k_in.tbc.c
    out.translations.num_keys = src.translations.num_keys
    out.translations.interpolation = src.translations.interpolation
    out.translations.keys.update_size()
    for k_out, k_in in zip(out.translations.keys, src.translations.keys):
        k_out.time = k_in.time
        v = k_in.value
        _set_vec(k_out.value, map_translation((v.x, v.y, v.z), link))
        if src.translations.interpolation == 2:
            fw, bw = k_in.forward, k_in.backward
            _set_vec(k_out.forward, map_translation((fw.x, fw.y, fw.z), link))
            _set_vec(k_out.backward, map_translation((bw.x, bw.y, bw.z), link))
        elif src.translations.interpolation == 3:
            k_out.tbc.t, k_out.tbc.b, k_out.tbc.c = k_in.tbc.t, k_in.tbc.b, k_in.tbc.c
    out.scales.num_keys = src.scales.num_keys
    out.scales.interpolation = src.scales.interpolation
    out.scales.keys.update_size()
    for k_out, k_in in zip(out.scales.keys, src.scales.keys):
        k_out.time = k_in.time
        k_out.value = k_in.value
        if src.scales.interpolation == 2:
            k_out.forward, k_out.backward = k_in.forward, k_in.backward
        elif src.scales.interpolation == 3:
            k_out.tbc.t, k_out.tbc.b, k_out.tbc.c = k_in.tbc.t, k_in.tbc.b, k_in.tbc.c
    return out


def main(argv):
    if len(argv) != 4:
        raise SystemExit(__doc__)
    axis, bones = mirror_file(argv[1], argv[2], argv[3])
    print("mirrored %s -> %s: lateral axis %s, %d bones mapped" % (argv[2], argv[3], "xyz"[axis], bones))


if __name__ == "__main__":
    main(sys.argv)
