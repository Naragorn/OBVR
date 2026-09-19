import struct
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from water_camera_decode import decode_camera, read_camera

T = [1,0,0,0,1,0,0,0,1,12,34,56,1]
F = [-1,1,.75,-.75,10,10000]
def packed(t=T,f=F,ortho=0):
    return struct.pack('<13f',*t),struct.pack('<6fB3x',*f,ortho)

class CameraDecodeTest(unittest.TestCase):
    def test_decode(self):
        self.assertEqual(decode_camera(*packed())['position'],(12,34,56))
        for i,value in ((0,2),(1,.5),(12,0),(12,-1),(9,float('nan'))):
            t=T.copy();t[i]=value
            with self.subTest(t=t),self.assertRaises(ValueError):decode_camera(*packed(t=t))
        for i,value in ((0,2),(2,-2),(4,0),(5,1),(1,float('inf'))):
            f=F.copy();f[i]=value
            with self.subTest(f=f),self.assertRaises(ValueError):decode_camera(*packed(f=f))
        with self.assertRaises(ValueError):decode_camera(*packed(ortho=1))
        for a,b in ((b'',packed()[1]),(packed()[0],b'')):
            with self.assertRaises(ValueError):decode_camera(a,b)

    def test_read_flows(self):
        t,f=packed()
        memory={0xB333CC:struct.pack('<I',0x10000),0x100DC:struct.pack('<I',0x20000),0x20064:t,0x200EC:f}
        def read(a,n):return memory[a]
        self.assertEqual(read_camera(read)['cameraAddress'],0x20000)
        for address in (0xB333CC,0x100DC):
            original=memory[address]
            for value in (b'',struct.pack('<I',0),struct.pack('<I',0x10001),struct.pack('<I',0xffffffff)):
                memory[address]=value
                with self.assertRaises(ValueError):read_camera(read)
            memory[address]=original
        calls=0
        def moving(a,n):
            nonlocal calls
            calls+=1
            return bytes(n) if calls==5 else read(a,n)
        with self.assertRaisesRegex(ValueError,'changed'):read_camera(moving)
        def inaccessible(a,n):raise OSError('read failed')
        with self.assertRaises(OSError):read_camera(inaccessible)

if __name__=='__main__':unittest.main()
