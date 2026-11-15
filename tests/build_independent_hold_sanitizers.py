#!/usr/bin/env python3
"""Build independent alarm/source hold fixtures using a verified native ASan core."""
import argparse
import concurrent.futures
import hashlib
import json
import subprocess
from pathlib import Path


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('base', type=Path)
    parser.add_argument('sdk', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    base, sdk, out = args.base.resolve(), args.sdk.resolve(), args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    previous = json.loads((base / 'source-sha256.json').read_text())
    unused_tests = {'tests/native_ecc_coordinator_test.cpp', 'tests/ecc_network_service_test.cpp'}
    for name, digest in previous.items():
        if name not in unused_tests:
            assert sha(root / name) == digest, name
    for file in ('dependency-sha256.json', 'generated-sha256.json'):
        for name, digest in json.loads((base / file).read_text()).items():
            assert sha(root / name) == digest, name
    recipes = [r['command'] for r in json.loads((base / 'commands.json').read_text())]
    original_source = str(root / 'tests/native_ecc_coordinator_test.cpp')
    template = next(r for r in recipes if '-c' in r and original_source in r)
    original_object = template[template.index('-o') + 1]
    original_link = next(r for r in recipes if '-c' not in r and '-o' in r and
                         r[r.index('-o') + 1] == str(base / 'native-ecc-coordinator-tests'))
    sources = [root / name for name in (
        'apps/icecontrol/controlledicesource.cpp', 'apps/icecontrol/sourcecontrolservant.cpp',
        'apps/iceingress/icesourceclient.cpp', 'apps/sourcereader/sourceringreader.cpp',
        'apps/sourcereader/sourceringreadout.cpp', 'apps/sourcereader/sourcemfmring.cpp',
        'tests/native_ecc_ice_receiver_test.cpp', 'tests/ecc_network_service_test.cpp')]
    sources.append(sdk / 'ice-ingress/generated/IceIngress.cpp')
    includes = [root / 'apps/icecontrol', root / 'apps/iceingress', root / 'apps/sourcereader',
                sdk / 'ice-ingress/generated']
    commands, objects = [], {}
    for source in sources:
        command = list(template)
        obj = out / (source.stem + '.o')
        command[command.index('-c') + 1] = str(source)
        command[command.index('-o') + 1] = str(obj)
        command += ['-I' + str(p) for p in includes] + ['-DGETDAQ_TEST_ICE_SOURCE']
        assert '-std=c++11' in command and '-pedantic-errors' in command
        assert '-fsanitize=address,undefined' in command
        commands.append(command)
        objects[source] = obj
    tracked = {root / name: digest for name, digest in previous.items() if name not in unused_tests}
    tracked.update({p: sha(p) for p in sources})
    tracked[Path(__file__).resolve()] = sha(Path(__file__).resolve())
    for folder in (root / 'apps/icecontrol', root / 'apps/iceingress', root / 'apps/sourcereader'):
        tracked.update({p: sha(p) for p in folder.glob('*.h')})
    tracked.update({p: sha(p) for p in (root / 'tests').glob('*.h')})
    tracked[sdk / 'ice-ingress/generated/IceIngress.h'] = sha(sdk / 'ice-ingress/generated/IceIngress.h')
    reused = {word: sha(Path(word)) for word in original_link if word.endswith('.o') and word != original_object}
    with (out / 'compile.log').open('w') as log:
        def compile_one(command):
            subprocess.run(command, cwd=root, stdout=log, stderr=subprocess.STDOUT, check=True)
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
            list(pool.map(compile_one, commands))
        programs = {}
        for name, fixture in (('native-ecc-ice-receiver-tests', 'native_ecc_ice_receiver_test.cpp'),
                              ('ecc-network-service-tests', 'ecc_network_service_test.cpp')):
            link = [str(objects[root / 'tests' / fixture]) if word == original_object else word for word in original_link]
            link[link.index('-o') + 1] = str(out / name)
            # Static archives must follow every object that references their symbols.
            archive = next(i for i, word in enumerate(link) if word.endswith('.a'))
            link[archive:archive] = [str(obj) for source, obj in objects.items() if source.parent != root / 'tests']
            subprocess.run(link, cwd=root, stdout=log, stderr=subprocess.STDOUT, check=True)
            commands.append(link)
            programs[name] = {'path': str(out / name), 'sha256': sha(out / name)}
    for path, digest in tracked.items():
        assert sha(path) == digest, str(path)
    for name, digest in reused.items():
        assert sha(Path(name)) == digest, name
    receipt = {'schema': 1, 'new_compile_units': len(sources), 'programs': programs,
               'source_sha256': {str(p): h for p, h in tracked.items()}, 'reused_objects': reused,
               'base_commands_sha256': sha(base / 'commands.json'),
               'dependency_manifest_sha256': sha(base / 'dependency-sha256.json'),
               'old_vendor_libraries_and_system_qt_instrumented': False}
    (out / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
    (out / 'build-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print('PASS independent hold sanitizer fixtures:', len(sources), 'strict compiles;', len(reused), 'verified core objects')


if __name__ == '__main__':
    main()
