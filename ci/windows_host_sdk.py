# SPDX-License-Identifier: GPL-3.0-or-later
"""Native SDK inspection shared by the Windows plugin qualification."""
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tarfile
import urllib.request
import xml.etree.ElementTree as ET

def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def download(url, path, expected):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists():
        partial = path.with_suffix(path.suffix + '.part')
        with urllib.request.urlopen(url, timeout=120) as response, partial.open('wb') as output:
            shutil.copyfileobj(response, output)
        if digest(partial) != expected:
            raise RuntimeError(f'Checksum mismatch: {path.name}')
        partial.replace(path)
    if digest(path) != expected:
        raise RuntimeError(f'Checksum mismatch: {path.name}')

def pe_sections(path):
    data = path.read_bytes()
    if data[:2] != b'MZ':
        raise ValueError(f'Not a PE image: {path}')
    offset = struct.unpack_from('<I', data, 60)[0]
    if data[offset:offset + 4] != b'PE\x00\x00':
        raise ValueError(f'Invalid PE signature: {path}')
    machine, count = struct.unpack_from('<HH', data, offset + 4)
    optional_size = struct.unpack_from('<H', data, offset + 20)[0]
    magic = struct.unpack_from('<H', data, offset + 24)[0]
    if machine != 34404 or magic != 523:
        raise ValueError(f'Expected AMD64 PE32+: {path}')
    sections = []
    for index in range(count):
        at = offset + 24 + optional_size + index * 40
        size, rva = struct.unpack_from('<II', data, at + 8)
        flags = struct.unpack_from('<I', data, at + 36)[0]
        sections.append((rva, rva + size, bool(flags & 536870912)))
    return sections

def exports(path):
    sections = pe_sections(path)
    text = subprocess.check_output(['dumpbin', '/exports', str(path)], text=True)
    result = {}
    for line in text.splitlines():
        match = re.match('\\s*\\d+\\s+[0-9A-Fa-f]+\\s+([0-9A-Fa-f]+)\\s+(\\S+)', line)
        if match:
            rva = int(match[1], 16)
            code = any((start <= rva < end and executable for start, end, executable in sections))
            result[match[2]] = '' if code else ' DATA'
    if not result:
        raise RuntimeError(f'No named exports: {path}')
    return result

def import_library(image, destination):
    names = exports(image)
    definition = destination.with_suffix('.def')
    definition.write_text('LIBRARY ' + image.name + '\nEXPORTS\n' + '\n'.join((name + kind for name, kind in sorted(names.items()))) + '\n')
    run('lib', '/nologo', '/machine:x64', '/def:' + str(definition), '/out:' + str(destination))
    return names

def imported_symbols(text):
    """Read named C++ imports from dumpbin, grouped by their owning image."""
    result = {}
    active = None
    for line in text.splitlines():
        if line.strip().lower().endswith(('.dll', '.exe')):
            active = line.strip().lower()
            result.setdefault(active, [])
        elif active:
            match = re.match('\\s*[0-9A-Fa-f]+\\s+(\\S+)', line)
            if match and match[1].startswith('?'):
                result[active].append(match[1])
    return result

def validate_wx_dependencies(dependencies, version):
    if not dependencies:
        raise RuntimeError('No wxWidgets imports were found')
    family = {'3.2.9': '32', '3.3.4': '334'}[version]
    pattern = 'wx(?:base|msw)' + family + 'u(?:_[a-z0-9]+)?_vc14x_x64\\.dll'
    for name in dependencies:
        if not re.fullmatch(pattern, name):
            raise RuntimeError(f'Unexpected wxWidgets ABI dependency: {name}')

def run(*args, **kwargs):
    subprocess.run([str(a) for a in args], check=True, **kwargs)
