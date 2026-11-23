#!/usr/bin/env python3
"""Decode mkfs output independently of the C structs and assert initial policy."""
from pathlib import Path
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
    tmp = Path(tmp)
    subprocess.run(['gcc', '-I.', 'mkfs/mkfs.c', '-o', str(tmp / 'mkfs')], cwd=root, check=True)
    (tmp / 'user').mkdir()
    (tmp / 'user/_demo').write_bytes(b'program')
    (tmp / 'README').write_text('data')
    subprocess.run([str(tmp / 'mkfs'), 'fs.img', 'README', 'user/_demo'], cwd=tmp, check=True)
    raw = (tmp / 'fs.img').read_bytes()
    sb = struct.unpack_from('<8I', raw, 1024)
    assert sb[0] == 0x10203041
    start = sb[6] * 1024
    def inode(i):
        return struct.unpack_from('<6hI13I15I', raw, start + 128 * i)
    def entries(i):
        d = inode(i)
        block = d[7] * 1024
        result = {}
        for off in range(0, d[6], 16):
            ino, name = struct.unpack_from('<H14s', raw, block + off)
            if ino:
                result[name.split(b'\0')[0].decode()] = ino
        return result
    rootdir = entries(1)
    console = inode(rootdir['console'])
    assert console[:3] == (3, 1, 0)  # T_DEVICE, console major, minor
    assert console[4:6] == (0, 0o644)
    home = rootdir['home']
    homes = entries(home)
    expected = [(1, 0, 0o755), (home, 0, 0o755),
                (homes['isaac'], 1001, 0o700), (homes['samir'], 1002, 0o700),
                (rootdir['README'], 0, 0o644), (rootdir['demo'], 0, 0o755)]
    for i, uid, mode in expected:
        assert inode(i)[4:6] == (uid, mode), (i, inode(i))
    assert entries(homes['samir'])['..'] == home
    assert entries(home)['..'] == 1
    assert inode(1)[3] == 2 and inode(home)[3] == 3
    print('PASS: 128-byte inodes, owners, modes, executable/data distinction, parent entries and links')
