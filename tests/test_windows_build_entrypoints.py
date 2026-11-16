#!/usr/bin/env python3
"""Exercise path and command selection for WSL and native Windows entrypoints."""
import importlib.util
from pathlib import Path
import hashlib
import re
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    'getdaq_windows_build', ROOT / 'tools/build-windows-from-wsl.py')
BUILD = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BUILD)
TEST_BUILD_SPEC = importlib.util.spec_from_file_location(
    'getdaq_windows_qt_test_build', ROOT / 'tests/build_windows_qt_validation.py')
TEST_BUILD = importlib.util.module_from_spec(TEST_BUILD_SPEC)
TEST_BUILD_SPEC.loader.exec_module(TEST_BUILD)
TEST_RUN_SPEC = importlib.util.spec_from_file_location(
    'getdaq_windows_qt_test_run', ROOT / 'tests/run_windows_qt_validation.py')
TEST_RUN = importlib.util.module_from_spec(TEST_RUN_SPEC)
TEST_RUN_SPEC.loader.exec_module(TEST_RUN)
DEPLOY_SPEC = importlib.util.spec_from_file_location(
    'getdaq_windows_deploy', ROOT / 'tools/deploy-windows-from-wsl.py')
DEPLOY = importlib.util.module_from_spec(DEPLOY_SPEC)
DEPLOY_SPEC.loader.exec_module(DEPLOY)
REFRESH_SPEC = importlib.util.spec_from_file_location(
    'getdaq_ubuntu24_program_refresh', ROOT / 'tools/refresh-ubuntu24-program.py')
REFRESH = importlib.util.module_from_spec(REFRESH_SPEC)
REFRESH_SPEC.loader.exec_module(REFRESH)
PLATFORM_SPEC = importlib.util.spec_from_file_location(
    'getdaq_ubuntu24_validation', ROOT / 'tests/ubuntu24_platform_validation.py')
PLATFORM = importlib.util.module_from_spec(PLATFORM_SPEC)
PLATFORM_SPEC.loader.exec_module(PLATFORM)


class WindowsBuildEntrypointsTest(unittest.TestCase):
    def test_root_qmake_control_project_includes_mcp_in_shared_output(self):
        project = (ROOT / 'GetDAQ.pro').read_text(encoding='utf-8')
        mcp_project = (ROOT / 'apps/ecc/mcp/mcp.pro').read_text(encoding='utf-8')
        project_paths = re.findall(r'(?m)^[A-Za-z0-9_]+\.file\s*=\s*(\S+)\s*$', project)
        targets = set()
        for relative in project_paths:
            subproject = (ROOT / relative).read_text(encoding='utf-8')
            target = re.search(r'(?m)^TARGET\s*=\s*([A-Za-z0-9_-]+)\s*$', subproject)
            self.assertIsNotNone(target, relative)
            targets.add(target.group(1))
        self.assertEqual(targets, set(BUILD.suite.CONTROL))
        self.assertIn('common/build-output.pri', mcp_project)

    def test_ubuntu24_catalog_refresh_invalidates_old_catalog_receipts(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            stale = root / 'validation/old-catalog.json'
            replacement = root / 'validation/threshold-catalog-analysis.json'
            stale.parent.mkdir(parents=True)
            stale.write_text('stale\n', encoding='utf-8')
            replacement.write_text('replacement\n', encoding='utf-8')
            manifest = {'validation': {
                'catalog_plain_tcp': {'evidence_in_package': 'validation/old-catalog.json'},
                'catalog_threshold_analysis': {
                    'evidence_in_package': 'validation/threshold-catalog-analysis.json'},
                'hitpattern_scan': {'evidence_in_package': 'validation/hitpattern.json'},
            }}
            REFRESH.invalidate_catalog_validation(
                manifest, root, 'validation/threshold-catalog-analysis.json')
            self.assertFalse(stale.exists())
            self.assertTrue(replacement.is_file())
            self.assertEqual(set(manifest['validation']), {'hitpattern_scan'})

    def test_native_windows_paths_do_not_call_wslpath(self):
        with mock.patch.object(BUILD.os, 'name', 'nt'), \
                mock.patch.object(BUILD.subprocess, 'check_output') as check_output:
            self.assertEqual(BUILD.windows(Path(r'D:\Qt Kit\bin\qmake.exe')),
                             r'D:\Qt Kit\bin\qmake.exe')
            check_output.assert_not_called()

    def test_native_windows_batch_and_powershell_commands(self):
        script = Path(r'D:\GetDAQ\build task.cmd')
        with mock.patch.object(BUILD.os, 'name', 'nt'):
            self.assertEqual(BUILD.windows_command('cmd.exe', script),
                             ['cmd.exe', '/d', '/c', str(script)])
            self.assertEqual(BUILD.powershell_command(
                script, '-NoProfile', '-NonInteractive'),
                ['powershell.exe', '-NoProfile', '-NonInteractive', '-File', str(script)])

    def test_wsl_commands_keep_interop_executables(self):
        script = Path('/mnt/c/GetDAQ/build task.cmd')
        with mock.patch.object(BUILD.os, 'name', 'posix'), \
                mock.patch.object(BUILD, 'windows', return_value=r'C:\GetDAQ\build task.cmd'):
            self.assertEqual(BUILD.windows_command('cmd.exe', script),
                             ['/mnt/c/Windows/System32/cmd.exe', '/d', '/c',
                              r'C:\GetDAQ\build task.cmd'])
            self.assertEqual(BUILD.powershell_command(script, '-NoProfile'),
                             ['/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe',
                              '-NoProfile', '-File', r'C:\GetDAQ\build task.cmd'])

    def test_native_qt_test_tools_keep_native_paths(self):
        kit = Path(r'D:\Software\QT\5.14.2\mingw73_64')
        with mock.patch.object(BUILD.os, 'name', 'nt'), \
                mock.patch.object(BUILD.subprocess, 'check_output') as check_output:
            self.assertEqual(TEST_BUILD.win.windows(kit), str(kit))
            self.assertEqual(TEST_RUN.windows(kit), str(kit))
            self.assertEqual(DEPLOY.winbuild.windows(kit), str(kit))
            check_output.assert_not_called()

    def test_windows_ci_builds_tests_then_packages_and_verifies_runtime(self):
        workflow = (ROOT / '.github/workflows/native-data-path.yml').read_text(encoding='utf-8')
        windows_job = workflow[workflow.index('  windows-qt5142-control-build:'):]
        required_steps = [
            'Build the eight Windows control programs from current source',
            'Build current-source Windows Widgets and ECC client QtTests',
            'Run complete Windows Widgets and ECC client QtTests',
            'Verify the complete eight-program build receipt',
            'Deploy and smoke-test the self-contained Windows runtime',
            'python tools/deploy-windows-from-wsl.py',
            'Verify native Windows configuration and all offline scan planners',
            'python tests/system_experiment_integration.py',
            'runtime/getdaq-system-config.exe',
            '--scan-checker "build-platform/windows-qt5142-control/runtime/getdaq-config-check.exe"',
            'Exercise the packaged native supervisor query client',
            'python tests/windows_supervisor_query_integration.py',
            'python tools/refresh-package-docs.py',
            '--verify "build-platform/windows-qt5142-control/runtime"',
            '\n            build-platform/windows-qt5142-control/runtime/\n',
        ]
        positions = [windows_job.index(step) for step in required_steps]
        self.assertEqual(positions, sorted(positions))
        self.assertIn("Get-Content 'build-platform/windows-qt5142-control/windows-build-plan.json' -Raw", windows_job)
        self.assertIn('$expected = @($plan.programs)', windows_job)
        self.assertIn('Compare-Object $wanted $actual', windows_job)
        self.assertEqual(len(BUILD.suite.CONTROL), 10)
        self.assertIn("getdaq-cconfig-editor", BUILD.suite.CONTROL)
        self.assertIn('getdaq-control-mcp', BUILD.suite.CONTROL)
        self.assertIn('getdaq-waveform-viewer', BUILD.suite.CONTROL)

    def test_macos_full_suite_ci_builds_and_runs_both_qttest_suites(self):
        workflow = (ROOT / '.github/workflows/native-data-path.yml').read_text(encoding='utf-8')
        macos_job = workflow[workflow.index('  macos-host-suite:'):workflow.index('  native-data-path:')]
        required_steps = [
            'Build every POSIX host program',
            'Verify the complete suite receipt',
            "program['runtime']['mach_o']",
            "mach_o['host_architecture'] == host_architecture",
            "host_architecture in mach_o['architectures']",
            'Run native macOS integration for all offline scan planners',
            'tests/system_experiment_integration.py',
            '--scan-checker build-platform/macos-full-suite/full/bin/getdaq-config-check',
            'config-scan-tests/result.json',
            'Build the macOS Core, Widgets, and ECC client QtTest suites',
            'tests/test_macos_qttest_cleanup.py',
            'tests/build_qt_control_validation.py tests/xcfg_shared.pro',
            'tests/build_qt_control_validation.py tests/tests.pro',
            'tests/build_qt_control_validation.py tests/ecc_system_client.pro',
            'Run offscreen macOS Core, Widgets, and ECC client QtTests',
            'xcfg-shared-tests',
            'getdaq-tests',
            'ecc-system-client-tests',
            'Clean macOS QtTest private temporary data',
            'Run macOS receiver and Sentry lifecycle integration',
            'Upload macOS host suite evidence',
        ]
        positions = [macos_job.index(step) for step in required_steps]
        self.assertEqual(positions, sorted(positions))
        self.assertIn('getdaq-config-check', BUILD.suite.CONTROL)
        self.assertIn('getdaq-system-config', BUILD.suite.NATIVE)
        self.assertIn('full/bin/getdaq-config-check', macos_job)
        self.assertIn('full/native/getdaq-system-config', macos_job)
        artifacts = macos_job[macos_job.index('Upload macOS host suite evidence'):]
        for evidence in ('qt-test-cleanup.json', 'macos-toolchain.txt', 'full/*.log',
                         'qt-tests/core/results.txt', 'qt-tests/core/build.log',
                         'qt-tests/widgets/build.log', 'qt-tests/ecc-client/build.log'):
            self.assertIn(evidence, artifacts)
        cleanup_helper = (ROOT / 'tests/cleanup_macos_qttest.py').read_text(encoding='utf-8')
        self.assertIn('RAW_LIMIT_BYTES = 20 * 1024 ** 3', cleanup_helper)
        self.assertIn('QT_QPA_PLATFORM: offscreen', macos_job)
        for variable in ('HOME:', 'XDG_CACHE_HOME:', 'XDG_CONFIG_HOME:',
                         'XDG_DATA_HOME:', 'XDG_RUNTIME_DIR:', 'TMPDIR:'):
            self.assertIn(variable+' ${{ github.workspace }}/build-platform/macos-full-suite/qt-test-temp', macos_job)
        self.assertIn('chmod 700 "$XDG_RUNTIME_DIR"', macos_job)

    def test_windows_build_contains_native_supervisor_query_without_qt_network_dependency(self):
        project = (ROOT / 'GetDAQ.pro').read_text(encoding='utf-8')
        query_project = (ROOT / 'apps/datasupervisor/getdaq-supervisor-query.pro').read_text(encoding='utf-8')
        cmake = (ROOT / 'apps/datasupervisor/CMakeLists.txt').read_text(encoding='utf-8')
        self.assertIn('supervisorquery.file = apps/datasupervisor/getdaq-supervisor-query.pro', project)
        self.assertIn('QT =', query_project)
        self.assertIn('controlhttp_win.cpp credential_win.cpp queryfile_win.cpp', query_project)
        self.assertIn('queryrunner_validation.cpp', query_project)
        self.assertIn('queryrunner_plantransition.cpp', query_project)
        self.assertIn('queryrunner_catalogreplacement.cpp', query_project)
        self.assertIn('if(WIN32)', cmake)
        self.assertEqual(cmake.count('queryrunner_validation.cpp'), 2)
        self.assertEqual(cmake.count('queryrunner_plantransition.cpp'), 2)
        self.assertEqual(cmake.count('queryrunner_catalogreplacement.cpp'), 2)
        self.assertIn('getdaq-supervisor-query', BUILD.suite.CONTROL)
        self.assertNotIn('getdaq-supervisor-query', BUILD.suite.NATIVE)
        self.assertIn('for name in suite.QT_CONTROL:', (ROOT / 'tools/deploy-windows-from-wsl.py').read_text())

    def test_native_data_ci_runs_the_packaged_example_regression(self):
        workflow = (ROOT / '.github/workflows/native-data-path.yml').read_text(encoding='utf-8')
        required_steps = [
            'Verify current and archive Sentry examples',
            'Verify packaged configuration example refresh',
            'run: python3 tests/test_refresh_package_examples.py',
            'Verify offline data profile inputs',
        ]
        positions = [workflow.index(step) for step in required_steps]
        self.assertEqual(positions, sorted(positions))

    def test_ubuntu24_getdaq_package_requires_a_clean_startup_log(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with self.assertRaisesRegex(RuntimeError, 'did not create its application log'):
                REFRESH.validate_envoy_application_log(root)
            log = root / 'GetDAQ/logs/startup.log'
            log.parent.mkdir(parents=True)
            log.write_text('GetDAQ ready. Simulation is enabled by default.\n')
            self.assertEqual(REFRESH.validate_envoy_application_log(root), log.stat().st_size)
            log.write_text('Cannot open application log: No such file or directory\n')
            with self.assertRaisesRegex(RuntimeError, 'missing the ready record or reports an error'):
                REFRESH.validate_envoy_application_log(root)

    def test_ubuntu24_getdaq_refresh_requires_legacy_soap_action_regression(self):
        source = (ROOT / 'tools/refresh-ubuntu24-program.py').read_text(encoding='utf-8')
        runner = (ROOT / 'tests/run_envoy_ui_package_test.py').read_text(encoding='utf-8')
        self.assertIn("required = ('native_gui_address_and_topology_hints', 'native_single_cobo_lifecycle_mapping', 'legacy_soap_action_header')", source)
        self.assertIn("'native_single_cobo_lifecycle_mapping': True", runner)
        self.assertIn("'legacy_soap_action_header': True", runner)
        self.assertIn("legacySoapHttpRequestSendsQuotedEmptySoapAction", runner)

    def test_ubuntu24_inspect_refresh_requires_hitpattern_cli_evidence(self):
        source = (ROOT / 'tools/refresh-ubuntu24-program.py').read_text(encoding='utf-8')
        runner = (ROOT / 'tests/run_hitpattern_scan_package_test.py').read_text(encoding='utf-8')
        self.assertIn("elif program == 'getdaq-inspect':", source)
        self.assertIn("getdaq.hitpattern-scan-regression:1", source)
        self.assertIn("validation/hitpattern-scan-regression.json", source)
        self.assertIn("'multi_file_aggregation'", source)
        self.assertIn("'mapping_marked_unverified'", source)
        self.assertIn("getdaq.hitpattern-scan-regression:1", runner)
        self.assertIn("'synthetic_raw_bytes_deleted': generated", runner)

    def test_ubuntu24_catalog_refresh_accepts_threshold_analysis_evidence(self):
        source = (ROOT / 'tools/refresh-ubuntu24-program.py').read_text(encoding='utf-8')
        runner = (ROOT / 'tests/run_threshold_catalog_cli_test.py').read_text(encoding='utf-8')
        self.assertIn("getdaq.threshold-catalog-analysis-regression:1", source)
        self.assertIn("validation/threshold-catalog-analysis.json", source)
        self.assertIn("catalog_threshold_analysis", source)
        self.assertIn("'catalog_report_not_overwritten'", source)
        self.assertIn("getdaq.threshold-catalog-analysis-regression:1", runner)
        self.assertIn("FIXTURE_BYTES = 320", runner)
        self.assertIn("--threshold-cli-smoke-with-loader", (ROOT / 'tests/receiver_catalog_test.cpp').read_text(encoding='utf-8'))

    def test_ubuntu24_platform_gate_matches_current_qttest_totals(self):
        self.assertEqual(PLATFORM.EXPECTED_QTTEST_TOTALS,
                         {'core': 151, 'widgets': 119})
        PLATFORM.validate_qttest_result(
            'widgets', 'Totals: 119 passed, 0 failed, 0 skipped, 0 blacklisted')
        with self.assertRaisesRegex(RuntimeError, 'expected 119 passing cases'):
            PLATFORM.validate_qttest_result(
                'widgets', 'Totals: 118 passed, 0 failed, 0 skipped, 0 blacklisted')

    def test_qttest_platform_runners_isolate_test_mode_storage(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            environment = PLATFORM.validation_environment(root)
            self.assertEqual(environment['HOME'], str(root / 'home'))
            self.assertEqual(environment['XDG_CACHE_HOME'], str(root / 'cache'))
            self.assertTrue((root / 'home').is_dir())
            test_data = root / 'home/.qttest'
            test_data.mkdir()
            (test_data / 'preferences.ini').write_text('test only')
            PLATFORM.cleanup_test_mode_data(root / 'home')
            self.assertFalse(test_data.exists())

            script = root / 'run-tests.ps1'
            with mock.patch.object(TEST_RUN, 'windows', side_effect=lambda path: str(path).replace('/', '\\')):
                TEST_RUN.create_runner(script, root / 'windows-owned', r'C:\GetDAQ\tests', [
                    ('widgets', r'C:\GetDAQ\getdaq-tests.exe', r'C:\GetDAQ',
                     root / 'widgets-out.txt', root / 'widgets-err.txt')])
            script_text = script.read_text(encoding='utf-8-sig')
            self.assertIn("$env:APPDATA = Join-Path $root 'appdata-roaming'", script_text)
            self.assertIn("$env:LOCALAPPDATA = Join-Path $root 'appdata-local'", script_text)
            self.assertIn('New-Item -ItemType Directory -Force -Path $env:APPDATA, $env:LOCALAPPDATA', script_text)

    def test_native_qt_test_runner_selects_native_windows_root(self):
        with mock.patch.object(TEST_RUN.os, 'name', 'nt'), \
                mock.patch.object(TEST_RUN.subprocess, 'check_output') as check_output:
            self.assertEqual(TEST_RUN.windows(Path(r'D:\GetDAQ\results.txt')),
                             r'D:\GetDAQ\results.txt')
            check_output.assert_not_called()

    def test_native_qttest_build_directory_matches_receipted_output(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            makefile = root / 'widgets/Makefile.Release'
            makefile.parent.mkdir()
            makefile.write_text(r'DEFINES = -DQT_TESTCASE_BUILDDIR="\"D:/Build/widgets\""' + '\n')
            actual = TEST_RUN.verify_native_test_build_dir(
                makefile, Path('D:/Build/widgets'))
            self.assertEqual(actual, TEST_RUN.PureWindowsPath('D:/Build/widgets'))
            with self.assertRaisesRegex(RuntimeError, 'does not match its receipt location'):
                TEST_RUN.verify_native_test_build_dir(makefile, Path('D:/Other/widgets'))

    def test_native_qt_test_runner_checks_every_receipted_runtime_file(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            executable = root / 'widgets/release/getdaq-tests.exe'
            runtime = root / 'widgets/release/Qt5Core.dll'
            executable.parent.mkdir(parents=True)
            executable.write_bytes(b'fake PE')
            runtime.write_bytes(b'Qt runtime')
            digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
            receipt = {'path': str(executable), 'sha256': digest(executable),
                       'runtime_sha256': {'widgets/release/getdaq-tests.exe': digest(executable),
                                          'widgets/release/Qt5Core.dll': digest(runtime)}}
            self.assertEqual(TEST_RUN.verify_native_program_runtime(root, 'widgets', receipt),
                             executable.resolve())
            runtime.write_bytes(b'changed')
            with self.assertRaisesRegex(RuntimeError, 'runtime file changed'):
                TEST_RUN.verify_native_program_runtime(root, 'widgets', receipt)
            receipt['runtime_sha256'] = {'../outside.dll': digest(runtime)}
            with self.assertRaisesRegex(RuntimeError, 'escaped its build directory'):
                TEST_RUN.verify_native_program_runtime(root, 'widgets', receipt)

    def test_partial_fixture_copy_is_registered_and_cleaned_without_following_symlinks(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / 'input'
            source.mkdir()
            (source / 'fixture.json').write_text('{}')
            owned = root / 'owned'
            destination = owned / 'nested'
            registered = []

            def fail_after_partial_copy(_source, target):
                target.mkdir(parents=True)
                (target / 'partial').write_text('partial')
                raise OSError('simulated copy interruption')

            with mock.patch.object(TEST_RUN.shutil, 'copytree', side_effect=fail_after_partial_copy):
                with self.assertRaisesRegex(OSError, 'simulated copy interruption'):
                    TEST_RUN.copy_owned_fixture(source, destination, owned, registered)
            self.assertEqual(registered, [owned])

            external = root / 'external'
            external.mkdir()
            (external / 'keep.txt').write_text('keep')
            link = root / 'link'
            try:
                link.symlink_to(external, target_is_directory=True)
                symlink_path = link
            except (OSError, NotImplementedError):
                # Standard Windows accounts may not have symlink privilege.
                symlink_path = mock.Mock()
                symlink_path.is_dir.return_value = True
                symlink_path.is_symlink.return_value = True
            with mock.patch.object(TEST_RUN.shutil, 'rmtree', wraps=TEST_RUN.shutil.rmtree) as rmtree:
                TEST_RUN.cleanup_owned_fixtures([owned, symlink_path])
                self.assertEqual(rmtree.call_count, 1)
                self.assertEqual(rmtree.call_args.args[0], owned)
            self.assertFalse(owned.exists())
            self.assertTrue((external / 'keep.txt').is_file())
            if symlink_path is link:
                self.assertTrue(link.is_symlink())


if __name__ == '__main__':
    unittest.main()
