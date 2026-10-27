#!/usr/bin/env python3
"""Names the export nearest below each RVA in a 32-bit PE (ntdll.dll, win32u.dll
from C:\\Windows\\SysWOW64) - for the render sampler's eips in system DLLs.
Usage: pe-exports.py <dll> <rva hex> [...]
"""
import struct, sys
# Names the export nearest below each RVA in a PE file (32-bit), for ntdll syscall stubs seen by the sampler.
path = sys.argv[1]; rvas = [int(x, 16) for x in sys.argv[2:]]
d = open(path, "rb").read()
pe = struct.unpack_from("<I", d, 0x3C)[0]
assert d[pe:pe+4] == b"PE\0\0"
nsec = struct.unpack_from("<H", d, pe + 6)[0]
opt = pe + 24; magic = struct.unpack_from("<H", d, opt)[0]
dd = opt + (96 if magic == 0x10B else 112)
exp_rva, exp_size = struct.unpack_from("<II", d, dd)
secs = []
so = opt + struct.unpack_from("<H", d, pe + 20)[0]
for i in range(nsec):
    name, vsize, va, rsize, raw = struct.unpack_from("<8sIIII", d, so + i * 40)
    secs.append((va, vsize, raw, rsize, name.rstrip(b"\0").decode()))
def off(rva):
    for va, vsize, raw, rsize, _ in secs:
        if va <= rva < va + max(vsize, rsize): return raw + rva - va
    raise ValueError(hex(rva))
e = off(exp_rva)
nfunc, nnames, addr_t, name_t, ord_t = struct.unpack_from("<IIIII", d, e + 20)
funcs = [struct.unpack_from("<I", d, off(addr_t) + 4 * i)[0] for i in range(nfunc)]
names = {}
for i in range(nnames):
    nrva = struct.unpack_from("<I", d, off(name_t) + 4 * i)[0]
    o = struct.unpack_from("<H", d, off(ord_t) + 2 * i)[0]
    s = d[off(nrva):d.index(b"\0", off(nrva))].decode()
    names[funcs[o]] = s
items = sorted(names.items())
import bisect
keys = [k for k, _ in items]
print("sections:", [(hex(va), n) for va, _, _, _, n in secs])
for r in rvas:
    i = bisect.bisect_right(keys, r) - 1
    k, n = items[i]
    print("rva %06X -> %s + 0x%X" % (r, n, r - k))
