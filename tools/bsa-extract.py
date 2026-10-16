#!/usr/bin/env python3
"""Lists or extracts files from an Oblivion BSA (version 103).

Usage: bsa_extract.py <archive.bsa> list [substring]
       bsa_extract.py <archive.bsa> extract <path-in-archive> <out-file>
"""
import struct
import sys
import zlib


def read_bzstring(data, at):
    n = data[at]
    s = data[at + 1:at + 1 + n]
    return s.rstrip(bytes([0])).decode('latin-1'), at + 1 + n


def load(path):
    with open(path, 'rb') as f:
        data = f.read()
    magic, version, folder_offset, flags, folder_count, file_count, folder_names_len, file_names_len, file_flags = struct.unpack_from('<4sIIIIIIII', data, 0)
    if magic != b'BSA' + bytes([0]) or version != 103:
        raise SystemExit('not an Oblivion BSA: %r %d' % (magic, version))
    compressed_default = (flags & 0x4) != 0
    at = folder_offset
    folders = []
    for _ in range(folder_count):
        h, count, offset = struct.unpack_from('<QII', data, at)
        folders.append((count, offset))
        at += 16
    entries = []
    for count, offset in folders:
        at = offset - file_names_len
        name, at = read_bzstring(data, at)
        for _ in range(count):
            h, size, off = struct.unpack_from('<QII', data, at)
            entries.append([name, size, off])
            at += 16
    names_at = at
    names = data[names_at:names_at + file_names_len].split(bytes([0]))
    files = {}
    for i, e in enumerate(entries):
        folder, size, off = e
        full = (folder + '/' + names[i].decode('latin-1')).lower().replace(chr(92), '/')
        files[full] = (size, off)
    return data, files, compressed_default


def main():
    archive = sys.argv[1]
    data, files, compressed_default = load(archive)
    if sys.argv[2] == 'list':
        sub = sys.argv[3].lower() if len(sys.argv) > 3 else ''
        for name in sorted(files):
            if sub in name:
                print(name, files[name][0])
        return
    want = sys.argv[3].lower().replace(chr(92), '/')
    size, off = files[want]
    compressed = compressed_default
    if size & 0x40000000:
        compressed = not compressed
        size &= ~0x40000000
    blob = data[off:off + size]
    if compressed:
        original = struct.unpack_from('<I', blob, 0)[0]
        blob = zlib.decompress(blob[4:])
        assert len(blob) == original, (len(blob), original)
    with open(sys.argv[4], 'wb') as f:
        f.write(blob)
    print('wrote', sys.argv[4], len(blob), 'bytes', 'compressed' if compressed else 'stored')


if __name__ == '__main__':
    main()
