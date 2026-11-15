#!/usr/bin/env python3
"""Refresh packaged example configurations without rebuilding programs."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PACKAGES = ROOT / 'build-packages'


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def package_verifier(kind):
    if kind == 'windows':
        deploy = load('getdaq_windows_deploy', ROOT / 'tools/deploy-windows-from-wsl.py')
        return deploy.verify_runtime
    suite = load('getdaq_build_suite', ROOT / 'tools/build-suite.py')
    return suite.verify_stage


def source_examples():
    suite = load('getdaq_build_suite', ROOT / 'tools/build-suite.py')
    result = {}
    for source, relative in suite.stage_files():
        relative = Path(relative).as_posix()
        if relative.startswith('share/config/'):
            if source.is_symlink() or not source.is_file():
                raise RuntimeError('Packaged example source is not a regular file: ' + str(source))
            result[relative] = source
    if not result:
        raise RuntimeError('The host-suite build plan contains no packaged configuration examples')
    return result


def refresh(kind, candidate):
    candidate = Path(candidate).absolute()
    if candidate.is_symlink() or not candidate.is_dir() or candidate.resolve() != candidate:
        raise ValueError('Candidate must be an existing directory without symbolic links')
    try:
        candidate.relative_to(PACKAGES.resolve())
    except ValueError:
        raise ValueError('Candidate must be inside build-packages')
    for path in candidate.rglob('*'):
        if path.is_symlink():
            raise ValueError('Candidate contains a symbolic link: ' + str(path.relative_to(candidate)))

    verify = package_verifier(kind)
    verify(candidate)
    manifest = json.loads((candidate / 'manifest.json').read_text(encoding='utf-8'))
    if kind == 'ubuntu24' and manifest.get('profile') != 'full':
        raise RuntimeError('Ubuntu24 candidate must contain the full host suite')
    examples = source_examples()
    manifest_names = set(manifest.get('files', {}))
    packaged_names = {name for name in manifest_names if name.startswith('share/config/')}
    if packaged_names != set(examples):
        missing = sorted(set(examples) - packaged_names)
        extra = sorted(packaged_names - set(examples))
        raise RuntimeError('Candidate example inventory differs from source; missing=' +
                           ','.join(missing) + '; extra=' + ','.join(extra))

    protected = {
        path.relative_to(candidate).as_posix(): sha(path)
        for path in candidate.rglob('*')
        if path.is_file() and path.name != 'manifest.json'
        and not path.relative_to(candidate).as_posix().startswith('share/config/')
    }
    staging = Path(tempfile.mkdtemp(prefix='.' + candidate.name + '.examples-', dir=str(candidate.parent)))
    backup = candidate.parent / ('.' + candidate.name + '.previous-' + str(os.getpid()))
    if backup.exists() or backup.is_symlink():
        shutil.rmtree(staging)
        raise RuntimeError('A prior package-refresh backup exists; inspect it before retrying')
    moved_original = False
    try:
        shutil.rmtree(staging)
        shutil.copytree(candidate, staging)
        updated = json.loads((staging / 'manifest.json').read_text(encoding='utf-8'))
        hashes = {}
        for relative, source in sorted(examples.items()):
            target = staging / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            temporary = target.with_name(target.name + '.refresh-tmp')
            shutil.copyfile(source, temporary)
            os.replace(temporary, target)
            hashes[relative] = sha(target)
            updated['files'][relative] = hashes[relative]
        updated['configuration_examples'] = {
            'status': 'refreshed-from-current-source',
            'files': hashes,
        }
        manifest_temp = staging / '.manifest-examples.tmp'
        with manifest_temp.open('w', encoding='utf-8', newline='\n') as stream:
            json.dump(updated, stream, ensure_ascii=False, indent=2)
            stream.write('\n')
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(manifest_temp, staging / 'manifest.json')
        verify(staging)
        after = {
            path.relative_to(staging).as_posix(): sha(path)
            for path in staging.rglob('*')
            if path.is_file() and path.name != 'manifest.json'
            and not path.relative_to(staging).as_posix().startswith('share/config/')
        }
        if after != protected:
            raise RuntimeError('A program, runtime dependency, document or existing validation receipt changed')

        os.replace(candidate, backup)
        moved_original = True
        try:
            os.replace(staging, candidate)
        except BaseException:
            os.replace(backup, candidate)
            moved_original = False
            raise
        shutil.rmtree(backup)
        moved_original = False
        print('Refreshed', len(hashes), 'packaged examples; programs and other files were preserved:', candidate)
        for relative, digest in sorted(hashes.items()):
            print(relative, digest)
    finally:
        if staging.exists():
            shutil.rmtree(staging)
        if moved_original and backup.exists() and not candidate.exists():
            os.replace(backup, candidate)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kind', choices=('ubuntu24', 'windows'), required=True)
    parser.add_argument('--candidate', required=True, type=Path)
    args = parser.parse_args()
    refresh(args.kind, args.candidate)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, RuntimeError, KeyError) as error:
        print('Package example refresh failed: ' + str(error), file=sys.stderr)
        sys.exit(1)
