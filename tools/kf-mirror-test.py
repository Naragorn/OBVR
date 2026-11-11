#!/usr/bin/env python3
"""Checks for tools/kf-mirror.py: the mirror maths on plain values, without a
.kf file or pyffi.
Run with: python3 tools/kf-mirror-test.py"""
import importlib.util
import math
import sys
from pathlib import Path

here = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("kf_mirror", here / "kf-mirror.py")
km = importlib.util.module_from_spec(spec)
spec.loader.exec_module(km)

failures = 0


def check(condition, what):
    global failures
    print("  %-5s %s" % ("ok" if condition else "FAIL", what))
    if not condition:
        failures += 1


def near_mat(a, b, eps=1e-5):
    return all(abs(a[i][j] - b[i][j]) < eps for i in range(3) for j in range(3))


def near_vec(a, b, eps=1e-4):
    return all(abs(x - y) < eps for x, y in zip(a, b))


# --- names and text keys -------------------------------------------------
check(km.partner("Bip01 L UpperArm") == "Bip01 R UpperArm", "a left bone's partner is the right one")
check(km.partner("Bip01 R Finger42") == "Bip01 L Finger42", "a right finger's partner is the left one")
check(km.partner("Bip01 Spine1") == "Bip01 Spine1", "a centre bone is its own partner")
check(km.partner("Weapon") == "Weapon", "an attachment node is its own partner")
check(km.swap_text_key("Enum: Left") == "Enum: Right", "a left footstep becomes a right one")
check(km.swap_text_key("Enum: Right") == "Enum: Left", "a right footstep becomes a left one")
check(km.swap_text_key("Blend: 0") == "Blend: 0", "other text keys stay")
check(km.swap_text_key("start") == "start", "the start key stays")

# --- quaternions -----------------------------------------------------------
q = km.quat_from_axis_angle((0.3, -0.5, 0.8), 0.9)
check(near_mat(km.mat_from_quat(km.quat_from_mat(km.mat_from_quat(q))), km.mat_from_quat(q)),
      "matrix -> quaternion -> matrix round trip")
for axis in range(3):
    m = km.reflection(axis)
    r = km.mat_from_quat(q)
    want = km.mat_mul(km.mat_mul(m, r), m)
    check(near_mat(km.mat_from_quat(km.mirror_quat(q, axis)), want),
          "mirror_quat is M R M along axis %d" % axis)

# --- a symmetric skeleton --------------------------------------------------
# Root, a centre spine, and left/right arms whose bind frames are mirror
# images with the local z flipped (the way the biped has them). The lateral
# axis is x.


def rot_z(a):
    c, s = math.cos(a), math.sin(a)
    return [[c, -s, 0.0], [s, c, 0.0], [0.0, 0.0, 1.0]]


def rot_x(a):
    c, s = math.cos(a), math.sin(a)
    return [[1.0, 0.0, 0.0], [0.0, c, -s], [0.0, s, c]]


parents = {
    "Bip01": None,
    "Bip01 Spine": "Bip01",
    "Bip01 L Arm": "Bip01 Spine",
    "Bip01 L Hand": "Bip01 L Arm",
    "Bip01 R Arm": "Bip01 Spine",
    "Bip01 R Hand": "Bip01 R Arm",
}
local_bind = {
    "Bip01": (rot_z(math.pi / 2), (0.0, 0.0, 60.0)),
    "Bip01 Spine": (rot_x(0.1), (0.0, 0.0, 10.0)),
}
left_arm_world = km.mat_mul(rot_z(0.4), rot_x(0.7))
mirror_x = km.reflection(0)
# The right arm's world bind: the mirror image, with the local z flipped to
# stay a rotation (M L F, det(M) det(F) = +1).
flip_z = [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, -1.0]]
right_arm_world = km.mat_mul(km.mat_mul(mirror_x, left_arm_world), flip_z)
world_bind = km.forward_kinematics(parents, local_bind)
spine_r, spine_p = world_bind["Bip01 Spine"]
world_bind["Bip01 L Arm"] = (left_arm_world, km.vec_add(spine_p, (-8.0, 1.0, 2.0)))
world_bind["Bip01 R Arm"] = (right_arm_world, km.vec_add(spine_p, (8.0, 1.0, 2.0)))
world_bind["Bip01 L Hand"] = (km.mat_mul(left_arm_world, rot_x(0.2)), km.vec_add(world_bind["Bip01 L Arm"][1], km.mat_vec(left_arm_world, (20.0, 0.0, 0.0))))
world_bind["Bip01 R Hand"] = (km.mat_mul(right_arm_world, rot_x(-0.2)), km.vec_add(world_bind["Bip01 R Arm"][1], km.mat_vec(right_arm_world, (20.0, 0.0, 0.0))))

check(km.lateral_axis(world_bind, "Bip01 L Arm", "Bip01 R Arm") == 0, "the lateral axis is read from the arms")

maps = km.bone_maps(parents, world_bind, 0)
check(maps["Bip01 L Arm"].source == "Bip01 R Arm", "the left arm is driven by the right arm's track")
check(maps["Bip01 Spine"].source == "Bip01 Spine", "the spine is driven by its own track")

# An animated pose: every bone with some local rotation and translation.
pose = {
    "Bip01": (km.quat_from_axis_angle((0.1, 0.2, 1.0), 1.7), (3.0, -2.0, 61.0)),
    "Bip01 Spine": (km.quat_from_axis_angle((1.0, 0.3, 0.0), 0.4), (0.5, 0.2, 10.0)),
    "Bip01 L Arm": (km.quat_from_axis_angle((0.2, 1.0, 0.5), 0.8), (-8.0, 1.5, 2.0)),
    "Bip01 L Hand": (km.quat_from_axis_angle((1.0, 0.0, 0.2), -0.6), (20.0, 0.3, 0.0)),
    "Bip01 R Arm": (km.quat_from_axis_angle((0.7, -0.2, 0.1), 1.1), (7.0, 0.5, 3.0)),
    "Bip01 R Hand": (km.quat_from_axis_angle((0.0, 1.0, 0.0), 0.3), (19.0, 0.0, 1.0)),
}
mirrored = {}
for bone, link in maps.items():
    sq, st = pose[link.source]
    mirrored[bone] = (km.map_rotation(sq, link), km.map_translation(st, link))

as_local = lambda p: {b: (km.mat_from_quat(q), t) for b, (q, t) in p.items()}
world = km.forward_kinematics(parents, as_local(pose))
world_m = km.forward_kinematics(parents, as_local(mirrored))
for bone, link in maps.items():
    src_r, src_p = world[link.source]
    want_r = km.mat_mul(km.mat_mul(km.mat_mul(mirror_x, src_r), mirror_x), km.mat_from_quat(link.c_self))
    got_r, got_p = world_m[bone]
    check(near_vec(got_p, km.mat_vec(mirror_x, src_p)), "%s sits where its partner's mirror image does" % bone)
    check(near_mat(got_r, want_r), "%s turns as its partner's mirror image does" % bone)

# In the bind pose the mirror changes nothing: each bone keeps its own bind.
bind_local = km.local_from_world(parents, world_bind)
bind_m = {}
for bone, link in maps.items():
    r, t = bind_local[link.source]
    bind_m[bone] = (km.mat_from_quat(km.map_rotation(km.quat_from_mat(r), link)), km.map_translation(t, link))
for bone in maps:
    check(near_mat(bind_m[bone][0], bind_local[bone][0]) and near_vec(bind_m[bone][1], bind_local[bone][1]),
          "%s keeps its bind pose under the mirror" % bone)

# The map is linear: a spline's control points can be mapped one by one.
link = maps["Bip01 L Arm"]
a = (0.3, 0.1, -0.2, 0.5)
b = (-0.1, 0.4, 0.2, 0.05)
s = tuple(x + y for x, y in zip(a, b))
mapped_sum = km.map_rotation(s, link)
sum_mapped = tuple(x + y for x, y in zip(km.map_rotation(a, link), km.map_rotation(b, link)))
check(near_vec(mapped_sum, sum_mapped, 1e-9), "the rotation map is linear (control points map one by one)")
check(near_vec(km.map_translation((2.0, 4.0, 6.0), link),
               tuple(2.0 * x for x in km.map_translation((1.0, 2.0, 3.0), link)), 1e-9),
      "the translation map is linear")

# A pair whose frames differ by a reflection cannot be mapped by a turn.
bad = dict(world_bind)
bad["Bip01 R Arm"] = (km.mat_mul(km.mat_mul(mirror_x, left_arm_world), km.mat_mul(mirror_x, flip_z)), world_bind["Bip01 R Arm"][1])
try:
    km.bone_maps(parents, bad, 0)
    refused = False
except ValueError:
    refused = True
check(refused, "a pair that is no mirror image up to a turn is refused")

# The animation holds the root unturned while the bind turns it a quarter
# (skeleton.nif: Bip01 -90 degrees about z): rebased on the animation's own
# turn, the root takes no C, so an unturned key stays unturned and no half
# turn is left on the body under it (the mirrored staggers turned the body
# round, 2026-11-11); the reflection's plane stays the actor's (axis 0 here).
identity = [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]]
rebased = km.rebase_root(parents, world_bind, km.ACCUMULATION_ROOT, identity)
check(near_mat(rebased[km.ACCUMULATION_ROOT][0], identity), "the rebased root stands unturned")
check(near_mat(km.mat_mul(km.mat_t(rebased["Bip01 Spine"][0]), rebased["Bip01 L Arm"][0]),
               km.mat_mul(km.mat_t(world_bind["Bip01 Spine"][0]), world_bind["Bip01 L Arm"][0])),
      "under the root every bone keeps its bind against its parent")
rebased_maps = km.bone_maps(parents, rebased, 0)
root_link = rebased_maps[km.ACCUMULATION_ROOT]
check(near_vec(km.map_rotation((1.0, 0.0, 0.0, 0.0), root_link), (1.0, 0.0, 0.0, 0.0), 1e-9),
      "an unturned key of the root stays unturned (no half turn)")
old_link = maps[km.ACCUMULATION_ROOT]
check(not near_vec(km.map_rotation((1.0, 0.0, 0.0, 0.0), old_link), (1.0, 0.0, 0.0, 0.0), 1e-3),
      "on the bind's turned root the same key came out turned (the bug)")

# Every bone under the rebased root is still the mirror image of its partner,
# the root unturned as the animation keys it.
anim = dict(pose)
anim[km.ACCUMULATION_ROOT] = ((1.0, 0.0, 0.0, 0.0), pose[km.ACCUMULATION_ROOT][1])
anim_m = {}
for bone, link in rebased_maps.items():
    sq, st = anim[link.source]
    anim_m[bone] = (km.map_rotation(sq, link), km.map_translation(st, link))
w = km.forward_kinematics(parents, as_local(anim))
wm = km.forward_kinematics(parents, as_local(anim_m))
for bone, link in rebased_maps.items():
    src_r, src_p = w[link.source]
    want_r = km.mat_mul(km.mat_mul(km.mat_mul(mirror_x, src_r), mirror_x), km.mat_from_quat(link.c_self))
    check(near_vec(wm[bone][1], km.mat_vec(mirror_x, src_p)) and near_mat(wm[bone][0], want_r),
          "rebased: %s is its partner's mirror image" % bone)

print("%d failure(s)" % failures)
sys.exit(1 if failures else 0)
