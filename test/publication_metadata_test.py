"""Exercise the catalogue schema on finished external/embedded package pairs."""
import importlib.util
import io
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import unittest
import json
import hashlib
import os
import sys
from unittest.mock import patch
import xml.etree.ElementTree as ET

ROOT = Path(sys.argv.pop(1)).resolve()
sys.path.insert(0, str(ROOT / 'ci'))
spec = importlib.util.spec_from_file_location('catalogue_validator', ROOT / 'ci/validate-catalogue-packages.py')
validator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(validator)


def metadata(target='msvc-64', build_target='msvc'):
    root = ET.Element('plugin', version='1')
    for key, value in [('name', 'WeatherRouting'), ('version', '1.30.1'), ('release', '0'),
                       ('summary', 'Weather routing'), ('api-version', '1.21'),
                       ('open-source', 'yes'), ('author', 'Team'),
                       ('source', 'https://github.com/rgleason/weather_routing_pi'),
                       ('description', 'Weather routing'), ('target', target),
                       ('build-target', build_target), ('target-version', '10'),
                       ('target-arch', 'x86_64'),
                       ('tarball-url', 'https://example.test/plugin.tar.gz')]:
        ET.SubElement(root, key).text = value
    return ET.tostring(root, encoding='utf-8', xml_declaration=True)


@unittest.skipUnless(shutil.which('xmllint'), 'Offline publication gate requires xmllint')
class CatalogueMetadata(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)

    def pair(self, external, embedded=None):
        (self.root / 'plugin.xml').write_bytes(external)
        with tarfile.open(self.root / 'plugin.tar.gz', 'w:gz') as archive:
            data = embedded if embedded is not None else external
            member = tarfile.TarInfo('./metadata.xml')
            member.size = len(data)
            archive.addfile(member, io.BytesIO(data))

    def test_correct_windows64_pair_passes(self):
        self.pair(metadata())
        validator.validate(self.root)

    def test_published_windows64_build_target_regression_is_rejected(self):
        self.pair(metadata(build_target='msvc-64'))
        with self.assertRaisesRegex(ValueError, 'schema validation'):
            validator.validate(self.root)

    def test_celnav_unsupported_windows64_target_is_rejected(self):
        self.pair(metadata(target='msvc-wx32-x64'))
        with self.assertRaisesRegex(ValueError, 'schema validation'):
            validator.validate(self.root)

    def test_fixing_only_external_xml_does_not_pass(self):
        self.pair(metadata(), metadata(build_target='msvc-64'))
        with self.assertRaisesRegex(ValueError, 'schema validation'):
            validator.validate(self.root)

    def test_schema_valid_but_different_embedded_source_is_rejected(self):
        self.pair(metadata(), metadata().replace(b'rgleason', b'pob220'))
        with self.assertRaisesRegex(ValueError, 'metadata differ'):
            validator.validate(self.root)

    def test_windows64_cmake_generates_separate_compatibility_and_build_targets(self):
        source = (ROOT / 'cmake/PluginSetup.cmake').read_text()
        branch = source.split('elseif (MSVC)\n', 1)[1].split('elseif (APPLE)', 1)[0]
        branch += '\n' + source.split('if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)\n', 1)[1].split('endif()', 1)[0]
        script = self.root / 'windows64.cmake'
        script.write_text('set(PKG_TARGET \"msvc-64\")\nset(CMAKE_SIZEOF_VOID_P 8)\nset(ENV{WX_VER} 32)\n' + branch +
                          '\nif(NOT PKG_TARGET STREQUAL "msvc-64" OR NOT PKG_BUILD_TARGET STREQUAL "msvc")\n'
                          'message(FATAL_ERROR "Incorrect Windows x64 metadata generators")\nendif()\n')
        subprocess.run(['cmake', '-P', str(script)], check=True, capture_output=True)


class PublicationStaging(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        spec = importlib.util.spec_from_file_location('stage_publication', ROOT / 'ci/stage-publication.py')
        self.stage = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.stage)
        self.sha = 'a' * 40
        self.source = self.root / 'retained' / 'windows64'
        self.source.mkdir(parents=True)
        self.archive = self.source / 'plugin.tar.gz'
        with tarfile.open(self.archive, 'w:gz') as archive:
            data = b'unchanged native DLL bytes'
            member = tarfile.TarInfo('plugins/celestial_navigation_pi.dll')
            member.size = len(data)
            archive.addfile(member, io.BytesIO(data))
        xml = metadata().replace(b'WeatherRouting', b'Celestial Navigation').replace(b'1.30.1', b'2.9.10.0')
        (self.source / 'plugin.xml').write_bytes(xml)
        self.provenance = self.source / 'build-provenance.json'
        self.provenance.write_text(json.dumps(dict(source_sha=self.sha, packages=[dict(
            name=self.archive.name, sha256=hashlib.sha256(self.archive.read_bytes()).hexdigest()), dict(
            name='plugin.xml', sha256=hashlib.sha256(xml).hexdigest())])))
        self.output = self.root / 'reviewed'

    def prepare(self):
        with patch.dict(os.environ, {'CIRCLE_PROJECT_USERNAME': 'rgleason',
                                     'CIRCLE_PROJECT_REPONAME': 'celestial_navigation_pi'}):
            self.stage.stage(self.source.parent, self.output,
                             'opencpn/celestial-navigation-prod', self.sha, '123')

    def test_final_metadata_matches_archive_and_source_and_preserves_dll(self):
        self.prepare()
        archive = next(self.output.rglob('*.tar.gz'))
        with tarfile.open(archive) as package:
            xml = ET.fromstring(package.extractfile('metadata.xml').read())
            self.assertEqual(xml.findtext('source'),
                'https://github.com/rgleason/celestial_navigation_pi/tree/' + self.sha)
            self.assertIn('/opencpn/celestial-navigation-prod/', xml.findtext('tarball-url'))
            self.assertEqual(package.extractfile('plugins/celestial_navigation_pi.dll').read(),
                             b'unchanged native DLL bytes')
        validator.validate(self.output)

    def test_source_repository_follows_the_actual_ci_fork(self):
        from plugin_version import source_repository
        for owner in ('rgleason', 'pob220'):
            with patch.dict(os.environ, {'CIRCLE_PROJECT_USERNAME': owner,
                                         'CIRCLE_PROJECT_REPONAME': 'celestial_navigation_pi'}):
                self.assertEqual(source_repository(),
                                 'https://github.com/' + owner + '/celestial_navigation_pi')
        with patch.dict(os.environ, {}, clear=True):
            self.assertEqual(source_repository(), 'https://github.com/rgleason/celestial_navigation_pi')

    def test_stale_workflow_artifacts_are_rejected(self):
        data = json.loads(self.provenance.read_text()); data['source_sha'] = 'b' * 40
        self.provenance.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError, 'different commit'):
            self.prepare()

    def test_changed_retained_metadata_is_rejected(self):
        (self.source / 'plugin.xml').write_bytes(metadata())
        with self.assertRaisesRegex(ValueError, 'Retained metadata changed'):
            self.prepare()

    def test_changed_reviewed_file_prevents_every_upload(self):
        self.prepare()
        next(self.output.rglob('*.tar.gz')).write_bytes(b'changed')
        with patch.object(self.stage.subprocess, 'run') as upload:
            with self.assertRaisesRegex(ValueError, 'changed'):
                self.stage.publish(self.output, 'opencpn/celestial-navigation-prod')
            upload.assert_not_called()

    def test_destination_change_prevents_every_upload(self):
        self.prepare()
        with patch.object(self.stage.subprocess, 'run') as upload:
            with self.assertRaisesRegex(ValueError, 'repository differs'):
                self.stage.publish(self.output, 'someone/else')
            upload.assert_not_called()


if __name__ == '__main__':
    unittest.main()
