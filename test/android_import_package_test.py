"""Exercise the actual final Android manual-import packaging path."""
import importlib.util
import io
from pathlib import Path
import struct
import sys
import tarfile
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'ci'))
from bundled_guides import ANDROID_GUIDES
from plugin_version import plugin_version

spec = importlib.util.spec_from_file_location('android_import', ROOT / 'ci/package-android-import.py')
packer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packer)


def elf_fixture(abi):
    # Small section-table fixture, not a loadable plugin. Exercises the exact
    # ELF runtime-section comparison without cross-compiling during unit tests.
    names = b'\0.shstrtab\0.text\0'
    wide = abi == 'arm64'
    header_size, section_size = (64, 64) if wide else (52, 40)
    names_start = header_size + 3 * section_size
    text_start = names_start + len(names)
    identity = b'\x7fELF' + bytes((2 if wide else 1, 1, 1)) + bytes(9)
    header = struct.pack('<HHIQQQIHHHHHH' if wide else '<HHIIIIIHHHHHH',
                         3, 183 if wide else 40, 1, 0, 0, header_size, 0,
                         header_size, 0, 0, section_size, 3, 1)
    section_format = '<IIQQQQIIQQ' if wide else '<IIIIIIIIII'
    sections = bytes(section_size)
    sections += struct.pack(section_format, 1, 3, 0, 0, names_start, len(names), 0, 0, 1, 0)
    sections += struct.pack(section_format, names.index(b'.text'), 1, 6, 0x1000, text_start, 4, 0, 0, 4, 0)
    return identity + header + sections + names + b'code'


class AndroidImportPackage(unittest.TestCase):
    def fixtures(self, folder, exclude=None, corrupt=None, extra=None, abi='arm64'):
        source, metadata, library = [folder / name for name in ('source.tar.gz', 'source.xml', 'library.so')]
        library.write_bytes(elf_fixture(abi))
        root = ET.Element('plugin')
        for key, value in {'name': 'Celestial Navigation', 'version': plugin_version(ROOT),
                           'target': 'android-' + abi, 'api-version': '1.18',
                           'tarball-url': 'https://example.test/old.tar.gz', 'source': 'test'}.items():
            ET.SubElement(root, key).text = value
        metadata.write_bytes(ET.tostring(root))
        assets = {'package/lib/opencpn/libcelestial_navigation_pi.so': library.read_bytes(),
                  'package/share/opencpn/plugins/celestial_navigation_pi/data/vsop87d.txt': b'fixture'}
        for name in ANDROID_GUIDES:
            if name != exclude:
                assets['package/share/opencpn/plugins/celestial_navigation_pi/data/' + name] = (
                    b'corrupt' if name == corrupt else (ROOT / 'data' / name).read_bytes())
        if extra:
            assets['package/share/opencpn/plugins/celestial_navigation_pi/data/' + extra] = b'unwanted desktop guide'
        with tarfile.open(source, 'w:gz') as archive:
            for name, content in assets.items():
                entry = tarfile.TarInfo(name)
                entry.size = len(content)
                archive.addfile(entry, io.BytesIO(content))
        return source, metadata, library

    def test_android_only_guides_and_root_metadata_succeed(self):
        for abi in ('arm64', 'armhf'):
            with self.subTest(abi=abi), tempfile.TemporaryDirectory() as temp:
                folder = Path(temp)
                source, metadata, library = self.fixtures(folder, abi=abi)
                # Explicit target must work even with a staging filename
                # which does not contain the word "android".
                output = folder / 'manual-import.tar.gz'
                url = 'https://example.test/' + output.name
                packer.package(source, metadata, output, url, 'a' * 40, library)
                with tarfile.open(output) as archive:
                    self.assertEqual(archive.getnames().count('metadata.xml'), 1)
                    xml = ET.fromstring(archive.extractfile('metadata.xml').read())
                    self.assertEqual(xml.findtext('tarball-url'), url)
                    self.assertEqual(xml.findtext('target'), 'android-' + abi)
                    self.assertFalse(any('Practical_Guide' in name for name in archive.getnames()))

    def test_missing_corrupt_and_desktop_guides_fail_without_replacing_output(self):
        cases = [{'exclude': 'Android_Quick_Guide.html'},
                 {'corrupt': 'Celestial_Navigation_Manual_v2.pdf'},
                 {'extra': 'Practical_Guide.pdf'}, {'extra': 'Practical_Guide.html'},
                 {'extra': 'practical-guide/page-09-figure-1.png'}]
        for case in cases:
            with self.subTest(case=case), tempfile.TemporaryDirectory() as temp:
                folder = Path(temp)
                source, metadata, library = self.fixtures(folder, **case)
                output = folder / 'manual-import.tar.gz'
                output.write_bytes(b'previous verified output')
                with self.assertRaises(ValueError):
                    packer.package(source, metadata, output, 'https://example.test/' + output.name,
                                   'a' * 40, library)
                self.assertEqual(output.read_bytes(), b'previous verified output')
                self.assertFalse(list(folder.glob('*.tmp')))


if __name__ == '__main__':
    unittest.main()
