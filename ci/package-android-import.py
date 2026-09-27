#!/usr/bin/env python3
"""Create and verify a manual-import Android archive with its exact download URL."""
import argparse
import hashlib
import io
import json
import os
import tempfile
import tarfile
import urllib.parse
import xml.etree.ElementTree as ET
from pathlib import Path


def package(source: Path, metadata: Path, output: Path, url: str, sha: str) -> None:
    parsed = urllib.parse.urlparse(url)
    if parsed.scheme != 'https' or not parsed.netloc or Path(parsed.path).name != output.name or '--' in url:
        raise ValueError('An exact HTTPS URL ending in the final archive filename is required')
    root = ET.fromstring(metadata.read_bytes())
    values = {child.tag: (child.text or '').strip() for child in root}
    if values.get('name') != 'Celestial Navigation' or values.get('version') != '2.8.13.0':
        raise ValueError('Unexpected plugin identity/version')
    if values.get('target') not in ('android-arm64', 'android-armhf') or values.get('api-version') != '1.18':
        raise ValueError('Unexpected Android target/plugin API')
    root.find('tarball-url').text = url
    root.find('source').text = 'https://github.com/pob220/celestial_navigation_pi/tree/' + sha
    xml = ET.tostring(root, encoding='utf-8', xml_declaration=True)
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
                names.append(member.name)
                result.addfile(member, archive.extractfile(member) if member.isfile() else None)
            info = tarfile.TarInfo('metadata.xml')
            info.size, info.mode = len(xml), 0o644
            result.addfile(info, io.BytesIO(xml))
        if not any(name.endswith('/lib/opencpn/libcelestial_navigation_pi.so') for name in names):
            raise ValueError('Plugin library absent')
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
                      version=values['version'], tarball_url=url, sha256=digest)
    output.with_suffix('').with_suffix('.provenance.json').write_text(json.dumps(provenance, indent=2) + '\n')
    print(digest + '  ' + str(output))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('metadata', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--url', required=True)
    parser.add_argument('--source-sha', required=True)
    args = parser.parse_args()
    if len(args.source_sha) != 40 or any(c not in '0123456789abcdef' for c in args.source_sha):
        parser.error('A full Git source SHA is required')
    package(args.source, args.metadata, args.output, args.url, args.source_sha)
