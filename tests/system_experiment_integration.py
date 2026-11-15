#!/usr/bin/env python3
"""Real xcfg -> immutable settings -> native loader/binder and optional full TCP execution."""
import copy
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('compiler', type=Path)
    parser.add_argument('native', type=Path)
    parser.add_argument('fixture', nargs='?', type=Path)
    parser.add_argument('--scan-checker', type=Path,
                        help='Optionally validate generated scan plans with the Qt-free checker')
    parser.add_argument('--rootfs', type=Path, help='Run both programs using this Ubuntu userspace loader')
    parser.add_argument('--evidence', type=Path, help='Write a compact successful regression receipt')
    args = parser.parse_args()
    compiler, native = args.compiler.resolve(), args.native.resolve()
    fixture = args.fixture.resolve() if args.fixture else None
    scan_checker = args.scan_checker.resolve() if args.scan_checker else None
    rootfs = args.rootfs.resolve(strict=True) if args.rootfs else None
    loader = libraries = None
    if rootfs:
        loader = rootfs / 'usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2'
        if not loader.is_file():
            raise RuntimeError('Ubuntu userspace loader is missing')
        library_dirs = [rootfs / part for part in ('lib/x86_64-linux-gnu', 'lib',
                         'usr/lib/x86_64-linux-gnu', 'usr/lib', 'usr/local/lib')
                        if (rootfs / part).is_dir()]
        libraries = ':'.join(str(path) for path in library_dirs)
    production = root / 'source/get/GetBench-software-release-20210208/data/config/configure-ExternalTrigger-2CoBo_1Mutant.xcfg'
    profile_source = root / 'config/experiment/system-profile.external-trigger.example.json'
    original = production.read_bytes()
    profile_bytes = profile_source.read_bytes()
    profile_base = json.loads(profile_bytes)
    cases = 0
    scenarios = {}
    with tempfile.TemporaryDirectory(prefix='getdaq-system-config-') as temporary:
        folder = Path(temporary)
        process_environment = dict(os.environ)
        for name in ('home', 'cache', 'config', 'data', 'runtime'):
            directory = folder / name
            directory.mkdir(mode=0o700)
            process_environment[{'home':'HOME', 'cache':'XDG_CACHE_HOME', 'config':'XDG_CONFIG_HOME',
                                'data':'XDG_DATA_HOME', 'runtime':'XDG_RUNTIME_DIR'}[name]] = str(directory)
        process_environment['TMPDIR'] = str(folder)
        process_environment['QT_QPA_PLATFORM'] = 'offscreen'
        if rootfs:
            plugin_path = rootfs / 'usr/lib/x86_64-linux-gnu/qt5/plugins'
            process_environment['QT_PLUGIN_PATH'] = str(plugin_path)
            process_environment['QT_QPA_PLATFORM_PLUGIN_PATH'] = str(plugin_path / 'platforms')

        def target_command(executable, arguments):
            if rootfs:
                return [str(loader), '--library-path', libraries, str(executable), *map(str, arguments)]
            return [str(executable), *map(str, arguments)]

        def command(args, success=True, timeout=30):
            nonlocal cases
            executable, *arguments = args
            result = subprocess.run(target_command(executable, arguments), capture_output=True, text=True,
                                    timeout=timeout, env=process_environment)
            cases += 1
            assert (result.returncode == 0) == success, (args, result.returncode, result.stdout, result.stderr)
            return result

        def compile_case(profile=None, source=production, success=True):
            p = folder / ('profile-%d.json' % cases)
            p.write_text(json.dumps(profile or profile_base, ensure_ascii=False, indent=2))
            output = folder / ('artifact-%d.json' % cases)
            command([compiler, 'compile-system', source, p, output], success)
            if not success:
                assert not output.exists(), 'Rejected compilation published output'
                return None
            before = output.read_bytes()
            assert json.loads(before)['profile_sha256'] == hashlib.sha256(p.read_bytes()).hexdigest()
            command([compiler, 'compile-system', source, p, output], False)
            assert output.read_bytes() == before, 'Existing artifact was overwritten'
            return output

        def validate_scan_plan(directory):
            if not scan_checker:
                return
            manifest_path = directory / 'scan-manifest.json'
            original_manifest = manifest_path.read_bytes()
            summary = json.loads(command([scan_checker, 'scan-plan', directory]).stdout)
            assert summary['valid'] and summary['point_count'] > 0
            policy_path = directory.with_name(directory.name + '-execution-policy.json')
            manifest = json.loads(original_manifest)
            if manifest['kind'] == 'getdaq.aget-threshold-scan':
                policy_command = [compiler, 'create-scan-policy', directory,
                                  'timed-windows', '3', '1000', policy_path]
                command(policy_command, False)
                assert not policy_path.exists(), 'Threshold automatic Run policy was published'
                return
            else:
                policy_command = [compiler, 'create-scan-policy', directory,
                                  'event-count', '100000', '3600000', policy_path]
            command(policy_command)
            policy_bytes = policy_path.read_bytes()
            policy = json.loads(policy_bytes)
            assert policy['source_plan_sha256'] == hashlib.sha256(original_manifest).hexdigest()
            command(policy_command, False)
            assert policy_path.read_bytes() == policy_bytes, 'Existing execution policy was overwritten'
            checked_policy = json.loads(command(
                [scan_checker, 'scan-execution-policy', directory, policy_path]).stdout)
            assert checked_policy['valid'] and not checked_policy['hardware_command_sent']
            assert checked_policy['source_plan_sha256'] == policy['source_plan_sha256']
            changed_policy = dict(policy, source_plan_sha256='0' * 64)
            policy_path.write_text(json.dumps(changed_policy), encoding='utf-8')
            command([scan_checker, 'scan-execution-policy', directory, policy_path], False)
            policy_path.write_text(json.dumps(policy), encoding='utf-8')
            manifest = json.loads(original_manifest)
            manifest['unexpected_protocol_field'] = True
            manifest_path.write_text(json.dumps(manifest, ensure_ascii=False), encoding='utf-8')
            command([scan_checker, 'scan-plan', directory], False)
            manifest_path.write_bytes(original_manifest)
            point_path = directory / manifest['points'][0]['artifact']
            original_point = point_path.read_bytes()
            point = json.loads(original_point)
            point['unexpected_protocol_field'] = True
            point_path.write_text(json.dumps(point, ensure_ascii=False), encoding='utf-8')
            command([scan_checker, 'scan-plan', directory], False)
            point_path.write_bytes(original_point)
            command([scan_checker, 'scan-plan', directory])

        artifact = compile_case()
        image = json.loads(artifact.read_text())
        settings = image['settings']
        artifact_check = None
        if scan_checker:
            artifact_check = json.loads(command([scan_checker, 'experiment', artifact]).stdout)
            assert artifact_check['valid'] and artifact_check['configuration_sha256'] == image['configuration_sha256']
            assert artifact_check['artifact_sha256'] == hashlib.sha256(artifact.read_bytes()).hexdigest()
            assert not artifact_check['hardware_loaded_verified']
        canonical = json.dumps(settings, ensure_ascii=False, separators=(',', ':')).encode()
        expected = hashlib.sha256(b'getdaq.system-experiment-settings:1\n' + canonical).hexdigest()
        assert image['configuration_sha256'] == expected
        assert image['sources'] == [{'path': str(production), 'sha256': hashlib.sha256(original).hexdigest()}]
        inspected = json.loads(command([native, 'inspect', artifact]).stdout)
        assert inspected['artifact_sha256'] == hashlib.sha256(artifact.read_bytes()).hexdigest()
        assert inspected['configuration_sha256'] == expected and not inspected['hardware_connected'] and not inspected['production_ecc_state_committed']
        scan_dir = folder / 'peaking-scan-plan'
        scan_cobo_id = settings['cobos'][0]['id']
        command([compiler, 'plan-peaking-scan', artifact, scan_cobo_id, scan_dir])
        validate_scan_plan(scan_dir)
        scan_manifest = json.loads((scan_dir / 'scan-manifest.json').read_text())
        assert scan_manifest['schema_version'] == 1 and scan_manifest['kind'] == 'getdaq.peaking-time-scan'
        assert scan_manifest['hardware_loaded_verified'] is False
        assert scan_manifest['target_cobo'] == scan_cobo_id
        assert scan_manifest['requires_full_configure_per_point'] is True
        assert scan_manifest['acquisition_event_count_unspecified'] is True
        aget_times = [70,117,232,280,334,383,502,541,568,632,721,760,831,870,976,1014]
        astre_times = [68,116,180,225,343,442,541,641,739,836,943,1039,2034,3985,5770,7700]
        assert len(scan_manifest['points']) == 16
        scan_hashes = set()
        for index, point in enumerate(scan_manifest['points']):
            assert point['index'] == index and point['artifact'] == 'point-%02d.json' % index
            scan_path = scan_dir / point['artifact']
            scan_bytes = scan_path.read_bytes()
            scan_artifact = json.loads(scan_bytes)
            assert hashlib.sha256(scan_bytes).hexdigest() == point['artifact_sha256']
            assert scan_artifact['configuration_sha256'] == point['configuration_sha256']
            scan_hashes.add(point['configuration_sha256'])
            assert scan_artifact['sources'] == image['sources']
            for scan_cobo, source_cobo in zip(scan_artifact['settings']['cobos'], settings['cobos']):
                if source_cobo['id'] != scan_cobo_id:
                    assert scan_cobo == source_cobo, 'Scan changed a non-target CoBo'
                    continue
                expected_cobo = copy.deepcopy(source_cobo)
                for expected_chip in expected_cobo['configure']['readout']['aget']['chips']:
                    table = astre_times if int(expected_chip['settings']['chipVersion']) == 0x203 else aget_times
                    expected_chip['settings']['globals']['Reg1/peackingTime'] = str(table[index])
                assert scan_cobo == expected_cobo, 'Scan changed fields outside the selected CoBo peaking time'
            assert json.loads(command([native, 'inspect', scan_path]).stdout)['configuration_sha256'] == point['configuration_sha256']
        assert len(scan_hashes) == 16
        command([compiler, 'plan-peaking-scan', artifact, scan_cobo_id, scan_dir], False)
        assert len(list(scan_dir.iterdir())) == 17, 'Existing scan plan was overwritten or augmented'
        pulser_generator = settings['cobos'][0]['configure']['asad']['generator']
        pulser_start = int(float(pulser_generator['amplitudeStart']))
        pulser_stop = min(pulser_start + 10, 1000)
        pulser_step = 5
        pulser_dir = folder / 'pulser-scan-plan'
        command([compiler, 'plan-pulser-scan', artifact, scan_cobo_id,
                 str(pulser_start), str(pulser_stop), str(pulser_step), pulser_dir])
        validate_scan_plan(pulser_dir)
        pulser_manifest = json.loads((pulser_dir / 'scan-manifest.json').read_text())
        assert pulser_manifest['schema_version'] == 1 and pulser_manifest['kind'] == 'getdaq.pulser-amplitude-scan'
        assert pulser_manifest['target_cobo'] == scan_cobo_id
        assert pulser_manifest['hardware_loaded_verified'] is False
        assert pulser_manifest['requires_full_configure_per_point'] is True
        assert pulser_manifest['acquisition_event_count_unspecified'] is True
        expected_voltages = list(range(pulser_start, pulser_stop + 1, pulser_step))
        assert [point['voltage_mv'] for point in pulser_manifest['points']] == expected_voltages
        for index, point in enumerate(pulser_manifest['points']):
            assert point['index'] == index and point['artifact'] == 'point-%03d.json' % index
            scan_path = pulser_dir / point['artifact']
            scan_bytes = scan_path.read_bytes()
            scan_artifact = json.loads(scan_bytes)
            assert hashlib.sha256(scan_bytes).hexdigest() == point['artifact_sha256']
            assert scan_artifact['configuration_sha256'] == point['configuration_sha256']
            assert scan_artifact['sources'] == image['sources']
            expected_settings = copy.deepcopy(settings)
            expected_settings['cobos'][0]['configure']['asad']['generator']['amplitudeStart'] = str(point['voltage_mv'])
            assert scan_artifact['settings'] == expected_settings, 'Pulser scan changed settings outside selected CoBo amplitude'
            assert json.loads(command([native, 'inspect', scan_path]).stdout)['configuration_sha256'] == point['configuration_sha256']
        command([compiler, 'plan-pulser-scan', artifact, scan_cobo_id,
                 str(pulser_start), str(pulser_stop), str(pulser_step), pulser_dir], False)
        assert len(list(pulser_dir.iterdir())) == len(expected_voltages) + 1
        threshold_cobo = next(cobo for cobo in settings['cobos'] if cobo['id'] == scan_cobo_id)
        threshold_chip = threshold_cobo['configure']['readout']['aget']['chips'][0]
        threshold_asad = int(threshold_chip['asadId'])
        threshold_aget = int(threshold_chip['agetId'])
        threshold_globals = threshold_chip['settings']['globals']
        threshold_start = int(threshold_globals['Reg1/GlobalThresholdValue']) * 16
        threshold_stop = threshold_start + 8
        threshold_step = 4
        threshold_dir = folder / 'threshold-scan-plan'
        command([compiler, 'plan-threshold-scan', artifact, scan_cobo_id,
                 str(threshold_asad), str(threshold_aget), str(threshold_start),
                 str(threshold_stop), str(threshold_step), threshold_dir])
        validate_scan_plan(threshold_dir)
        threshold_manifest = json.loads((threshold_dir / 'scan-manifest.json').read_text())
        assert threshold_manifest['schema_version'] == 1
        assert threshold_manifest['kind'] == 'getdaq.aget-threshold-scan'
        assert threshold_manifest['target_cobo'] == scan_cobo_id
        assert threshold_manifest['target_asad'] == threshold_asad
        assert threshold_manifest['target_aget'] == threshold_aget
        assert threshold_manifest['hardware_loaded_verified'] is False
        assert threshold_manifest['requires_full_configure_per_point'] is True
        assert threshold_manifest['acquisition_event_count_unspecified'] is True
        expected_codes = list(range(threshold_start, threshold_stop + 1, threshold_step))
        assert [point['threshold_code'] for point in threshold_manifest['points']] == expected_codes
        for index, point in enumerate(threshold_manifest['points']):
            assert point['index'] == index and point['artifact'] == 'point-%03d.json' % index
            scan_path = threshold_dir / point['artifact']
            scan_bytes = scan_path.read_bytes()
            scan_artifact = json.loads(scan_bytes)
            assert hashlib.sha256(scan_bytes).hexdigest() == point['artifact_sha256']
            assert scan_artifact['configuration_sha256'] == point['configuration_sha256']
            expected_settings = copy.deepcopy(settings)
            selected_cobo = next(cobo for cobo in expected_settings['cobos'] if cobo['id'] == scan_cobo_id)
            selected_chip = next(chip for chip in selected_cobo['configure']['readout']['aget']['chips']
                                 if int(chip['asadId']) == threshold_asad and int(chip['agetId']) == threshold_aget)
            code = point['threshold_code']
            selected_chip['settings']['globals']['Reg1/GlobalThresholdValue'] = str(code // 16)
            for channel in selected_chip['settings']['channels']:
                channel['threshold'] = str(code % 16)
            assert scan_artifact['settings'] == expected_settings, 'Threshold scan changed settings outside the selected AGET'
            assert json.loads(command([native, 'inspect', scan_path]).stdout)['configuration_sha256'] == point['configuration_sha256']
        command([compiler, 'plan-threshold-scan', artifact, scan_cobo_id,
                 str(threshold_asad), str(threshold_aget), str(threshold_start),
                 str(threshold_stop), str(threshold_step), threshold_dir], False)
        assert len(list(threshold_dir.iterdir())) == len(expected_codes) + 1

        # AutoGain is represented only as a bounded set of complete offline
        # configuration points. Pick an actually active channel from the
        # selected AGET rather than assuming a detector-specific channel map.
        autogain_chip = threshold_chip
        active_channels = [int(channel['id']) for channel in autogain_chip['settings']['channels']
                           if channel['active']]
        assert active_channels, 'Production fixture has no active channel for AutoGain planning'
        autogain_channel = active_channels[0]
        autogain_start = int(float(pulser_generator['amplitudeStart']))
        autogain_stop = min(autogain_start + 5, 1000)
        autogain_dir = folder / 'autogain-scan-plan'
        command([compiler, 'plan-autogain-scan', artifact, scan_cobo_id,
                 str(threshold_asad), str(threshold_aget), str(autogain_channel),
                 str(autogain_channel), str(autogain_start), str(autogain_stop), '5', autogain_dir])
        validate_scan_plan(autogain_dir)
        autogain_manifest = json.loads((autogain_dir / 'scan-manifest.json').read_text())
        assert autogain_manifest['schema_version'] == 1
        assert autogain_manifest['kind'] == 'getdaq.auto-gain-scan'
        assert autogain_manifest['target_cobo'] == scan_cobo_id
        assert autogain_manifest['target_asad'] == threshold_asad
        assert autogain_manifest['target_aget'] == threshold_aget
        assert autogain_manifest['first_channel'] == autogain_channel
        assert autogain_manifest['last_channel'] == autogain_channel
        assert autogain_manifest['channel_order'] == \
            'aget-reg3-bit-ascending-then-reg4-bit-ascending'
        assert autogain_manifest['legacy_channel_mask_mapping_verified_from_source'] is True
        assert autogain_manifest['hardware_loaded_verified'] is False
        assert autogain_manifest['requires_full_configure_per_point'] is True
        assert autogain_manifest['acquisition_event_count_unspecified'] is True
        assert autogain_manifest['gain_result_interpretation_verified'] is False
        expected_voltages = list(range(autogain_start, autogain_stop + 1, 5))
        assert [point['voltage_mv'] for point in autogain_manifest['points']] == expected_voltages
        for index, point in enumerate(autogain_manifest['points']):
            assert point['index'] == index and point['channel_id'] == autogain_channel
            assert point['artifact'] == 'point-%03d.json' % index
            scan_path = autogain_dir / point['artifact']
            scan_bytes = scan_path.read_bytes()
            scan_artifact = json.loads(scan_bytes)
            assert hashlib.sha256(scan_bytes).hexdigest() == point['artifact_sha256']
            assert scan_artifact['configuration_sha256'] == point['configuration_sha256']
            expected_settings = copy.deepcopy(settings)
            selected_cobo = next(cobo for cobo in expected_settings['cobos'] if cobo['id'] == scan_cobo_id)
            selected_cobo['configure']['asad']['generator']['amplitudeStart'] = str(point['voltage_mv'])
            selected_chip = next(chip for chip in selected_cobo['configure']['readout']['aget']['chips']
                                 if int(chip['asadId']) == threshold_asad and int(chip['agetId']) == threshold_aget)
            for channel in selected_chip['settings']['channels']:
                channel['selectedForTest'] = int(channel['id']) == autogain_channel
            assert scan_artifact['settings'] == expected_settings, \
                'AutoGain point changed fields outside selected channel and Pulser amplitude'
            assert json.loads(command([native, 'inspect', scan_path]).stdout)['configuration_sha256'] == point['configuration_sha256']
        command([compiler, 'plan-autogain-scan', artifact, scan_cobo_id,
                 str(threshold_asad), str(threshold_aget), str(autogain_channel),
                 str(autogain_channel), str(autogain_start), str(autogain_stop), '5', autogain_dir], False)
        assert len(list(autogain_dir.iterdir())) == len(expected_voltages) + 1
        scenarios = {
            'complete_system_artifact_compiled': True,
            'selected_cobo_only_changed': True,
            'sixteen_scan_points_generated': True,
            'manifest_and_artifact_sha256_verified': True,
            'configuration_digests_verified': True,
            'native_system_artifact_checker_passed': True,
            'configcheck_experiment_command_passed': bool(artifact_check),
            'existing_plan_overwrite_rejected': True,
            'original_configuration_unchanged': True,
            'no_hardware_commands': True,
            'pulser_amplitude_points_generated': True,
            'pulser_manifest_and_artifact_sha256_verified': True,
            'pulser_complete_settings_and_digests_verified': True,
            'existing_pulser_plan_overwrite_rejected': True,
            'aget_threshold_points_generated': True,
            'threshold_manifest_and_artifact_sha256_verified': True,
            'threshold_complete_settings_and_digests_verified': True,
            'existing_threshold_plan_overwrite_rejected': True,
            'autogain_active_channel_and_amplitude_points_generated': True,
            'autogain_manifest_and_artifact_sha256_verified': True,
            'autogain_complete_settings_and_digests_verified': True,
            'qt_free_scan_checker_and_unknown_field_rejection': bool(scan_checker),
            'existing_autogain_plan_overwrite_rejected': True,
        }
        assert settings['cobos'][0]['prepare']['foundation']['lemo'] == ['external_trigger_input', 'trigger_pulse_output', 'busy_output', 'none']
        assert settings['cobos'][0]['prepare']['manualReadDataDelay'] == '14'
        assert settings['mutants'][0]['settings']['moduleCount'] == '1'
        assert settings['mutants'][0]['settings']['gmcShelfMask'] == '0'
        topology = {'schema_version': 1, 'kind': 'getdaq.alignment-topology', 'encoding': '1.0', 'timeout_ms': 1000,
                    'cobos': [{'id': c['id'], 'expected_node_name': 'CoBo%d' % i, 'node_proxy': 'CoBo%d:tcp -h 127.0.0.1 -p 1' % i,
                              'link_proxy': 'Link%d:tcp -h 127.0.0.1 -p 1' % i, 'enabled': c['enabled'],
                              'hardware_description_file': str(root / 'tests/data/cobo-hardware.json')} for i, c in enumerate(settings['cobos'])],
                    'mutants': [{'id': 'master', 'expected_node_name': 'MuTaNT0', 'node_proxy': 'MuTaNT0:tcp -h 127.0.0.1 -p 1',
                                'alignment_proxy': 'Alignment0:tcp -h 127.0.0.1 -p 1', 'role': 'master',
                                'hardware_description_file': str(root / 'tests/data/mutant-hardware.json')}]}

        def binding(top, success=True, selected=artifact):
            path = folder / ('topology-%d.json' % cases)
            path.write_text(json.dumps(top))
            return command([native, 'validate-binding', selected, path], success)

        assert json.loads(binding(topology).stdout)['topology_bound']
        reversed_topology = copy.deepcopy(topology)
        reversed_topology['cobos'].reverse()
        binding(reversed_topology)
        for mutate in [lambda t: t['cobos'].pop(), lambda t: t['cobos'][0].update(id='wrong'),
                       lambda t: t['cobos'][1].update(enabled=True), lambda t: t['mutants'][0].update(role='slave'),
                       lambda t: t['cobos'][1].update(id=t['cobos'][0]['id'])]:
            changed = copy.deepcopy(topology)
            mutate(changed)
            binding(changed, False)
        for mutate in [lambda p: p['cobos'].pop(), lambda p: p['cobos'][0].update(enabled=False),
                       lambda p: p['cobos'][0].update(hardware_version='02'), lambda p: p['cobos'][0].update(hardware_version='0'),
                       lambda p: p['cobos'][0]['chip_versions'].update({'4': '0x202'}), lambda p: p['cobos'][0]['chip_versions'].pop('2'),
                       lambda p: p['cobos'][0].update(adc_mode='AUTO'), lambda p: p['cobos'][0].update(delay_layout='auto'),
                       lambda p: p['cobos'][0].update(led_policy='auto'), lambda p: p['mutants'][0].update(clock_policy='auto'),
                       lambda p: p.update(unexpected=True), lambda p: p['cobos'][1].update(hardware_version='2')]:
            changed = copy.deepcopy(profile_base)
            mutate(changed)
            compile_case(changed, success=False)
        bad_profile = folder / 'duplicate-profile.json'
        bad_profile.write_text(json.dumps(profile_base).replace('"schema_version": 1', '"schema_version": 1, "schema_version": 1'))
        duplicate_output = folder / 'duplicate-profile-artifact.json'
        command([compiler, 'compile-system', production, bad_profile, duplicate_output], False)
        assert not duplicate_output.exists()

        for mutate in [lambda i: i['settings']['cobos'][0]['prepare'].update(manualReadDataDelay='15'),
                       lambda i: i['settings']['cobos'][0]['prepare'].update(unknown=True),
                       lambda i: i['settings']['cobos'][0]['prepare'].pop('manualReadDataDelay'),
                       lambda i: i['settings']['mutants'][0]['settings'].update(timestamp10ns=2**48-1),
                       lambda i: i.update(configuration_sha256='0'*64), lambda i: i.update(schema_version=2)]:
            changed = copy.deepcopy(image)
            mutate(changed)
            path = folder / ('rejected-%d.json' % cases)
            path.write_text(json.dumps(changed))
            command([native, 'inspect', path], False)
        reordered = folder / 'reordered.json'
        reordered.write_text(json.dumps(image, sort_keys=True, indent=3))
        reformatted = json.loads(command([native, 'inspect', reordered]).stdout)
        assert reformatted['configuration_sha256'] == expected and reformatted['artifact_sha256'] != inspected['artifact_sha256']

        # A missing early legacy field must not discard later explicit protection settings.
        tree = ET.fromstring(original)
        control = tree.find('Node[@id="CoBo"]/Instance[@id="*"]/AsAd[@id="*"]/Aget[@id="*"]/Control')
        control.remove(control.find('enableAsAdPosition'))
        control.find('connectGroundToZAP').text = 'true'
        control.find('antiSparklingEnabled').text = 'false'
        source = folder / 'independent-input-defaults.xcfg'
        ET.ElementTree(tree).write(source, encoding='utf-8')
        modified = json.loads(compile_case(source=source).read_text())
        position = modified['settings']['cobos'][0]['prepare']['inputs']['boards'][0]['positions'][0]
        assert position['enabled'] and position['connectGroundToZAP'] and not position['antiSparklingEnabled']

        # Source snapshots are exact and include all files; compiler must not synthesize wildcard instances.
        tree = ET.fromstring(original)
        tree.find('Node[@id="CoBo"]/Instance[@id="*"]/Module/readDataDelay').set('unit', 'ns')
        source = folder / 'bad-delay-unit.xcfg'
        ET.ElementTree(tree).write(source, encoding='utf-8')
        compile_case(source=source, success=False)

        tree = ET.fromstring(original)
        tree.remove(tree.find('Node[@id="Mutant"]'))
        source = folder / 'standalone.xcfg'
        ET.ElementTree(tree).write(source, encoding='utf-8')
        standalone_profile = copy.deepcopy(profile_base)
        standalone_profile['mutants'] = []
        standalone = compile_case(standalone_profile, source)
        standalone_topology = copy.deepcopy(topology)
        standalone_topology['mutants'] = []
        binding(standalone_topology, selected=standalone)
        if fixture:
            result = command([fixture, root / 'tests/data/cobo-hardware.json', root / 'tests/data/mutant-hardware.json', '--artifact', artifact], timeout=300)
            print(result.stdout, end='')
        assert production.read_bytes() == original and profile_source.read_bytes() == profile_bytes
        if args.evidence:
            evidence = {
                'schema': 'getdaq.config-scan-regression:2',
                'passed': True,
                'runner_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                'program_sha256': {'config': hashlib.sha256(compiler.read_bytes()).hexdigest(),
                                   'system_config': hashlib.sha256(native.read_bytes()).hexdigest()},
                'process_cases': cases,
                'scenario': scenarios,
                'synthetic_raw_bytes_generated': 0,
                'synthetic_raw_bytes_deleted': 0,
                'maximum_aggregate_test_bytes': 0,
                'raw_remaining': 0,
                'real_hardware': False,
                'target_runtime': 'Ubuntu 24 loader' if rootfs else 'host runtime',
            }
            args.evidence.parent.mkdir(parents=True, exist_ok=True)
            args.evidence.write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + '\n')
    print('PASS system experiment compiler/native loader/binding: %d process cases; original source preserved' % cases)


if __name__ == '__main__':
    main()
