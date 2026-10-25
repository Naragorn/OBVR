#!/usr/bin/env python3
"""Fits body capsules to skinned meshes, offline: the prototype of
mesh-fitted-capsules-spec 4.2 (todo 1). For each group of bones (a limb
segment, the head, the torso) it takes the vertices those bones own (the
bone with the largest weight), puts them in the skeleton's bind pose and
fits a capsule along the bone's axis (to its child bone), or along the
vertices' principal axis for a bone without a child (head, hand, foot):
the ends from the vertices' extent trimmed by the radius, the radius a
percentile of their distance to the axis. Where the cross-section is flat
(the torso), it also fits two capsules side by side.

Usage: capsule-fit.py <skeleton.nif> <outfit>=<a.nif>,<b.nif>,... [...]
           [--percentile 95] [--json out.json]
       capsule-fit.py --selftest
Vanilla meshes come out of the BSA with tools/bsa-extract.py.

Run with ~/obvr-venv/bin/python -I (pyffi 2.2.4.dev4). Lengths are game
units (70 a metre).
"""
import json
import math
import sys

# ---- vector maths over plain tuples (no numpy in the venv) -----------------


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def mul(a, k):
    return (a[0] * k, a[1] * k, a[2] * k)


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def length(a):
    return math.sqrt(dot(a, a))


def unit(a):
    n = length(a)
    return mul(a, 1.0 / n) if n > 1e-9 else (1.0, 0.0, 0.0)


def mean(points):
    n = float(len(points))
    return (sum(p[0] for p in points) / n, sum(p[1] for p in points) / n, sum(p[2] for p in points) / n)


def percentile(values, pct):
    """Linear interpolation between the closest ranks, as numpy's default."""
    s = sorted(values)
    if not s:
        return 0.0
    k = (len(s) - 1) * pct / 100.0
    lo = int(math.floor(k))
    hi = min(lo + 1, len(s) - 1)
    return s[lo] + (s[hi] - s[lo]) * (k - lo)


def jacobi_eigen(a):
    """Eigenvalues and eigenvectors of a symmetric 3x3, largest first."""
    a = [row[:] for row in a]
    v = [[1.0 if i == j else 0.0 for j in range(3)] for i in range(3)]
    for _ in range(50):
        off = abs(a[0][1]) + abs(a[0][2]) + abs(a[1][2])
        if off < 1e-12:
            break
        for p, q in ((0, 1), (0, 2), (1, 2)):
            if abs(a[p][q]) < 1e-15:
                continue
            theta = (a[q][q] - a[p][p]) / (2.0 * a[p][q])
            t = (1.0 if theta >= 0 else -1.0) / (abs(theta) + math.sqrt(theta * theta + 1.0))
            c = 1.0 / math.sqrt(t * t + 1.0)
            s = t * c
            for k in range(3):
                akp, akq = a[k][p], a[k][q]
                a[k][p] = c * akp - s * akq
                a[k][q] = s * akp + c * akq
            for k in range(3):
                apk, aqk = a[p][k], a[q][k]
                a[p][k] = c * apk - s * aqk
                a[q][k] = s * apk + c * aqk
            for k in range(3):
                vkp, vkq = v[k][p], v[k][q]
                v[k][p] = c * vkp - s * vkq
                v[k][q] = s * vkp + c * vkq
    pairs = [(a[i][i], (v[0][i], v[1][i], v[2][i])) for i in range(3)]
    pairs.sort(key=lambda e: -e[0])
    return pairs


def covariance(points, centre):
    c = [[0.0] * 3 for _ in range(3)]
    for p in points:
        d = sub(p, centre)
        for i in range(3):
            for j in range(3):
                c[i][j] += d[i] * d[j]
    n = float(max(len(points), 1))
    return [[c[i][j] / n for j in range(3)] for i in range(3)]


def perpendicular_basis(axis):
    helper = (0.0, 0.0, 1.0) if abs(axis[2]) < 0.9 else (1.0, 0.0, 0.0)
    u = unit(cross(axis, helper))
    return u, cross(axis, u)


def circle_two(a, b):
    c = ((a[0] + b[0]) / 2.0, (a[1] + b[1]) / 2.0)
    return (c[0], c[1], math.hypot(a[0] - c[0], a[1] - c[1]))


def circle_three(a, b, c):
    d = 2.0 * (a[0] * (b[1] - c[1]) + b[0] * (c[1] - a[1]) + c[0] * (a[1] - b[1]))
    if abs(d) < 1e-12:
        # in a line: the circle over the two farthest apart
        pairs = [circle_two(a, b), circle_two(a, c), circle_two(b, c)]
        return max(pairs, key=lambda e: e[2])
    a2, b2, c2 = a[0] ** 2 + a[1] ** 2, b[0] ** 2 + b[1] ** 2, c[0] ** 2 + c[1] ** 2
    x = (a2 * (b[1] - c[1]) + b2 * (c[1] - a[1]) + c2 * (a[1] - b[1])) / d
    y = (a2 * (c[0] - b[0]) + b2 * (a[0] - c[0]) + c2 * (b[0] - a[0])) / d
    return (x, y, math.hypot(a[0] - x, a[1] - y))


def enclosing_circle(points):
    """The smallest circle round 2D points (Welzl, iterative, a fixed shuffle
    so a run repeats)."""
    import random
    pts = list(points)
    random.Random(1).shuffle(pts)

    def inside(c, p):
        return c is not None and math.hypot(p[0] - c[0], p[1] - c[1]) <= c[2] + 1e-9

    c = None
    for i, p in enumerate(pts):
        if inside(c, p):
            continue
        c = (p[0], p[1], 0.0)
        for j in range(i):
            q = pts[j]
            if inside(c, q):
                continue
            c = circle_two(p, q)
            for k in range(j):
                if not inside(c, pts[k]):
                    c = circle_three(p, q, pts[k])
    return c


def section_circle(us, vs, pct):
    """The circle of a cross-section: the vertices nearer than the percentile
    to the middle of their range (each axis' central `pct` share) form its
    core, and the circle is the smallest round that core. The percentile
    keeps a stray vertex out; the smallest circle centres a lopsided section
    (a half torso, a calf) on its shape, not on where its vertices crowd."""
    lo, hi = (100.0 - pct) / 2.0, 100.0 - (100.0 - pct) / 2.0
    mu = (percentile(us, lo) + percentile(us, hi)) / 2.0
    mv = (percentile(vs, lo) + percentile(vs, hi)) / 2.0
    d = [math.hypot(x - mu, y - mv) for x, y in zip(us, vs)]
    cut = percentile(d, pct)
    core = [(x, y) for x, y, e in zip(us, vs, d) if e <= cut + 1e-9]
    return enclosing_circle(core)


# ---- the fit (mesh-fitted-capsules-spec 4.2) --------------------------------


def fit_capsule(points, origin=None, towards=None, pct=95.0):
    """One capsule round the points. The axis runs from `origin` to `towards`
    (a bone to its child) or, without them, along the points' principal axis.
    The line is moved onto the centre of the points' cross-section
    (section_circle), so a bone that sits off its flesh (the spine at the
    back) does not fatten the capsule. Answers the segment's ends, the
    radius and the cross-section's flatness (major over minor reach) with its
    major direction."""
    if not points:
        return None
    centroid = mean(points)
    if origin is not None and towards is not None:
        axis = unit(sub(towards, origin))
        base = origin
    else:
        axis = jacobi_eigen(covariance(points, centroid))[0][1]
        base = centroid
    u, v = perpendicular_basis(axis)
    ts, us, vs = [], [], []
    for p in points:
        d = sub(p, base)
        ts.append(dot(d, axis))
        us.append(dot(d, u))
        vs.append(dot(d, v))
    cu, cv, r = section_circle(us, vs, pct)
    radii = [math.hypot(x - cu, y - cv) for x, y in zip(us, vs)]
    # the cross-section's major direction, without the vertices past the
    # radius: a 2x2 eigen problem
    kept = [(x, y) for x, y, d in zip(us, vs, radii) if d <= r + 1e-9]
    n = float(len(kept))
    suu = sum((x - cu) ** 2 for x, _ in kept) / n
    svv = sum((y - cv) ** 2 for _, y in kept) / n
    suv = sum((x - cu) * (y - cv) for x, y in kept) / n
    tr, det = suu + svv, suu * svv - suv * suv
    disc = math.sqrt(max(tr * tr / 4.0 - det, 0.0))
    l1 = tr / 2.0 + disc
    if abs(suv) > 1e-12:
        mu, mv = l1 - svv, suv
    else:
        mu, mv = (1.0, 0.0) if suu >= svv else (0.0, 1.0)
    m = math.hypot(mu, mv)
    major = unit(add(mul(u, mu / m), mul(v, mv / m)))
    minor = cross(axis, major)
    # flatness: how far the section reaches along its major direction over
    # its minor one, both at the percentile (a stray vertex does not count)
    line = add(base, add(mul(u, cu), mul(v, cv)))
    reach_major = percentile([abs(dot(sub(p, line), major)) for p in points], pct)
    reach_minor = percentile([abs(dot(sub(p, line), minor)) for p in points], pct)
    flat = reach_major / max(reach_minor, 1e-9)
    off_axis = [math.hypot(x, y) for x, y in zip(us, vs)]
    tmin, tmax = min(ts), max(ts)
    t0, t1 = tmin + r, tmax - r
    if t0 > t1:
        t0 = t1 = (tmin + tmax) / 2.0
    return {
        'count': len(points),
        'axis': axis,
        'a': add(line, mul(axis, t0)),
        'b': add(line, mul(axis, t1)),
        'radius': r,
        'radius_median': percentile(radii, 50.0),
        'radius_max': max(radii),
        'radius_about_bone': percentile(off_axis, pct),
        'line_offset': math.hypot(cu, cv),
        'extent': (tmin, tmax),
        'flatness': flat,
        'major': major,
        'line': line,
    }


def split_capsule(points, fit, pct=95.0):
    """Two capsules side by side along the cross-section's major direction,
    each fitted to its half (B&S's collider group of 4.2 step 4)."""
    axis, major, line = fit['axis'], fit['major'], fit['line']
    halves = ([], [])
    for p in points:
        side = dot(sub(p, line), major)
        # a vertex on the dividing line belongs to both halves
        if side <= 1e-6:
            halves[0].append(p)
        if side >= -1e-6:
            halves[1].append(p)
    out = []
    for half in halves:
        if not half:
            return None
        out.append(fit_capsule(half, line, add(line, axis), pct))
    return out


# ---- reading the meshes ---------------------------------------------------


def load(path):
    from pyffi.formats.nif import NifFormat
    data = NifFormat.Data()
    with open(path, 'rb') as f:
        data.read(f)
    return NifFormat, data


def as_tuple(v):
    return (v.x, v.y, v.z)


def skeleton_bones(path):
    """Each Bip01 bone's bind transform in the skeleton root's space."""
    NifFormat, data = load(path)
    root = data.roots[0]
    bones = {}
    for block in root.tree():
        if isinstance(block, NifFormat.NiNode) and block.name.decode().startswith('Bip01'):
            bones[block.name.decode()] = block.get_transform(root)
    return bones


def bone_origin(bones, name):
    m = bones[name]
    return (m.m_41, m.m_42, m.m_43)


def skinned_vertices(path, bones):
    """Every skinned vertex of a file in the skeleton's bind pose, with the
    bone that owns it, and the worst disagreement between the bones of one
    vertex (0 when the mesh's bind pose is the skeleton's)."""
    NifFormat, data = load(path)
    out, worst, missing = [], 0.0, set()
    for root in data.roots:
        for geom in root.tree():
            if not isinstance(geom, NifFormat.NiTriBasedGeom) or not geom.skin_instance or not geom.data:
                continue
            skin = geom.skin_instance
            names = [b.name.decode() for b in skin.bones]
            weights = [[] for _ in range(geom.data.num_vertices)]
            for bi, bd in enumerate(skin.data.bone_list):
                for w in bd.vertex_weights:
                    if w.weight > 0.0:
                        weights[w.index].append((w.weight, bi))
            transforms = []
            for bi, name in enumerate(names):
                if name not in bones:
                    missing.add(name)
                    transforms.append(None)
                    continue
                transforms.append(skin.data.bone_list[bi].skin_transform.get_transform() * bones[name])
            for index, ws in enumerate(weights):
                if not ws:
                    continue
                raw = geom.data.vertices[index]
                total, place, seen = 0.0, (0.0, 0.0, 0.0), []
                for w, bi in ws:
                    if transforms[bi] is None:
                        continue
                    p = as_tuple(raw * transforms[bi])
                    seen.append(p)
                    place = add(place, mul(p, w))
                    total += w
                if total <= 0.0:
                    continue
                place = mul(place, 1.0 / total)
                for p in seen:
                    worst = max(worst, length(sub(p, place)))
                owner = names[max(ws)[1]]
                out.append((owner, place))
    return out, worst, missing


# ---- the groups and the outfits -------------------------------------------


def groups():
    g = [
        ('Head', lambda b: b == 'Bip01 Head', 'Bip01 Head', None),
        ('Neck', lambda b: b in ('Bip01 Neck', 'Bip01 Neck1'), 'Bip01 Neck', 'Bip01 Head'),
        ('Spine2', lambda b: b == 'Bip01 Spine2', 'Bip01 Spine2', 'Bip01 Neck'),
        ('Spine1', lambda b: b == 'Bip01 Spine1', 'Bip01 Spine1', 'Bip01 Spine2'),
        ('Spine', lambda b: b == 'Bip01 Spine', 'Bip01 Spine', 'Bip01 Spine1'),
        ('Pelvis', lambda b: b == 'Bip01 Pelvis', 'Bip01 Pelvis', 'Bip01 Spine'),
        ('Torso (Spine..Spine2)', lambda b: b in ('Bip01 Spine', 'Bip01 Spine1', 'Bip01 Spine2'),
         'Bip01 Spine', 'Bip01 Neck'),
    ]
    for s in ('L', 'R'):
        p = 'Bip01 %s ' % s
        g += [
            (s + ' Clavicle', lambda b, p=p: b == p + 'Clavicle', p + 'Clavicle', p + 'UpperArm'),
            (s + ' UpperArm', lambda b, p=p: b in (p + 'UpperArm', p + 'UpperArmTwist'), p + 'UpperArm', p + 'Forearm'),
            (s + ' Forearm', lambda b, p=p: b in (p + 'Forearm', p + 'ForearmTwist'), p + 'Forearm', p + 'Hand'),
            (s + ' Hand', lambda b, p=p: b == p + 'Hand' or b.startswith(p + 'Finger'), p + 'Hand', None),
            (s + ' Thigh', lambda b, p=p: b == p + 'Thigh', p + 'Thigh', p + 'Calf'),
            (s + ' Calf', lambda b, p=p: b == p + 'Calf', p + 'Calf', p + 'Foot'),
            (s + ' Foot', lambda b, p=p: b == p + 'Foot' or b.startswith(p + 'Toe'), p + 'Foot', None),
        ]
    return g


def local_point(bones, name, p):
    from pyffi.formats.nif import NifFormat
    v = NifFormat.Vector3()
    v.x, v.y, v.z = p
    return as_tuple(v * bones[name].get_inverse(fast=False))


def fit_outfit(bones, vertices, pct):
    rows = []
    for name, member, frm, to in groups():
        pts = [p for owner, p in vertices if member(owner)]
        if not pts:
            continue
        if frm not in bones or (to is not None and to not in bones):
            continue
        fit = fit_capsule(pts, bone_origin(bones, frm), bone_origin(bones, to) if to else None, pct)
        two = split_capsule(pts, fit, pct)
        rows.append((name, frm, fit, two))
    return rows


def report(bones, outfits, pct, json_path):
    dump = {}
    for outfit, files in outfits:
        vertices, worst, missing = [], 0.0, set()
        for f in files:
            v, w, m = skinned_vertices(f, bones)
            vertices += v
            worst = max(worst, w)
            missing |= m
        print('== %s (%s): %d vertices; bones of one vertex disagree by at most %.3f units%s' % (
            outfit, ', '.join(f.rsplit('/', 1)[-1] for f in files), len(vertices), worst,
            ('; not in the skeleton: ' + ', '.join(sorted(missing))) if missing else ''))
        print('   %-22s %5s %7s %7s %7s %7s %7s %6s %6s   %s' % (
            'group', 'verts', 'r%d' % pct, 'r50', 'rmax', 'r@bone', 'offset', 'len', 'flat', 'two side by side (r, r)'))
        rows = fit_outfit(bones, vertices, pct)
        dump[outfit] = {}
        for name, frame, fit, two in rows:
            seg = length(sub(fit['b'], fit['a']))
            pair = '%.1f, %.1f' % (two[0]['radius'], two[1]['radius']) if two else '-'
            print('   %-22s %5d %7.2f %7.2f %7.2f %7.2f %7.2f %6.1f %6.2f   %s' % (
                name, fit['count'], fit['radius'], fit['radius_median'], fit['radius_max'],
                fit['radius_about_bone'], fit['line_offset'], seg, fit['flatness'], pair))
            entry = {
                'frame': frame,
                'vertices': fit['count'],
                'radius': round(fit['radius'], 3),
                'a_local': [round(x, 3) for x in local_point(bones, frame, fit['a'])],
                'b_local': [round(x, 3) for x in local_point(bones, frame, fit['b'])],
                'flatness': round(fit['flatness'], 3),
            }
            if two:
                entry['pair'] = [{
                    'radius': round(c['radius'], 3),
                    'a_local': [round(x, 3) for x in local_point(bones, frame, c['a'])],
                    'b_local': [round(x, 3) for x in local_point(bones, frame, c['b'])],
                } for c in two]
            dump[outfit][name] = entry
    if json_path:
        with open(json_path, 'w') as f:
            json.dump(dump, f, indent=1)
        print('wrote', json_path)


# ---- self test: the flows of mesh-fitted-capsules-spec 6 -------------------


def ring_points(radius, x0, x1, rings=21, per_ring=24, squash=1.0, centre=(0.0, 0.0)):
    pts = []
    for i in range(rings):
        x = x0 + (x1 - x0) * i / (rings - 1)
        for k in range(per_ring):
            a = 2.0 * math.pi * k / per_ring
            pts.append((x, centre[0] + radius * math.cos(a), centre[1] + radius * squash * math.sin(a)))
    return pts


def selftest():
    checks = []

    def check(name, ok, detail):
        checks.append(ok)
        print('%s %s: %s' % ('PASS' if ok else 'FAIL', name, detail))

    # a cylinder of points gives its radius and its ends, trimmed by the radius
    cyl = ring_points(5.0, 0.0, 30.0)
    f = fit_capsule(cyl, (0.0, 0.0, 0.0), (1.0, 0.0, 0.0))
    check('cylinder radius', abs(f['radius'] - 5.0) < 1e-6, '%.4f' % f['radius'])
    check('cylinder ends', abs(f['a'][0] - 5.0) < 1e-6 and abs(f['b'][0] - 25.0) < 1e-6,
          'a.x %.3f b.x %.3f' % (f['a'][0], f['b'][0]))
    check('round is not flat', f['flatness'] < 1.01, '%.3f' % f['flatness'])
    # a stray point does not move the percentile
    g = fit_capsule(cyl + [(15.0, 50.0, 0.0)], (0.0, 0.0, 0.0), (1.0, 0.0, 0.0))
    check('stray point', abs(g['radius'] - 5.0) < 0.05 and g['radius_max'] > 40.0,
          'r %.3f, max %.1f' % (g['radius'], g['radius_max']))
    # a bone off its flesh: the line moves onto the flesh's centre
    off = ring_points(5.0, 0.0, 30.0, centre=(4.0, 0.0))
    h = fit_capsule(off, (0.0, 0.0, 0.0), (1.0, 0.0, 0.0))
    check('off-centre bone', abs(h['radius'] - 5.0) < 1e-6 and abs(h['line_offset'] - 4.0) < 1e-6
          and h['radius_about_bone'] > 8.9, 'r %.3f offset %.3f r@bone %.3f' % (
              h['radius'], h['line_offset'], h['radius_about_bone']))
    # a flat cross-section is flat and splits into two rounder halves
    flat = ring_points(10.0, 0.0, 30.0, squash=0.5)
    k = fit_capsule(flat, (0.0, 0.0, 0.0), (1.0, 0.0, 0.0))
    two = split_capsule(flat, k)
    check('flat cross-section', abs(k['flatness'] - 2.0) < 0.05, '%.3f' % k['flatness'])
    check('flat splits rounder, both alike', two is not None and max(c['radius'] for c in two) < 0.75 * k['radius']
          and abs(two[0]['radius'] - two[1]['radius']) < 0.01,
          'one %.2f, two %.2f %.2f' % (k['radius'], two[0]['radius'], two[1]['radius']))
    # a bone without a child takes the principal axis
    d = unit((1.0, 1.0, 0.0))
    diag = [add(mul(d, t), mul((0.0, 0.0, 1.0), 0.5 * math.sin(t))) for t in [i * 0.5 for i in range(-40, 41)]]
    p = fit_capsule(diag)
    check('principal axis', abs(abs(dot(p['axis'], d)) - 1.0) < 1e-3, 'dot %.5f' % abs(dot(p['axis'], d)))
    # a short blob: the ends meet in the middle (a sphere)
    s = fit_capsule(ring_points(5.0, 0.0, 6.0), (0.0, 0.0, 0.0), (1.0, 0.0, 0.0))
    check('short becomes a sphere', abs(s['a'][0] - 3.0) < 1e-6 and abs(s['b'][0] - 3.0) < 1e-6,
          'a.x %.3f b.x %.3f' % (s['a'][0], s['b'][0]))
    # no points, no capsule
    check('no vertices', fit_capsule([]) is None, 'None')
    print('%d/%d passed' % (sum(checks), len(checks)))
    return all(checks)


def main(argv):
    if argv[1:] == ['--selftest']:
        return 0 if selftest() else 1
    pct, json_path, args = 95.0, None, []
    it = iter(argv[1:])
    for a in it:
        if a == '--percentile':
            pct = float(next(it))
        elif a == '--json':
            json_path = next(it)
        else:
            args.append(a)
    if len(args) < 2:
        print(__doc__)
        return 2
    bones = skeleton_bones(args[0])
    outfits = []
    for spec in args[1:]:
        name, files = spec.split('=', 1)
        outfits.append((name, files.split(',')))
    report(bones, outfits, int(pct) if pct.is_integer() else pct, json_path)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
