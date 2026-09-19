import ctypes as c
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from water_camera_read import process_reader

class Kernel:
    def __init__(self):self.open=True;self.ok=True;self.partial=False;self.close=True;self.closed=[]
    def OpenProcess(self,access,inherit,pid):
        assert access==0x0010 and not inherit
        return 123 if self.open else None
    def ReadProcessMemory(self,handle,address,buffer,size,count):
        c.memset(buffer,65,size)
        c.cast(count,c.POINTER(c.c_size_t))[0]=size-1 if self.partial else size
        return self.ok
    def CloseHandle(self,handle):self.closed.append(handle);return self.close

class ReaderTest(unittest.TestCase):
    def test_read_and_refusals(self):
        for ok in (False,True):
            for partial in (False,True):
                k=Kernel();k.ok=ok;k.partial=partial
                with process_reader(1,k) as read:
                    if ok and not partial:self.assertEqual(read(0x10000,4),b'AAAA')
                    else:
                        with self.assertRaises(OSError):read(0x10000,4)
                    for a,n in ((-1,4),(0,0),(0,4097),(0xffffffff,4),(0x100000000,1)):
                        with self.assertRaises(ValueError):read(a,n)
                self.assertEqual(k.closed,[123])
    def test_lifetime(self):
        for pid in (0,-1,True,2**32,1.5):
            k=Kernel()
            with self.assertRaises(ValueError):
                with process_reader(pid,k):pass
            self.assertEqual(k.closed,[])
        k=Kernel();k.open=False
        with self.assertRaises(OSError):
            with process_reader(1,k):pass
        self.assertEqual(k.closed,[])
        k=Kernel()
        with self.assertRaises(RuntimeError):
            with process_reader(1,k):raise RuntimeError('decode failed')
        self.assertEqual(k.closed,[123])
        k=Kernel();k.close=False
        with self.assertRaisesRegex(OSError,'CloseHandle'):
            with process_reader(1,k):pass

if __name__=='__main__':unittest.main()
