#!/usr/bin/env python3
"""Prepare metadata for an already approved Cloudsmith publication."""
import argparse
from pathlib import Path
from urllib.parse import quote
import xml.etree.ElementTree as ET


def prepare(source, output, repository, name, version, filename):
    parts = repository.split('/')
    if len(parts) != 2 or any(not p or p in ('.', '..') for p in parts):
        raise ValueError('Expected a Cloudsmith owner/repository')
    if Path(filename).name != filename or not filename.endswith('.tar.gz'):
        raise ValueError('Expected a final tarball filename')
    replacements = {'--pkg_repo--': repository, '--name--': name,
                    '--version--': version, '--filename--': filename}
    text = source.read_text()
    for old, new in replacements.items():
        text = text.replace(old, new)
    root = ET.fromstring(text)
    url = root.find('tarball-url')
    if root.tag != 'plugin' or url is None:
        raise ValueError('Plugin metadata with tarball-url is required')
    url.text = ('https://dl.cloudsmith.io/public/' +
                '/'.join(quote(p, safe='') for p in parts) +
                '/raw/names/' + quote(name, safe='') +
                '/versions/' + quote(version, safe='') + '/' +
                quote(filename, safe=''))
    output.write_bytes(ET.tostring(root, encoding='utf-8', xml_declaration=True))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    for argument in ('repository', 'name', 'version', 'filename'):
        parser.add_argument(argument)
    args = parser.parse_args()
    prepare(**vars(args))
