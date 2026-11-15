#!/usr/bin/env python3
"""真实GET配置→发布包→无Qt读者→ECC离线配置检查；不连接板卡或接收器。"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import secrets
import subprocess
import tempfile
import xml.etree.ElementTree as ET


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('publisher', type=Path)
    parser.add_argument('reader', type=Path)
    parser.add_argument('ecc', type=Path)
    parser.add_argument('--evidence', type=Path, required=True)
    args = parser.parse_args()
    programs = [p.resolve() for p in (args.publisher, args.reader, args.ecc)]
    repo = Path(__file__).resolve().parents[1]
    args.evidence.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix='configuration-publication-', dir=args.evidence)).resolve()
    original = repo / 'source/get/GetBench-software-release-20210208/data/config/configure-ExternalTrigger-2CoBo_1Mutant.xcfg'
    profile = repo / 'config/experiment/system-profile.external-trigger.example.json'
    originals = {p: p.read_bytes() for p in (original, profile)}
    traces = []

    def run(program, command, success=True):
        result = subprocess.run([str(program)] + list(map(str, command)), capture_output=True, text=True, timeout=40)
        traces.append({'program': str(program), 'args': list(map(str, command)), 'returncode': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr})
        assert (result.returncode == 0) == success, traces[-1]
        return json.loads(result.stdout) if success and result.stdout.lstrip().startswith('{') else None

    def write(path, value):
        path.write_text(json.dumps(value, indent=2) + '\n')
        path.chmod(0o600)

    package = root / 'v1'
    receipt = run(programs[0], ['publish-system', original, profile, package])
    manifest_bytes = (package / 'publication.json').read_bytes()
    manifest = json.loads(manifest_bytes)
    assert receipt['manifest_sha256'] == sha(manifest_bytes) and not receipt['hardware_loaded_verified']
    inspected = run(programs[1], ['inspect-publication', package / 'publication.json'])
    assert inspected['publication_sha256'] == receipt['manifest_sha256'] and not inspected['hardware_loaded_verified'] and inspected['effective_snapshot_semantics_recomputed']
    for entry in [manifest['artifact'], manifest['profile'], manifest['effective_snapshot']] + [s['file'] for s in manifest['sources']]:
        stored = package / entry['stored']
        assert stored.stat().st_size == int(entry['bytes']) and sha(stored.read_bytes()) == entry['sha256']
        assert stored.stat().st_mode & 0o222 == 0
    assert package.stat().st_mode & 0o777 == 0o700
    source_bytes = [(s, (package / s['file']['stored']).read_bytes()) for s in manifest['sources']]
    assert source_bytes[0][1] == originals[original]
    run(programs[0], ['publish-system', original, profile, package], False)
    assert (package / 'publication.json').read_bytes() == manifest_bytes
    existing = root / 'existing-empty'
    existing.mkdir()
    run(programs[0], ['publish-system', original, profile, existing], False)
    assert not list(existing.iterdir())
    bad_profile = root / 'bad-profile.json'
    bad_profile.write_text('{}')
    run(programs[0], ['publish-system', original, bad_profile, root / 'invalid-profile'], False)
    assert not (root / 'invalid-profile').exists()
    inputs = root / 'inputs'
    inputs.mkdir()
    (inputs / 'config.xcfg').write_bytes(originals[original])
    wrapper = root / 'wrapper.xcfg'
    original_root=ET.fromstring(originals[original])
    include_root=ET.Element(original_root.tag,dict(original_root.attrib))
    include_root.set('include','inputs/config.xcfg')
    wrapper.write_bytes(ET.tostring(include_root,encoding='utf-8'))
    included = root / 'v2-include'
    second = run(programs[0], ['publish-system', wrapper, profile, included])
    run(programs[1], ['inspect-publication', included / 'publication.json'])
    assert second['configuration_sha256'] == receipt['configuration_sha256'] and second['effective_sha256'] == receipt['effective_sha256']
    assert len(json.loads((included / 'publication.json').read_bytes())['sources']) == 2
    image = json.loads((package / 'experiment.json').read_bytes())
    descriptions = []
    for role in ('cobo', 'mutant'):
        path = root / (role + '-hardware.json')
        path.write_bytes((repo / 'tests/data' / (role + '-hardware.json')).read_bytes())
        path.chmod(0o600)
        descriptions.append({'path': str(path), 'sha256': sha(path.read_bytes())})
    settings = image['settings']
    topology = {'schema_version': 1, 'kind': 'getdaq.alignment-topology', 'encoding': '1.0', 'timeout_ms': 1000,
                'cobos': [{'id': c['id'], 'expected_node_name': f'CoBo{i}', 'node_proxy': f'CoBo{i}:tcp -h 127.0.0.1 -p 1', 'link_proxy': f'Link{i}:tcp -h 127.0.0.1 -p 1', 'enabled': c['enabled'], 'hardware_description_file': descriptions[0]['path']} for i, c in enumerate(settings['cobos'])],
                'mutants': [{'id': m['id'], 'expected_node_name': f'MuTaNT{i}', 'node_proxy': f'MuTaNT{i}:tcp -h 127.0.0.1 -p 1', 'alignment_proxy': f'Alignment{i}:tcp -h 127.0.0.1 -p 1', 'role': 'slave' if m['settings']['isSlave'] else 'master', 'hardware_description_file': descriptions[1]['path']} for i, m in enumerate(settings['mutants'])]}
    topology_path = root / 'topology.json'
    write(topology_path, topology)
    credential = root / 'credential-private'
    credential.write_text(secrets.token_hex(32) + '\n')
    credential.chmod(0o600)
    config = json.loads((repo / 'config/ecc/service.example.json').read_text())
    config.update({'credential_file': str(credential), 'audit_root': str(root / 'ecc-never-created'), 'deployment_id': 'publication-offline-test',
                   'experiment_artifact': {'path': str(package / 'experiment.json'), 'sha256': manifest['artifact']['sha256']},
                   'configuration_publication': {'path': str(package / 'publication.json'), 'sha256': sha(manifest_bytes)},
                   'topology': {'path': str(topology_path), 'sha256': sha(topology_path.read_bytes())}, 'hardware_descriptions': descriptions,
                   'cobo_policies': [{'id': c['id'], 'callback_endpoints': 'tcp -h 127.0.0.1 -p 0', 'published_endpoints': '', 'max_pending_alarms': 4096, 'max_alarm_audit_bytes': 1048576} for c in settings['cobos']],
                   'senders': [{'id': c['id'], 'control_proxy': f'Daq{i}:tcp -h 127.0.0.1 -p 1', 'receiver_address': f'127.0.0.1:{47000+i}', 'circular_buffer_mask': int(c['configure']['readout']['aget']['asadMask']), 'always_flush': False, 'exclusive_stopped_owner_confirmed': True, 'encoding_minor': 0, 'timeout_ms': 1000} for i, c in enumerate(settings['cobos']) if c['enabled']]})
    config['senders'] += [{'id': m['id'], 'control_proxy': f'MutantDaq{i}:tcp -h 127.0.0.1 -p 1', 'receiver_address': f'127.0.0.1:{47100+i}', 'circular_buffer_mask': 1, 'always_flush': False, 'exclusive_stopped_owner_confirmed': True, 'encoding_minor': 0, 'timeout_ms': 1000} for i, m in enumerate(settings['mutants']) if m['settings']['readoutEnabled']]
    config['remote_receivers']['endpoints'][0]['credential_file'] = str(credential)
    config['remote_receivers']['endpoints'][0]['plan_sha256'] = 'b' * 64
    config_path = root / 'service.json'
    write(config_path, config)
    run(programs[2], ['--check-config', '--config', config_path])
    assert not (root / 'ecc-never-created').exists()
    # 外层文件/清单SHA都按修改后的字节重新计算，仍须因内层语义错误拒绝。
    effective_path=package/'effective.json'
    effective_original=effective_path.read_bytes()
    effective=json.loads(effective_original)
    variants=[]
    bad=copy.deepcopy(effective)
    bad['scalars'][0]['normalized_value']+='tampered'
    variants.append(('normalized-value',bad))
    bad=copy.deepcopy(effective)
    bad['effective_sha256']='0'*64
    variants.append(('claimed-digest',bad))
    bad=copy.deepcopy(effective)
    scalar=next(item for item in bad['scalars'] if item['type']=='int')
    scalar['range']='[2,2]' if scalar['normalized_value']=='1' else '[1,1]'
    variants.append(('range-membership',bad))
    for name,bad in variants:
        modified=copy.deepcopy(manifest)
        effective_path.chmod(0o600)
        effective_path.write_text(json.dumps(bad,ensure_ascii=False,separators=(',',':'))+'\n')
        effective_path.chmod(0o400)
        modified['effective_snapshot']['sha256']=sha(effective_path.read_bytes())
        modified['effective_snapshot']['bytes']=str(effective_path.stat().st_size)
        modified['effective_sha256']=bad['effective_sha256']
        modified['snapshot_sha256']=bad['snapshot_sha256']
        manifest_path=package/'publication.json'
        manifest_path.chmod(0o600)
        manifest_path.write_text(json.dumps(modified,separators=(',',':'))+'\n')
        manifest_path.chmod(0o400)
        bad_config=copy.deepcopy(config)
        bad_config['configuration_publication']['sha256']=sha(manifest_path.read_bytes())
        bad_config_path=root/(name+'-service.json')
        write(bad_config_path,bad_config)
        run(programs[1],['inspect-publication',manifest_path],False)
        assert 'Snapshot' in traces[-1]['stderr'],traces[-1]
        run(programs[2],['--check-config','--config',bad_config_path],False)
        assert 'Snapshot' in traces[-1]['stderr'],traces[-1]
    for file,data in [(effective_path,effective_original),(package/'publication.json',manifest_bytes)]:
        file.chmod(0o600); file.write_bytes(data); file.chmod(0o400)
    selected = package / manifest['sources'][0]['file']['stored']
    selected.chmod(0o600)
    run(programs[2], ['--check-config', '--config', config_path], False)
    selected.write_bytes(b'x' + source_bytes[0][1][1:])
    selected.chmod(0o400)
    run(programs[1], ['inspect-publication', package / 'publication.json'], False)
    run(programs[2], ['--check-config', '--config', config_path], False)
    selected.chmod(0o600)
    selected.write_bytes(source_bytes[0][1])
    selected.chmod(0o400)
    wrong = copy.deepcopy(config)
    external = root / 'external-identical-artifact.json'
    external.write_bytes((package / 'experiment.json').read_bytes())
    external.chmod(0o600)
    wrong['experiment_artifact']['path'] = str(external)
    wrong_path = root / 'wrong-artifact-service.json'
    write(wrong_path, wrong)
    run(programs[2], ['--check-config', '--config', wrong_path], False)
    malformed = dict(manifest, unexpected=True)
    wrong_manifest = package / 'wrong-manifest.json'
    write(wrong_manifest, malformed)
    run(programs[1], ['inspect-publication', wrong_manifest], False)
    partial = root / 'partial'
    partial.mkdir()
    run(programs[1], ['inspect-publication', partial / 'publication.json'], False)
    assert all(path.read_bytes() == data for path, data in originals.items())
    (root / 'processes.json').write_text(json.dumps(traces, indent=2) + '\n')
    (root / 'scope.json').write_text(json.dumps({'offline_only': True, 'hardware_connected': False, 'receiver_started': False, 'process_cases': len(traces), 'program_sha256': {str(p): sha(p.read_bytes()) for p in programs}, 'excluded_evidence_files': ['credential-private'], 'original_sha256': {str(p): sha(data) for p, data in originals.items()}}, indent=2) + '\n')
    print(f'PASS configuration publication: {len(traces)} process cases; real source/profile/include, immutable byte bindings, native inspection and ECC offline acceptance/rejection; evidence {root}')


if __name__ == '__main__':
    main()
