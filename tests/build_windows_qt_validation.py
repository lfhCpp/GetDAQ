#!/usr/bin/env python3
"""Build/deploy complete Qt widget and native-control client tests with Windows Qt 5.14.2."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('winbuild', ROOT / 'tools/build-windows-from-wsl.py')
win = importlib.util.module_from_spec(spec)
spec.loader.exec_module(win)


def inputs():
    result = win.suite.source_hashes()
    for directory in [ROOT / 'tests', ROOT / 'config/experiment',
                      ROOT / 'source/get/GetBench-software-release-20210208/data']:
        for path in directory.rglob('*'):
            if path.is_file() and path.suffix in ['.cpp', '.h', '.pro', '.py', '.xcfg', '.json']:
                result[str(path.relative_to(ROOT))] = win.suite.sha(path)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kit', required=True, type=Path)
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--resume', action='store_true', help='Rebuild an existing owned output with the same kit')
    parser.add_argument('--clean', action='store_true', help='Discard owned objects before rebuilding changed compiler options')
    args = parser.parse_args()
    kit, compiler, base = args.kit.resolve(), args.compiler.resolve(), args.output.resolve()
    base.relative_to(ROOT)
    if base == ROOT:
        raise ValueError('Use a dedicated build directory')
    version = subprocess.check_output([str(kit / 'bin/qmake.exe'), '-query', 'QT_VERSION'], text=True).strip()
    if version != '5.14.2':
        raise ValueError('Use the user Qt 5.14.2 MinGW kit')
    if args.resume:
        old = json.loads((base / 'build-receipt.json').read_text())
        if old.get('schema') != 'getdaq.windows-qt-tests:1' or old.get('kit') != str(kit) or old.get('compiler') != str(compiler):
            raise ValueError('Existing output does not belong to this validation kit')
    else:
        base.mkdir(parents=True, exist_ok=False)
    lock = base / '.build.lock'
    try:
        lock.mkdir()
    except FileExistsError:
        raise RuntimeError('Existing Windows Qt test build lock; inspect the owner before removing it')
    receipt = {'schema': 'getdaq.windows-qt-tests:1', 'status': 'building', 'qt_version': version,
               'kit': str(kit), 'compiler': str(compiler), 'inputs': inputs(), 'programs': {},
               'hardware_accepted': False, 'independent_machine_accepted': False}
    destination = base / 'build-receipt.json'
    try:
        win.write_json(destination, receipt)
        for name, project in [('widgets', 'tests.pro'), ('client', 'ecc_system_client.pro')]:
            out = base / name
            out.mkdir(exist_ok=args.resume)
            makefile = out / 'Makefile.Release'
            old_recipe = [line for line in makefile.read_text().splitlines()
                          if re.match(r'^(CXXFLAGS|DEFINES|INCPATH|LFLAGS|LIBS)\s*=', line)] if makefile.exists() else None
            relative = Path(os.path.relpath(ROOT / 'tests' / project, out)).as_posix()
            lines = ['@echo off', 'setlocal',
                     'set "PATH=' + win.windows(compiler / 'bin') + ';' + win.windows(kit / 'bin') + ';%PATH%"',
                     'pushd "' + win.windows(out) + '"', 'if errorlevel 1 exit /b 10',
                     '"' + win.windows(kit / 'bin/qmake.exe') + '" "' + relative + '" "DESTDIR=." '
                     '-after "CONFIG+=release" "CONFIG-=debug" '
                     '"QMAKE_CXXFLAGS+=-pedantic-errors" "QMAKE_CXXFLAGS_CXX11=-std=c++11"',
                     'if errorlevel 1 goto failed',
                     *(['"' + win.windows(compiler / 'bin/mingw32-make.exe') + '" clean',
                        'if errorlevel 1 goto failed'] if args.clean else []),
                     '"' + win.windows(compiler / 'bin/mingw32-make.exe') + '" -j2',
                     'if errorlevel 1 goto failed', 'popd', 'exit /b 0', ':failed',
                     'set "getdaq_status=%errorlevel%"', 'popd', 'exit /b %getdaq_status%']
            batch = out / 'build.cmd'
            batch.write_bytes(('\r\n'.join(lines) + '\r\n').encode('utf-8'))
            with (out / 'build.log').open('w') as log:
                subprocess.run(win.windows_command('cmd.exe', batch),
                               stdout=log, stderr=subprocess.STDOUT, check=True)
            new_recipe = [line for line in makefile.read_text().splitlines()
                          if re.match(r'^(CXXFLAGS|DEFINES|INCPATH|LFLAGS|LIBS)\s*=', line)]
            if old_recipe and not args.clean and old_recipe != new_recipe:
                raise RuntimeError('Compiler options changed; repeat the owned build with --resume --clean')
            target = re.search(r'^DESTDIR_TARGET\s*=\s*(.+)$', makefile.read_text(), re.M).group(1).strip().replace('\\', '/')
            if '$' in target or ':' in target:
                raise ValueError('Unexpected target path')
            executable = (out / target).resolve()
            executable.relative_to(out)
            if os.name != 'nt':
                executable.chmod(executable.stat().st_mode | 0o111)
            deploy = out / 'deploy.cmd'
            deployment = lines[:5] + [
                '"' + win.windows(kit / 'bin/windeployqt.exe') + '" --compiler-runtime --no-translations "' + target + '"',
                'if errorlevel 1 goto failed', *lines[-6:]]
            deploy.write_bytes(('\r\n'.join(deployment) + '\r\n').encode('utf-8'))
            with (out / 'deploy.log').open('w') as log:
                subprocess.run(win.windows_command('cmd.exe', deploy),
                               stdout=log, stderr=subprocess.STDOUT, check=True)
            runtime = executable.parent
            for module in ['Qt5Core', 'Qt5Network', 'Qt5Test']:
                shutil.copy2(kit / ('bin/' + module + '.dll'), runtime / (module + '.dll'))
            for dll in ['libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll']:
                shutil.copy2(compiler / 'bin' / dll, runtime / dll)
            (runtime / 'platforms').mkdir(exist_ok=True)
            for plugin in ['qwindows.dll', 'qoffscreen.dll']:
                shutil.copy2(kit / 'plugins/platforms' / plugin, runtime / 'platforms' / plugin)
            (runtime / 'qt.conf').write_text('[Paths]\nPrefix=.\nPlugins=.\n')
            receipt['programs'][name] = {'path': str(executable), 'sha256': win.suite.sha(executable),
                'pe_machine': win.pe_machine(executable), 'makefile_sha256': win.suite.sha(makefile),
                'runtime_sha256': {str(p.relative_to(base)): win.suite.sha(p)
                                   for p in runtime.rglob('*') if p.is_file()}}
            win.write_json(destination, receipt)
        if inputs() != receipt['inputs']:
            raise RuntimeError('Test inputs changed during compilation')
        receipt['status'] = 'complete'
        win.write_json(destination, receipt)
        print('PASS Windows Qt 5.14.2 MinGW strict C++11: complete widget and ECC-client test programs built/deployed')
    except BaseException as error:
        receipt['status'] = 'failed'
        receipt['error'] = str(error)
        win.write_json(destination, receipt)
        raise
    finally:
        lock.rmdir()


if __name__ == '__main__':
    main()
