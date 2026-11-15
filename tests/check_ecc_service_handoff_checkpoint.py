#!/usr/bin/env python3
"""Verify original ECC handoff, independent startup and build provenance without network calls."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
from check_controller_handoff_evidence import check_node

ROOT=Path(__file__).resolve().parents[1]
PHASE=ROOT/'build-ecc-service-handoff'


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def read(path):return json.loads(path.read_text())
def hashes(files):return {str(p.relative_to(ROOT)):sha(p) for p in files}


def verify_map(values,base=ROOT):
    for name,digest in values.items():
        path=Path(name);path=path if path.is_absolute() else base/path
        assert path.is_file() and not path.is_symlink() and sha(path)==digest,'Changed artifact: '+str(path)
    return len(values)


def handoff(case,require_lost_reply=True):
    proof=read(case/'service-handoff-result.json')
    assert proof['passed'] and proof['two_independent_ecc_processes'] and proof['cold_original_fenced']
    assert proof['consumed_plan_restart_refused'] and proof['handoff_hardware_writes']==0
    plan_file=case/'successor-inputs/successor.json';plan=read(plan_file)
    marker=read(Path(str(plan_file)+'.activation.json'));op=read(case/'handoff-operation.json');request=op['request']
    old=read(case/'original-fenced-status.json');new=read(case/'successor-startup-status.json')
    receipt=read(case/'successor-inputs/old-handoff.json')
    canonical=json.dumps(request,ensure_ascii=False,separators=(',',':'))+'\n'
    assert op['phase']=='complete' and request['command']=='handoff-data-control'
    assert op['request_key']==hashlib.sha256(b'getdaq.ecc-control-request:1\n'+canonical.encode()).hexdigest()
    assert request['successor_plan_sha256']==sha(plan_file)==marker['plan_sha256']
    assert request['successor_scope_sha256']==plan['data_controller_scope_sha256']==receipt['successor_scope_sha256']==new['data']['controller_scope_sha256']
    assert request['successor_epoch']==plan['receiver_controller_epoch']==receipt['successor_epoch']==new['data']['controller_epoch']
    assert request['successor_data_binding_sha256']==plan['data_lifecycle_binding_sha256']
    assert old['instance_id']==request['instance_id'] and old['instance_id']!=new['instance_id']
    assert new['snapshot_sha256']==plan['snapshot_sha256']
    assert receipt['original_scope_sha256']==old['data']['controller_scope_sha256'] and receipt['original_epoch']==old['data']['controller_epoch']
    assert old['control_needs_reconciliation'] and not new['control_needs_reconciliation'] and not new['data']['needs_reconciliation']
    rows=[json.loads(line) for line in (case/'controller/network/operations.jsonl').read_text().splitlines()]
    accepted=[row for row in rows if row['event']=='accepted' and json.loads(row['request'])['command']=='handoff-data-control']
    assert len(accepted)==1 and accepted[0]['request']==canonical and accepted[0]['request_key']==op['request_key']
    assert len([row for row in rows if row['event']=='opened'])==2,'Original cold process was not checked'
    assert len(new['data']['nodes'])==2
    for node in new['data']['nodes']:
        life=node['lifecycle'];assert life['state']=='stopped' and life['receivers']
        assert all(r['reaped'] and r['seal_verified'] and r['exit_code']==0 for r in life['receivers'])
    assert len(receipt['nodes'])==2
    for node in receipt['nodes']:
        directory=case/node['agent_id']/'deployment/control';assert check_node(directory)==1
        assert sha(directory/'operations'/node['operation_id']/'handoff-proof.json')==node['proof_sha256']
    if proof['operator_cli']:
        tool=case/'successor-inputs/tool-evidence'
        assert (tool/'request.json').read_text()==canonical and (tool/'receipt.json').read_bytes()==(case/'successor-inputs/old-handoff.json').read_bytes()
        if require_lost_reply:assert read(tool/'submission-held.json')['post_attempted_once']


def collect():
    evidence={};programs={};results={}
    for family in ['locked-final-ordinary','locked-final-sanitized']:
        found=list((PHASE/family).glob('*/result.json'));assert len(found)==1
        file=found[0];result=read(file);assert result['passed'] and len(result['runs'])==2 and result['raw_remaining']==0
        assert result['sanitized']==family.endswith('sanitized') and result['leak_detection']==result['sanitized']
        assert result['runner_sha256']==sha(ROOT/'tests/run_description_recovery_tests.py')
        assert (file.parent/'handoff-reply-events.log').read_bytes()==b'post\ndrop\n'
        locks=read(file.parent/'handoff-reply-events.log.locks.json');assert all(locks.values()) and len(locks)==2
        assert read(file.parent/'cleanup.json')['remaining_raw']==[]
        for number in (0,1):handoff(file.parent/('handoff-service-'+str(number)))
        expected={
            'fixture':ROOT/('build-ecc-service-handoff/sanitized-network/ecc-network-service-tests' if result['sanitized'] else 'build-ice-compat/ecc-network-service-tests'),
            'ecc':ROOT/('build-ecc-service-handoff/sanitized-service/getdaq-ecc-server' if result['sanitized'] else 'build-delivery/full/native/getdaq-ecc-server'),
            'receiver':ROOT/'build-source-retirement/runtime-baseline/getdaq-receiver',
            'data_service':ROOT/'build-delivery/full/native/getdaq-supervisor',
            'handoff_tool':ROOT/('build-ecc-service-handoff/sanitized-tool-final/getdaq-ecc-handoff' if result['sanitized'] else 'build-delivery/full/native/getdaq-ecc-handoff'),
            'handoff_reply_guard':PHASE/'handoff-reply-guard.so','handoff_tool_wrapper':file.parent/'handoff-tool-wrapper.py'}
        assert result['program_sha256']=={name:sha(path) for name,path in expected.items()}
        programs.update(hashes(expected.values()))
        evidence.update(hashes([p for p in file.parent.rglob('*') if p.is_file() and not p.is_symlink() and p.name not in ('credential','ecc-credential')]))
        results[family]={'file':str(file.relative_to(ROOT)),'sha256':sha(file)}
    inputs={};builds={}
    spec=importlib.util.spec_from_file_location('suite',ROOT/'tools/build-suite.py');suite=importlib.util.module_from_spec(spec);spec.loader.exec_module(suite)
    for family,file,count in [('linux',ROOT/'build-delivery/full/build-receipt.json',24),('windows',ROOT/'build-windows-qt5142/windows-build-receipt.json',6)]:
        value=read(file);assert value['status']=='complete' and len(value['programs'])==count
        verify_map(value['inputs']);inputs.update(value['inputs'])
        if family=='linux':assert value['inputs']==suite.source_hashes()
        for entry in value['programs'].values():assert sha(Path(entry['path']))==entry['sha256'];programs.update(hashes([Path(entry['path'])]))
        builds[family]={'file':str(file.relative_to(ROOT)),'sha256':sha(file),'programs':count,'inputs':len(value['inputs'])}
    overlays={}
    for name in ['sanitized-network','sanitized-service','sanitized-tool-final']:
        file=PHASE/name/'build-receipt.json';value=read(file)
        for field in ['dependencies','tracked_inputs','reused_objects','compiled_objects']:verify_map(value[field])
        if value['reuse_overlay']:verify_map({value['reuse_overlay']['file']:value['reuse_overlay']['sha256']})
        evidence.update(hashes([file]));overlays[name]={'file':str(file.relative_to(ROOT)),'sha256':sha(file),'compiled':value['compile_count'],'reused':len(value['reused_objects']),'dependencies':len(value['dependencies'])}
    client=ROOT/'build-source-retirement/client-tests/ecc-system-client-tests';programs.update(hashes([client]))
    assert 'Totals: 52 passed, 0 failed' in (PHASE/'client-tests.log').read_text()
    assert 'PASS portable C++11 ECC codec' in (PHASE/'protocol-tests.log').read_text()
    assert 'actual records 140;' in (PHASE/'protocol-actual-records.log').read_text()
    assert 'Qt' not in (PHASE/'handoff-tool-dependencies.txt').read_text()
    programs.update(hashes([ROOT/'build-ice-compat/data-supervisor/control-protocol/ecc-protocol-tests']))
    evidence.update(hashes([PHASE/name for name in ['client-tests.log','client-build.log','protocol-tests.log','protocol-actual-records.log','handoff-tool-dependencies.txt','linux-suite-final-build.log','windows-final-build.log','windows-runtime-deploy.log','sanitized-network-build.log','sanitized-service-build.log','sanitized-tool-final-build.log']]))
    packages={}
    for name in ['getdaq-host-20261015-ecc-service-handoff-candidate','getdaq-windows-qt5142-20261015-ecc-service-handoff-candidate']:
        directory=ROOT/'build-packages'/name;manifest=read(directory/'manifest.json');verify_map(manifest['files'],directory)
        if 'windows' in name:assert manifest['native_windows_platform_smoke'] and manifest['sdk_path_removed_from_smoke_environment']
        packages[name]={'directory':str(directory.relative_to(ROOT)),'manifest_sha256':sha(directory/'manifest.json'),'files':manifest['files']}
    sources=['common/control/eccprotocol.h','common/control/eccprotocol.cpp','apps/ecc/native/ecccontroljobs.h','apps/ecc/native/ecccontroljobs.cpp',
        'apps/ecc/native/CMakeLists.txt','apps/ecc/service/eccservice.cpp','apps/ecc/handoff/main.cpp','apps/datasupervisor/controlhttp.h','apps/datasupervisor/controlhttp.cpp',
        'tools/build-suite.py','tests/ecc_network_service_test.cpp','tests/ecc_protocol_test.cpp','tests/run_description_recovery_tests.py','tests/build_controller_handoff_sanitizers.py',
        'tests/ecc_handoff_reply_guard.c','tests/ecc_handoff_reply_wrapper.py','tests/check_controller_handoff_evidence.py','tests/test_ecc_system_client.cpp',str(Path(__file__).relative_to(ROOT))]
    docs=['README.md','docs/ECC_SERVICE_HANDOFF.md','docs/ECC_SUCCESSOR_IDENTITY.md','docs/ECC_CONFIGURATION_SELECTION.md',
        'docs/DATA_CONTROL_HANDOFF.md','docs/ARCHITECTURE.md','docs/HOST_SUITE_BUILD.md','docs/REFACTOR_STATUS.md','docs/VALIDATION.md']
    assert not list(PHASE.rglob('*.graw'))
    return dict(schema=1,phase='70.53 original ECC service handoff and independent successor startup',goal_complete=False,
        network_scenarios=4,client_tests=52,portable_codec_passed=True,portable_actual_records=140,passed_results=results,programs=programs,production_inputs=inputs,
        builds=builds,sanitizer_builds=overlays,evidence=evidence,sources=hashes([ROOT/p for p in sources]),documentation=hashes([ROOT/p for p in docs]),packages=packages,
        raw_remaining=0,maximum_concurrent_budget_bytes=1024**3,aggregate_user_limit_bytes=20*1024**3,
        real_hardware=False,data_services_receivers_old_libraries_and_system_qt_fully_instrumented=False,
        independent_target_platforms_accepted=False,windows_fresh_native_initial_window_smoke=True,
        preliminary_results_not_counted=True,remaining=['Changed experiment/receiver plan activation','Continuous host board reservation transfer and GUI',
            'Partial/cold handoff recovery','Ubuntu24/macOS/independent Windows, physical hardware and sustained cross-host throughput acceptance'])


def verify(file):
    value=read(file);assert not value['goal_complete'] and value['network_scenarios']==4
    count=0
    for field in ['programs','production_inputs','evidence','sources','documentation']:count+=verify_map(value[field])
    for entry in list(value['builds'].values())+list(value['sanitizer_builds'].values()):count+=verify_map({entry['file']:entry['sha256']})
    for entry in value['packages'].values():
        directory=ROOT/entry['directory'];count+=verify_map({'manifest.json':entry['manifest_sha256']},directory);count+=verify_map(entry['files'],directory)
    for entry in value['passed_results'].values():
        result=ROOT/entry['file'];count+=verify_map({entry['file']:entry['sha256']});assert read(result)['passed']
        for number in (0,1):handoff(result.parent/('handoff-service-'+str(number)))
    assert not list(PHASE.rglob('*.graw'))
    print('PASS',count,'digest checks; four independent ECC process handoff scenarios; 52 Qt client tests; no owned raw; full project incomplete')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--write',action='store_true');args=parser.parse_args()
    file=PHASE/'checkpoint.json'
    if args.write:file.write_text(json.dumps(collect(),ensure_ascii=False,indent=2)+'\n')
    verify(file)
