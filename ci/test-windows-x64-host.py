# SPDX-License-Identifier: GPL-3.0-or-later
"""Import, initialize, quit normally, reinstall and repeat on disposable Windows CI."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time


def run(*args, **kwargs):
    print('+', ' '.join(map(str, args)), flush=True)
    return subprocess.run(list(map(str, args)), check=True, **kwargs)


def installed_host(host, identity, package, receipt, out):
    import ctypes
    # Only disposable native CI may create this system-wide OpenCPN profile.
    assert sys.platform == 'win32' and (os.environ.get('GITHUB_ACTIONS') == 'true' or os.environ.get('CIRCLECI') == 'true')
    ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002)
    profile = Path(os.environ['PROGRAMDATA']) / 'opencpn'
    assert not (profile / 'opencpn.ini').exists(), 'CI profile already exists'
    profile.mkdir(exist_ok=True)
    binary = (host / 'opencpn.exe').read_bytes()
    versions = re.findall(rb'5\.\d+\.\d+(?:-[A-Za-z0-9._]+)?(?:\+[A-Za-z0-9._]+)?\x00', binary)
    dates = re.findall(rb'20\d{2}-\d{2}-\d{2}\x00', binary)
    version = max(versions, key=len)[:-1].decode()
    date = max(dates)[:-1].decode()
    config = ('[Settings]\nConfigVersionString=Version ' + version + ' Build ' + date +
              '\nNavMessageShown=1\nOpenGL=0\nDisableOpenGL=1\nShowMenuBar=1\nDataConnections=\n' +
              '[PlugIns/' + identity + '.dll]\nbEnabled=1\n')
    for bundled in (host / 'plugins').glob('*.dll'):
        config += '[PlugIns/' + bundled.name + ']\nbEnabled=0\n'
    (profile / 'opencpn.ini').write_text(config)
    for cycle in range(2):
        imported = run(host / 'opencpn-cmd.exe', 'import-plugin', package, cwd=host,
                       capture_output=True, text=True, timeout=90)
        (out / ('import-' + str(cycle) + '.log')).write_text(imported.stdout + imported.stderr)
        log = profile / 'opencpn.log'
        if log.exists():
            log.unlink()
        with (out / ('stdout-' + str(cycle) + '.log')).open('w') as stdout:
            process = subprocess.Popen([host / 'opencpn.exe', '--no_opengl'], cwd=host,
                                       stdout=stdout, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + 90
                while time.monotonic() < deadline and process.poll() is None:
                    if log.exists() and 'Finalize Canvases' in log.read_text(errors='replace'):
                        break
                    time.sleep(0.2)
                else:
                    raise RuntimeError('Host did not complete startup')
                run(host / 'opencpn.exe', '--remote', '--quit', cwd=host, timeout=30)
                code = process.wait(timeout=30)
                if code != 0:
                    raise RuntimeError('Normal host shutdown failed: ' + str(code))
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait()
                if log.exists():
                    shutil.copy2(log, out / ('host-' + str(cycle) + '.log'))
        text = log.read_text(errors='replace')
        if not re.search(r'PluginLoader: (?:Loading|Initializing) PlugIn:.*' +
                         re.escape(identity) + r'\.dll', text):
            raise RuntimeError('The installed plugin was not initialized')
        receipt['normal_shutdown'].append(dict(cycle=cycle, exit_code=code, plugin_initialized=True))
        (out / 'provenance.json').write_text(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('host', type=Path)
    parser.add_argument('identity')
    parser.add_argument('package', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    installed_host(args.host.resolve(), args.identity, args.package.resolve(),
                   {'normal_shutdown': []}, args.output.resolve())
