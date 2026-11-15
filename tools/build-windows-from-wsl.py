#!/usr/bin/env python3
"""Use an installed Windows Qt/MinGW kit from WSL; keep build output in this workspace."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import time
import importlib.util

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('getdaq_suite', ROOT / 'tools/build-suite.py')
suite = importlib.util.module_from_spec(spec)
spec.loader.exec_module(suite)
WINDOWS_PROGRAMS = suite.CONTROL


def windows(path):
    if os.name == 'nt':
        value = str(path)
    else:
        value = subprocess.check_output(['/usr/bin/wslpath', '-w', str(path)], text=True).strip()
    if any(c in value for c in ('%', '!', '"', '\r', '\n')):
        raise ValueError('Build paths contain unsupported batch metacharacters')
    return value


def windows_command(executable, script):
    """Return the command used to run a generated Windows batch/PowerShell file."""
    if os.name == 'nt':
        return [executable, '/d', '/c', windows(script)]
    if executable == 'cmd.exe':
        executable = '/mnt/c/Windows/System32/cmd.exe'
    elif executable == 'powershell.exe':
        executable = '/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe'
    return [executable, '/d', '/c', windows(script)]


def powershell_command(script, *arguments):
    if os.name == 'nt':
        executable = 'powershell.exe'
    else:
        executable = '/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe'
    return [executable, *arguments, '-File', windows(script)]


def write_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def pe_machine(path):
    with path.open('rb') as stream:
        header = stream.read(64)
        if header[:2] != b'MZ' or len(header) != 64:
            raise ValueError('Expected Windows PE executable: ' + str(path))
        offset = struct.unpack_from('<I', header, 60)[0]
        stream.seek(offset)
        signature = stream.read(6)
        if signature[:4] != b'PE\0\0':
            raise ValueError('Invalid PE signature: ' + str(path))
        machine = struct.unpack_from('<H', signature, 4)[0]
        if machine != 0x8664:
            raise ValueError('Qt/MinGW acceptance requires 64-bit x86 PE executables')
        return 'x86_64'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kit', type=Path, required=True, help='Windows Qt MinGW kit path (native Windows or WSL path)')
    parser.add_argument('--compiler', type=Path, required=True, help='Matching MinGW toolchain path (native Windows or WSL path)')
    parser.add_argument('--build-root', type=Path, default=ROOT / 'build-windows-qt5142')
    parser.add_argument('--jobs', type=int, default=2)
    parser.add_argument('--plan', action='store_true')
    args = parser.parse_args()
    if not 1 <= args.jobs <= 8:
        parser.error('--jobs must be 1..8')
    build = args.build_root.resolve()
    build.relative_to(ROOT)
    if build == ROOT:
        parser.error('Use a dedicated build directory')
    qmake = args.kit.resolve() / 'bin/qmake.exe'
    compiler = args.compiler.resolve() / 'bin'
    make = compiler / 'mingw32-make.exe'
    if not all(p.is_file() for p in (qmake, make, compiler / 'g++.exe')):
        parser.error('Select an existing Qt MinGW kit and matching compiler')
    relative_project = (Path('..') / Path('..') / 'GetDAQ.pro').as_posix()
    # Default nesting is two levels; calculate it for any in-workspace build root.
    relative_project = Path(os.path.relpath(ROOT / 'GetDAQ.pro', build / 'control')).as_posix()
    lines = ['@echo off', 'setlocal',
             'set "PATH=' + windows(compiler) + ';' + windows(qmake.parent) + ';%PATH%"',
             'pushd "' + windows(build / 'control') + '"',
             'if errorlevel 1 exit /b 10',
             'for %%I in ("%CD%\\..\\bin") do set "getdaq_output=%%~fI"',
             '"' + windows(qmake) + '" "' + relative_project + '" "GETDAQ_DESTDIR=%getdaq_output:\\=/%" -after "CONFIG+=release" "CONFIG-=debug"',
             'if errorlevel 1 goto failed',
             '"' + windows(make) + '" -j' + str(args.jobs),
             'if errorlevel 1 goto failed', 'popd', 'exit /b 0', ':failed',
             'set "getdaq_status=%errorlevel%"', 'popd', 'exit /b %getdaq_status%']
    plan = {'platform': 'native Windows' if os.name == 'nt' else 'Windows through WSL interop', 'qmake': windows(qmake),
            'compiler': windows(compiler), 'build_root': str(build), 'programs': WINDOWS_PROGRAMS,
            'source_project_relative_to_mapped_build': relative_project, 'batch': lines}
    if args.plan:
        print(json.dumps(plan, ensure_ascii=False, indent=2))
        return
    dependency_verification = suite.verify_dependencies('control')
    version = subprocess.check_output([str(qmake), '-query', 'QT_VERSION'], cwd=qmake.parent, text=True).strip()
    if version != '5.14.2':
        parser.error('This acceptance entrance targets the user Qt 5.14.2 kit exactly')
    build.mkdir(parents=True, exist_ok=True)
    lock = build / '.windows-build-lock'
    try:
        lock.mkdir()
    except FileExistsError:
        raise RuntimeError('Existing Windows build lock; check the process before removing it')
    receipt = {'schema': 'getdaq.windows-control:1', 'status': 'building', 'qt_version': version,
               'created_utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
               'programs': {}, 'kit': plan['qmake'], 'compiler': plan['compiler'],
               'embedded_dependencies': dependency_verification,
               'hardware_accepted': False, 'independent_windows_machine_accepted': False}
    try:
        (build / 'control').mkdir(exist_ok=True)
        (build / 'bin').mkdir(exist_ok=True)
        script = build / 'build-control.cmd'
        script.write_bytes(('\r\n'.join(lines) + '\r\n').encode('utf-8'))
        inputs = suite.source_hashes()
        inputs[str(Path(__file__).resolve().relative_to(ROOT))] = suite.sha(Path(__file__).resolve())
        receipt['inputs'] = inputs
        write_json(build / 'windows-build-plan.json', plan)
        write_json(build / 'windows-build-receipt.json', receipt)
        with (build / 'control-windows.log').open('w', encoding='utf-8') as log:
            command = windows_command('cmd.exe', script)
            receipt['command'] = command
            subprocess.run(command, cwd=qmake.parent, stdout=log, stderr=subprocess.STDOUT, check=True)
        after = suite.source_hashes()
        after[str(Path(__file__).resolve().relative_to(ROOT))] = suite.sha(Path(__file__).resolve())
        if after != inputs:
            raise RuntimeError('Source inputs changed during Windows build')
        for name in WINDOWS_PROGRAMS:
            path = build / 'bin' / (name + '.exe')
            receipt['programs'][name] = {'path': str(path), 'sha256': suite.sha(path), 'pe_machine': pe_machine(path)}
        receipt['status'] = 'complete'
        write_json(build / 'windows-build-receipt.json', receipt)
        print('PASS actual Windows Qt 5.14.2 MinGW x86_64 build:', len(receipt['programs']), 'programs;', build)
    except BaseException as error:
        receipt['status'] = 'failed'
        receipt['failure_type'] = type(error).__name__
        write_json(build / 'windows-build-receipt.json', receipt)
        raise
    finally:
        lock.rmdir()


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print('Windows build failed: ' + str(error), file=sys.stderr)
        sys.exit(1)
