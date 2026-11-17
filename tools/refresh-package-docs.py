#!/usr/bin/env python3
"""Refresh README and user documentation in an already-built delivery package.

This intentionally preserves every executable and runtime dependency. The
package manifest is recalculated and the native package verifier is run both
before and after the update.
"""
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


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def verify(kind, candidate):
    if kind == 'host':
        module = load('getdaq_build_suite', ROOT / 'tools/build-suite.py')
        module.verify_stage(candidate)
    elif kind == 'windows':
        module = load('getdaq_windows_deploy', ROOT / 'tools/deploy-windows-from-wsl.py')
        module.verify_runtime(candidate)
    else:
        raise ValueError('Retired native package format: use the current host suite')


def source_docs(staging):
    staging.mkdir()
    (staging / 'docs').mkdir()
    shutil.copy2(ROOT / 'README.md', staging / 'README.md')
    for source in sorted((ROOT / 'docs').iterdir()):
        if source.is_file() and source.suffix in ('.md', '.png'):
            shutil.copy2(source, staging / 'docs' / source.name)


def refresh(kind, candidate):
    candidate = candidate.absolute()
    packages = (ROOT / 'build-packages').resolve()
    if candidate.is_symlink() or not candidate.is_dir():
        raise ValueError('Candidate must be an existing, non-symlink directory')
    resolved = candidate.resolve()
    try:
        resolved.relative_to(packages)
    except ValueError:
        raise ValueError('Candidate must be inside build-packages')
    if resolved != candidate:
        raise ValueError('Candidate path contains a symbolic link')
    for path in candidate.rglob('*'):
        if path.is_symlink():
            raise ValueError('Candidate contains a symbolic link: ' + str(path))
    verify(kind, candidate)
    protected_hashes = {
        path.relative_to(candidate).as_posix(): sha(path)
        for path in candidate.rglob('*')
        if path.is_file() and path.name != 'manifest.json' and path != candidate / 'README.md'
        and not path.relative_to(candidate).as_posix().startswith('docs/')
    }
    manifest_path = candidate / 'manifest.json'
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    docs = candidate / 'docs'
    if docs.is_symlink() or (docs.exists() and not docs.is_dir()):
        raise ValueError('Package docs path is not a plain directory')

    with tempfile.TemporaryDirectory(prefix='getdaq-package-docs-') as temp_name:
        temp = Path(temp_name)
        new_docs = temp / 'docs'
        source_docs(new_docs)
        old_docs = temp / 'old-docs'
        if docs.exists():
            shutil.copytree(docs, old_docs)
        old_readme = temp / 'old-README.md'
        shutil.copy2(candidate / 'README.md', old_readme)
        old_manifest = temp / 'old-manifest.json'
        shutil.copy2(manifest_path, old_manifest)
        try:
            if docs.exists():
                shutil.rmtree(docs)
            shutil.copytree(new_docs / 'docs', docs)
            shutil.copy2(new_docs / 'README.md', candidate / 'README.md')

            for name in list(manifest['files']):
                if name == 'README.md' or (name.startswith('docs/') and Path(name).suffix in ('.md', '.png')):
                    manifest['files'].pop(name)
            manifest['files']['README.md'] = sha(candidate / 'README.md')
            for path in sorted(docs.rglob('*')):
                if path.is_symlink():
                    raise ValueError('Source documentation contains a symbolic link')
                if path.is_file():
                    relative = path.relative_to(candidate).as_posix()
                    manifest['files'][relative] = sha(path)

            temp_manifest = candidate / '.manifest-refresh.tmp'
            with temp_manifest.open('w', encoding='utf-8', newline='\n') as stream:
                json.dump(manifest, stream, ensure_ascii=False, indent=2)
                stream.write('\n')
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(temp_manifest, manifest_path)
            verify(kind, candidate)
            current_hashes = {
                path.relative_to(candidate).as_posix(): sha(path)
                for path in candidate.rglob('*')
                if path.is_file() and path.name != 'manifest.json' and path != candidate / 'README.md'
                and not path.relative_to(candidate).as_posix().startswith('docs/')
            }
            if current_hashes != protected_hashes:
                raise RuntimeError('A program, runtime file or validation receipt changed during documentation refresh')
        except BaseException:
            if docs.exists():
                shutil.rmtree(docs)
            if old_docs.exists():
                shutil.copytree(old_docs, docs)
            shutil.copy2(old_readme, candidate / 'README.md')
            shutil.copy2(old_manifest, manifest_path)
            raise
    print('Refreshed package documentation; executables and runtime files were preserved:', candidate)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kind', choices=('host', 'windows'), required=True)
    parser.add_argument('--candidate', type=Path, required=True)
    args = parser.parse_args()
    refresh(args.kind, args.candidate)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, RuntimeError, KeyError) as error:
        print('Package documentation refresh failed: ' + str(error), file=sys.stderr)
        sys.exit(1)
