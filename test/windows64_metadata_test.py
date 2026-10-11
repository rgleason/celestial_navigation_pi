#!/usr/bin/env python3
"""Keep Windows x64 metadata tied to the checked-out release, across bumps."""
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

root = Path(sys.argv.pop(1))
sys.path.insert(0, str(root / 'ci'))
spec = importlib.util.spec_from_file_location('windows64_build', root / 'ci/circleci-build-windows64.py')
build = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build)


SOURCE_CMAKE = '\n'.join([
    'set(VERSION_MAJOR "2")', 'set(VERSION_MINOR "9")',
    'set(VERSION_PATCH "3")', 'set(VERSION_TWEAK "0")',
    'set(OCPN_API_VERSION_MAJOR "1")', 'set(OCPN_API_VERSION_MINOR "18")',
])


class WindowsMetadataTest(unittest.TestCase):
    def test_release_bump_uses_new_source_and_rejects_old_metadata(self):
        with tempfile.TemporaryDirectory() as temporary:
            cmake = Path(temporary) / 'CMakeLists.txt'
            source = SOURCE_CMAKE
            cmake.write_text(source.replace('set(VERSION_PATCH "3")', 'set(VERSION_PATCH "4")'))
            plugin, api = build.project_versions(cmake)
            self.assertEqual((plugin, api), ('2.9.4.0', '1.18'))
            xml = ET.fromstring('<plugin><target>msvc-wx32-x64</target>'
                                '<target-version>10</target-version><target-arch>x86_64</target-arch>'
                                '<version>2.9.3.0</version><api-version>1.18</api-version></plugin>')
            with self.assertRaisesRegex(RuntimeError, 'Incorrect Windows x64 metadata'):
                build.verify_metadata(xml, plugin, api)
            xml.find('version').text = plugin
            build.verify_metadata(xml, plugin, api)

    def test_current_metadata_must_match_platform_and_api(self):
        plugin, api = build.project_versions(root / 'CMakeLists.txt')
        xml = ET.fromstring('<plugin><target>msvc-wx32-x64</target>'
                            '<target-version>10</target-version><target-arch>x86_64</target-arch>'
                            '<version/><api-version/></plugin>')
        xml.find('version').text = plugin
        xml.find('api-version').text = api
        build.verify_metadata(xml, plugin, api)
        for key, bad in [('target', 'msvc'), ('target-version', '11'),
                         ('target-arch', 'x86'), ('api-version', '1.17'),
                         ('version', '2.9.2.0')]:
            with self.subTest(key=key):
                old = xml.find(key).text
                xml.find(key).text = bad
                with self.assertRaisesRegex(RuntimeError, 'Incorrect Windows x64 metadata'):
                    build.verify_metadata(xml, plugin, api)
                xml.find(key).text = old

    def test_missing_or_duplicate_source_version_fails(self):
        with tempfile.TemporaryDirectory() as temporary:
            cmake = Path(temporary) / 'CMakeLists.txt'
            source = SOURCE_CMAKE
            for content in [source.replace('set(VERSION_PATCH "3")', ''),
                            source + '\nset(VERSION_PATCH "4")\n']:
                cmake.write_text(content)
                with self.assertRaisesRegex(RuntimeError, 'VERSION_PATCH'):
                    build.project_versions(cmake)


if __name__ == '__main__':
    unittest.main()
