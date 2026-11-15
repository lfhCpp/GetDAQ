#!/usr/bin/env python3
"""Bind final ECC plan transition runs, builds, runtime packages and their scope."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
from check_ecc_plan_transition_evidence import check
from check_ecc_service_handoff_checkpoint import handoff

ROOT=Path(__file__).resolve().parents[1]
PHASE=ROOT/'build-ecc-plan-transition'


def read(path):return json.loads(path.read_text())
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def relative(path):return str(path.resolve().relative_to(ROOT))
def hashes(paths):return {relative(path):sha(path) for path in sorted(set(paths))}


def verify_map(values):
    for name,digest in values.items():
        path=Path(name);path=path if path.is_absolute() else ROOT/path
        assert path.is_file() and not path.is_symlink() and sha(path)==digest,'Changed input: '+str(path)
    return len(values)


def check_run(root,expected,sanitized):
    result=read(root/'result.json');cleanup=read(root/'cleanup.json')
    assert result['passed'] and not result['real_hardware'] and result['sanitized']==result['leak_detection']==sanitized
    assert result['runner_sha256']==sha(ROOT/'tests/run_description_recovery_tests.py')
    assert result['raw_remaining']==0 and not cleanup['remaining_raw'] and not list(root.rglob('*.graw'))
    assert {item['scenario'] for item in result['runs']}==set(expected)
    assert sum(item['bytes'] for item in cleanup['deleted'])<=result['maximum_aggregate_test_bytes']<=512*1024**2
    for name in expected:
        if name.startswith('plan-transition-'):check(root/name)
        else:handoff(root/name,require_lost_reply=False)
    return result


def create(args):
    assert not (PHASE/'checkpoint.json').exists(),'Preserve the original checkpoint'
    ordinary=args.ordinary.resolve();sanitized=args.sanitized.resolve();package=args.package.resolve()
    spec=importlib.util.spec_from_file_location('suite',ROOT/'tools/build-suite.py');suite=importlib.util.module_from_spec(spec);spec.loader.exec_module(suite)
    linux=read(ROOT/'build-delivery/full/build-receipt.json');windows=read(ROOT/'build-windows-qt5142/windows-build-receipt.json')
    assert linux['status']==windows['status']=='complete' and linux['inputs']==suite.source_hashes()
    verify_map(windows['inputs']);production=dict(linux['inputs']);production.update(windows['inputs'])
    files=[ROOT/'build-delivery/full/build-receipt.json',ROOT/'build-windows-qt5142/windows-build-receipt.json']
    for entry in linux['programs'].values():assert sha(Path(entry['path']))==entry['sha256']
    builds={}
    for name in ['sanitized-network','sanitized-service','sanitized-tool']:
        file=PHASE/name/'build-receipt.json';value=read(file);assert value['sanitized']
        for field in ['dependencies','tracked_inputs','reused_objects','compiled_objects']:verify_map(value[field])
        if value['reuse_overlay']:verify_map({value['reuse_overlay']['file']:value['reuse_overlay']['sha256']})
        files.append(file);builds[name]=dict(compiled=value['compile_count'],reused=len(value['reused_objects']),dependencies=len(value['dependencies']))
    new={'plan-transition-0','plan-transition-1','plan-transition-partial-0'}
    runs=[]
    for root,expected,is_sanitized in [(ordinary,new|{'handoff-service-0','handoff-service-1'},False),(sanitized,new,True),(package,{'plan-transition-1'},False)]:
        result=check_run(root,expected,is_sanitized)
        files.extend(root.rglob('*.json'));files.extend(root.rglob('*.jsonl'))
        runs.append(dict(root=relative(root),scenarios=sorted(expected),sanitized=is_sanitized,program_sha256=result['program_sha256']))
    actual=[
        dict(fixture=ROOT/'build-ice-compat/ecc-network-service-tests',ecc=ROOT/'build-ice-compat/getdaq-ecc-server',handoff_tool=ROOT/'build-ice-compat/getdaq-ecc-handoff',data_service=ROOT/'build-data-plan-transition/ordinary/getdaq-supervisor',receiver=ROOT/'build-source-retirement/runtime-baseline/getdaq-receiver'),
        dict(fixture=PHASE/'sanitized-network/ecc-network-service-tests',ecc=PHASE/'sanitized-service/getdaq-ecc-server',handoff_tool=PHASE/'sanitized-tool/getdaq-ecc-handoff',data_service=ROOT/'build-data-plan-transition/ordinary/getdaq-supervisor',receiver=ROOT/'build-source-retirement/runtime-baseline/getdaq-receiver'),
        dict(fixture=ROOT/'build-ice-compat/ecc-network-service-tests',ecc=args.host_package.resolve()/'bin/getdaq-ecc-server',handoff_tool=args.host_package.resolve()/'bin/getdaq-ecc-handoff',data_service=args.host_package.resolve()/'bin/getdaq-supervisor',receiver=args.host_package.resolve()/'bin/getdaq-receiver')]
    for run,programs in zip(runs,actual):
        for name,path in programs.items():assert sha(path)==run['program_sha256'][name];files.append(path)
    for name in ['portable-codec.log','portable-actual-records.log','handoff-dependencies.txt']:
        files.append(PHASE/name)
    files.append(ROOT/'build-ice-compat/data-supervisor/control-protocol/ecc-protocol-tests')
    assert 'PASS portable C++11 ECC codec' in (PHASE/'portable-actual-records.log').read_text()
    assert 'Qt' not in (PHASE/'handoff-dependencies.txt').read_text()
    packages=[]
    for folder in [args.host_package.resolve(),args.windows_package.resolve()]:
        manifest=read(folder/'manifest.json');assert manifest['status']=='complete'
        files.append(folder/'manifest.json')
        for name,digest in manifest['files'].items():assert sha(folder/name)==digest;files.append(folder/name)
        packages.append(relative(folder))
    for name in ['check_ecc_plan_transition_checkpoint.py','check_ecc_plan_transition_evidence.py','check_ecc_service_handoff_checkpoint.py','check_controller_handoff_evidence.py','run_description_recovery_tests.py','build_controller_handoff_sanitizers.py','ecc_protocol_test.cpp','ecc_network_service_test.cpp','native_remote_service_fixture.h']:
        files.append(ROOT/'tests'/name)
    for name in ['ECC_PLAN_TRANSITION.md','ECC_SERVICE_HANDOFF.md','DATA_PLAN_TRANSITION.md','ARCHITECTURE.md','REFACTOR_STATUS.md','VALIDATION.md']:files.append(ROOT/'docs'/name)
    files.append(ROOT/'README.md')
    checkpoint=dict(schema=1,phase='70.55',goal_complete=False,real_hardware=False,gui_plan_transition_integrated=False,
        continuous_board_reservation_transfer=False,partial_or_cold_transition_recovery=False,
        production_inputs=production,artifacts=hashes(files),runs=runs,packages=packages,sanitizer_builds=builds,
        network_scenarios=9,new_configuration_cycles=5,partial_retirement_holds=2,same_plan_regressions=2,
        maximum_concurrent_test_bytes=4*512*1024**2,aggregate_user_limit_bytes=20*1024**3,
        linux_programs=len(linux['programs']),windows_programs=len(windows['programs']))
    verify_map(production);verify_map(checkpoint['artifacts'])
    (PHASE/'checkpoint.json').write_text(json.dumps(checkpoint,ensure_ascii=False,indent=2)+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--index',action='store_true')
    for name in ['ordinary','sanitized','package','host-package','windows-package']:parser.add_argument('--'+name,type=Path)
    args=parser.parse_args()
    if args.index:
        assert all(getattr(args,name.replace('-','_')) for name in ['ordinary','sanitized','package','host-package','windows-package']);create(args)
    value=read(PHASE/'checkpoint.json');assert not value['goal_complete'] and value['network_scenarios']==9
    count=verify_map(value['production_inputs'])+verify_map(value['artifacts'])
    for run in value['runs']:check_run(ROOT/run['root'],run['scenarios'],run['sanitized'])
    assert not list(PHASE.rglob('*.graw'))
    print('PASS ECC plan transition checkpoint:',count,'digests; 5 new-configuration cycles, 2 partial holds, 2 same-plan regressions; whole goal incomplete')


if __name__=='__main__':main()
