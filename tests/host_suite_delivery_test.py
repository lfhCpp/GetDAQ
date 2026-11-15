"""Packaging checks complement the actual build, without DAQ or hardware traffic."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('host_suite_builder', ROOT / 'tools/build-suite.py')
suite = importlib.util.module_from_spec(spec)
spec.loader.exec_module(suite)
platform_spec = importlib.util.spec_from_file_location(
    'ubuntu24_platform', ROOT / 'tests/ubuntu24_platform_validation.py')
platform_validation = importlib.util.module_from_spec(platform_spec)
platform_spec.loader.exec_module(platform_validation)


class HostSuiteDeliveryTest(unittest.TestCase):
    def test_macos_runtime_dependencies_use_otool_and_are_recorded(self):
        with tempfile.TemporaryDirectory(prefix='getdaq-ice-compat-runtime-deps-') as temporary:
            program = Path(temporary) / 'getdaq-receiver'
            program.write_bytes(b'Mach-O inspection fixture; not an executable')
            output = ('%s:\n\t/usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 1.0.0)\n'
                      '\t/System/Library/Frameworks/CoreFoundation.framework/Versions/A/CoreFoundation (compatibility version 150.0.0, current version 2500.0.0)\n') % program
            otool_result = SimpleNamespace(stdout=output, returncode=0)
            lipo_result = SimpleNamespace(stdout='arm64\n', returncode=0)
            with mock.patch.object(suite.platform, 'system', return_value='Darwin'), \
                    mock.patch.object(suite.platform, 'machine', return_value='arm64'), \
                    mock.patch.object(suite.subprocess, 'run', side_effect=[otool_result, lipo_result]) as run:
                receipt = suite.runtime_dependencies(program, 'getdaq-receiver')
            self.assertEqual(run.call_args_list[0].args[0], ['otool', '-L', str(program)])
            self.assertEqual(run.call_args_list[1].args[0], ['lipo', '-archs', str(program)])
            self.assertTrue(receipt['checked'])
            self.assertEqual(receipt['inspector'], 'otool')
            self.assertEqual(receipt['otool'], output.strip())
            self.assertEqual(receipt['mach_o'], {'architectures': ['arm64'], 'host_architecture': 'arm64'})
            self.assertFalse(receipt['system_libraries_bundled'])

    def test_macos_architecture_check_records_universal_binary_and_alias(self):
        program = Path('/private/build/getdaq')
        with mock.patch.object(suite.platform, 'machine', return_value='aarch64'), \
                mock.patch.object(suite.subprocess, 'run', return_value=SimpleNamespace(
                    stdout='x86_64 arm64\n', returncode=0)) as run:
            record = suite.macos_architectures(program)
        self.assertEqual(run.call_args.args[0], ['lipo', '-archs', str(program)])
        self.assertEqual(record, {'architectures': ['x86_64', 'arm64'], 'host_architecture': 'arm64'})

    def test_macos_architecture_check_rejects_lipo_info_description_output(self):
        program = Path('/private/build/path with spaces/getdaq')
        output = 'Non-fat file: %s is architecture: arm64\n' % program
        with mock.patch.object(suite.platform, 'machine', return_value='arm64'), \
                mock.patch.object(suite.subprocess, 'run', return_value=SimpleNamespace(
                    stdout=output, returncode=0)):
            with self.assertRaisesRegex(RuntimeError, 'Invalid lipo architecture output'):
                suite.macos_architectures(program)

    def test_macos_architecture_mismatch_and_lipo_failure_are_fatal(self):
        program = Path('/private/build/getdaq')
        with mock.patch.object(suite.platform, 'machine', return_value='arm64'), \
                mock.patch.object(suite.subprocess, 'run', return_value=SimpleNamespace(
                    stdout='x86_64\n', returncode=0)):
            with self.assertRaisesRegex(RuntimeError, 'architecture mismatch'):
                suite.macos_architectures(program)
        with mock.patch.object(suite.subprocess, 'run', return_value=SimpleNamespace(
                stdout='lipo: cannot open input', returncode=1)):
            with self.assertRaisesRegex(RuntimeError, 'with lipo'):
                suite.macos_architectures(program)

    def test_macos_native_data_program_rejects_qt_and_third_party_runtime(self):
        with tempfile.TemporaryDirectory(prefix='getdaq-macos-runtime-deps-') as temporary:
            program = Path(temporary) / 'getdaq-receiver'
            program.write_bytes(b'Mach-O inspection fixture; not an executable')
            for library in ('@rpath/QtCore.framework/Versions/5/QtCore',
                            '@rpath/libyaml-cpp.0.dylib',
                            '@rpath/libboost_system.dylib',
                            '@rpath/libcurl.4.dylib',
                            '@rpath/libIce.3.5.1.dylib'):
                output = '%s:\n\t%s (compatibility version 1.0.0, current version 1.0.0)\n' % (program, library)
                with self.subTest(library=library), \
                        mock.patch.object(suite.platform, 'system', return_value='Darwin'), \
                        mock.patch.object(suite.subprocess, 'run', return_value=SimpleNamespace(stdout=output, returncode=0)):
                    with self.assertRaisesRegex(RuntimeError, 'Unexpected dependency'):
                        suite.runtime_dependencies(program, 'getdaq-receiver')

    def test_analysis_converters_reject_qt_but_root_viewer_allows_it(self):
        with tempfile.TemporaryDirectory(prefix='getdaq-analysis-runtime-deps-') as temporary:
            program = Path(temporary) / 'analysis-program'
            program.write_bytes(b'Linux dependency inspection fixture; not an executable')
            qt_output = 'libQt5Core.so.5 => /usr/lib/libQt5Core.so.5 (0x0)\n'
            cfitsio_dependency_output = ('libcfitsio.so.8 => /usr/lib/libcfitsio.so.8 (0x0)\n'
                                         'libcurl-gnutls.so.4 => /usr/lib/libcurl-gnutls.so.4 (0x0)\n')
            with mock.patch.object(suite.platform, 'system', return_value='Linux'):
                for name in ('getdaq-fits', 'getdaq-root'):
                    with self.subTest(program=name), \
                            mock.patch.object(suite.subprocess, 'run', return_value=SimpleNamespace(
                                stdout=qt_output, returncode=0)), \
                            self.assertRaisesRegex(RuntimeError, 'Unexpected Qt/Ice dependency'):
                        suite.runtime_dependencies(program, name)
                    with self.subTest(program=name, transitive='CFITSIO'), \
                            mock.patch.object(suite.subprocess, 'run', return_value=SimpleNamespace(
                                stdout=cfitsio_dependency_output, returncode=0)):
                        converter = suite.runtime_dependencies(program, name)
                        self.assertTrue(converter['checked'])
                with mock.patch.object(suite.subprocess, 'run', return_value=SimpleNamespace(
                        stdout=qt_output, returncode=0)):
                    viewer = suite.runtime_dependencies(program, 'getdaq-root-viewer')
            self.assertTrue(viewer['checked'])
            self.assertIn('libQt5Core.so.5', viewer['ldd'])

    def test_macos_optional_ice_tools_still_reject_qt(self):
        with tempfile.TemporaryDirectory(prefix='getdaq-macos-runtime-deps-') as temporary:
            program = Path(temporary) / 'getdaq-ice-send'
            program.write_bytes(b'Mach-O inspection fixture; not an executable')
            output = '%s:\n\t@rpath/QtNetwork.framework/Versions/5/QtNetwork (compatibility version 5.0.0, current version 5.15.0)\n' % program
            with mock.patch.object(suite.platform, 'system', return_value='Darwin'), \
                    mock.patch.object(suite.subprocess, 'run', return_value=SimpleNamespace(stdout=output, returncode=0)):
                with self.assertRaisesRegex(RuntimeError, 'Qt must not enter'):
                    suite.runtime_dependencies(program, 'getdaq-ice-send')

    def test_macos_runtime_inspection_failure_is_not_recorded_as_unchecked(self):
        with tempfile.TemporaryDirectory(prefix='getdaq-macos-runtime-deps-') as temporary:
            program = Path(temporary) / 'getdaq-receiver'
            program.write_bytes(b'Mach-O inspection fixture; not an executable')
            with mock.patch.object(suite.platform, 'system', return_value='Darwin'), \
                    mock.patch.object(suite.subprocess, 'run', return_value=SimpleNamespace(stdout='otool: bad object', returncode=1)):
                with self.assertRaisesRegex(RuntimeError, 'with otool'):
                    suite.runtime_dependencies(program, 'getdaq-receiver')

    def test_data_profile_has_no_qt_or_ice_configuration(self):
        args = SimpleNamespace(profile='data', jobs=2, cmake='cmake', qmake='qmake', make='make')
        steps = suite.plan(args, Path('/tmp/host-suite-plan'))
        self.assertEqual({s['component'] for s in steps}, {'receiver', 'supervisor'})
        programs = {name for step in steps for name in step['programs']}
        self.assertEqual(programs, set(suite.RECEIVER + suite.DATA_TOOLS))
        self.assertIn('getdaq-merge', programs)
        self.assertIn('getdaq-matrix', programs)
        self.assertIn('-DGETDAQ_BUILD_SUPERVISOR_SERVICE=OFF', steps[1]['configure'])
        self.assertNotIn('qmake', json.dumps(steps))
        self.assertNotIn('ice-compat', json.dumps(steps))

    def test_analysis_profile_isolated_and_explicitly_enables_optional_tools(self):
        args = SimpleNamespace(profile='analysis', jobs=2, cmake='cmake', qmake='qmake',
                               make='make', root_config='root-config')
        with mock.patch.object(suite, 'tool_output', return_value='/opt/root'):
            steps = suite.plan(args, Path('/tmp/host-suite-analysis-plan'))
        self.assertEqual([step['component'] for step in steps], ['analysis'])
        step = steps[0]
        self.assertEqual(set(step['programs']), set(suite.ANALYSIS))
        self.assertIn('-DGETDAQ_BUILD_OPTIONAL_FITS_TOOLS=ON', step['configure'])
        self.assertIn('-DGETDAQ_BUILD_OPTIONAL_ROOT_TOOLS=ON', step['configure'])
        self.assertIn('-DGETDAQ_BUILD_OPTIONAL_ROOT_VIEWER=ON', step['configure'])
        self.assertIn('-DROOT_DIR=/opt/root/cmake', step['configure'])
        self.assertNotIn('apps/envoy', json.dumps(steps))
        self.assertNotIn('ice-compat', json.dumps(steps))

    def test_native_data_supervisor_configures_without_qt(self):
        if os.name == 'nt':
            self.skipTest('The native data supervisor core is POSIX-only')
        with tempfile.TemporaryDirectory(prefix='getdaq-data-noqt-config-') as temporary:
            result = subprocess.run([
                'cmake', '-S', str(ROOT / 'apps/datasupervisor'), '-B', temporary,
                '-DGETDAQ_BUILD_SUPERVISOR_SERVICE=OFF',
                '-DCMAKE_DISABLE_FIND_PACKAGE_Qt5=TRUE', '-DBUILD_TESTING=OFF'],
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            self.assertEqual(result.returncode, 0, result.stdout)

    def test_supervisor_service_requires_qt_during_configuration(self):
        if os.name == 'nt':
            self.skipTest('The POSIX supervisor service target is not configured on Windows')
        with tempfile.TemporaryDirectory(prefix='getdaq-supervisor-noqt-config-') as temporary:
            result = subprocess.run([
                'cmake', '-S', str(ROOT / 'apps/datasupervisor'), '-B', temporary,
                '-DGETDAQ_BUILD_SUPERVISOR_SERVICE=ON',
                '-DCMAKE_DISABLE_FIND_PACKAGE_Qt5=TRUE'],
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertIn('REQUIRED', result.stdout)
            self.assertIn('CMAKE_DISABLE_FIND_PACKAGE_Qt5', result.stdout)

    def test_ubuntu24_full_inventory_tracks_current_suite_plan(self):
        expected = platform_validation.expected_full_programs(suite, 2)
        args = SimpleNamespace(profile='full', jobs=2, cmake='cmake', qmake='qmake', make='make')
        steps = suite.plan(args, Path('/tmp/host-suite-full-plan'))
        planned = {name for step in steps for name in step['programs']}
        self.assertEqual(expected, planned)
        self.assertIn('getdaq-merge', expected)
        self.assertIn('getdaq-matrix', expected)

    def test_ubuntu24_handoff_runtime_uses_build_receipt_paths(self):
        work = Path('/tmp/host-suite-full-plan')
        records = {
            'getdaq-supervisor': {'path': '/build/current/native/getdaq-supervisor'},
            'getdaq-receiver': {'path': '/build/current/receiver/getdaq-receiver'},
            'getdaq-supervisor-query': {'path': '/build/current/bin/getdaq-supervisor-query'},
        }
        entry = next(item for item in platform_validation.runtime_plan(work, records)
                     if item['label'] == 'failed-handoff-window')
        arguments = entry['arguments']
        self.assertEqual(arguments[arguments.index('--service') + 1], records['getdaq-supervisor']['path'])
        self.assertEqual(arguments[arguments.index('--receiver') + 1], records['getdaq-receiver']['path'])
        self.assertEqual(arguments[arguments.index('--client') + 1], records['getdaq-supervisor-query']['path'])

    def test_package_integrity_and_no_overwrite(self):
        with tempfile.TemporaryDirectory(prefix='getdaq-delivery-check-') as temporary:
            root = Path(temporary)
            program = root / 'getdaq-test-program'
            program.write_bytes(b'fixture for copy verification only\n')
            receipt = {'schema': 'getdaq.host-suite:1', 'status': 'complete',
                       'profile': 'test', 'programs': {
                           program.name: {'path': str(program), 'sha256': suite.sha(program)}}}
            destination = root / 'package'
            suite.stage(destination, receipt)
            manifest = suite.verify_stage(destination)
            self.assertIn('share/config/simulation.yml', manifest['files'])
            self.assertEqual(manifest['programs'][program.name]['path'],
                             'bin/' + program.name)
            self.assertFalse(Path(manifest['programs'][program.name]['path']).is_absolute())
            with self.assertRaises(FileExistsError):
                suite.stage(destination, receipt)
            (destination / 'unlisted-evidence.txt').write_text('not in the manifest')
            with self.assertRaisesRegex(RuntimeError, 'inventory mismatch'):
                suite.verify_stage(destination)
            (destination / 'unlisted-evidence.txt').unlink()
            (destination / 'bin' / program.name).write_bytes(b'changed')
            with self.assertRaisesRegex(RuntimeError, 'changed'):
                suite.verify_stage(destination)

    def test_complete_inventory_rejects_unmanifested_symlink(self):
        with tempfile.TemporaryDirectory(prefix='getdaq-delivery-symlink-') as temporary:
            root = Path(temporary)
            (root / 'manifest.json').write_text(json.dumps({
                'schema': 'getdaq.host-suite:1', 'status': 'complete',
                'complete_file_inventory': True, 'files': {}}))
            target = root / 'target.txt'
            target.write_text('owned file')
            link = root / 'link.txt'
            try:
                link.symlink_to(target)
            except (OSError, NotImplementedError):
                self.skipTest('Symlinks unavailable')
            with self.assertRaisesRegex(RuntimeError, 'symbolic links'):
                suite.verify_stage(root)

    def test_incomplete_package_is_rejected(self):
        with tempfile.TemporaryDirectory(prefix='getdaq-delivery-incomplete-') as temporary:
            root = Path(temporary)
            suite.write_json(root / 'manifest.json', {
                'schema': 'getdaq.host-suite:1', 'status': 'complete', 'files': {}})
            (root / 'BUILD_INCOMPLETE').write_text('failed')
            with self.assertRaisesRegex(RuntimeError, 'incomplete'):
                suite.verify_stage(root)


if __name__ == '__main__':
    unittest.main()
