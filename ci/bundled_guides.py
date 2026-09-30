"""Verify complete offline guides, including every HTML page/illustration."""
import hashlib
import tarfile
from pathlib import Path

GUIDES = ('Practical_Guide.pdf', 'Practical_Guide.html', 'Celestial_Navigation_Manual_v2.pdf',
          'Celestial_Navigation_Information.html')
DESKTOP_REFERENCES = ('Celestial_Navigation_Definitions.html',)
ANDROID_GUIDES = ('Android_Quick_Guide.html', 'Celestial_Navigation_Manual_v2.pdf',
                  'Celestial_Navigation_Information.html')


def check_guides(archive_path, root, target=None):
    android = (target in ('android-arm64', 'android-armhf') if target is not None
               else '-android-' in Path(archive_path).name.lower())
    required = ANDROID_GUIDES if android else GUIDES
    with tarfile.open(archive_path, 'r:gz') as archive:
        filenames = list(required) + ([] if android else list(DESKTOP_REFERENCES) + ['practical-guide/' + p.name for p in
                                   sorted((Path(root) / 'data/practical-guide').glob('*'))
                                   if p.is_file()])
        members = archive.getmembers()
        if android and any(m.isfile() and (Path(m.name).name in GUIDES[:2] or
                                           '/practical-guide/' in m.name) for m in members):
            raise ValueError('Desktop practical guide must not be bundled for Android')
        for filename in filenames:
            entries = [m for m in members
                       if m.isfile() and (m.name == filename or m.name.endswith('/' + filename))]
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
            for name in required}
