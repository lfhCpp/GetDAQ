#!/usr/bin/env python3
"""Build unchanged legacy file/XML CCfg + TinyXml as an isolated test oracle.

The production build does not invoke this script or add its include directories.
No network/download and no changes to source/, MUSIC/, or third-party originals.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[1]
ARCHIVE_SHA = 'fff00023dd79486d444c8e29922f4072e1d451fc5a4d2b6075852ead7f2b7b52'


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build-legacy-config')
    parser.add_argument('--jobs', type=int, choices=range(1, 5), default=2)
    parser.add_argument('--compiler', default='g++')
    parser.add_argument('--binary', action='store_true', help='Also build unchanged version-1 binary format as a test oracle')
    parser.add_argument('--probe', type=Path, default=ROOT / 'tests/legacy_configuration_probe.cpp')
    args = parser.parse_args()
    build = args.build_dir.resolve()
    build.mkdir(parents=True, exist_ok=True)
    archive = ROOT / 'third_party/validation/boost_1_55_0.tar.bz2'
    assert sha(archive) == ARCHIVE_SHA, 'Official Boost archive hash mismatch'
    headers = build / 'reference-dependencies'
    headers.mkdir(exist_ok=True)
    selected = []
    extracted_hashes = {}
    total = 0
    with tarfile.open(str(archive), 'r:bz2') as tar:
        for member in tar:
            parts = Path(member.name).parts
            wanted = (member.name.startswith('boost_1_55_0/boost/') or
                      member.name == 'boost_1_55_0/LICENSE_1_0.txt')
            if not wanted or member.isdir():
                continue
            assert member.isfile() and '..' not in parts and not Path(member.name).is_absolute()
            total += member.size
            assert member.size <= 16 * 1024 * 1024 and total <= 256 * 1024 * 1024
            destination = headers / member.name
            destination.resolve().relative_to(headers.resolve())
            destination.parent.mkdir(parents=True, exist_ok=True)
            assert not destination.is_symlink()
            content = tar.extractfile(member).read()
            assert len(content) == member.size
            destination.write_bytes(content)
            selected.append(destination)
            extracted_hashes[destination.resolve()] = hashlib.sha256(content).hexdigest()
    ccfg = ROOT / 'source/cconfig/CompoundConfig-release-20210208/src'
    tiny = ROOT / 'source/cconfig/TinyXml-release-20210208/src'
    makefile = ccfg / 'Makefile.am'
    text = makefile.read_text()
    common = text.split('\nsources = ', 1)[1].split('\n\nAM_CXXFLAGS', 1)[0]
    xml = text.split('if BUILD_CCFG_FMT_XML', 1)[1].split('endif', 1)[0].split('sources +=', 1)[1]
    binary = text.split('if BUILD_CCFG_FMT_BINARY', 1)[1].split('endif', 1)[0].split('sources +=', 1)[1] if args.binary else ''
    sources = [ccfg / p for p in re.findall(r'[\w/]+\.cpp', common + '\n' + xml + '\n' + binary)]
    sources += [tiny / p for p in ('tinyxml.cpp', 'tinyxmlparser.cpp', 'tinyxmlerror.cpp', 'tinystr.cpp')]
    probe=args.probe.resolve();probe.relative_to(ROOT)
    sources += [probe]
    assert len(sources) == len(set(sources)) and all(p.is_file() for p in sources)
    original_inputs = {makefile, Path(__file__).resolve(), archive} | set(sources)
    original_inputs.update(ccfg.rglob('*.h'))
    original_inputs.update(ccfg.rglob('*.hpp'))
    original_inputs.update(tiny.glob('*.h'))
    before = {p.resolve(): sha(p) for p in original_inputs}
    flags = ['-std=c++98', '-O1', '-g', '-DBUILD_CCFG_FMT_XML=1', '-DBUILD_CCFG_FMT_BINARY=' + ('1' if args.binary else '0'),
             '-DTIXML_USE_STL', '-DWORDS_LITTLE_ENDIAN', '-I' + str(ccfg), '-I' + str(tiny),
             '-I' + str(headers / 'boost_1_55_0')]
    objects = build / 'objects'
    objects.mkdir(exist_ok=True)

    def compile_one(pair):
        index, source = pair
        obj = objects / ('%03d_%s.o' % (index, source.stem))
        dep = obj.with_suffix('.d')
        command = [args.compiler] + flags + ['-MMD', '-MF', str(dep), '-c', str(source), '-o', str(obj)]
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        log = objects / ('%03d_%s.log' % (index, source.stem))
        log.write_text(result.stdout)
        if result.returncode:
            raise RuntimeError('Reference compile failed: %s\n%s' % (source, result.stdout[-6000:]))
        return {'source': str(source), 'object': str(obj), 'object_sha256': sha(obj),
                'command': command, 'dependencies': str(dep), 'log': str(log)}

    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        recipes = list(pool.map(compile_one, enumerate(sources)))
    program = build / 'legacy-configuration-probe'
    link = [args.compiler] + [r['object'] for r in recipes] + ['-o', str(program)]
    subprocess.run(link, check=True)
    inputs = {makefile, ROOT / 'tests/build_legacy_configuration_reference.py', archive}
    for recipe in recipes:
        dependency_text = Path(recipe['dependencies']).read_text().replace('\\\n', ' ')
        inputs.update(Path(p).resolve() for p in shlex.split(dependency_text.split(':', 1)[1]))
    input_hashes = {p.resolve(): sha(p) for p in inputs}
    for path, digest in input_hashes.items():
        assert digest == before.get(path, extracted_hashes.get(path)), ('Input changed during reference build', path)
    assert all(sha(p) == digest for p, digest in before.items()), 'Historical source changed during compilation'
    receipt = {'scope': 'unchanged historical CCfg file/text/XML + TinyXml, original Spirit Classic runtime; test reference only',
               'binary_format_enabled': args.binary,
               'excluded': ([] if args.binary else ['binary format']) + ['database/network extensions', 'GUI', 'hardware', 'production DAQ'],
               'compiler': subprocess.check_output([args.compiler, '--version'], text=True).splitlines()[0],
               'flags': flags, 'boost_archive_sha256': ARCHIVE_SHA, 'extracted_files': len(selected),
               'extracted_bytes': total, 'recipes': recipes, 'link_command': link,
               'inputs': {str(p): digest for p, digest in sorted(input_hashes.items())},
               'original_source_unchanged_during_build': True,
               'program': str(program), 'program_sha256': sha(program)}
    (build / 'build-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print('Built unchanged legacy reference: %d compilations; %d tracked inputs; %s' %
          (len(recipes), len(inputs), program), flush=True)


if __name__ == '__main__':
    main()
