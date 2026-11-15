#!/usr/bin/env python3
"""Run the Qt 5.14.2 test binaries in an isolated native Windows workspace."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path, PureWindowsPath
import re
import shutil
import subprocess
import tempfile
import uuid

ROOT = Path(__file__).resolve().parents[1]
WINDOWS = Path(os.environ.get('SystemRoot', '/mnt/c/Windows')) / 'System32'
POWERSHELL = WINDOWS / 'WindowsPowerShell/v1.0/powershell.exe'
TESTS = ('widgets', 'client')
MAX_RAW_BYTES = 20 * 1024 ** 3
MAX_TEMP_BYTES = 512 * 1024 ** 2
MIN_FREE_BYTES = 1024 ** 3


def load_builder():
    spec = importlib.util.spec_from_file_location('windows_qt_builder', ROOT / 'tests/build_windows_qt_validation.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def verify_native_program_runtime(base, name, program):
    executable = Path(program['path']).resolve()
    if not executable.is_file() or sha(executable) != program['sha256']:
        raise RuntimeError('Test executable does not match its build receipt: ' + name)
    for relative, digest in program['runtime_sha256'].items():
        if not relative.startswith(name + '/'):
            raise RuntimeError('Test runtime receipt escaped its build directory: ' + relative)
        source = (base / relative).resolve()
        source.relative_to(base)
        if not source.is_file() or sha(source) != digest:
            raise RuntimeError('Test runtime file changed: ' + relative)
    return executable


def windows(path):
    if os.name == 'nt':
        value = str(path)
        if any(char in value for char in ('%', '!', '"', '\r', '\n')):
            raise ValueError('Test paths contain unsupported Windows batch characters')
        return value
    value = subprocess.check_output(['/usr/bin/wslpath', '-w', str(path)], text=True).strip()
    if any(char in value for char in ('%', '!', '"', '\r', '\n')):
        raise ValueError('Test paths contain unsupported Windows batch characters')
    return value


def powershell_path(expression):
    result = subprocess.check_output(
        [str(POWERSHELL), '-NoProfile', '-NonInteractive', '-Command', expression],
        cwd=WINDOWS, text=True, stderr=subprocess.STDOUT).strip()
    if not result:
        raise RuntimeError('Windows did not return a usable local temporary path')
    return result


def qt_test_build_dir(makefile):
    text = Path(makefile).read_text(errors='replace')
    match = re.search(r'-DQT_TESTCASE_BUILDDIR="\\\"(.+?)\\\""', text)
    if not match:
        raise RuntimeError('QtTest Makefile does not declare QT_TESTCASE_BUILDDIR')
    value = match.group(1).replace('\\', '/')
    return PureWindowsPath(value)


def test_build_dir(makefile, expected_drive='Z:'):
    value = qt_test_build_dir(makefile)
    if value.drive.upper() != expected_drive.upper():
        raise RuntimeError('Unexpected QtTest build path: ' + str(value))
    parts = value.parts
    if not parts or parts[0].upper() != expected_drive.upper() + '\\':
        # PureWindowsPath represents a drive-root as "Z:\\".
        raise RuntimeError('QtTest build path is not rooted on ' + expected_drive + ': ' + str(value))
    return Path(*parts[1:])


def verify_native_test_build_dir(makefile, expected):
    actual = qt_test_build_dir(makefile)
    target = PureWindowsPath(str(expected))
    if (actual.drive.upper() != target.drive.upper() or
            tuple(part.casefold() for part in actual.parts[1:]) !=
            tuple(part.casefold() for part in target.parts[1:])):
        raise RuntimeError('QtTest build directory does not match its receipt location: ' + str(actual))
    return actual


def copy_runtime(receipt, base, test_name, local_root):
    program = receipt['programs'][test_name]
    executable = Path(program['path']).resolve()
    expected = program['sha256']
    if not executable.is_file() or sha(executable) != expected:
        raise RuntimeError('Test executable does not match its build receipt: ' + test_name)
    build_dir = base / test_name
    relative_build = test_build_dir(build_dir / 'Makefile.Release')
    local_build_dir = local_root / relative_build
    local_build_dir.mkdir(parents=True, exist_ok=True)
    prefix = test_name + '/'
    for relative, digest in program['runtime_sha256'].items():
        if not relative.startswith(prefix):
            raise RuntimeError('Test runtime receipt escaped its build directory: ' + relative)
        source = base / relative
        if not source.is_file() or sha(source) != digest:
            raise RuntimeError('Test runtime file changed: ' + relative)
        destination = local_root / relative_build / Path(relative[len(prefix):])
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
        if sha(destination) != digest:
            raise RuntimeError('Test runtime copy changed: ' + relative)
    return local_build_dir, local_build_dir / 'release' / executable.name, program


def copy_owned_fixture(source, destination, owned_root, owned_paths):
    if owned_root.exists() or owned_root.is_symlink():
        raise RuntimeError('Refusing to replace existing QtTest fixture path: ' + str(owned_root))
    owned_paths.append(owned_root)
    shutil.copytree(source, destination)


def cleanup_owned_fixtures(paths):
    for fixture_path in paths:
        if fixture_path.is_dir() and not fixture_path.is_symlink():
            shutil.rmtree(fixture_path)


def powershell_quote(value):
    return "'" + str(value).replace("'", "''") + "'"


def create_runner(path, local_root, test_cwd, staged_tests):
    temp_path = local_root / 'process-temp'
    temp_path.mkdir(parents=True, exist_ok=True)
    lines = [
        "$ErrorActionPreference = 'Stop'",
        "$root = " + powershell_quote(windows(local_root)),
        "$temp = " + powershell_quote(windows(temp_path)),
        "$cwd = " + powershell_quote(test_cwd),
        "$driveRoot = [System.IO.Path]::GetPathRoot($root)",
        "$driveName = $driveRoot.Substring(0, 1)",
        "$available = (Get-PSDrive -Name $driveName).Free",
        "if ($available -lt " + str(MIN_FREE_BYTES) + ") { throw 'Less than 1 GiB free on the Windows test volume' }",
        "$env:PATH = $env:SystemRoot + '\\System32;' + $env:SystemRoot",
        "Remove-Item Env:QT_PLUGIN_PATH -ErrorAction SilentlyContinue",
        "Remove-Item Env:QT_QPA_PLATFORM_PLUGIN_PATH -ErrorAction SilentlyContinue",
        "$env:QT_QPA_PLATFORM = 'windows'",
        "$env:QT_STYLE_OVERRIDE = 'Fusion'",
        "$env:TEMP = $temp; $env:TMP = $temp",
        "$env:APPDATA = Join-Path $root 'appdata-roaming'",
        "$env:LOCALAPPDATA = Join-Path $root 'appdata-local'",
        "New-Item -ItemType Directory -Force -Path $env:APPDATA, $env:LOCALAPPDATA | Out-Null",
        "$results = @()",
    ]
    for name, executable, runtime, out_log, err_log in staged_tests:
        lines += [
            "$env:PATH = " + powershell_quote(runtime) + " + ';' + $env:SystemRoot + '\\System32;' + $env:SystemRoot",
            "$env:QT_QPA_PLATFORM_PLUGIN_PATH = " + powershell_quote(runtime.rstrip('\\/') + '\\platforms'),
            "$out = " + powershell_quote(windows(out_log)),
            "$err = " + powershell_quote(windows(err_log)),
            "$start = New-Object System.Diagnostics.ProcessStartInfo",
            "$start.FileName = " + powershell_quote(executable),
            "$start.Arguments = " + powershell_quote(('-style Fusion ' if name == 'widgets' else '') + '-o ') + " + '\"' + $out + ',txt\"'",
            "$start.WorkingDirectory = $cwd; $start.UseShellExecute = $false; $start.CreateNoWindow = $false",
            "$process = New-Object System.Diagnostics.Process; $process.StartInfo = $start",
            "if (-not $process.Start()) { throw 'Unable to launch QtTest: " + name + "' }",
            "  $raw = @(Get-ChildItem -LiteralPath $root -Filter '*.graw' -File -Recurse -ErrorAction SilentlyContinue)",
            "  $rawBytes = [Int64](($raw | Measure-Object -Property Length -Sum).Sum)",
            "$deadline = [DateTime]::UtcNow.AddSeconds(240)",
            "while (-not $process.HasExited) {",
            "  Start-Sleep -Milliseconds 500; $process.Refresh()",
            "  if ([DateTime]::UtcNow -gt $deadline) { & ($env:SystemRoot + '\\System32\\taskkill.exe') /PID $process.Id /T /F | Out-Null; throw 'QtTest exceeded 240 seconds: " + name + "' }",
            "  $raw = @(Get-ChildItem -LiteralPath $root -Filter '*.graw' -File -Recurse -ErrorAction SilentlyContinue)",
            "  $rawBytes = [Int64](($raw | Measure-Object -Property Length -Sum).Sum)",
            "  if ($rawBytes -gt " + str(MAX_RAW_BYTES) + ") { & ($env:SystemRoot + '\\System32\\taskkill.exe') /PID $process.Id /T /F | Out-Null; throw 'Synthetic raw data exceeded 20 GiB' }",
            "  $tempFiles = @(Get-ChildItem -LiteralPath $root -File -Recurse -ErrorAction SilentlyContinue)",
            "  $tempBytes = [Int64](($tempFiles | Measure-Object -Property Length -Sum).Sum)",
            "  if ($tempBytes -gt " + str(MAX_TEMP_BYTES) + ") { & ($env:SystemRoot + '\\System32\\taskkill.exe') /PID $process.Id /T /F | Out-Null; throw 'Owned Windows test workspace exceeded 512 MiB' }",
            "}",
            "$process.WaitForExit(); $process.Refresh()",
            "$exitCode = [int]$process.ExitCode",
            "$resultText = Get-Content -LiteralPath $out -Raw",
            "$match = [regex]::Match($resultText, 'Totals:\\s+(\\d+) passed, (\\d+) failed, (\\d+) skipped')",
            "if (-not $match.Success) { throw 'QtTest did not emit a parseable totals line: " + name + "' }",
            "$entry = [ordered]@{name='" + name + "';exit_code=$exitCode;passed=[int]$match.Groups[1].Value;failed=[int]$match.Groups[2].Value;skipped=[int]$match.Groups[3].Value;raw_files_remaining=$raw.Count;raw_bytes_remaining=$rawBytes}",
            "$results += [pscustomobject]$entry",
            "$results | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath " + powershell_quote(windows(local_root / 'progress.json')) + " -Encoding UTF8",
            "if ($exitCode -ne 0 -or [int]$match.Groups[2].Value -ne 0) { throw 'QtTest failed: " + name + " (exit ' + $exitCode + ')' }",
        ]
    lines += [
        "$results | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath " + powershell_quote(windows(local_root / 'summary.json')) + " -Encoding UTF8",
    ]
    path.write_text('\n'.join(lines) + '\n', encoding='utf-8-sig')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-root', required=True, type=Path)
    parser.add_argument('--only', choices=('all', *TESTS), default='all')
    args = parser.parse_args()
    base = args.build_root.resolve()
    base.relative_to(ROOT)
    receipt_path = base / 'build-receipt.json'
    receipt = json.loads(receipt_path.read_text())
    if receipt.get('schema') != 'getdaq.windows-qt-tests:1' or receipt.get('status') != 'complete' or receipt.get('qt_version') != '5.14.2':
        parser.error('A completed Qt 5.14.2 Windows test build is required')
    builder = load_builder()
    if builder.inputs() != receipt.get('inputs'):
        raise RuntimeError('Windows test sources changed after build; rebuild before running')
    if any(name not in receipt.get('programs', {}) for name in TESTS):
        raise RuntimeError('Both test programs must be present in the build receipt')
    temp_windows = powershell_path('[Console]::WriteLine([System.IO.Path]::GetTempPath())')
    temp_unix = (temp_windows if os.name == 'nt' else
                 subprocess.check_output(['/usr/bin/wslpath', '-u', temp_windows], text=True).strip())
    free = int(powershell_path("$drive = [System.IO.Path]::GetPathRoot($env:TEMP).Substring(0,1); [Console]::WriteLine([Int64](Get-PSDrive -Name $drive).Free)"))
    if free < MIN_FREE_BYTES:
        raise RuntimeError('The Windows temporary volume has less than 1 GiB free')
    existing_wsl_drive = False
    if os.name != 'nt':
        drive_json = powershell_path("$d=Get-PSDrive -Name Z -ErrorAction SilentlyContinue; if ($d) { [pscustomobject]@{root=$d.Root;display=$d.DisplayRoot} | ConvertTo-Json -Compress } else { '{}' }")
        drive_state = json.loads(drive_json)
        existing_wsl_drive = bool(drive_state.get('display') and
                                  drive_state['display'].lower().startswith(('\\\\wsl.localhost\\', '\\\\wsl$\\')))
        if drive_state.get('root') and not existing_wsl_drive:
            raise RuntimeError('Z: is already owned by a non-WSL mapping; leave it untouched')
    output = base / 'native-windows-results'
    output.mkdir(exist_ok=True)
    local_root = None
    staged = []
    selected = TESTS if args.only == 'all' else (args.only,)
    subst_created = False
    owned_fixture_paths = []
    script = base / ('run-native-windows-tests-' + uuid.uuid4().hex + '.ps1')
    runner_output = ''
    try:
        local_root = Path(tempfile.mkdtemp(prefix='getdaq-windows-qt-tests-', dir=temp_unix))
        local_root_win = windows(local_root)
        if os.name != 'nt' and not existing_wsl_drive:
            subprocess.run([str(WINDOWS / 'subst.exe'), 'Z:', local_root_win], check=True,
                           cwd=WINDOWS, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            subst_created = True
        build_dirs = {}
        if os.name == 'nt':
            for name in selected:
                verify_native_test_build_dir(base / name / 'Makefile.Release', base / name)
                program = receipt['programs'][name]
                executable = verify_native_program_runtime(base, name, program)
                runtime = executable.parent
                staged.append((name, windows(executable), windows(runtime),
                               local_root / (name + '-stdout.txt'), local_root / (name + '-stderr.txt')))
            source_fixtures = base / 'source/get/GetBench-software-release-20210208/data'
            config_fixtures = base / 'config/experiment'
            if any((base / path).exists() for path in ('source', 'config', 'tests')):
                raise RuntimeError('Refusing to replace existing QtTest fixture paths')
            copy_owned_fixture(ROOT / 'source/get/GetBench-software-release-20210208/data',
                               source_fixtures, base / 'source', owned_fixture_paths)
            copy_owned_fixture(ROOT / 'config/experiment', config_fixtures,
                               base / 'config', owned_fixture_paths)
            (base / 'tests').mkdir()
            owned_fixture_paths.append(base / 'tests')
            test_cwd = windows(base / 'tests')
        elif existing_wsl_drive:
            common_build_root = None
            windows_cwd = None
            for name in selected:
                makefile = base / name / 'Makefile.Release'
                build_dir = qt_test_build_dir(makefile)
                if build_dir.drive.upper() != 'Z:':
                    raise RuntimeError('QtTest build path is not on the existing WSL Z: drive')
                test_root = build_dir.parent
                if common_build_root is None:
                    common_build_root = test_root
                    windows_cwd = str(test_root / 'tests')
                elif test_root != common_build_root:
                    raise RuntimeError('QtTest binaries use inconsistent build roots')
                program = receipt['programs'][name]
                executable_source = Path(program['path']).resolve()
                if not executable_source.is_file() or sha(executable_source) != program['sha256']:
                    raise RuntimeError('Test executable does not match its build receipt: ' + name)
                runtime_win = str(build_dir / 'release')
                executable_win = str(PureWindowsPath(runtime_win) / executable_source.name)
                staged.append((name, executable_win, runtime_win,
                               local_root / (name + '-stdout.txt'), local_root / (name + '-stderr.txt')))
            source_fixtures = base / 'source/get/GetBench-software-release-20210208/data'
            config_fixtures = base / 'config/experiment'
            if (base / 'source').exists() or (base / 'config').exists():
                raise RuntimeError('Refusing to replace existing QtTest fixture paths')
            copy_owned_fixture(ROOT / 'source/get/GetBench-software-release-20210208/data',
                               source_fixtures, base / 'source', owned_fixture_paths)
            copy_owned_fixture(ROOT / 'config/experiment', config_fixtures,
                               base / 'config', owned_fixture_paths)
            (base / 'tests').mkdir(parents=True, exist_ok=True)
            test_cwd = windows_cwd
        else:
            for name in selected:
                build_dir, executable, program = copy_runtime(receipt, base, name, local_root)
                build_dirs[name] = build_dir
                runtime = executable.parent
                staged.append((name, windows(executable), windows(runtime),
                               local_root / (name + '-stdout.txt'), local_root / (name + '-stderr.txt')))
            common_build_root = build_dirs[selected[0]].parent
            for name in selected[1:]:
                if build_dirs[name].parent != common_build_root:
                    raise RuntimeError('QtTest binaries use inconsistent build roots')
            shutil.copytree(ROOT / 'source/get/GetBench-software-release-20210208/data',
                            common_build_root / 'source/get/GetBench-software-release-20210208/data')
            shutil.copytree(ROOT / 'config/experiment', common_build_root / 'config/experiment')
            (common_build_root / 'tests').mkdir(parents=True, exist_ok=True)
            test_cwd = windows(common_build_root / 'tests')
        create_runner(script, local_root, test_cwd, staged)
        completed = subprocess.run([str(POWERSHELL), '-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass',
                                    '-File', windows(script)], cwd=WINDOWS, timeout=600,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                                   encoding='utf-8', errors='replace')
        runner_output = completed.stdout
        if completed.returncode:
            raise RuntimeError('Native Windows Qt tests failed; see native-windows-results/runner.log')
        summary = json.loads((local_root / 'summary.json').read_text(encoding='utf-8-sig'))
        if isinstance(summary, dict):
            summary = [summary]
        for name, _, _, stdout, stderr in staged:
            stdout_source = local_root / stdout.name
            stderr_source = local_root / stderr.name
            if stdout_source.is_file():
                shutil.copy2(stdout_source, output / stdout.name)
            if stderr_source.is_file():
                shutil.copy2(stderr_source, output / stderr.name)
        (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
        print(json.dumps(summary, indent=2))
    finally:
        try:
            if subst_created:
                subprocess.run([str(WINDOWS / 'subst.exe'), 'Z:', '/D'], cwd=WINDOWS,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=10)
        except subprocess.SubprocessError:
            pass
        output.mkdir(exist_ok=True)
        if runner_output:
            (output / 'runner.log').write_text(runner_output, encoding='utf-8')
        if local_root is not None and local_root.exists():
            for artifact in local_root.iterdir():
                if artifact.is_file() and artifact.suffix in ('.txt', '.json'):
                    shutil.copy2(artifact, output / artifact.name)
        cleanup_owned_fixtures(owned_fixture_paths)
        if script.exists():
            script.unlink()
        raw = list(local_root.rglob('*.graw')) if local_root is not None and local_root.exists() else []
        raw_bytes = sum(path.stat().st_size for path in raw if path.is_file())
        if local_root is not None and local_root.exists():
            shutil.rmtree(local_root)
        (output / 'cleanup-receipt.json').write_text(json.dumps({
            'temporary_root_removed': local_root is None or not local_root.exists(),
            'raw_files_removed': len(raw),
            'raw_bytes_removed': raw_bytes,
            'raw_limit_bytes': MAX_RAW_BYTES,
            'raw_limit_exceeded': raw_bytes > MAX_RAW_BYTES,
        }, indent=2) + '\n')
        if raw:
            message = 'Owned Windows test data exceeded 20 GiB' if raw_bytes > MAX_RAW_BYTES else 'Owned test .graw files remained after test completion'
            raise RuntimeError(message + '; files were removed and recorded')


if __name__ == '__main__':
    main()
