#!/usr/bin/env python3
"""Create and verify a manual-import Android archive with its exact download URL."""
import argparse
import hashlib
import io
import json
import os
import struct
import tempfile
import tarfile
import urllib.parse
import xml.etree.ElementTree as ET
from pathlib import Path


def allocated_sections(binary: bytes) -> dict:
    """Compare executable ELF content across debug stripping, for either ABI."""
    if binary[:4] != b'\x7fELF' or binary[4] not in (1, 2) or binary[5] not in (1, 2):
        raise ValueError('Expected a supported ELF plugin library')
    endian = '<' if binary[5] == 1 else '>'
    wide = binary[4] == 2
    header = struct.unpack_from(endian + ('HHIQQQIHHHHHH' if wide else 'HHIIIIIHHHHHH'), binary, 16)
    offset, entry_size, count, names_index = header[5], header[10], header[11], header[12]
    fmt = endian + ('IIQQQQIIQQ' if wide else 'IIIIIIIIII')
    entries = [struct.unpack_from(fmt, binary, offset + i * entry_size) for i in range(count)]
    names_header = entries[names_index]
    names = binary[names_header[4]:names_header[4] + names_header[5]]
    result = {'ELF identity': (binary[4], binary[5], header[1])}
    for name_offset, kind, flags, address, start, size, *_ in entries:
        if not flags & 2:  # SHF_ALLOC: all runtime code, data, symbols and notes.
            continue
        name = names[name_offset:names.index(b'\0', name_offset)].decode('ascii')
        if kind != 8 and start + size > len(binary):
            raise ValueError('Truncated ELF section: ' + name)
        result[name] = (address, size, None if kind == 8 else
                        hashlib.sha256(binary[start:start + size]).hexdigest())
    if '.text' not in result:
        raise ValueError('ELF executable code absent')
    return result


def package(source: Path, metadata: Path, output: Path, url: str, sha: str,
            library: Path) -> None:
    parsed = urllib.parse.urlparse(url)
    if parsed.scheme != 'https' or not parsed.netloc or Path(parsed.path).name != output.name or '--' in url:
        raise ValueError('An exact HTTPS URL ending in the final archive filename is required')
    root = ET.fromstring(metadata.read_bytes())
    values = {child.tag: (child.text or '').strip() for child in root}
    if values.get('name') != 'Celestial Navigation' or values.get('version') != '2.9.2.0':
        raise ValueError('Unexpected plugin identity/version')
    if values.get('target') not in ('android-arm64', 'android-armhf') or values.get('api-version') != '1.18':
        raise ValueError('Unexpected Android target/plugin API')
    root.find('tarball-url').text = url
    root.find('source').text = 'https://github.com/pob220/celestial_navigation_pi/tree/' + sha
    xml = ET.tostring(root, encoding='utf-8', xml_declaration=True)
    expected_sections = allocated_sections(library.read_bytes())
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(prefix=output.name + '.', suffix='.tmp',
                                     dir=output.parent, delete=False) as temporary:
        staged = Path(temporary.name)
    try:
        names = []
        with tarfile.open(source, 'r:gz') as archive, tarfile.open(staged, 'w:gz') as result:
            for member in archive:
                path = Path(member.name)
                if path.is_absolute() or '..' in path.parts or not (member.isfile() or member.isdir()):
                    raise ValueError('Unsafe archive entry: ' + member.name)
                if str(path) == 'metadata.xml':
                    continue
                if member.name.endswith('/lib/opencpn/libcelestial_navigation_pi.so'):
                    actual = archive.extractfile(member).read()
                    if allocated_sections(actual) != expected_sections:
                        raise ValueError('Archive library differs from the intended build: ' + str(library))
                names.append(member.name)
                result.addfile(member, archive.extractfile(member) if member.isfile() else None)
            info = tarfile.TarInfo('metadata.xml')
            info.size, info.mode = len(xml), 0o644
            result.addfile(info, io.BytesIO(xml))
        if sum(name.endswith('/lib/opencpn/libcelestial_navigation_pi.so') for name in names) != 1:
            raise ValueError('Exactly one plugin library is required')
        if not any(name.endswith('/data/vsop87d.txt') for name in names):
            raise ValueError('Required analytical ephemeris absent')
        with tarfile.open(staged, 'r:gz') as check:
            if check.getnames().count('metadata.xml') != 1:
                raise ValueError('Exactly one root metadata.xml is required')
            embedded = ET.fromstring(check.extractfile('metadata.xml').read())
            if embedded.findtext('tarball-url') != url:
                raise ValueError('Embedded URL mismatch')
        os.replace(staged, output)
    finally:
        staged.unlink(missing_ok=True)
    output.with_suffix('').with_suffix('.xml').write_bytes(xml)
    digest = hashlib.sha256(output.read_bytes()).hexdigest()
    provenance = dict(source_sha=sha, target=values['target'], plugin_api=values['api-version'],
                      version=values['version'], tarball_url=url, sha256=digest,
                      unstripped_library_sha256=hashlib.sha256(library.read_bytes()).hexdigest())
    output.with_suffix('').with_suffix('.provenance.json').write_text(json.dumps(provenance, indent=2) + '\n')
    print(digest + '  ' + str(output))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('metadata', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--url', required=True)
    parser.add_argument('--source-sha', required=True)
    parser.add_argument('--library', type=Path, required=True,
                        help='Exact build library; runtime ELF sections must match the archive')
    args = parser.parse_args()
    if len(args.source_sha) != 40 or any(c not in '0123456789abcdef' for c in args.source_sha):
        parser.error('A full Git source SHA is required')
    package(args.source, args.metadata, args.output, args.url, args.source_sha, args.library)
