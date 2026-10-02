"""Reject mislabeled or mixed-architecture Windows runtime trees."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

def inspect(path):
    with path.open('rb') as f:
        if f.read(2) != b'MZ':
            raise ValueError(f'Not a PE image: {path}')
        f.seek(0x3c)
        offset = struct.unpack('<I', f.read(4))[0]
        f.seek(offset)
        if f.read(4) != b'PE\0\0':
            raise ValueError(f'Invalid PE signature: {path}')
        header = f.read(20)
        machine = struct.unpack_from('<H', header)[0]
        flags = struct.unpack_from('<H', header, 18)[0]
        magic = struct.unpack('<H', f.read(2))[0]
    if machine != 0x8664 or magic != 0x20b:
        raise ValueError(f'Not AMD64 PE32+: {path} (machine={machine:#x}, magic={magic:#x})')
    return {'machine': 'AMD64', 'pe': 'PE32+', 'large_address_aware': bool(flags & 0x20),
            'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}

def verify(root):
    result = {}
    for path in sorted(root.rglob('*')):
        if path.is_file() and path.suffix.lower() in ('.exe', '.dll'):
            info = inspect(path)
            if path.name.lower() == 'opencpn.exe' and not info['large_address_aware']:
                raise ValueError('OpenCPN must be large-address-aware')
            result[str(path.relative_to(root))] = info
    if not result:
        raise ValueError('No Windows runtime images found')
    return result

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('root', type=Path)
    args = parser.parse_args()
    result = verify(args.root)
    (args.root / 'architecture-manifest.json').write_text(json.dumps(result, indent=2) + '\n')
    print(f'Verified {len(result)} AMD64 images')
