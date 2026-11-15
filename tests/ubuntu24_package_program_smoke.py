#!/usr/bin/env python3
"""Launch every program in a staged Linux package without touching hardware."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def expected_programs():
    path = ROOT / 'tools/package_ubuntu24_catalog_ssh.py'
    spec = importlib.util.spec_from_file_location('getdaq_ubuntu24_package', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.expected_full_programs()


def target_command(executable, arguments, rootfs):
    if rootfs is None:
        return [str(executable), *arguments]
    rootfs = rootfs.resolve(strict=True)
    loader = rootfs / 'usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2'
    if not loader.is_file():
        raise RuntimeError('Ubuntu 24 dynamic loader is missing: ' + str(loader))
    libraries = [rootfs / part for part in (
        'lib/x86_64-linux-gnu', 'lib', 'usr/lib/x86_64-linux-gnu',
        'usr/lib', 'usr/local/lib'
    ) if (rootfs / part).is_dir()]
    return [str(loader), '--library-path', ':'.join(map(str, libraries)),
            str(executable), *arguments]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--candidate', required=True, type=Path)
    parser.add_argument('--evidence', required=True, type=Path)
    parser.add_argument('--rootfs', type=Path,
                        help='Run target binaries through this Ubuntu userspace loader')
    args = parser.parse_args()
    candidate = args.candidate.resolve(strict=True)
    manifest = json.loads((candidate / 'manifest.json').read_text(encoding='utf-8'))
    programs = manifest.get('programs', {})
    expected = expected_programs()
    if manifest.get('profile') != 'full' or set(programs) != expected:
        missing = sorted(expected - set(programs))
        extra = sorted(set(programs) - expected)
        raise RuntimeError('Full-profile package inventory differs from current build plan; '
                           'missing=' + repr(missing) + ', extra=' + repr(extra))

    results = []
    with tempfile.TemporaryDirectory(prefix='getdaq-package-smoke-') as temporary:
        cwd = Path(temporary)
        home = cwd / 'home'
        runtime = cwd / 'runtime'
        cache = cwd / 'cache'
        for directory in (home, runtime, cache):
            directory.mkdir(mode=0o700)
        environment = dict(os.environ, HOME=str(home), XDG_RUNTIME_DIR=str(runtime),
                           XDG_CACHE_HOME=str(cache), TMPDIR=str(cwd), QT_QPA_PLATFORM='offscreen')
        if args.rootfs is not None:
            plugin_path = args.rootfs.resolve(strict=True) / 'usr/lib/x86_64-linux-gnu/qt5/plugins'
            environment['QT_PLUGIN_PATH'] = str(plugin_path)
            environment['QT_QPA_PLATFORM_PLUGIN_PATH'] = str(plugin_path / 'platforms')

        for name, record in sorted(programs.items()):
            executable = candidate / record['path']
            if not executable.is_file() or sha(executable) != record['sha256']:
                raise RuntimeError('Package executable differs from manifest: ' + name)
            screenshot = cwd / 'GetDAQ-smoke.png'
            if name == 'GetDAQ':
                arguments = ['--screenshot', str(screenshot)]
            else:
                arguments = ['--help']
            try:
                process = subprocess.run(target_command(executable, arguments, args.rootfs),
                                         cwd=cwd, env=environment, stdout=subprocess.PIPE,
                                         stderr=subprocess.PIPE, timeout=30, check=False)
            except subprocess.TimeoutExpired as error:
                raise RuntimeError('Package program did not exit promptly: ' + name) from error
            output = process.stdout + process.stderr
            if process.returncode != 0:
                raise RuntimeError(name + ' exited ' + str(process.returncode) +
                                   ', expected 0: ' +
                                   output[-2048:].decode(errors='replace'))
            row = {'program': name, 'sha256': record['sha256'], 'arguments': arguments,
                   'exit_code': process.returncode, 'output_bytes': len(output),
                   'output_sha256': hashlib.sha256(output).hexdigest()}
            if name == 'GetDAQ':
                if not screenshot.is_file() or screenshot.read_bytes()[:8] != b'\x89PNG\r\n\x1a\n':
                    raise RuntimeError('GetDAQ did not save its startup smoke screenshot')
                row['screenshot_bytes'] = screenshot.stat().st_size
                row['screenshot_sha256'] = sha(screenshot)
                screenshot.unlink()
            results.append(row)

        qttest = home / '.qttest'
        if qttest.is_symlink() or (qttest.exists() and not qttest.is_dir()):
            raise RuntimeError('Unsafe Qt test-mode path appeared in the private package smoke HOME')
        if qttest.exists():
            import shutil
            shutil.rmtree(qttest)
        raw = list(cwd.rglob('*.graw'))
        if raw:
            raise RuntimeError('A package smoke invocation created raw payload data')

    inventory = {row['program']: row['sha256'] for row in results}
    inventory_bytes = json.dumps(inventory, sort_keys=True, separators=(',', ':')).encode()
    receipt = {'schema': 'getdaq.ubuntu24-package-smoke:1', 'status': 'complete',
               'program_inventory_sha256': hashlib.sha256(inventory_bytes).hexdigest(),
               'runner_sha256': sha(__file__), 'program_count': len(results),
               'hardware_accessed': False, 'raw_files_remaining': 0,
               'private_temp_cleaned': True, 'programs': results}
    args.evidence.parent.mkdir(parents=True, exist_ok=True)
    args.evidence.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    print('PASS package launch smoke:', len(results), 'programs; no hardware; no raw payload')


if __name__ == '__main__':
    main()
