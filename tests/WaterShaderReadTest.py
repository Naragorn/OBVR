import struct
import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from water_shader_read import read_water_shaders


class WaterShaderReadTest(unittest.TestCase):
    def test_present_absent_and_partial(self):
        for water in (0, 0x10000):
            for a in (0, 0x20000):
                for b in (0, 0x30000):
                    for ha in (0, 0x40000):
                        for hb in (0, 0x50000):
                            memory = {0xB45DCC: water, 0x100BC: a, 0x100C0: b,
                                      0x20030: ha, 0x30030: hb}
                            result = read_water_shaders(lambda p, n: struct.pack('<I', memory[p]))
                            self.assertEqual(result['handles'], [ha if water and a else 0, hb if water and b else 0])

    def test_bad_reads_at_each_level(self):
        memory = {0xB45DCC: 0x10000, 0x100BC: 0x20000, 0x100C0: 0x30000,
                  0x20030: 0x40000, 0x30030: 0x50000}
        for bad_address in memory:
            for bad in (b'', struct.pack('<I', 4), struct.pack('<I', 0x10001), struct.pack('<I', 0xffff0000)):
                with self.subTest(address=bad_address, bad=bad), self.assertRaises(ValueError):
                    read_water_shaders(lambda p, n: bad if p == bad_address else struct.pack('<I', memory[p]))
        calls = 0
        def changing(p, n):
            nonlocal calls
            calls += 1
            return struct.pack('<I', 0 if calls == 6 else memory[p])
        with self.assertRaisesRegex(ValueError, 'changed'):
            read_water_shaders(changing)
        def inaccessible(p, n):
            raise OSError('unreadable')
        with self.assertRaises(OSError):
            read_water_shaders(inaccessible)


if __name__ == '__main__':
    unittest.main()
