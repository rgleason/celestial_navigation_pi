"""POBsoft (1985-2026): build CelNav with the pinned native Windows x64 SDK.

No source overlay or publication. Inspect imports, exports, architecture,
metadata, documentation and analytical data before retaining the package.
"""
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request
import xml.etree.ElementTree as ET
import zipfile

from windows64_pe import verify

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / '.windows64'
ARTIFACTS = ROOT / 'artifacts/windows-x64'


def run(*args, cwd=ROOT):
    print('+', ' '.join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), cwd=cwd, check=True)


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(chunk)
    return result.hexdigest()


def download(url, path, expected):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists():
        partial = path.with_suffix(path.suffix + '.part')
        with urllib.request.urlopen(url, timeout=120) as response, partial.open('wb') as output:
            shutil.copyfileobj(response, output)
        if digest(partial) != expected:
            raise RuntimeError(f'Archive checksum mismatch: {path.name}')
        partial.replace(path)
    if digest(path) != expected:
        raise RuntimeError(f'Archive checksum mismatch: {path.name}')


def prepare_sdk():
    pin = json.loads((ROOT / 'ci/windows64-sdk.json').read_text())
    archive = ROOT / pin['archive']
    if digest(archive) != pin['sha256']:
        raise RuntimeError('Native host SDK archive checksum mismatch')
    sdk = WORK / 'sdk'
    with zipfile.ZipFile(archive) as package:
        for member in package.namelist():
            if Path(member).is_absolute() or '..' in Path(member).parts:
                raise RuntimeError('Unsafe SDK archive path')
        package.extractall(sdk)
    manifest = json.loads((sdk / 'manifest.json').read_text())
    if manifest['architecture'] != 'AMD64' or manifest['core_revision'] != pin['core_revision']:
        raise RuntimeError('Native host SDK provenance mismatch')
    for relative, expected in manifest['files'].items():
        if digest(sdk / relative) != expected:
            raise RuntimeError(f'Native SDK file checksum mismatch: {relative}')
    verify(sdk)
    wx = WORK / 'cache/wxWidgets-3.2.8'
    for filename, expected in pin['wx_archives'].items():
        path = WORK / 'cache' / filename
        download('https://github.com/wxWidgets/wxWidgets/releases/download/v3.2.8/' + filename,
                 path, expected)
        run('7z', 'x', '-y', '-o' + str(wx), path)
    wxlib = wx / 'lib/vc14x_x64_dll'
    verify(wxlib)
    os.environ['PATH'] = str(wxlib) + os.pathsep + str(sdk / 'bin') + os.pathsep + os.environ['PATH']
    os.environ['WX_VER'] = '32'
    os.environ['OCPN_TARGET'] = 'MSVC-x64'
    return sdk, wx, wxlib


def main():
    if sys.platform != 'win32':
        raise SystemExit('Use a native Windows x64 MSVC environment')
    ARTIFACTS.mkdir(parents=True, exist_ok=True)
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    sdk, wx, wxlib = prepare_sdk()
    # Independently inspect all COFF members, rather than trust the filename.
    for lib in (sdk / 'lib/opencpn.lib', sdk / 'lib/z.lib'):
        headers = subprocess.check_output(['dumpbin', '/nologo', '/headers', str(lib)], text=True)
        machines = re.findall(r'(?im)^\s*(?:Machine\s*:\s*)?([0-9A-F]+)\s+\((?:x64|x86|ARM64)\)', headers)
        if not machines or any(int(machine, 16) != 0x8664 for machine in machines):
            raise RuntimeError('Not an exclusively AMD64 import library: ' + str(lib))
        (ARTIFACTS / (lib.stem + '-headers.txt')).write_text(headers)
    run('git', 'submodule', 'update', '--init', 'opencpn-libs')
    work = ROOT / 'build'
    if work.exists():
        raise RuntimeError('Use a clean build directory for x64 qualification')
    run('cmake', '-S', ROOT, '-B', work, '-G', 'Visual Studio 17 2022', '-A', 'x64',
        '-DCMAKE_BUILD_TYPE=Release', '-DOCPN_BUILD_TEST=OFF',
        '-DCELESTIAL_WINDOWS_IMPORT_LIBRARY=' + str(sdk / 'lib/opencpn.lib'),
        '-DZLIB_ROOT=' + str(sdk), '-DZLIB_LIBRARY_RELEASE=' + str(sdk / 'lib/z.lib'),
        '-DwxWidgets_ROOT_DIR=' + str(wx), '-DwxWidgets_LIB_DIR=' + str(wxlib),
        '-DwxWidgets_CONFIGURATION=mswu')
    run('cmake', '--build', work, '--config', 'Release', '--parallel', '4')
    run('powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
        ROOT / 'ci/check-msvc-mutex-imports.ps1', '-BuildDirectory', work)
    dlls = list((work / 'Release').glob('celestial_navigation_pi.dll'))
    if len(dlls) != 1:
        raise RuntimeError('Expected one Release plugin DLL')
    imports = subprocess.check_output(['dumpbin', '/nologo', '/imports', str(dlls[0])], text=True)
    exports = subprocess.check_output(['dumpbin', '/nologo', '/exports', str(dlls[0])], text=True)
    if 'opencpn.exe' not in imports.lower() or not all(re.search(r'\b' + name + r'\b', exports) for name in ('create_pi', 'destroy_pi')):
        raise RuntimeError('Missing native host imports or plugin entry points')
    (ARTIFACTS / 'plugin-imports.txt').write_text(imports)
    (ARTIFACTS / 'plugin-exports.txt').write_text(exports)
    run('cpack', '-G', 'TGZ', '-C', 'Release', '--config', 'CPackConfig.cmake', cwd=work)
    archives = list(work.glob('celestial_navigation_pi-*.tar.gz'))
    metadata = list(work.glob('celestial_navigation_pi-*.xml'))
    if len(archives) != 1 or len(metadata) != 1:
        raise RuntimeError('Expected exactly one archive and metadata pair')
    xml = ET.parse(metadata[0]).getroot()
    values = [(xml.findtext(k) or '').strip() for k in ('target', 'target-version', 'target-arch', 'version', 'api-version')]
    if values != ['msvc-wx32-x64', '10', 'x86_64', '2.9.2.0', '1.18']:
        raise RuntimeError('Incorrect Windows x64 metadata: ' + repr(values))
    xml.find('source').text = 'https://github.com/pob220/celestial_navigation_pi/tree/' + revision
    xml.find('tarball-url').text = 'https://github.com/pob220/celestial_navigation_pi/releases/download/v2.9.2.0-alpha1/' + archives[0].name
    raw_xml = ET.tostring(xml, encoding='utf-8', xml_declaration=True)
    metadata[0].write_bytes(raw_xml)
    with tempfile.TemporaryDirectory() as temporary:
        extracted = Path(temporary)
        with tarfile.open(archives[0]) as archive:
            for member in archive.getmembers():
                if Path(member.name).is_absolute() or '..' in Path(member.name).parts or not (member.isfile() or member.isdir()):
                    raise RuntimeError('Unsafe package member')
            archive.extractall(extracted, filter='data')
        (extracted / 'metadata.xml').write_bytes(raw_xml)
        architecture = verify(extracted)
        if len(architecture) != 1 or not any(Path(p).name == 'celestial_navigation_pi.dll' for p in architecture):
            raise RuntimeError('Unexpected packaged native images')
        for asset in ('vsop87d.txt', 'Android_Quick_Guide.html', 'Celestial_Navigation_Manual_v2.pdf'):
            # Compare the actual source bytes for the selected bundled filename.
            originals = list((ROOT / 'data').rglob(asset))
            if not originals:
                raise RuntimeError('Review the bundled asset filename: ' + asset)
            packaged = list(extracted.rglob(asset))
            if len(packaged) != 1 or digest(packaged[0]) != digest(originals[0]):
                raise RuntimeError('Packaged asset differs from source: ' + asset)
        output = ARTIFACTS / archives[0].name
        with tarfile.open(output, 'w:gz') as archive:
            for path in sorted(extracted.rglob('*')):
                archive.add(path, arcname=path.relative_to(extracted), recursive=False)
    shutil.copy2(metadata[0], ARTIFACTS / metadata[0].name)
    shutil.copy2(work / 'CMakeCache.txt', ARTIFACTS / 'CMakeCache.txt')
    shutil.copy2(ROOT / 'docs/v2/output/Celestial_Navigation_Manual_v2.docx', ARTIFACTS / 'Celestial_Navigation_Manual_v2.docx')
    for pdb in (work / 'Release').glob('celestial_navigation_pi.pdb'):
        shutil.copy2(pdb, ARTIFACTS / pdb.name)
    provenance = {'source_revision': revision, 'sdk': json.loads((sdk / 'manifest.json').read_text()),
        'architecture': architecture, 'metadata': values,
        'sha256': {p.name: digest(p) for p in (output, ARTIFACTS / metadata[0].name)},
        'qualification': 'Native build, link and package verification; desktop GUI runtime not exercised here'}
    (ARTIFACTS / 'windows-x64-provenance.json').write_text(json.dumps(provenance, indent=2) + '\n')
    print('Verified Windows x64 package:', output.name, flush=True)


if __name__ == '__main__':
    main()
