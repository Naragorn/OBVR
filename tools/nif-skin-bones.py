#!/usr/bin/env python3
"""Lists each skinned shape of a .nif: its vertex count, its bones, how many
vertices each bone owns (its largest weight) and which bone combinations
share the vertices whose largest weight is under 0.99. Tells where a mesh
could be cut by bone (OBNG knowledge/dismemberment.md) and which bones a
fitted capsule would take its vertices from (mesh-fitted-capsules-spec).

Usage: nif-skin-bones.py <file.nif> ...
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
            if not isinstance(block, NifFormat.NiTriBasedGeom):
                continue
            skin = block.skin_instance
            n = block.data.num_vertices if block.data else 0
            if not skin:
                print('  %s: %d vertices, not skinned' % (block.name.decode(), n))
                continue
            bones = [b.name.decode() for b in skin.bones]
            weights = collections.defaultdict(list)
            for bi, bd in enumerate(skin.data.bone_list):
                for w in bd.vertex_weights:
                    weights[w.index].append((w.weight, bi))
            owned = collections.Counter()
            shared = collections.Counter()
            for v, ws in weights.items():
                ws.sort(reverse=True)
                owned[bones[ws[0][1]]] += 1
                if ws[0][0] < 0.99:
                    shared[tuple(sorted(bones[b] for _, b in ws if _ > 0.01))] += 1
            print('  %s: %d vertices, %d bones' % (block.name.decode(), n, len(bones)))
            for b, c in owned.most_common():
                print('    owns %-22s %5d' % (b, c))
            for combo, c in shared.most_common(8):
                print('    shared %s: %d' % (' + '.join(combo), c))


for p in sys.argv[1:]:
    report(p)
