"""Decode read-only native camera snapshots for OBVR-disabled reference runs.

Layouts: src/game/GameTypes.h (world transform 0x64), GameCamera.h
(scene camera 0xDC, frustum 0xEC), NiMath.h (row-major transform).
The caller supplies a reader; this module cannot write game memory.
"""
import math
import struct


def decode_camera(transform, frustum):
    if len(transform) != 52 or len(frustum) != 28:
        raise ValueError('Incomplete camera snapshot')
    values = struct.unpack('<13f', transform)
    planes = struct.unpack('<6f', frustum[:24])
    if not all(math.isfinite(v) for v in values + planes):
        raise ValueError('Non-finite camera snapshot')
    rotation = [values[i:i+3] for i in (0, 3, 6)]
    for i in range(3):
        for j in range(3):
            dot = sum(rotation[i][k]*rotation[j][k] for k in range(3))
            if abs(dot - (1 if i == j else 0)) > .01:
                raise ValueError('Invalid camera basis')
    l,r,t,b,n,f = planes
    if values[12] <= 0 or not (l < r and b < t and 0 < n < f) or frustum[24] != 0:
        raise ValueError('Invalid scale or perspective frustum')
    return {'rotation': rotation, 'position': values[9:12], 'scale': values[12],
            'frustum': dict(zip(('left','right','top','bottom','near','far'),planes))}


def read_camera(read):
    def pointer(address):
        data = read(address, 4)
        if len(data) != 4:
            raise ValueError('Incomplete pointer')
        value, = struct.unpack('<I', data)
        if value < 0x10000 or value > 0xfffffff0 or value % 4:
            raise ValueError('Invalid scene/camera pointer')
        return value
    scene = pointer(0x00B333CC)
    camera = pointer(scene + 0xDC)
    # Two identical reads reject moving/torn snapshots, rather than pretending
    # that a cross-process read is synchronized to the rendered frame.
    first = (read(camera+0x64,52), read(camera+0xEC,28))
    second = (read(camera+0x64,52), read(camera+0xEC,28))
    if first != second:
        raise ValueError('Camera changed during snapshot')
    result = decode_camera(*first)
    result.update(sceneAddress=scene, cameraAddress=camera)
    return result
