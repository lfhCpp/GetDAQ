#!/usr/bin/env python3
"""Create and inspect a NEW Windows runtime directory using the user's installed Qt kit."""
import argparse
import importlib.util
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('winbuild', ROOT / 'tools/build-windows-from-wsl.py')
winbuild = importlib.util.module_from_spec(spec)
spec.loader.exec_module(winbuild)
suite = winbuild.suite


def smoke_runtime(build, destination, native=False):
    # Redirect the GUI application's handles explicitly: Qt's Windows parser
    # otherwise shows --help in a modal MessageBox when no console is attached.
    quoted = winbuild.windows(destination).replace("'", "''")
    platform = 'windows' if native else 'offscreen'
    screenshot = 'native-window.png' if native else 'initial-window.png'
    lines = ["$ErrorActionPreference = 'Stop'", "$directory = '" + quoted + "'",
             "$env:PATH = $env:SystemRoot + '\\System32;' + $env:SystemRoot",
             "Remove-Item Env:QT_PLUGIN_PATH -ErrorAction SilentlyContinue",
             "Remove-Item Env:QT_QPA_PLATFORM_PLUGIN_PATH -ErrorAction SilentlyContinue",
             "$env:QT_QPA_PLATFORM = '" + platform + "'",
             'function Run-Check($name, $arguments, $label) {',
             "  $out = Join-Path $directory ($label + '-stdout.txt')",
             "  $err = Join-Path $directory ($label + '-stderr.txt')",
             "  $process = Start-Process -FilePath (Join-Path $directory ($name + '.exe')) -ArgumentList $arguments -WorkingDirectory $directory -RedirectStandardOutput $out -RedirectStandardError $err -PassThru",
             '  $null = $process.Handle',
             '  if (-not $process.WaitForExit(10000)) {',
             '    & "$env:SystemRoot\\System32\\taskkill.exe" /PID $process.Id /T /F | Out-Null',
             "    throw ('Owned runtime check timed out: ' + $name)", '  }',
             '  $process.Refresh()', '  Get-Content $out', '  Get-Content $err',
             "  if ($process.ExitCode -ne 0) { throw ('Runtime check failed: ' + $name + ' exit ' + $process.ExitCode) }",
             '}']
    for name in suite.CONTROL:
        lines.append("Run-Check '" + name + "' '--help' 'help-" + name + "-" + platform + "'")
    lines.append("Run-Check 'GetDAQ' '--screenshot " + screenshot + "' 'screenshot-" + platform + "'")
    script = build / ('smoke-' + platform + '.ps1')
    script.write_text('\n'.join(lines) + '\n', encoding='utf-8-sig')
    logpath = build / ('native-runtime-smoke-windows.log' if native else 'runtime-smoke-windows.log')
    with logpath.open('w') as log:
        subprocess.run(winbuild.powershell_command(
                           script, '-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass'),
                       cwd=destination, stdout=log, stderr=subprocess.STDOUT, timeout=120, check=True)
    if not (destination / screenshot).is_file():
        raise RuntimeError('Initial window screenshot missing: ' + screenshot)


def verify_runtime(directory):
    directory = directory.resolve()
    manifest = json.loads((directory / 'manifest.json').read_text(encoding='utf-8'))
    if manifest.get('schema') != 'getdaq.windows-runtime:1' or manifest.get('status') != 'complete' or (directory / 'BUILD_INCOMPLETE').exists():
        raise ValueError('Not a complete Windows runtime package')
    for relative, digest in manifest['files'].items():
        path = Path(relative)
        if path.is_absolute() or '..' in path.parts or '\\' in relative or ':' in relative:
            raise ValueError('Invalid manifest path')
        target = directory / path
        target.resolve().relative_to(directory)
        if not target.is_file() or suite.sha(target) != digest:
            raise ValueError('Runtime file missing or changed: ' + relative)
    actual = {str(p.relative_to(directory)) for p in directory.rglob('*') if p.is_file() and p != directory / 'manifest.json'}
    if actual != set(manifest['files']):
        raise ValueError('Runtime directory contains unlisted files')
    for name, program in manifest['programs'].items():
        path = directory / (name + '.exe')
        if suite.sha(path) != program['sha256']:
            raise ValueError('Program differs from build receipt: ' + name)
        winbuild.pe_machine(path)
    print('PASS Windows runtime integrity:', len(manifest['programs']), 'programs;', len(actual), 'files')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kit', type=Path)
    parser.add_argument('--compiler', type=Path)
    parser.add_argument('--build-root', type=Path, default=ROOT / 'build-windows-qt5142')
    parser.add_argument('--destination', type=Path)
    parser.add_argument('--verify', type=Path, help='Check an existing runtime without executing Windows tools')
    parser.add_argument('--native-window-smoke', action='store_true', help='Briefly open the Windows initial window, save a screenshot and exit')
    args = parser.parse_args()
    if args.verify:
        verify_runtime(args.verify)
        return
    if not all((args.kit, args.compiler, args.destination)):
        parser.error('--kit, --compiler and --destination are required for deployment')
    build = args.build_root.resolve()
    destination = args.destination.resolve()
    destination.relative_to(ROOT)
    if destination.exists():
        parser.error('Use a NEW runtime directory')
    receipt = json.loads((build / 'windows-build-receipt.json').read_text())
    if receipt['status'] != 'complete' or receipt['qt_version'] != '5.14.2':
        parser.error('A completed actual Qt 5.14.2 Windows build is required')
    kit = args.kit.resolve()
    compiler = args.compiler.resolve()
    if winbuild.windows(kit / 'bin/qmake.exe') != receipt['kit'] or winbuild.windows(compiler / 'bin') != receipt['compiler']:
        parser.error('Deploy using the exact Qt kit and compiler recorded by the build')
    for relative, digest in receipt['inputs'].items():
        if suite.sha(ROOT / relative) != digest:
            raise RuntimeError('Build input changed; rebuild before deployment: ' + relative)
    deploy = kit / 'bin/windeployqt.exe'
    objdump = compiler / 'bin/objdump.exe'
    if not deploy.is_file() or not objdump.is_file():
        parser.error('Qt deployment and MinGW import inspection tools are required')
    destination.mkdir(parents=True)
    marker = destination / 'BUILD_INCOMPLETE'
    marker.write_text('Runtime deployment not verified; do not distribute.\n')
    for name, program in receipt['programs'].items():
        source = Path(program['path'])
        if suite.sha(source) != program['sha256']:
            raise RuntimeError('Windows executable changed: ' + name)
        shutil.copy2(source, destination / source.name)
    lines = ['@echo off', 'setlocal',
             'set "PATH=' + winbuild.windows(compiler / 'bin') + ';' + winbuild.windows(kit / 'bin') + ';%PATH%"',
             'pushd "' + winbuild.windows(destination) + '"', 'if errorlevel 1 exit /b 10']
    for name in suite.QT_CONTROL:
        if name != 'getdaq-config-check':
            # Qt 5.14.2 cannot reliably infer MinGW plugin debug mode from PE flags.
            # Automatic mode accepts matching kit plugins; forcing --release can reject them all.
            lines += ['"' + winbuild.windows(deploy) + '" --compiler-runtime --no-translations "' + name + '.exe"',
                      'if errorlevel 1 goto failed']
    lines += ['popd', 'exit /b 0', ':failed', 'set "getdaq_status=%errorlevel%"', 'popd', 'exit /b %getdaq_status%']
    script = build / 'deploy-runtime.cmd'
    script.write_bytes(('\r\n'.join(lines) + '\r\n').encode('utf-8'))
    with (build / 'deployment-windows.log').open('w') as log:
        subprocess.run(winbuild.windows_command('cmd.exe', script),
                       cwd=kit / 'bin', stdout=log, stderr=subprocess.STDOUT, check=True)
    # QPA plugins resolve within the runtime directory, rather than the installed SDK.
    (destination / 'qt.conf').write_text('[Paths]\nPrefix=.\nPlugins=.\n', encoding='ascii')
    (destination / 'platforms').mkdir(exist_ok=True)
    offscreen = kit / 'plugins/platforms/qoffscreen.dll'
    shutil.copy2(offscreen, destination / 'platforms/qoffscreen.dll')
    for name in ['libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll']:
        source = compiler / 'bin' / name
        if not source.is_file():
            raise RuntimeError('Matching compiler runtime absent: ' + name)
        target = destination / name
        if target.exists() and suite.sha(target) != suite.sha(source):
            raise RuntimeError('Mixed compiler runtime in package: ' + name)
        if not target.exists():
            shutil.copy2(source, target)
    for source, relative in suite.stage_files():
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
    for source_root, subfolder in [(kit.parents[1] / 'Licenses', 'qt-sdk'), (compiler / 'licenses', 'mingw')]:
        if source_root.is_dir():
            for source in source_root.rglob('*'):
                if source.is_file():
                    if source.stat().st_size > 8*1024*1024:
                        raise RuntimeError('Unexpected large license asset')
                    target = destination / 'share/licenses' / subfolder / source.relative_to(source_root)
                    target.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(source, target)
    imports = {}
    packaged = {p.name.lower(): p for p in destination.rglob('*.dll')}
    for path in sorted([*destination.glob('*.exe'), *destination.rglob('*.dll')]):
        winbuild.pe_machine(path)
        output = subprocess.check_output([str(objdump), '-p', winbuild.windows(path)], cwd=compiler / 'bin', text=True)
        names = re.findall(r'DLL Name:\s*(\S+)', output)
        imports[str(path.relative_to(destination))] = names
        for name in names:
            lower = name.lower()
            if lower.startswith(('qt5', 'libgcc_', 'libstdc++', 'libwinpthread', 'libegl', 'libgles')) and lower not in packaged:
                raise RuntimeError('Required non-system DLL missing: ' + name)
    smoke_runtime(build, destination)
    if args.native_window_smoke:
        smoke_runtime(build, destination, native=True)
    manifest = {'schema': 'getdaq.windows-runtime:1', 'status': 'complete', 'qt_version': '5.14.2', 'pe_machine': 'x86_64',
                'sdk_path_removed_from_smoke_environment': True, 'initial_window_rendered': True,
                'native_windows_platform_smoke': args.native_window_smoke,
                'independent_machine_accepted': False, 'real_hardware_accepted': False,
                'programs': {name: dict(program, path=name + '.exe') for name, program in receipt['programs'].items()},
                'imports': imports, 'files': {}}
    for path in sorted(destination.rglob('*')):
        if path.is_file() and path != marker:
            manifest['files'][str(path.relative_to(destination))] = suite.sha(path)
    (destination / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n')
    marker.unlink()
    try:
        verify_runtime(destination)
    except BaseException:
        marker.write_text('Runtime integrity verification failed; do not distribute.\n')
        raise
    print('PASS deployed Windows x86_64 runtime; SDK-free PATH smoke and offscreen window;', destination)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print('Windows deployment failed: ' + str(error), file=sys.stderr)
        sys.exit(1)
