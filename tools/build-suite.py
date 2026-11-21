#!/usr/bin/env python3
"""Build the host suite offline, with independent control and raw-data programs."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
CONTROL = ['GetDAQ', 'getdaq-sentry', 'getdaq-waveform-viewer', 'getdaq-cconfig-editor', 'getdaq-online-waveform-viewer']
GUI_CONTROL = CONTROL[:]
QT_CONTROL = CONTROL[:]
RECEIVER = ['getdaq-inspect', 'getdaq-merge', 'getdaq-matrix']
ANALYSIS = RECEIVER + ['getdaq-fits', 'getdaq-root', 'getdaq-root-viewer']
DATA_TOOLS = []
NATIVE = []
REPOSITORY = ['getdaq-config-repository', 'getdaq-config-service']
PROFILES = ('control', 'data', 'data-host', 'analysis', 'full')


def verify_dependencies(profile):
    spec = importlib.util.spec_from_file_location('getdaq_dependencies', ROOT / 'tools/verify-dependencies.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.verify(ROOT, profile)


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def write_json(path, value):
    path = Path(path)
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n',
                         encoding='utf-8')
    temporary.replace(path)


def source_hashes():
    """Small tracked inputs, including Slice and local dependency archives."""
    files = {ROOT / 'GetDAQ.pro', ROOT / 'build-macos.command', Path(__file__).resolve()}
    files.update(ROOT / name for name in ('tools/verify-dependencies.py',
                 'third_party/dependencies.json', 'third_party/archives/SHA256SUMS',
                 'third_party/yaml-cpp/SHA256SUMS'))
    suffixes = {'.cpp', '.c', '.h', '.hpp', '.pri', '.pro', '.ui', '.ice', '.qrc', '.patch'}
    for directory in ('apps', 'common', 'third_party/yaml-cpp'):
        for path in (ROOT / directory).rglob('*'):
            if path.is_file() and (path.suffix in suffixes or path.name == 'CMakeLists.txt'):
                files.add(path)
    files.add(ROOT / 'third_party/ice-compat/CMakeLists.txt')
    for name in ('original-backend-patches.py', 'verify-original-backends.py', 'build-original-backends.py', 'prepare-original-cconfig.py', 'verify-legacy-sources.py', 'prepare-ice-platform.py', 'build-macos.py', 'package-macos.py'):
        files.add(ROOT / 'tools' / name)
    files.update(p for directory in ('third_party/legacy-get', 'third_party/DataExporter') for p in (ROOT/directory).rglob('*') if p.is_file())
    files.update((ROOT / 'third_party/archives').glob('*.tar.gz'))
    files.update(p for p in (ROOT / 'config').rglob('*') if p.is_file())
    return {str(p.relative_to(ROOT)): sha(p) for p in sorted(files)}


def plan(args, build):
    items = []
    suffix = '.exe' if os.name == 'nt' else ''
    def cmake(component, source, targets, outputs, definitions=()):
        directory = build / component
        items.append({'component': component, 'directory': str(directory),
                      'configure': [args.cmake, '-S', str(ROOT / source), '-B', str(directory),
                                    '-DCMAKE_BUILD_TYPE=Release', '-DBUILD_TESTING=OFF',
                                    *definitions],
                      'build': [args.cmake, '--build', str(directory), '--parallel',
                                str(args.jobs), '--target', *targets],
                      'programs': {n: str(directory / (n + suffix)) for n in outputs}})
    if args.profile == 'analysis':
        root_prefix = tool_output([args.root_config, '--prefix'])
        cmake('analysis', 'apps/datareceiver', ANALYSIS, ANALYSIS,
              ['-DGETDAQ_BUILD_OPTIONAL_FITS_TOOLS=ON',
               '-DGETDAQ_BUILD_OPTIONAL_ROOT_TOOLS=ON',
               '-DGETDAQ_BUILD_OPTIONAL_ROOT_VIEWER=ON',
               '-DROOT_DIR=' + str(Path(root_prefix) / 'cmake')])
    elif args.profile != 'control':
        cmake('receiver', 'apps/datareceiver', RECEIVER, RECEIVER)
        if args.profile == 'full':
            cmake('native', 'third_party/ice-compat', ['compat-GET', 'getdaq-cobo-info'], ['getdaq-cobo-info'],
                  [])
            cmake('repository', 'apps/configrepository', REPOSITORY, REPOSITORY)
    if args.profile in ('control', 'full'):
        directory = build / 'control'
        items.append({'component': 'control', 'directory': str(directory),
                      'configure': [args.qmake, str(ROOT / 'GetDAQ.pro'),
                                    'GETDAQ_DESTDIR=' + str(build / 'bin'), 'GETDAQ_PYTHON=' + sys.executable, '-after',
                                    'CONFIG+=release', 'CONFIG-=debug', 'CONFIG-=app_bundle'],
                      'build': [args.make, '-j' + str(args.jobs)],
                      'programs': {n: str(build / 'bin' / (n + suffix)) for n in GUI_CONTROL}})
    if args.profile in ('full', 'data-host') and platform.system() == 'Linux':
        directory = build / 'node-agent'
        items.append({'component': 'node-agent', 'directory': str(directory),
                      'configure': [args.qmake, str(ROOT / 'apps/deployment/nodeagent.pro'),
                                    'DESTDIR=' + str(build / 'bin'), '-after',
                                    'CONFIG+=release', 'CONFIG-=debug'],
                      'build': [args.make, '-j' + str(args.jobs)],
                      'programs': {'getdaq-node-agent': str(build / 'bin/getdaq-node-agent')}})
    if args.profile == 'data-host' and platform.system() in ('Linux', 'Darwin'):
        directory = build / 'sentry'
        items.append({'component': 'sentry', 'directory': str(directory),
                      'configure': [args.qmake, str(ROOT / 'apps/sentry/sentry.pro'),
                                    'DESTDIR=' + str(build / 'bin'), '-after', 'CONFIG+=release', 'CONFIG-=debug'],
                      'build': [args.make, '-j' + str(args.jobs)],
                      'programs': {'getdaq-sentry': str(build / 'bin/getdaq-sentry')}})
    if args.profile in ('full', 'control', 'data-host') and platform.system() in ('Linux', 'Darwin'):
        directory = build / 'original-backends'
        ice_build = build / 'native' if args.profile == 'full' else build / 'ice'
        if args.profile in ('control', 'data-host'):
            cmake('ice', 'third_party/ice-compat', ['compat-GET', 'getdaq-cobo-info'], ['getdaq-cobo-info'],
                  [])
        command = [sys.executable, str(ROOT / 'tools/build-original-backends.py'),
                   '--build-dir', str(directory), '--ice-build', str(ice_build), '--jobs', str(args.jobs)]
        items.append({'component': 'original-backends', 'directory': str(directory),
                      'configure': command + ['--prepare-only'], 'build': command,
                      'programs': {name: str(directory / 'build' / name) for name in ('getEccSoapServer', 'DataExporter', 'dataRouter')}})
    return items


def tool_output(command):
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, timeout=30, check=True)
    return result.stdout.strip()


def add_cmake_qt_prefix(steps, qt_prefix):
    """Add Qt's install prefix only to CMake configure commands.

    qmake uses its selected Qt installation directly and rejects CMake's
    ``-D...`` syntax, so the configure command shape is the source of truth.
    """
    for step in steps:
        command = step['configure']
        if '-S' in command and '-B' in command:
            command.append('-DCMAKE_PREFIX_PATH=' + qt_prefix)


def macos_architectures(program):
    result = subprocess.run(['lipo', '-archs', str(program)],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, timeout=30)
    if result.returncode:
        raise RuntimeError('Cannot inspect Mach-O architectures for ' + str(program) +
                           ' with lipo: ' + result.stdout)
    aliases = {'aarch64': 'arm64', 'amd64': 'x86_64'}
    host = aliases.get(platform.machine().lower(), platform.machine().lower())
    architectures = result.stdout.strip().split()
    if not architectures or any(not item.replace('_', '').isalnum() for item in architectures):
        raise RuntimeError('Invalid lipo architecture output for ' + str(program) + ': ' + result.stdout)
    if host not in architectures:
        raise RuntimeError('Mach-O architecture mismatch for ' + str(program) +
                           ': host=' + host + ', binary=' + ','.join(architectures))
    return {'architectures': architectures, 'host_architecture': host}


def runtime_dependencies(program, name):
    system = platform.system()
    if system == 'Linux':
        command = ['ldd', str(program)]
    elif system == 'Darwin':
        command = ['otool', '-L', str(program)]
    else:
        return {'checked': False, 'reason': 'Use the target platform deployment tools'}
    result = subprocess.run(command, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, timeout=30)
    text = result.stdout
    static_linux = system == 'Linux' and result.returncode and (
        'not a dynamic executable' in text or 'statically linked' in text)
    if result.returncode and not static_linux:
        raise RuntimeError('Cannot inspect runtime dependencies for ' + name +
                           ' with ' + command[0] + ': ' + text)
    if system == 'Linux' and 'not found' in text:
        raise RuntimeError('Unresolved runtime dependency for ' + name + ': ' + text)
    if system == 'Darwin':
        dependencies = [line.strip().split(' (', 1)[0] for line in text.splitlines()[1:]
                        if line.strip()]
        dependency_names = [Path(item).name.lower() for item in dependencies]
        forbidden_prefixes = {
            'qt': ('qt', 'libqt'), 'ice': ('ice', 'libice'),
            'yaml': ('yaml', 'libyaml'), 'boost': ('boost', 'libboost'),
            'curl': ('curl', 'libcurl'),
        }
        has_forbidden = lambda library: any(
            item.startswith(forbidden_prefixes[library]) for item in dependency_names)
    else:
        forbidden_tokens = {'qt': 'libQt', 'ice': 'libIce', 'yaml': 'libyaml',
                            'boost': 'libboost', 'curl': 'libcurl'}
        has_forbidden = lambda library: forbidden_tokens[library] in text
    if name in RECEIVER + DATA_TOOLS + ['getdaq-config-repository']:
        if any(has_forbidden(library) for library in ('qt', 'ice', 'yaml', 'boost', 'curl')):
            raise RuntimeError('Unexpected dependency in native program ' + name)
    if name in ('getdaq-fits', 'getdaq-root') and any(
            has_forbidden(library) for library in ('qt', 'ice')):
        raise RuntimeError('Unexpected Qt/Ice dependency in analysis converter ' + name)
    if name in ('getdaq-ice-receiver', 'getdaq-ice-send') and has_forbidden('qt'):
        raise RuntimeError('Qt must not enter the optional ICE data path')
    record = {'checked': True, 'inspector': command[0],
              'system_libraries_bundled': False}
    record['ldd' if system == 'Linux' else 'otool'] = text.strip()
    if system == 'Darwin':
        record['mach_o'] = macos_architectures(program)
    return record


def stage_files():
    """Only documented templates; never include private credentials or run data."""
    files = [(ROOT / 'README.md', 'README.md'),
             (ROOT / 'config/product-features.json', 'share/product-features.json'),
             (ROOT / 'docs/manual/GetDAQ-Quick-Guide.pdf', 'docs/manual/GetDAQ-Quick-Guide.pdf')]
    files += [(p, 'docs/' + p.name) for p in sorted((ROOT / 'docs').glob('*'))
              if p.is_file() and p.suffix in ('.md', '.png')]
    for p in sorted((ROOT / 'config').rglob('*')):
        if p.is_file() and ('.example.' in p.name):
            files.append((p, 'share/' + str(p.relative_to(ROOT))))
    files += [(p, 'share/third_party/' + str(p.relative_to(ROOT / 'third_party')))
              for p in sorted((ROOT / 'third_party/licenses').rglob('*')) if p.is_file()]
    files += [(p, 'share/licenses/cconfig-editor/' + p.name)
              for p in sorted((ROOT / 'apps/cconfigeditor/licenses').iterdir()) if p.is_file()]
    files += [(p, 'share/get-bench/format/' + p.name) for p in sorted(
              (ROOT / 'third_party/legacy-get/getbench/data/format').glob('*.xcfg'))]
    files += [(ROOT / 'third_party/ATTPC-LICENSE', 'share/third_party/ATTPC-LICENSE'),
              (ROOT / 'third_party/yaml-cpp/LICENSE', 'share/third_party/yaml-cpp/LICENSE'),
              (ROOT / 'third_party/dependencies.json', 'share/third_party/dependencies.json')]
    return files


def verify_stage(directory):
    directory = Path(directory).resolve()
    manifest_path = directory / 'manifest.json'
    if manifest_path.is_symlink() or not manifest_path.is_file():
        raise RuntimeError('Package manifest must be a regular file')
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    if manifest.get('schema') != 'getdaq.host-suite:1' or manifest.get('status') != 'complete':
        raise RuntimeError('Not a complete host-suite package')
    for name, digest in manifest['files'].items():
        relative = Path(name)
        if relative.is_absolute() or '..' in relative.parts:
            raise RuntimeError('Invalid package path')
        path = directory / relative
        if path.is_symlink() or not path.is_file() or sha(path) != digest:
            raise RuntimeError('Package file missing or changed: ' + name)
    if manifest.get('complete_file_inventory') is True:
        symlinks = [str(path.relative_to(directory)) for path in directory.rglob('*')
                    if path.is_symlink()]
        if symlinks:
            raise RuntimeError('Complete package inventory contains symbolic links: ' +
                               ', '.join(symlinks[:5]))
        actual = {path.relative_to(directory).as_posix()
                  for path in directory.rglob('*')
                  if path.is_file() and path != manifest_path}
        declared = set(manifest['files'])
        if actual != declared:
            unlisted = sorted(actual - declared)
            missing = sorted(declared - actual)
            raise RuntimeError('Package file inventory mismatch; unlisted=' +
                               repr(unlisted[:5]) + ', missing=' + repr(missing[:5]))
    if (directory / 'BUILD_INCOMPLETE').exists():
        raise RuntimeError('Package is marked incomplete')
    print('Verified package: ' + str(directory) + '; ' + str(len(manifest['files'])) + ' files')
    return manifest


def stage(directory, receipt):
    directory = Path(directory).resolve()
    directory.mkdir(parents=True, exist_ok=False)
    marker = directory / 'BUILD_INCOMPLETE'
    marker.write_text('Not a completed build; do not deploy.\n', encoding='utf-8')
    manifest = {k: v for k, v in receipt.items() if k not in ('commands', 'inputs')}
    manifest['complete_file_inventory'] = True
    manifest['programs'] = {name: dict(record)
                            for name, record in receipt['programs'].items()}
    for name, record in manifest['programs'].items():
        # A staged package must remain verifiable after it moves off the build
        # host or out of a chroot. Build receipt paths are machine-specific.
        record['path'] = (Path('bin') / Path(record['path']).name).as_posix()
    manifest['files'] = {}
    for name, record in receipt['programs'].items():
        source = Path(record['path'])
        if sha(source) != record['sha256']:
            raise RuntimeError('Program changed before packaging: ' + name)
        target = directory / 'bin' / source.name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        # Build hosts may use umask 002; installed executables must not retain
        # group/other write access (hardware/receiver launchers require this).
        if os.name == 'posix':
            target.chmod(target.stat().st_mode & ~0o022)
        manifest['files'][str(target.relative_to(directory))] = sha(target)
    for source, relative in stage_files():
        target = directory / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        manifest['files'][relative] = sha(target)
    write_json(directory / 'manifest.json', manifest)
    marker.unlink()
    verify_stage(directory)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', choices=PROFILES, default='full')
    parser.add_argument('--build-root', type=Path, default=ROOT / 'build-delivery')
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--qmake', default=os.environ.get('GETDAQ_QMAKE', 'qmake'))
    parser.add_argument('--cmake', default='cmake')
    parser.add_argument('--make', default='mingw32-make' if os.name == 'nt' else 'make')
    parser.add_argument('--root-config', default=os.environ.get('GETDAQ_ROOT_CONFIG', 'root-config'))
    parser.add_argument('--plan', action='store_true', help='Print commands without creating files')
    parser.add_argument('--stage', type=Path, help='Copy completed suite to a NEW directory')
    parser.add_argument('--verify-stage', type=Path, help='Check an existing package without building')
    args = parser.parse_args()
    if args.verify_stage:
        verify_stage(args.verify_stage)
        return
    if not 1 <= args.jobs <= 64:
        parser.error('--jobs must be 1..64')
    if os.name == 'nt' and args.profile != 'control':
        parser.error('Windows supports the control profile; POSIX servers run on Linux/macOS')
    build = args.build_root.resolve() / args.profile
    steps = plan(args, build)
    if args.plan:
        print(json.dumps({'profile': args.profile, 'steps': steps}, ensure_ascii=False, indent=2))
        return
    if args.stage and args.stage.resolve().exists():
        parser.error('--stage must name a new directory')
    dependency_verification = verify_dependencies(args.profile)
    print('Verified embedded dependencies: ' + str(len(dependency_verification['checked_files'])) + ' files', flush=True)
    versions = {'python': platform.python_version(), 'system': platform.platform()}
    needs_qt = args.profile in ('control', 'data-host', 'analysis', 'full')
    if args.profile != 'control':
        versions['cmake'] = tool_output([args.cmake, '--version']).splitlines()[0]
    if needs_qt:
        versions['qt'] = tool_output([args.qmake, '-query', 'QT_VERSION'])
        fields = versions['qt'].split('.')
        if len(fields) < 2 or int(fields[0]) != 5 or int(fields[1]) < 12:
            raise RuntimeError('Select Qt 5.12 or later Qt 5; Windows target is Qt 5.14.2 MinGW')
        qt_prefix = tool_output([args.qmake, '-query', 'QT_INSTALL_PREFIX'])
        add_cmake_qt_prefix(steps, qt_prefix)
    if args.profile == 'analysis':
        versions['root'] = tool_output([args.root_config, '--version'])
        versions['root_prefix'] = tool_output([args.root_config, '--prefix'])
        versions['cfitsio'] = tool_output(['pkg-config', '--modversion', 'cfitsio'])
    if args.profile in ('control', 'full'):
        versions['make'] = tool_output([args.make, '--version']).splitlines()[0]
    build.mkdir(parents=True, exist_ok=True)
    lock = build / '.build-lock'
    try:
        lock.mkdir()
    except FileExistsError:
        raise RuntimeError('Another build may use this profile. Check its process before removing ' + str(lock))
    receipt = {'schema': 'getdaq.host-suite:1', 'status': 'building', 'profile': args.profile,
               'platform': versions, 'created_utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
               'board_services_rewritten': False, 'target_platform_and_hardware_accepted': False,
               'embedded_dependencies': dependency_verification,
               'commands': [], 'programs': {}}
    try:
        inputs = source_hashes()
        receipt['inputs'] = inputs
        write_json(build / 'build-receipt.json', receipt)
        for step in steps:
            directory = Path(step['directory'])
            directory.mkdir(parents=True, exist_ok=True)
            print('Building ' + step['component'] + ' ...', flush=True)
            with (build / (step['component'] + '.log')).open('w', encoding='utf-8') as log:
                for command in (step['configure'], step['build']):
                    receipt['commands'].append({'cwd': str(directory), 'argv': command})
                    log.write(json.dumps(command, ensure_ascii=False) + '\n')
                    log.flush()
                    subprocess.run(command, cwd=directory, stdout=log, stderr=subprocess.STDOUT,
                                   check=True)
            for name, path in step['programs'].items():
                program = Path(path)
                if not program.is_file() or not os.access(program, os.X_OK):
                    raise RuntimeError('Expected executable absent: ' + str(program))
                receipt['programs'][name] = {'path': str(program), 'sha256': sha(program),
                                            'runtime': runtime_dependencies(program, name)}
        if source_hashes() != inputs:
            raise RuntimeError('Build inputs changed; rerun from stable sources')
        receipt['status'] = 'complete'
        write_json(build / 'build-receipt.json', receipt)
        if args.stage:
            stage(args.stage, receipt)
        print('Built ' + str(len(receipt['programs'])) + ' programs; receipt: ' + str(build / 'build-receipt.json'))
        print('Build success does not establish hardware or target-platform acceptance.')
    except BaseException as error:
        receipt['status'] = 'failed'
        receipt['failure_type'] = type(error).__name__
        write_json(build / 'build-receipt.json', receipt)
        raise
    finally:
        lock.rmdir()


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, subprocess.SubprocessError, ValueError) as error:
        print('Build failed: ' + str(error), file=sys.stderr)
        sys.exit(1)
