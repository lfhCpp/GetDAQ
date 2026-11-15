#!/usr/bin/env python3
"""Build the standalone standard C++11 hardware-description model with receipts."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path)
    parser.add_argument('--sanitized', action='store_true')
    args = parser.parse_args(); build = args.build.resolve(); build.mkdir(parents=True, exist_ok=True)
    command = ['cmake', '-S', str(root / 'common/hardware'), '-B', str(build), '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON']
    if args.sanitized:
        command += ['-DCMAKE_BUILD_TYPE=Debug', '-DCMAKE_CXX_FLAGS=-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -pedantic-errors',
                    '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined']
    else:
        command += ['-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_CXX_FLAGS=-pedantic-errors', '-DCMAKE_EXE_LINKER_FLAGS=']
    with (build / 'results-configure-verified.txt').open('w') as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
    all_recipes = json.loads((build / 'compile_commands.json').read_text())
    targets = ('getdaq-experiment-core.dir/', 'getdaq-hardware-description-core.dir/', 'hardware-description-tests.dir/')
    recipes = [entry for entry in all_recipes if any(target in entry['command'] for target in targets)]
    tracked = {Path(entry['file']).resolve() for entry in recipes} | {Path(__file__).resolve()}
    for folder in ('common/hardware', 'common/experiment', 'common/config'):
        tracked.update((root / folder).glob('*.h')); tracked.update((root / folder).glob('CMakeLists.txt'))
    before = {str(path.relative_to(root)): sha(path) for path in sorted(tracked)}
    for entry in recipes:
        assert '-std=c++11' in entry['command'] and '-pedantic-errors' in entry['command']
        if args.sanitized: assert '-fsanitize=address,undefined' in entry['command']
    build_command = ['cmake', '--build', str(build), '--parallel', '2', '--target', 'hardware-description-tests']
    with (build / 'results-build-verified.txt').open('w') as log:
        subprocess.run(build_command, stdout=log, stderr=subprocess.STDOUT, check=True)
    assert before == {str(path.relative_to(root)): sha(path) for path in sorted(tracked)}, 'Build inputs changed'
    program = build / 'hardware-description-tests'
    dependencies = subprocess.check_output(['ldd', str(program)], text=True)
    assert not any(name in dependencies for name in ('libQt', 'libIce', 'libyaml', 'libboost', 'libcurl', 'libCore.so'))
    (build / 'source-sha256.json').write_text(json.dumps(before, indent=2) + '\n')
    (build / 'verified-compile-commands.json').write_text(json.dumps(recipes, indent=2) + '\n')
    (build / 'build-receipt.json').write_text(json.dumps({'schema': 1, 'sanitized': args.sanitized,
        'program_sha256': sha(program), 'compile_count': len(recipes), 'commands': [command, build_command],
        'runtime_dependencies': dependencies, 'scope': 'description model only; native Ice/control and system libraries not wholly instrumented'}, indent=2) + '\n')
    print('PASS standalone description build:', len(recipes), 'strict C++11 compiles; sanitized', args.sanitized)


if __name__ == '__main__': main()
