"""Read native camera metadata without loading OBVR or writing game memory.

Win32 signatures: learn.microsoft.com/en-us/windows/win32/api/
memoryapi/nf-memoryapi-readprocessmemory,
processthreadsapi/nf-processthreadsapi-openprocess,
handleapi/nf-handleapi-closehandle.
"""
import argparse
import ctypes as c
import json
from contextlib import contextmanager
from pathlib import Path
from water_camera_decode import read_camera


def bind_kernel():
    kernel = c.WinDLL('kernel32', use_last_error=True)
    kernel.OpenProcess.argtypes = [c.c_uint32,c.c_int,c.c_uint32]
    kernel.OpenProcess.restype = c.c_void_p
    kernel.ReadProcessMemory.argtypes = [c.c_void_p,c.c_void_p,c.c_void_p,c.c_size_t,c.POINTER(c.c_size_t)]
    kernel.ReadProcessMemory.restype = c.c_int
    kernel.CloseHandle.argtypes = [c.c_void_p]
    kernel.CloseHandle.restype = c.c_int
    return kernel


@contextmanager
def process_reader(pid, kernel):
    if not isinstance(pid,int) or isinstance(pid,bool) or not 0 < pid <= 0xffffffff:
        raise ValueError('Invalid process ID')
    handle = kernel.OpenProcess(0x0010, False, pid)  # PROCESS_VM_READ only
    if not handle:
        raise OSError('OpenProcess failed')
    try:
        def read(address, size):
            if not 0 <= address <= 0xffffffff or not 0 < size <= 4096 or address+size > 0x100000000:
                raise ValueError('Invalid bounded read')
            buffer = c.create_string_buffer(size)
            count = c.c_size_t()
            ok = kernel.ReadProcessMemory(handle,address,buffer,size,c.byref(count))
            if not ok or count.value != size:
                raise OSError('ReadProcessMemory failed or returned partial data')
            return buffer.raw
        yield read
    finally:
        if not kernel.CloseHandle(handle):
            raise OSError('CloseHandle failed')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pid',type=int,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    with process_reader(args.pid,bind_kernel()) as read:
        result=read_camera(read)
    result['processId']=args.pid
    result['limitation']='Consecutive matching reads; not synchronized to screenshot or render frame.'
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()
