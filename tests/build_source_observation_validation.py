#!/usr/bin/env python3
"""Strict native sanitizer build; reuse only objects whose actual inputs still match."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
from run_receiver_observation_tests import sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['base', 'output']:
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    base, out = args.base.resolve(), args.output.resolve()
    assert base != out
    out.mkdir(parents=True, exist_ok=True)
    old = json.loads((base / 'source-sha256.json').read_text())
    rows = json.loads((base / 'commands.json').read_text())
    recipes = [row['command'] if isinstance(row, dict) else row for row in rows]
    libraries = json.loads((base / 'dependency-sha256.json').read_text())
    for name, digest in libraries.items():
        path = Path(name)
        if not path.is_absolute():
            path = root / path
        assert sha(path) == digest, ('Vendor library changed', path)
    old_generated = json.loads((base / 'generated-sha256.json').read_text())
    assert all(sha(Path(name)) == digest for name, digest in old_generated.items())
    generated = {}
    commands = []
    for recipe in recipes:
        if '-c' not in recipe and '-o' in recipe and recipe[recipe.index('-o') + 1].endswith('.cpp'):
            cmd = recipe.copy()
            previous = Path(cmd[cmd.index('-o') + 1])
            current = out / previous.name
            cmd[cmd.index('-o') + 1] = str(current)
            subprocess.run(cmd, check=True)
            generated[previous] = current
            commands.append(cmd)
    compiles = {Path(cmd[cmd.index('-o') + 1]): cmd for cmd in recipes if '-c' in cmd}
    template = next(cmd for cmd in compiles.values() if str(root / 'tests/ecc_network_service_test.cpp') in cmd)
    added = ['apps/sourcereader/sourceringreader.cpp', 'apps/sourcereader/sourceringreadout.cpp',
             'apps/sourcereader/sourcemfmring.cpp', 'tests/native_ecc_ice_receiver_test.cpp']
    new_objects = []
    for name in added:
        cmd = template.copy()
        obj = out / (Path(name).stem + '.o')
        cmd[cmd.index('-c') + 1] = str(root / name)
        cmd[cmd.index('-o') + 1] = str(obj)
        compiles[obj] = cmd
        new_objects.append(obj)
    programs = ['getdaq-ecc-server', 'ecc-network-service-tests', 'ice-source-control-tests']
    links = {}
    for name in programs:
        links[name] = next(cmd.copy() for cmd in recipes if '-c' not in cmd and '-o' in cmd and Path(cmd[cmd.index('-o') + 1]).name == name)
    links['native-ecc-ice-receiver-tests'] = links['ecc-network-service-tests'].copy()
    network_obj = next(obj for obj, cmd in compiles.items() if str(root / 'tests/ecc_network_service_test.cpp') in cmd)
    links['native-ecc-ice-receiver-tests'] = [str(new_objects[-1]) if word == str(network_obj) else word for word in links['native-ecc-ice-receiver-tests']]
    needed = {Path(word) for cmd in links.values() for word in cmd if word.endswith('.o')} | set(new_objects)
    replacements, reused, inputs, count = {}, {}, {}, 0
    for obj in sorted(needed):
        cmd = compiles[obj].copy()
        source = Path(cmd[cmd.index('-c') + 1])
        if source in generated:
            cmd[cmd.index('-c') + 1] = str(generated[source])
        cmd += ['-I' + str(root / 'apps/sourcereader')]
        assert all(flag in cmd for flag in ['-std=c++11', '-pedantic-errors', '-fsanitize=address,undefined'])
        dep = cmd.copy()
        dep.remove('-c')
        i = dep.index('-o')
        del dep[i:i + 2]
        dep += ['-MM', '-MT', 'observation']
        output = subprocess.check_output(dep, text=True)
        dependencies = [Path(word).resolve() for word in shlex.split(output.replace('\\\n', ' ').split(':', 1)[1])]
        unchanged = obj.parent == base and obj.is_file()
        for path in dependencies:
            digest = sha(path)
            inputs[str(path)] = digest
            if root in path.parents:
                unchanged = unchanged and old.get(str(path.relative_to(root))) == digest
            elif path.parent == out:
                original = next((oldpath for oldpath, newpath in generated.items() if newpath == path), None)
                unchanged = unchanged and original is not None and old_generated.get(str(original)) == digest
            else:
                unchanged = False
        if unchanged:
            reused[str(obj)] = sha(obj)
            continue
        target = out / obj.name
        cmd[cmd.index('-o') + 1] = str(target)
        with (out / (obj.stem + '.compile.txt')).open('w') as log:
            subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT, check=True)
        replacements[obj] = target
        commands.append(cmd)
        count += 1
        print('compiled', count, target.name, flush=True)
    for name, recipe in links.items():
        cmd = [str(replacements.get(Path(word), Path(word))) if word.endswith('.o') else word for word in recipe]
        cmd += [str(replacements.get(obj, obj)) for obj in new_objects[:3]]
        cmd[cmd.index('-o') + 1] = str(out / name)
        with (out / (name + '.link.txt')).open('w') as log:
            subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT, check=True)
        commands.append(cmd)
    assert all(sha(Path(name)) == digest for name, digest in inputs.items()), 'Inputs changed during build'
    assert all(sha(Path(name)) == digest for name, digest in reused.items()), 'Reused objects changed'
    (out / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
    inputs[str(Path(__file__).resolve())] = sha(Path(__file__).resolve())
    (out / 'input-sha256.json').write_text(json.dumps(inputs, indent=2) + '\n')
    (out / 'build-receipt.json').write_text(json.dumps({
        'schema': 1, 'sanitized': True, 'compile_count': count, 'reused_objects': reused,
        'base_commands_sha256': sha(base / 'commands.json'), 'vendor_library_sha256': libraries,
        'old_vendor_libraries_instrumented': False, 'system_qt_instrumented': False,
        'program_sha256': {name: sha(out / name) for name in links},
    }, indent=2) + '\n')
    print('PASS source observation native sanitizer build;', count, 'strict compiles;', len(reused), 'verified reused objects')


if __name__ == '__main__':
    main()
