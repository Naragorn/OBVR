"""Read water shader identities without modifying the game.

Layout evidence: src/render/WaterReprojection.cpp, kWaterShaderPointer,
kVertexArrayOffset, kVertexShaderHandleOffset. This observes identities,
not COM ownership or bytecode; unchanged addresses do not prove same objects.
"""
import argparse
import json
import struct
from pathlib import Path
from water_camera_read import bind_kernel, process_reader


def read_water_shaders(read):
    def pointer(address):
        data = read(address, 4)
        if len(data) != 4:
            raise ValueError('Incomplete shader pointer')
        value, = struct.unpack('<I', data)
        if value and not (0x10000 <= value <= 0xfffeffff and value % 4 == 0):
            raise ValueError('Invalid shader pointer')
        return value

    def snapshot():
        water = pointer(0xB45DCC)
        wrappers = [pointer(water + 0xBC + i * 4) for i in range(2)] if water else [0, 0]
        handles = [pointer(wrapper + 0x30) if wrapper else 0 for wrapper in wrappers]
        return {'water': water, 'wrappers': wrappers, 'handles': handles}

    first = snapshot()
    if first != snapshot():
        raise ValueError('Water shaders changed during snapshot')
    return first


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pid', type=int, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    with process_reader(args.pid, bind_kernel()) as read:
        result = read_water_shaders(read)
    result['processId'] = args.pid
    result['limitation'] = 'Two matching reads, not synchronized to a render frame; addresses only.'
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
