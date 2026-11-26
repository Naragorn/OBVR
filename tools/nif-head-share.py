#!/usr/bin/env python3
"""How much of each skinned shape is the head, by its vertex weights: for a
head cut, whether a body's shape holds the head itself (a troll's, a deer's
one mesh) or only a collar or a hood's rim (an NPC's shirt). Measures what
game/CutSkeletonLogic.h HeadCopyWanted decides on.

For every skinned shape: its vertices, its bones, how many of its bones are
the head's by the game's count (CountShapeBones: the head, what hangs under
it, Bip01 Neck and Bip01 Neck1), and how many vertices hang on the head and
what hangs under it by at least half of their weight ("mostly"), all of it
("whole") or a hundredth ("touched"). Which bones hang under the head is
read in the skeleton given with --skeleton (the human one for clothes and
armour, the creature's own for a creature); the head is "Bip01 Head", else
"Bip01 Skull".

Usage:
  nif-head-share.py [--skeleton SKEL.nif] FILE.nif ...
  nif-head-share.py --bsa ARCHIVE.bsa --skeleton SKEL.nif PREFIX ...
      every .nif in the archive whose path starts with a PREFIX
  nif-head-share.py --bsa ARCHIVE.bsa --creatures
      every creature folder of the archive against its own skeleton.nif

Output: one tab-separated line a skinned shape:
  file shape vertices bones headbones mostly whole touched share
(share = mostly / vertices). Vanilla meshes come out of the BSA through
tools/bsa-extract.py, imported. Run with ~/obvr-venv/bin/python -I
(pyffi 2.2.4.dev4).
"""
import collections
import importlib.util
import io
import os
import struct
import sys
import zlib

from pyffi.formats.nif import NifFormat

HEAD_NAMES = ('Bip01 Head', 'Bip01 Skull')
NECK_NAMES = ('Bip01 Neck', 'Bip01 Neck1')


def load_bsa_module():
    here = os.path.dirname(os.path.abspath(__file__))
    spec = importlib.util.spec_from_file_location('bsa_extract', os.path.join(here, 'bsa-extract.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def bsa_blob(archive, name):
    data, files, compressed_default = archive
    size, off = files[name]
    compressed = compressed_default
    if size & 0x40000000:
        compressed = not compressed
        size &= ~0x40000000
    blob = data[off:off + size]
    if compressed:
        blob = zlib.decompress(blob[4:])
    return blob


def read_nif(blob):
    data = NifFormat.Data()
    data.read(io.BytesIO(blob))
    return data


def head_names_of(skeleton):
    """The head bone's name and every node's under it, by the skeleton's tree."""
    names = set()
    for root in skeleton.roots:
        for block in root.tree():
            if not isinstance(block, NifFormat.NiNode):
                continue
            name = block.name.decode('latin-1')
            if name in HEAD_NAMES and not names:
                for under in block.tree():
                    if isinstance(under, NifFormat.NiAVObject):
                        names.add(under.name.decode('latin-1'))
    return names


def shapes_of(nif, head_names):
    for root in nif.roots:
        for block in root.tree():
            if not isinstance(block, NifFormat.NiTriBasedGeom) or not block.skin_instance:
                continue
            skin = block.skin_instance
            if not skin.data or not block.data:
                continue
            bones = [b.name.decode('latin-1') if b else '' for b in skin.bones]
            on_head = [b in head_names for b in bones]
            head_bones = sum(1 for b in bones if b in head_names or b in NECK_NAMES)
            share = collections.defaultdict(float)
            for bi, bd in enumerate(skin.data.bone_list):
                if bi < len(on_head) and on_head[bi]:
                    for w in bd.vertex_weights:
                        share[w.index] += w.weight
            n = block.data.num_vertices
            mostly = sum(1 for v in share.values() if v >= 0.5)
            whole = sum(1 for v in share.values() if v >= 0.99)
            touched = sum(1 for v in share.values() if v >= 0.01)
            yield block.name.decode('latin-1'), n, len(bones), head_bones, mostly, whole, touched


def line(path, row):
    name, n, bones, head_bones, mostly, whole, touched = row
    print('%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%.3f' % (path, name, n, bones, head_bones, mostly, whole, touched,
                                                     mostly / n if n else 0.0))


def report(path, blob, head_names):
    try:
        nif = read_nif(blob)
    except Exception as e:  # a file pyffi does not parse is named, not fatal
        print('%s\tUNREAD\t%s' % (path, type(e).__name__), file=sys.stderr)
        return
    for row in shapes_of(nif, head_names):
        line(path, row)


def main():
    args = sys.argv[1:]
    archive = None
    skeleton_names = set()
    creatures = False
    rest = []
    i = 0
    while i < len(args):
        if args[i] == '--bsa':
            archive = load_bsa_module().load(args[i + 1])
            i += 2
        elif args[i] == '--skeleton':
            path = args[i + 1]
            with open(path, 'rb') as f:
                skeleton_names = head_names_of(read_nif(f.read()))
            i += 2
        elif args[i] == '--creatures':
            creatures = True
            i += 1
        else:
            rest.append(args[i])
            i += 1
    if archive is None:
        for path in rest:
            with open(path, 'rb') as f:
                report(path, f.read(), skeleton_names)
        return
    names = sorted(n for n in archive[1] if n.endswith('.nif'))
    if creatures:
        folders = collections.defaultdict(list)
        for n in names:
            parts = n.split('/')
            if len(parts) >= 4 and parts[0] == 'meshes' and parts[1] == 'creatures':
                folders['/'.join(parts[:3])].append(n)
        for folder, files in sorted(folders.items()):
            skel = folder + '/skeleton.nif'
            if skel not in archive[1]:
                print('%s\tNO SKELETON' % folder, file=sys.stderr)
                continue
            try:
                head_names = head_names_of(read_nif(bsa_blob(archive, skel)))
            except Exception as e:
                print('%s\tSKELETON UNREAD\t%s' % (folder, type(e).__name__), file=sys.stderr)
                continue
            for n in files:
                if n != skel:
                    report(n, bsa_blob(archive, n), head_names)
        return
    for n in names:
        if any(n.startswith(p) for p in rest) and not n.endswith('_gnd.nif') and '/skeleton' not in n:
            report(n, bsa_blob(archive, n), skeleton_names)


if __name__ == '__main__':
    main()
