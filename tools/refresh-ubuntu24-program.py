#!/usr/bin/env python3
"""Safely refresh one program in an existing Ubuntu 24 host-suite candidate.

The base candidate is preserved until the staged package passes its inventory,
target-loader, --help, and caller-supplied executable regression checks. The
currently absent getdaq-merge or getdaq-matrix may be added to an older full candidate.
"""
import argparse
import datetime
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
ROOTFS = ROOT / 'build-ubuntu24-validation/rootfs'


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def load_build_suite():
    spec = importlib.util.spec_from_file_location('getdaq_build_suite', ROOT / 'tools/build-suite.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def json_write(path, value):
    path = Path(path)
    temporary = path.with_suffix(path.suffix + '.tmp')
    with temporary.open('w', encoding='utf-8', newline='\n') as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2, sort_keys=True)
        stream.write('\n')
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temporary, path)


def invalidate_catalog_validation(manifest, staging, replacement_evidence):
    """Remove receipts tied to an older getdaq-catalog binary before replacing it."""
    validation = manifest.setdefault('validation', {})
    for key in ('catalog_ssh_fake_endpoint_test', 'catalog_authenticated_tcp',
                'catalog_plain_tcp', 'catalog_threshold_analysis'):
        record = validation.pop(key, None)
        if not record:
            continue
        relative = Path(record.get('evidence_in_package', ''))
        if relative.is_absolute() or '..' in relative.parts or '.' in relative.parts or not relative.parts:
            raise RuntimeError('catalog validation has an unsafe evidence path: ' + key)
        evidence = Path(staging)
        for part in relative.parts:
            evidence = evidence / part
            if evidence.is_symlink():
                raise RuntimeError('catalog validation evidence traverses a symlink: ' + key)
        if relative.as_posix() != replacement_evidence and evidence.exists():
            if not evidence.is_file():
                raise RuntimeError('catalog validation evidence is not a regular file: ' + key)
            evidence.unlink()


def validate_inventory(candidate):
    if candidate.is_symlink() or not candidate.is_dir():
        raise RuntimeError('Candidate must be an existing, non-symlink directory')
    resolved = candidate.resolve()
    try:
        resolved.relative_to((ROOT / 'build-packages').resolve())
    except ValueError:
        raise RuntimeError('Candidate must be inside build-packages')
    if resolved != candidate.absolute():
        raise RuntimeError('Candidate path must not traverse a symbolic link')
    module = load_build_suite()
    module.verify_stage(candidate)
    manifest = json.loads((candidate / 'manifest.json').read_text(encoding='utf-8'))
    for path in candidate.rglob('*'):
        if path.is_symlink():
            raise RuntimeError('Candidate contains a symbolic link: ' + str(path.relative_to(candidate)))
    return manifest


def target_loader():
    root = ROOTFS.resolve()
    loader = root / 'usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2'
    if not loader.is_file():
        raise RuntimeError('Ubuntu 24 validation loader is missing')
    libraries = [root / part for part in ('lib/x86_64-linux-gnu', 'lib',
                 'usr/lib/x86_64-linux-gnu', 'usr/lib', 'usr/local/lib') if (root / part).is_dir()]
    return root, loader, ':'.join(str(path) for path in libraries)


def run_target(loader, libraries, *args):
    return subprocess.run([str(loader), '--library-path', libraries, *map(str, args)],
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          text=True, timeout=60, check=False)


def validate_envoy_application_log(data_directory):
    logs = list(Path(data_directory).rglob('*.log'))
    if not logs:
        raise RuntimeError('Ubuntu 24 GetDAQ startup did not create its application log')
    log_text = '\n'.join(path.read_text(encoding='utf-8', errors='replace') for path in logs)
    if 'GetDAQ ready' not in log_text or 'Cannot open application log' in log_text:
        raise RuntimeError('Ubuntu 24 GetDAQ application log is missing the ready record or reports an error')
    return sum(path.stat().st_size for path in logs)


def replace(candidate, program, binary, test_result):
    candidate = Path(candidate).absolute()
    binary = Path(binary).resolve(strict=True)
    test_result = Path(test_result).resolve(strict=True)
    manifest = validate_inventory(candidate)
    existing_programs = manifest.get('programs', {})
    is_new_program = program not in existing_programs
    if manifest.get('profile') != 'full' or (is_new_program and program not in (
            'getdaq-merge', 'getdaq-matrix', 'getdaq-waveform-viewer')):
        raise RuntimeError('Candidate must be a full suite; only approved offline tools may be added')
    if binary.name != program or not os.access(binary, os.X_OK):
        raise RuntimeError('Build input must be an executable named ' + program)
    test = json.loads(test_result.read_text(encoding='utf-8'))
    tested = test.get('program_sha256', {})
    scenario = test.get('scenario', {})
    if test.get('passed') is not True or test.get('raw_remaining') != 0:
        raise RuntimeError('Caller-supplied regression result is not passed or has raw files remaining')
    if program == 'getdaq-config':
        expected_runner = ROOT / 'tests/system_experiment_integration.py'
        required = ('complete_system_artifact_compiled', 'selected_cobo_only_changed',
                    'sixteen_scan_points_generated', 'manifest_and_artifact_sha256_verified',
                    'configuration_digests_verified', 'native_system_artifact_checker_passed',
                    'existing_plan_overwrite_rejected', 'pulser_amplitude_points_generated',
                    'pulser_manifest_and_artifact_sha256_verified', 'pulser_complete_settings_and_digests_verified',
                    'existing_pulser_plan_overwrite_rejected', 'aget_threshold_points_generated',
                    'threshold_manifest_and_artifact_sha256_verified',
                    'threshold_complete_settings_and_digests_verified',
                    'existing_threshold_plan_overwrite_rejected', 'original_configuration_unchanged',
                    'no_hardware_commands')
        if (test.get('schema') != 'getdaq.config-scan-regression:2' or
                test.get('runner_sha256') != sha(expected_runner) or
                not all(scenario.get(field) is True for field in required) or
                test.get('process_cases', 0) < 64 or
                test.get('target_runtime') != 'Ubuntu 24 loader'):
            raise RuntimeError('Regression result does not prove the Ubuntu 24 CoBo/AGET scan-planning scenarios')
        test_program_sha = tested.get('config')
        if test_program_sha != sha(binary):
            raise RuntimeError('Regression result was not produced by the supplied getdaq-config binary')
        native_program = candidate / 'bin' / 'getdaq-system-config'
        if not native_program.is_file() or tested.get('system_config') != sha(native_program):
            raise RuntimeError('Regression result was not produced with this candidate system-config checker')
        if (test.get('maximum_aggregate_test_bytes') != 0 or
                test.get('synthetic_raw_bytes_generated') != 0 or
                test.get('synthetic_raw_bytes_deleted') != 0 or
                test.get('real_hardware') is not False):
            raise RuntimeError('Configuration scan planning must not transfer data or access hardware')
        maximum = 0
        deleted = 0
        regression_relative = 'validation/config-peaking-scan-regression.json'
    elif program == 'getdaq-supervisor-query':
        expected_runner = ROOT / 'tests/run_failed_handoff_completion_tests.py'
        if test.get('runner_sha256') != sha(expected_runner):
            raise RuntimeError('Regression result was not produced by the current failed-handoff runner')
        if not all(scenario.get(field) is True for field in (
                'passed', 'original_failed_preserved', 'exact_live_completion',
                'successor_owner_observed', 'cold_restart_fenced')):
            raise RuntimeError('Regression result does not prove the required handoff scenario')
        test_program_sha = tested.get('client')
        if test_program_sha != sha(binary):
            raise RuntimeError('Regression result was not produced by the supplied source build binary')
        maximum = test.get('maximum_aggregate_test_bytes', 0)
        deleted = sum(item.get('bytes', 0) for item in test.get('deleted_raw', []))
        if (maximum > 64 * 1024 ** 2 or deleted > maximum or maximum > 20 * 1024 ** 3 or
                deleted != scenario.get('raw_bytes') or test.get('real_hardware') is not False):
            raise RuntimeError('Regression data exceeds this handoff refresh 64 MiB cap')
        regression_relative = 'validation/failed-handoff-completion-service.json'
    elif program == 'getdaq-catalog' and test.get('schema') == 'getdaq.threshold-catalog-analysis-regression:1':
        expected_runner = ROOT / 'tests/run_threshold_catalog_cli_test.py'
        required = ('bound_scan_point_to_sealed_catalog',
                    'packaged_cli_produced_expected_channel_counts',
                    'catalog_report_not_overwritten', 'fixture_root_removed')
        if (test.get('runner_sha256') != sha(expected_runner) or
                not all(scenario.get(field) is True for field in required) or
                test.get('owned_test_roots_empty') is not True or test.get('raw_remaining') != 0 or
                test.get('real_hardware') is not False):
            raise RuntimeError('Regression result does not prove the sealed threshold catalog CLI scenarios')
        test_program_sha = tested.get('catalog')
        if test_program_sha != sha(binary):
            raise RuntimeError('Regression result was not produced by the supplied catalog binary')
        maximum = test.get('maximum_aggregate_test_bytes', 0)
        generated = test.get('synthetic_raw_bytes_generated', -1)
        deleted = test.get('synthetic_raw_bytes_deleted', -1)
        if (maximum <= 0 or maximum > 1024 * 1024 or maximum > 20 * 1024 ** 3 or
                generated != 320 or deleted != generated or generated > maximum):
            raise RuntimeError('Threshold catalog regression has an invalid fixture budget or raw cleanup receipt')
        regression_relative = 'validation/threshold-catalog-analysis.json'
    elif program == 'getdaq-catalog':
        expected_runner = ROOT / 'tests/run_catalog_plain_tcp_tests.py'
        required = ('thread_local_sigpipe_isolation', 'oversized_metadata_fields_rejected_before_staging',
                    'plain_tcp_no_authentication', 'interrupted_partial_resumed',
                    'receiver_readback_verified', 'completed_resume_no_payload',
                    'native_catalog_regressions', 'separate_cli_listener_send_readback',
                    'cli_connection_refusal_bounded')
        if (test.get('schema') != 'getdaq.catalog-plain-tcp-regression:1' or
                test.get('runner_sha256') != sha(expected_runner) or
                not all(scenario.get(field) is True for field in required) or
                test.get('owned_test_roots_empty') is not True):
            raise RuntimeError('Regression result does not prove the plain TCP catalog scenarios')
        test_program_sha = tested.get('catalog')
        if test_program_sha != sha(binary):
            raise RuntimeError('Regression result was not produced by the supplied catalog binary')
        maximum = test.get('maximum_aggregate_test_bytes', 0)
        generated = test.get('synthetic_raw_bytes_generated', -1)
        deleted = test.get('synthetic_raw_bytes_deleted', -1)
        if (maximum > 64 * 1024 ** 2 or maximum > 20 * 1024 ** 3 or
                generated < 0 or deleted != generated or generated > maximum or
                test.get('real_hardware') is not False):
            raise RuntimeError('Catalog TCP regression exceeded its 64 MiB synthetic-data cap or left data behind')
        regression_relative = 'validation/catalog-plain-tcp.json'
    elif program == 'getdaq-merge':
        expected_runner = ROOT / 'tests/run_mfm_merge_package_test.py'
        required = ('event_sort', 'same_event_grouping', 'input_order_preserved',
                    'unsupported_type_skipped', 'embedded_bytes_preserved',
                    'mfm_outer_header', 'existing_output_not_overwritten',
                    'temporary_inputs_removed', 'cli_help', 'cli_merge_output')
        if (test.get('schema') != 'getdaq.mfm-merge-regression:1' or
                test.get('runner_sha256') != sha(expected_runner) or
                test.get('owned_test_roots_empty') is not True or
                not all(scenario.get(field) is True for field in required)):
            raise RuntimeError('Regression result does not prove the MFM merge CLI scenarios')
        test_program_sha = tested.get('merge')
        if test_program_sha != sha(binary):
            raise RuntimeError('Regression result was not produced by the supplied getdaq-merge binary')
        maximum = test.get('maximum_aggregate_test_bytes', 0)
        generated = test.get('synthetic_raw_bytes_generated', -1)
        deleted = test.get('synthetic_raw_bytes_deleted', -1)
        if (maximum > 64 * 1024 ** 2 or maximum > 20 * 1024 ** 3 or
                generated < 0 or deleted != generated or generated > maximum or
                test.get('raw_remaining') != 0 or test.get('real_hardware') is not False):
            raise RuntimeError('MFM merge regression exceeded its 64 MiB fixture cap or left raw files behind')
        regression_relative = 'validation/mfm-merge-regression.json'
    elif program == 'getdaq-matrix':
        expected_runner = ROOT / 'tests/run_gmat_package_test.py'
        required = ('library_layout_and_rejections', 'unsupported_revision_rejected',
                    'cli_help', 'cli_binary64_little_endian',
                    'cli_channel_and_bucket_order', 'cli_no_replace', 'cli_output_limit',
                    'synthetic_raw_cleanup')
        if (test.get('schema') != 'getdaq.gmat-conversion-regression:1' or
                test.get('runner_sha256') != sha(expected_runner) or
                test.get('owned_test_roots_empty') is not True or
                not all(scenario.get(field) is True for field in required)):
            raise RuntimeError('Regression result does not prove the .gmat conversion scenarios')
        test_program_sha = tested.get('matrix')
        if test_program_sha != sha(binary):
            raise RuntimeError('Regression result was not produced by the supplied getdaq-matrix binary')
        maximum = test.get('maximum_aggregate_test_bytes', 0)
        generated = test.get('synthetic_raw_bytes_generated', -1)
        deleted = test.get('synthetic_raw_bytes_deleted', -1)
        if (maximum > 64 * 1024 ** 2 or generated < 0 or deleted != generated or
                generated > maximum or test.get('raw_remaining') != 0 or
                test.get('real_hardware') is not False):
            raise RuntimeError('.gmat regression exceeded its 64 MiB fixture cap or left raw files behind')
        regression_relative = 'validation/gmat-conversion-regression.json'
    elif program == 'getdaq-inspect':
        expected_runner = ROOT / 'tests/run_hitpattern_scan_package_test.py'
        required = ('cli_help', 'single_file_counts', 'multi_file_aggregation',
                    'mixed_endian', 'mapping_marked_unverified',
                    'descending_code_rejected', 'malformed_file_rejected',
                    'temporary_raw_cleanup')
        if (test.get('schema') != 'getdaq.hitpattern-scan-regression:1' or
                test.get('runner_sha256') != sha(expected_runner) or
                test.get('owned_test_roots_empty') is not True or
                not all(scenario.get(field) is True for field in required)):
            raise RuntimeError('Regression result does not prove the hit-pattern scan CLI scenarios')
        test_program_sha = tested.get('inspect')
        if test_program_sha != sha(binary):
            raise RuntimeError('Regression result was not produced by the supplied getdaq-inspect binary')
        maximum = test.get('maximum_aggregate_test_bytes', 0)
        generated = test.get('synthetic_raw_bytes_generated', -1)
        deleted = test.get('synthetic_raw_bytes_deleted', -1)
        if (maximum > 1024 * 1024 or generated < 0 or deleted != generated or
                generated > maximum or test.get('raw_remaining') != 0 or
                test.get('real_hardware') is not False):
            raise RuntimeError('Hit-pattern regression exceeded its 1 MiB fixture cap or left raw files behind')
        regression_relative = 'validation/hitpattern-scan-regression.json'
    elif program == 'getdaq-waveform-viewer':
        expected_runner = ROOT / 'tests/run_waveform_viewer_package_test.py'
        if (test.get('schema') != 'getdaq.waveform-viewer-regression:1' or
                test.get('runner_sha256') != sha(expected_runner) or
                test.get('owned_test_roots_empty') is not True or
                not all(scenario.get(field) is True for field in
                        ('cli_help', 'offscreen_window_startup'))):
            raise RuntimeError('Regression result does not prove offline waveform viewer startup')
        test_program_sha = tested.get('waveform_viewer')
        if test_program_sha != sha(binary):
            raise RuntimeError('Regression result was not produced by the supplied waveform viewer')
        if (test.get('maximum_aggregate_test_bytes') != 0 or
                test.get('synthetic_raw_bytes_generated') != 0 or
                test.get('synthetic_raw_bytes_deleted') != 0 or
                test.get('raw_remaining') != 0 or
                test.get('real_hardware') is not False):
            raise RuntimeError('Offline viewer startup regression must not generate data or claim hardware access')
        maximum = 0
        deleted = 0
        regression_relative = 'validation/waveform-viewer-regression.json'
    elif program == 'getdaq-receiver':
        expected_runner = ROOT / 'tests/run_byte_storage_tests.py'
        required = ('passed', 'native_receiver_ctest', 'mfm_merge', 'receiver_core',
                    'exporter_lock', 'byte_storage_tcp_raw_preserved', 'eof_seal_verified',
                    'fdt_rejected', 'raw_cleanup')
        if (test.get('schema') != 'getdaq.receiver-byte-storage-regression:1' or
                test.get('runner_sha256') != sha(expected_runner) or
                not all(scenario.get(field) is True for field in required)):
            raise RuntimeError('Regression result does not prove the receiver ByteStorage scenarios')
        test_program_sha = test.get('receiver_sha256')
        if test_program_sha != sha(binary):
            raise RuntimeError('Regression result was not produced by the supplied receiver binary')
        maximum = test.get('maximum_aggregate_test_bytes', 0)
        generated = test.get('synthetic_raw_bytes_generated', -1)
        deleted = test.get('synthetic_raw_bytes_deleted', -1)
        if (maximum > 64 * 1024 ** 2 or generated < 0 or deleted != generated or
                generated > maximum or test.get('raw_remaining') != 0 or
                test.get('real_hardware') is not False):
            raise RuntimeError('Receiver regression exceeded its 64 MiB fixture cap or left raw files behind')
        regression_relative = 'validation/receiver-byte-storage.json'
    elif program == 'getdaq-sentry':
        expected_runner = ROOT / 'tests/run_sentry_config_tests.py'
        required = ('all_malformed_fields_rejected_before_listener',
                    'all_eight_cases_passed', 'owned_test_root_removed', 'raw_cleanup')
        if (test.get('schema') != 'getdaq.sentry-config-strict-regression:1' or
                test.get('runner_sha256') != sha(expected_runner) or
                test.get('owned_test_roots_empty') is not True or
                not all(scenario.get(field) is True for field in required)):
            raise RuntimeError('Regression result does not prove the Sentry strict configuration scenarios')
        test_program_sha = test.get('program_sha256', {}).get('sentry')
        if test_program_sha != sha(binary):
            raise RuntimeError('Regression result was not produced by the supplied Sentry binary')
        if (len(test.get('cases', [])) != 8 or
                not all(case.get('passed') is True and case.get('exit_code') == 2
                        for case in test['cases']) or
                test.get('synthetic_raw_bytes_generated') != 0 or
                test.get('synthetic_raw_bytes_deleted') != 0 or
                test.get('raw_remaining') != 0 or
                test.get('real_hardware') is not False):
            raise RuntimeError('Sentry regression has failed cases or unexpected data output')
        maximum = test.get('maximum_aggregate_test_bytes', 0)
        if maximum != 0 or maximum > 20 * 1024 ** 3:
            raise RuntimeError('Sentry configuration regression exceeded its zero-payload budget')
        deleted = 0
        regression_relative = 'validation/sentry-config-strict.json'
    elif program == 'GetDAQ':
        expected_runner = ROOT / 'tests/run_envoy_ui_package_test.py'
        required = ('native_gui_address_and_topology_hints', 'native_single_cobo_lifecycle_mapping', 'legacy_soap_action_header')
        if (test.get('schema') != 'getdaq.envoy-ui-package-regression:1' or
                test.get('runner_sha256') != sha(expected_runner) or
                test.get('owned_test_root_removed') is not True or
                not all(scenario.get(field) is True for field in required)):
            raise RuntimeError('Regression result does not prove the Envoy UI and Ubuntu 24 startup scenarios')
        test_program_sha = test.get('program_sha256', {}).get('GetDAQ')
        if test_program_sha != sha(binary):
            raise RuntimeError('Regression result was not produced by the supplied GetDAQ binary')
        if (test.get('synthetic_raw_bytes_generated') != 0 or
                test.get('synthetic_raw_bytes_deleted') != 0 or
                test.get('raw_remaining') != 0 or
                test.get('real_hardware') is not False):
            raise RuntimeError('Envoy UI regression must not generate raw data or claim hardware access')
        maximum = 0
        deleted = 0
        regression_relative = 'validation/envoy-ui-package.json'
    else:
        raise RuntimeError('No verified package-refresh regression policy exists for ' + program)

    parent = candidate.parent
    staging = Path(tempfile.mkdtemp(prefix='.' + candidate.name + '.refresh-', dir=str(parent)))
    backup = parent / ('.' + candidate.name + '.previous-' + str(os.getpid()))
    committed = False
    try:
        shutil.rmtree(staging)
        shutil.copytree(candidate, staging)
        (staging / 'validation').mkdir(parents=True, exist_ok=True)
        output = staging / 'bin' / program
        shutil.copy2(binary, output)
        stripped = subprocess.run(['strip', '--strip-debug', str(output)],
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                  text=True, check=False)
        if stripped.returncode:
            raise RuntimeError('Could not strip debug symbols from delivery copy: ' + stripped.stdout)

        root, loader, libraries = target_loader()
        listed = run_target(loader, libraries, '--list', output)
        if listed.returncode or 'not found' in listed.stdout:
            raise RuntimeError('Ubuntu 24 loader rejected the refreshed program:\n' + listed.stdout)
        prefix = str(root) + '/'
        for line in listed.stdout.splitlines():
            if '=>' in line:
                resolved = line.split('=>', 1)[1].strip().split()[0]
                if not resolved.startswith(prefix):
                    raise RuntimeError('Runtime dependency escaped Ubuntu 24 rootfs: ' + line)
        if program in ('getdaq-merge', 'getdaq-matrix', 'getdaq-inspect', 'getdaq-receiver') and any(lib in listed.stdout for lib in
                                             ('libQt', 'libIce', 'libboost', 'libyaml', 'libcurl')):
            raise RuntimeError(program + ' has a forbidden GUI/legacy runtime dependency:\n' + listed.stdout)
        if program == 'GetDAQ':
            screenshot = staging / 'validation' / '.GetDAQ-startup.png'
            with tempfile.TemporaryDirectory(prefix='.GetDAQ-runtime-', dir=str(staging / 'validation')) as runtime:
                Path(runtime).chmod(0o700)
                env = dict(os.environ, QT_QPA_PLATFORM='offscreen',
                           QT_QPA_PLATFORM_PLUGIN_PATH=str(root / 'usr/lib/x86_64-linux-gnu/qt5/plugins/platforms'),
                           QT_PLUGIN_PATH=str(root / 'usr/lib/x86_64-linux-gnu/qt5/plugins'),
                           XDG_RUNTIME_DIR=runtime,
                           XDG_CONFIG_HOME=runtime,
                           XDG_CACHE_HOME=runtime,
                           XDG_DATA_HOME=runtime)
                help_result = subprocess.run([str(loader), '--library-path', libraries,
                                              str(output), '--help'],
                                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                             text=True, timeout=30, check=False, env=env)
                if help_result.returncode or 'Usage:' not in help_result.stdout:
                    raise RuntimeError('Ubuntu 24 offscreen GetDAQ --help check failed:\n' + help_result.stdout)
                startup = subprocess.run([str(loader), '--library-path', libraries,
                                          str(output), '--screenshot', str(screenshot)],
                                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                         text=True, timeout=30, check=False, env=env)
                if startup.returncode or not screenshot.is_file() or screenshot.stat().st_size < 256:
                    raise RuntimeError('Ubuntu 24 offscreen GetDAQ startup/screenshot check failed:\n' + startup.stdout)
                if screenshot.read_bytes()[:8] != b'\x89PNG\r\n\x1a\n':
                    raise RuntimeError('Ubuntu 24 GetDAQ startup did not produce a PNG screenshot')
                application_log_bytes = validate_envoy_application_log(runtime)
                test['scenario']['ubuntu24_offscreen_screenshot'] = True
                test['scenario']['screenshot_removed'] = True
                test['scenario']['application_log_created'] = True
                test['ubuntu24_startup_exit_code'] = startup.returncode
                test['ubuntu24_screenshot_bytes'] = screenshot.stat().st_size
                test['ubuntu24_application_log_bytes'] = application_log_bytes
                screenshot.unlink()
            if not all(test['scenario'].get(field) is True for field in (
                    'native_gui_address_and_topology_hints', 'ubuntu24_offscreen_screenshot',
                    'screenshot_removed', 'application_log_created')):
                raise RuntimeError('GetDAQ regression is missing a completed package runtime scenario')
        else:
            help_result = run_target(loader, libraries, output, '--help')
            if help_result.returncode:
                raise RuntimeError('Ubuntu 24 --help check failed:\n' + help_result.stdout)

        refresh_name = 'program-refresh-' + program + '.json'
        refresh_relative = 'validation/' + refresh_name
        packaged_regression = dict(test)
        if program == 'getdaq-catalog':
            packaged_regression['catalog_delivery_binary_sha256'] = sha(output)
        json_write(staging / regression_relative, packaged_regression)
        source_hashes = load_build_suite().source_hashes()
        source_hash_map = json.dumps(source_hashes, sort_keys=True, separators=(',', ':')).encode('utf-8')
        refresh = {
            'schema': 'getdaq.ubuntu24-program-refresh:1',
            'program': program,
            'source_build_sha256': sha(binary),
            'delivery_sha256': sha(output),
            'packager_sha256': sha(Path(__file__)),
            'base_candidate_manifest_sha256': sha(candidate / 'manifest.json'),
            'build': {'standard': 'C++11', 'compiler': subprocess.check_output(['c++', '--version'], text=True).splitlines()[0],
                      'cmake': subprocess.check_output(['cmake', '--version'], text=True).splitlines()[0],
                      'full_suite_recompiled': False},
            'source_inputs': {'count': len(source_hashes),
                              'sha256': hashlib.sha256(source_hash_map).hexdigest(),
                              'querymain_sha256': sha(ROOT / 'apps/datasupervisor/querymain.cpp') if program == 'getdaq-supervisor-query' else None},
            'target_runtime': {'loader': 'Ubuntu 24 rootfs ld-linux-x86-64.so.2',
                               'resolved_dependencies': listed.stdout.replace(str(root), '<ubuntu24-rootfs>').splitlines(),
                               'help_exit_code': help_result.returncode},
            'regression': {'passed': True, 'result_sha256': sha(staging / regression_relative),
                           'source_result_sha256': sha(test_result),
                           'evidence_in_package': regression_relative,
                           'evidence_sha256': sha(staging / regression_relative),
                           'source_build_program_sha256': test_program_sha,
                           'synthetic_raw_deleted_bytes': deleted, 'raw_remaining': 0,
                           'max_aggregate_test_bytes': maximum, 'real_hardware': False},
            'reused_program_count': len(manifest['programs']) - (0 if is_new_program else 1),
            'created_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        }
        json_write(staging / refresh_relative, refresh)
        new_manifest = json.loads((staging / 'manifest.json').read_text(encoding='utf-8'))
        if program == 'getdaq-catalog':
            invalidate_catalog_validation(new_manifest, staging, regression_relative)
        original_provenance = dict(new_manifest.get('rebuild_provenance', {}))
        if is_new_program:
            new_manifest['programs'][program] = {}
        for name, program_entry in new_manifest['programs'].items():
            expected_delivery = staging / 'bin' / name
            if not expected_delivery.is_file():
                raise RuntimeError('Candidate is missing the staged program: ' + name)
            program_entry['path'] = 'bin/' + name
        entry = new_manifest['programs'][program]
        entry['sha256'] = sha(output)
        entry['runtime'] = {'checked': True, 'loader': 'Ubuntu 24 rootfs ld-linux-x86-64.so.2',
                            'resolved_dependencies': refresh['target_runtime']['resolved_dependencies'],
                            'system_libraries_bundled': False}
        new_manifest['created_utc'] = refresh['created_utc']
        new_manifest['rebuild_provenance'] = {
            'base_candidate': str(candidate.relative_to(ROOT)),
            'base_manifest_sha256': refresh['base_candidate_manifest_sha256'],
            'inherited_rebuild_provenance': original_provenance,
            'rebuilt': [program],
            'rebuilt_sha256': {program: sha(binary)},
            'delivery_sha256': {program: sha(output)},
            'reused_unchanged': sorted(set(new_manifest['programs']) - {program}),
            'full_suite_recompiled': False,
            'build_standard': 'C++11',
            'target_platform_and_hardware_accepted': False,
        }
        validation = dict(new_manifest.get('validation', {}))
        validation['incremental_program_refreshes'] = dict(validation.get('incremental_program_refreshes', {}))
        validation['incremental_program_refreshes'][program] = {
            'status': 'passed', 'evidence_in_package': refresh_relative,
            'evidence_sha256': sha(staging / refresh_relative),
            'source_build_sha256': sha(binary), 'delivery_sha256': sha(output),
            'regression_evidence': regression_relative,
            'regression_sha256': sha(staging / regression_relative), 'raw_remaining': 0,
        }
        if program == 'getdaq-sentry':
            validation['sentry_strict_configuration'] = {
                'status': 'passed', 'evidence_in_package': regression_relative,
                'result_sha256': sha(staging / regression_relative),
                'sentry_delivery_binary_sha256': sha(output),
                'malformed_configuration_cases': len(test['cases']),
                'synthetic_payload_max_bytes': maximum,
                'synthetic_raw_bytes_deleted': 0, 'raw_remaining': 0,
                'target_runtime': 'Ubuntu 24 rootfs loader; no hardware or DAQ transfer',
            }
        if program == 'getdaq-config':
            validation['peaking_time_scan_plan'] = {
                'status': 'passed', 'evidence_in_package': regression_relative,
                'result_sha256': sha(staging / regression_relative),
                'config_delivery_binary_sha256': sha(output),
                'system_config_checker_sha256': sha(staging / 'bin' / 'getdaq-system-config'),
                'process_cases': test['process_cases'],
                'scan_points': 16, 'synthetic_payload_max_bytes': 0,
                'synthetic_raw_bytes_deleted': 0, 'raw_remaining': 0,
                'target_runtime': 'Ubuntu 24 rootfs loader; no hardware or DAQ transfer',
            }
        if program == 'getdaq-catalog' and test.get('schema') == 'getdaq.threshold-catalog-analysis-regression:1':
            validation['catalog_threshold_analysis'] = {
                'status': 'passed', 'evidence_in_package': regression_relative,
                'result_sha256': sha(staging / regression_relative),
                'catalog_delivery_binary_sha256': sha(output),
                'synthetic_payload_max_bytes': maximum,
                'synthetic_raw_bytes_deleted': deleted, 'raw_remaining': 0,
                'candidate': str(candidate.relative_to(ROOT)),
                'target_runtime': test.get('runtime'),
            }
        elif program == 'getdaq-catalog':
            validation['catalog_plain_tcp'] = {
                'status': 'passed', 'evidence_in_package': regression_relative,
                'result_sha256': sha(staging / regression_relative),
                'catalog_delivery_binary_sha256': sha(output),
                'synthetic_payload_max_bytes': maximum,
                'synthetic_raw_bytes_deleted': deleted, 'raw_remaining': 0,
                'candidate': str(candidate.relative_to(ROOT)),
                'target_runtime': test.get('runtime'),
            }
        if program == 'getdaq-merge':
            validation['mfm_merge'] = {
                'status': 'passed', 'evidence_in_package': regression_relative,
                'result_sha256': sha(staging / regression_relative),
                'merge_delivery_binary_sha256': sha(output),
                'synthetic_payload_max_bytes': maximum,
                'synthetic_raw_bytes_deleted': deleted, 'raw_remaining': 0,
                'target_runtime': 'Ubuntu 24 rootfs loader; no Qt/Ice/Boost/yaml/curl runtime dependency',
            }
        if program == 'getdaq-inspect':
            validation['hitpattern_scan'] = {
                'status': 'passed', 'evidence_in_package': regression_relative,
                'result_sha256': sha(staging / regression_relative),
                'inspect_delivery_binary_sha256': sha(output),
                'synthetic_payload_max_bytes': maximum,
                'synthetic_raw_bytes_deleted': deleted, 'raw_remaining': 0,
                'target_runtime': 'Ubuntu 24 rootfs loader; no Qt/Ice/Boost/yaml/curl runtime dependency',
            }
        if program == 'getdaq-receiver':
            validation['receiver_byte_storage'] = {
                'status': 'passed', 'evidence_in_package': regression_relative,
                'result_sha256': sha(staging / regression_relative),
                'receiver_delivery_binary_sha256': sha(output),
                'synthetic_payload_max_bytes': maximum,
                'synthetic_raw_bytes_deleted': deleted, 'raw_remaining': 0,
                'target_runtime': 'Ubuntu 24 rootfs loader; no Qt/Ice/Boost/yaml/curl runtime dependency',
            }
        validation['target_platform_and_hardware_accepted'] = False
        new_manifest['validation'] = validation
        new_manifest['files'] = {str(path.relative_to(staging)): sha(path)
                                 for path in sorted(staging.rglob('*'))
                                 if path.is_file() and path != staging / 'manifest.json'}
        json_write(staging / 'manifest.json', new_manifest)
        validate_inventory(staging)

        if backup.exists():
            raise RuntimeError('Refusing to overwrite package rollback path: ' + str(backup))
        os.replace(candidate, backup)
        try:
            os.replace(staging, candidate)
            validate_inventory(candidate)
            committed = True
        except BaseException:
            if candidate.exists():
                shutil.rmtree(candidate)
            os.replace(backup, candidate)
            raise
        shutil.rmtree(backup)
        print(json.dumps({'status': 'passed', 'candidate': str(candidate.relative_to(ROOT)),
                          'program': program, 'source_build_sha256': sha(binary),
                          'delivery_sha256': sha(candidate / 'bin' / program),
                          'manifest_sha256': sha(candidate / 'manifest.json'),
                          'refresh_receipt': refresh_relative, 'raw_remaining': 0}, indent=2))
    finally:
        if staging.exists():
            shutil.rmtree(staging)
        if backup.exists() and committed:
            shutil.rmtree(backup)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--program', required=True)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--test-result', type=Path, required=True)
    args = parser.parse_args()
    replace(args.candidate, args.program, args.binary, args.test_result)


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, ValueError, KeyError, subprocess.SubprocessError) as error:
        print('Ubuntu 24 program refresh failed: ' + str(error), file=sys.stderr)
        sys.exit(1)
