#!/usr/bin/env python3
"""保存同版配置仓库构建凭据；只插桩新源码，不插桩系统Qt。"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


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
    command = ['cmake', '-S', str(root/'apps/configrepository'), '-B', str(build),
               '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON']
    if args.sanitized:
        command += ['-DCMAKE_BUILD_TYPE=Debug',
                    '-DCMAKE_CXX_FLAGS=-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -pedantic-errors',
                    '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined']
    else:
        command += ['-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_CXX_FLAGS=-pedantic-errors', '-DCMAKE_EXE_LINKER_FLAGS=']
    with (build/'results-configure-verified.txt').open('w') as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
    all_recipes = json.loads((build/'compile_commands.json').read_text())
    targets = ['getdaq-experiment-core.dir/', 'getdaq-configuration-repository.dir/',
               'getdaq-config-repository.dir/', 'getdaq-config-service.dir/', 'configuration-repository-tests.dir/']
    recipes = [r for r in all_recipes if any(target in r['command'] for target in targets)]
    tracked = {Path(__file__).resolve()}
    for recipe in recipes:
        path = Path(recipe['file']).resolve()
        if root in path.parents and build not in path.parents:
            tracked.add(path)
        flags = recipe['command']
        assert '-std=c++11' in flags and '-pedantic-errors' in flags, path
        if args.sanitized:
            assert '-fsanitize=address,undefined' in flags, path
    for folder in ['apps/configrepository', 'common/experiment', 'common/config', 'common/http']:
        tracked.update((root/folder).glob('*.h'))
        tracked.update((root/folder).glob('CMakeLists.txt'))
    tracked.update(root/'tests'/name for name in ('owned_test_directory.h', 'effective_record_fixture.h', 'cobo_configure_fixture.h'))
    before = {str(p.relative_to(root)):sha(p) for p in sorted(tracked)}
    with (build/'results-build-verified.txt').open('w') as log:
        subprocess.run(['cmake', '--build', str(build), '--parallel', '2', '--target',
                        'getdaq-config-repository', 'getdaq-config-service', 'configuration-repository-tests'],
                       stdout=log, stderr=subprocess.STDOUT, check=True)
    after = {str(p.relative_to(root)):sha(p) for p in sorted(tracked)}
    assert before == after, 'Build inputs changed during compilation'
    programs = ['getdaq-config-repository', 'getdaq-config-service', 'configuration-repository-tests']
    assert all((build/p).is_file() for p in programs), 'HTTP verification requires Qt service'
    generated = {str(p.relative_to(build)):sha(p) for p in (build/'getdaq-config-service_autogen').rglob('*.cpp')}
    (build/'source-sha256.json').write_text(json.dumps(after, indent=2)+'\n')
    (build/'generated-sha256.json').write_text(json.dumps(generated, indent=2)+'\n')
    (build/'verified-compile-commands.json').write_text(json.dumps(recipes, indent=2)+'\n')
    receipt = {'schema':1, 'sanitized':args.sanitized, 'system_qt_instrumented':False,
               'program_sha256':{p:sha(build/p) for p in programs}, 'compile_count':len(recipes),
               'tracked_input_count':len(after), 'commands':[command, ['cmake','--build',str(build),'--parallel','2','--target',*programs]]}
    (build/'build-receipt.json').write_text(json.dumps(receipt, indent=2)+'\n')
    print('PASS same-source repository build:', len(recipes), 'strict C++11 compiles;', len(after), 'inputs; sanitized', args.sanitized)


if __name__ == '__main__':
    main()
