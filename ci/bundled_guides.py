"""Verify that a retained package includes the exact three offline guides."""
import hashlib
import tarfile
from pathlib import Path

GUIDES = ('Practical_Guide.pdf', 'Celestial_Navigation_Manual_v2.pdf',
          'Celestial_Navigation_Information.html')


def check_guides(archive_path, root):
    with tarfile.open(archive_path, 'r:gz') as archive:
        for filename in GUIDES:
            entries = [m for m in archive.getmembers()
                       if m.isfile() and Path(m.name).name == filename]
            if len(entries) != 1:
                raise ValueError('Expected exactly one bundled ' + filename)
            actual = archive.extractfile(entries[0]).read()
            expected = (Path(root) / 'data' / filename).read_bytes()
            # Git for Windows can convert checked-out HTML to CRLF, but PDF
            # bytes must remain exact on every platform.
            if filename.endswith('.html'):
                actual, expected = actual.replace(b'\r\n', b'\n'), expected.replace(b'\r\n', b'\n')
            if actual != expected:
                raise ValueError('Bundled guide differs from source: ' + filename)
    return {name: hashlib.sha256((Path(root) / 'data' / name).read_bytes()).hexdigest()
            for name in GUIDES}
