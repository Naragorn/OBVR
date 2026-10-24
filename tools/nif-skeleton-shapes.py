#!/usr/bin/env python3
"""Lists the Havok collision shape and mass on each bone of a skeleton.nif:
the shape type, its radius (Havok units and game units) and, for a box,
its half extents. Game units are Havok units / 0.142877 (the scale at
0x00A39088, game/GrabPhysics.h).

Usage: nif-skeleton-shapes.py <skeleton.nif>
A vanilla skeleton comes out of the BSA with tools/bsa-extract.py.

Run with ~/obvr-venv/bin/python -I (pyffi 2.2.4.dev4).
"""
import sys

from pyffi.formats.nif import NifFormat

HAVOK_SCALE = 0.142877


def describe(shape, depth=0):
    if shape is None:
        return 'no shape'
    text = type(shape).__name__
    if hasattr(shape, 'radius'):
        text += ' r=%.2f (%.1f units)' % (shape.radius, shape.radius / HAVOK_SCALE)
    if isinstance(shape, NifFormat.bhkBoxShape):
        d = shape.dimensions
        text += ' half=(%.2f %.2f %.2f)' % (d.x, d.y, d.z)
    inner = getattr(shape, 'shape', None)
    if inner is not None and depth < 3:
        text += ' > ' + describe(inner, depth + 1)
    return text


def main():
    data = NifFormat.Data()
    with open(sys.argv[1], 'rb') as f:
        data.read(f)
    for root in data.roots:
        for block in root.tree():
            if not isinstance(block, NifFormat.NiNode) or not block.collision_object:
                continue
            body = block.collision_object.body
            mass = body.mass if body else -1.0
            print('%-20s mass=%6.1f  %s' % (block.name.decode(), mass,
                                            describe(body.shape if body else None)))


if __name__ == '__main__':
    main()
