#!/usr/bin/env python3
"""Build native receiver with source, compiler-command and executable receipts."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path)
    parser.add_argument('--sanitized', action='store_true')
    args = parser.parse_args()
    build = args.build.resolve()
    build.mkdir(parents=True, exist_ok=True)
    command = ['cmake', '-S', str(root / 'apps/datareceiver'), '-B', str(build),
               '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON']
    if args.sanitized:
        command += ['-DCMAKE_BUILD_TYPE=Debug',
                    '-DCMAKE_CXX_FLAGS=-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -pedantic-errors',
                    '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined']
    else:
        command += ['-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_CXX_FLAGS=-pedantic-errors',
                    '-DCMAKE_EXE_LINKER_FLAGS=']
    with (build / 'results-configure-verified.txt').open('w') as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
    recipes = json.loads((build / 'compile_commands.json').read_text())
    tracked = {Path(__file__).resolve(), root / 'apps/datareceiver/CMakeLists.txt'}
    for recipe in recipes:
        source = Path(recipe['file']).resolve()
        assert root in source.parents, source
        tracked.add(source)
        assert '-std=c++11' in recipe['command'] and '-pedantic-errors' in recipe['command']
        if args.sanitized:
            assert '-fsanitize=address,undefined' in recipe['command']
    for folder in ('apps/datareceiver', 'common/storage', 'common/text'):
        tracked.update((root / folder).glob('*.h'))
    before = {str(p.relative_to(root)): sha(p) for p in sorted(tracked)}
    build_command = ['cmake', '--build', str(build), '--parallel', '2']
    with (build / 'results-build-verified.txt').open('w') as log:
        subprocess.run(build_command, stdout=log, stderr=subprocess.STDOUT, check=True)
    after = {str(p.relative_to(root)): sha(p) for p in sorted(tracked)}
    assert before == after, 'Build inputs changed during compilation'
    programs = ['getdaq-receiver', 'getdaq-inspect', 'getdaq-merge', 'getdaq-matrix',
                'receiver-core-tests', 'merge-tests', 'matrix-tests', 'mfm-stream-benchmark']
    if sys.platform.startswith('linux'):
        programs.append('exporter-lock-tests')
    dependencies = {}
    for name in programs:
        linked = subprocess.run(['ldd', str(build / name)], capture_output=True, text=True, check=True).stdout
        dependencies[name] = linked
        assert not any(token in linked for token in ('libQt', 'libIce', 'libyaml', 'libboost', 'libcurl', 'libCore.so'))
    receipt = {'schema': 1, 'sanitized': args.sanitized, 'compile_count': len(recipes),
               'commands': [command, build_command], 'program_sha256': {p: sha(build / p) for p in programs},
               'compiler_version': subprocess.check_output(['c++', '--version'], text=True).splitlines()[0],
               'platform': subprocess.check_output(['uname', '-sm'], text=True).strip(),
               'raw_path_uses_qt': False, 'runtime_dependencies': dependencies}
    (build / 'source-sha256.json').write_text(json.dumps(after, indent=2) + '\n')
    (build / 'verified-compile-commands.json').write_text(json.dumps(recipes, indent=2) + '\n')
    (build / 'build-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print('PASS native receiver build:', len(recipes), 'strict C++11 compiles;',
          len(after), 'inputs; sanitized', args.sanitized)


if __name__ == '__main__':
    main()
