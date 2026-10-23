"""Export NX bitmaps to PNG: nxpng.py <file.nx> <out_dir> <path> [<path> ...]
A path naming a non-bitmap node exports every bitmap below it (one level of frames deep)."""
import os, struct, sys
import lz4.block
from PIL import Image
sys.path.insert(0, os.path.expanduser('~/Coding/MapleV83/client/tools'))
from nxdump import Nx

def export(nx, idx, out, name):
    name_i, children, num, ntype, payload = nx.node(idx)
    if ntype == 5:
        bid, w, h = struct.unpack_from('<IHH', payload)
        off = struct.unpack_from('<Q', nx.data, nx.bitmap_off + bid * 8)[0]
        length = struct.unpack_from('<I', nx.data, off)[0]
        raw = lz4.block.decompress(bytes(nx.data[off + 4:off + 4 + length]), uncompressed_size=w * h * 4)
        img = Image.frombytes('RGBA', (w, h), raw, 'raw', 'BGRA')
        ox = oy = 0
        for i in range(children, children + num):
            if nx.string(nx.node(i)[0]) == 'origin':
                ox, oy = struct.unpack_from('<ii', nx.node(i)[4])
        img.save(os.path.join(out, name + '.png'))
        print(f'{name}.png {w}x{h} origin=({ox},{oy})')
    for i in range(children, children + num):
        cname = nx.string(nx.node(i)[0])
        if cname != 'origin' and name.count('_') < 5:
            export(nx, i, out, f'{name}_{cname}')

nx = Nx(sys.argv[1]); nx.bitmap_off = struct.unpack_from('<I I Q I Q I Q I Q', nx.data, 0)[6]; out = sys.argv[2]; os.makedirs(out, exist_ok=True)
for p in sys.argv[3:]:
    export(nx, nx.resolve(p), out, p.replace('/', '_').replace('.img', ''))
