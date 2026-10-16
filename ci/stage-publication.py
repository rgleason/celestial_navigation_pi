#!/usr/bin/env python3
"""Prepare every final package and validate it before the publication hold."""
import argparse
import hashlib
import importlib.util
import io
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import tarfile
import xml.etree.ElementTree as ET


def module(filename):
    spec = importlib.util.spec_from_file_location(filename.replace('-', '_'),
                                                 Path(__file__).with_name(filename + '.py'))
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


metadata = module('prepare-published-metadata')
validator = module('validate-catalogue-packages')


def stage(artifacts, output, repository, revision, build_number):
    if not re.fullmatch(r'[0-9a-f]{40}', revision) or not build_number.isdigit():
        raise ValueError('Expected a full source SHA and numeric build number')
    archives = sorted(artifacts.rglob('*.tar.gz'))
    if not archives:
        raise ValueError('No retained packages found')
    output.mkdir(parents=True, exist_ok=False)
    uploads = []
    versions = set()
    identities = set()
    for index, archive in enumerate(archives):
        provenance = archive.parent / 'build-provenance.json'
        source = json.loads(provenance.read_text())
        if source['source_sha'] != revision:
            raise ValueError('Retained artifacts are from a different commit: ' + str(archive))
        record = next((r for r in source['packages'] if r['name'] == archive.name), None)
        if not record or hashlib.sha256(archive.read_bytes()).hexdigest() != record['sha256']:
            raise ValueError('Retained archive changed after its build: ' + str(archive))
        candidates = [p for p in archive.parent.glob('*.xml')
                      if ET.parse(p).getroot().tag == 'plugin']
        if len(candidates) != 1:
            raise ValueError('Expected exactly one metadata file beside ' + str(archive))
        xml_record = next((r for r in source['packages'] if r['name'] == candidates[0].name), None)
        if not xml_record or hashlib.sha256(candidates[0].read_bytes()).hexdigest() != xml_record['sha256']:
            raise ValueError('Retained metadata changed after its build: ' + str(candidates[0]))
        root = ET.parse(candidates[0]).getroot()
        value = lambda key: (root.findtext(key) or '').strip()
        version = value('version')
        if value('name') != 'Celestial Navigation' or not re.fullmatch(r'\d+\.\d+\.\d+\.\d+', version):
            raise ValueError('Unexpected plugin identity/version')
        versions.add(version)
        identity = '-'.join(value(k) for k in ('target', 'target-arch', 'target-version'))
        identity = re.sub(r'[/_ ]', '-', identity)
        if identity in identities:
            raise ValueError('Duplicate platform identity: ' + identity)
        identities.add(identity)
        cloud_version = f'{version}+{build_number}.{revision[:7]}'
        base = f'celestial_navigation_pi-{version}-{identity}'
        directory = output / str(index)
        directory.mkdir()
        final_xml = directory / (base + '-metadata.xml')
        metadata.prepare(candidates[0], final_xml, repository, base + '-tarball',
                         cloud_version, archive.name, revision=revision)
        xml = final_xml.read_bytes()
        final_archive = directory / archive.name
        with tarfile.open(archive, 'r:gz') as original, tarfile.open(final_archive, 'w:gz') as final:
            for member in original:
                path = PurePosixPath(member.name)
                if path.is_absolute() or '..' in path.parts or not (member.isfile() or member.isdir()):
                    raise ValueError('Unsafe archive member: ' + member.name)
                if path.as_posix() == 'metadata.xml' or (path.parts and path.parts[0] == 'root'):
                    continue
                final.addfile(member, original.extractfile(member) if member.isfile() else None)
            member = tarfile.TarInfo('metadata.xml')
            member.size, member.mode = len(xml), 0o644
            final.addfile(member, io.BytesIO(xml))
        for file, name in ((final_xml, base + '-metadata'), (final_archive, base + '-tarball')):
            uploads.append(dict(file=str(file.relative_to(output)), name=name,
                                version=cloud_version,
                                sha256=hashlib.sha256(file.read_bytes()).hexdigest()))
    if len(versions) != 1:
        raise ValueError('Mixed plugin versions in retained packages')
    validator.validate(output)
    manifest = dict(repository=repository, source_sha=revision, uploads=uploads)
    (output / 'publication.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'Review ready: {len(archives)} package pairs -> {repository}; source {revision}')


def publish(output, repository):
    manifest = json.loads((output / 'publication.json').read_text())
    if repository != manifest['repository']:
        raise ValueError('Deployment repository differs from the reviewed publication')
    uploads = manifest['uploads']
    if not uploads or len({item['file'] for item in uploads}) != len(uploads):
        raise ValueError('Empty or duplicate publication files')
    # Check the entire publication before the first external write.
    for item in uploads:
        path = (output / item['file']).resolve()
        if not path.is_relative_to(output.resolve()):
            raise ValueError('Publication file escapes the staging directory')
        if hashlib.sha256(path.read_bytes()).hexdigest() != item['sha256']:
            raise ValueError('Reviewed publication changed: ' + item['file'])
    validator.validate(output)
    for item in uploads:
        subprocess.run(['cloudsmith', 'push', 'raw', '--no-republish', '--no-wait-for-sync',
                        '--name', item['name'], '--version', item['version'],
                        '--summary', 'Celestial Navigation OpenCPN plugin', repository,
                        str(output / item['file'])], check=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('stage', 'publish'))
    parser.add_argument('--artifacts', type=Path, default=Path('retained-artifacts'))
    parser.add_argument('--output', type=Path, default=Path('reviewed-publication'))
    parser.add_argument('--repository', required=True)
    parser.add_argument('--revision')
    parser.add_argument('--build-number')
    args = parser.parse_args()
    if args.action == 'stage':
        stage(args.artifacts, args.output, args.repository, args.revision or '', args.build_number or '')
    else:
        publish(args.output, args.repository)
