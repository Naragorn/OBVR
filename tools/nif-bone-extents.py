#!/usr/bin/env python3
"""Lists, for each bone of each skinned shape of a .nif, how far the vertices
it owns (its is their largest weight) reach along the bone's own three axes,
in the bone's frame at the bind pose, in the model's units. Says how thick a
finger or a palm is round its bone - what stands out of a flat laid through
the bones (docs/inventory-tablet-spec.md section 21: a hand's skin lies up to
3 units, 4 cm, off the hand bone's own axis at the thumb's ball, a finger's
half to one and a third).

Usage: nif-bone-extents.py <file.nif> ...
Vanilla meshes come out of the BSA with tools/bsa-extract.py.

Run with ~/obvr-venv/bin/python -I (pyffi 2.2.4.dev4).
"""
import collections
import sys

from pyffi.formats.nif import NifFormat


def report(path):
    data = NifFormat.Data()
    with open(path, 'rb') as f:
        data.read(f)
    print('==', path.rsplit('/', 1)[-1])
    for root in data.roots:
        for block in root.tree():
            if not isinstance(block, NifFormat.NiTriBasedGeom) or not block.skin_instance:
                continue
            skin = block.skin_instance
            bones = [b.name.decode() for b in skin.bones]
            verts = block.data.vertices
            owner = {}
            for bi, bd in enumerate(skin.data.bone_list):
                for w in bd.vertex_weights:
                    if w.index not in owner or w.weight > owner[w.index][0]:
                        owner[w.index] = (w.weight, bi)
            # pyffi's vectors are rows: a vertex times the skin's transform,
            # times the bone's, is the vertex in the bone's frame
            overall = skin.data.get_transform()
            print('  shape %s: %d vertices' % (block.name.decode(), block.data.num_vertices))
            per = collections.defaultdict(list)
            for vi, (_, bi) in owner.items():
                per[bi].append(vi)
            for bi in sorted(per, key=lambda b: bones[b]):
                m = skin.data.bone_list[bi].get_transform()
                at = [verts[vi] * overall * m for vi in per[bi]]
                xs = [p.x for p in at]
                ys = [p.y for p in at]
                zs = [p.z for p in at]
                print('    %-22s %4d  x %6.2f..%6.2f  y %6.2f..%6.2f  z %6.2f..%6.2f' % (
                    bones[bi], len(at), min(xs), max(xs), min(ys), max(ys), min(zs), max(zs)))


for p in sys.argv[1:]:
    report(p)
