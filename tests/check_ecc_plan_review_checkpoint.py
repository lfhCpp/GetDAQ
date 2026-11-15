#!/usr/bin/env python3
"""Bind the actual plan-transition window, native review lease and final packages."""
import argparse
import importlib.util
import json
from pathlib import Path
from check_ecc_plan_transition_checkpoint import read, sha, relative, hashes, verify_map, check_run

ROOT=Path(__file__).resolve().parents[1]
PHASE=ROOT/'build-ecc-plan-review'
SCENARIO='plan-transition-window-1'


def validate_run(entry):
    root=ROOT/entry['root']
    result=check_run(root,{SCENARIO},entry['sanitized'])
    assert result['program_sha256']==entry['program_sha256']
    for name,path in entry['programs'].items():
        assert sha(ROOT/path)==result['program_sha256'][name],name
    return result


def create(args):
    assert not (PHASE/'checkpoint.json').exists(),'Keep the original checkpoint immutable'
    spec=importlib.util.spec_from_file_location('suite',ROOT/'tools/build-suite.py')
    suite=importlib.util.module_from_spec(spec);spec.loader.exec_module(suite)
    linux=read(ROOT/'build-delivery/full/build-receipt.json')
    windows=read(ROOT/'build-windows-qt5142/windows-build-receipt.json')
    assert linux['status']==windows['status']=='complete'
    assert linux['inputs']==suite.source_hashes()
    verify_map(windows['inputs'])
    production=dict(linux['inputs']);production.update(windows['inputs'])
    files=[ROOT/'build-delivery/full/build-receipt.json',ROOT/'build-windows-qt5142/windows-build-receipt.json']
    for receipt in [linux,windows]:
        for program in receipt['programs'].values():
            p=Path(program['path']);assert sha(p)==program['sha256'];files.append(p)
    builds={}
    for name in ['sanitized-network','sanitized-service']:
        file=PHASE/name/'build-receipt.json';value=read(file);assert value['sanitized']
        for field in ['dependencies','tracked_inputs','reused_objects','compiled_objects']:verify_map(value[field])
        if value['reuse_overlay']:verify_map({value['reuse_overlay']['file']:value['reuse_overlay']['sha256']})
        files.append(file);builds[name]=dict(compiled=value['compile_count'],reused=len(value['reused_objects']))
    packages=[]
    for folder in [args.host_package,args.windows_package]:
        manifest=read(folder/'manifest.json');assert manifest['status']=='complete'
        assert not (folder/'BUILD_INCOMPLETE').exists()
        files.append(folder/'manifest.json')
        for name,digest in manifest['files'].items():
            assert sha(folder/name)==digest;files.append(folder/name)
        packages.append(relative(folder))
    probe=PHASE/'guarded-window-probe.py'
    programs=[
        dict(fixture=ROOT/'build-ice-compat/ecc-network-service-tests',ecc=ROOT/'build-ice-compat/getdaq-ecc-server',receiver=ROOT/'build-source-retirement/runtime-baseline/getdaq-receiver',data_service=ROOT/'build-data-plan-transition/ordinary/getdaq-supervisor',client_probe=probe),
        dict(fixture=PHASE/'sanitized-network/ecc-network-service-tests',ecc=PHASE/'sanitized-service/getdaq-ecc-server',receiver=ROOT/'build-source-retirement/runtime-baseline/getdaq-receiver',data_service=ROOT/'build-data-plan-transition/ordinary/getdaq-supervisor',client_probe=probe),
        dict(fixture=ROOT/'build-ice-compat/ecc-network-service-tests',ecc=args.host_package/'bin/getdaq-ecc-server',receiver=args.host_package/'bin/getdaq-receiver',data_service=args.host_package/'bin/getdaq-supervisor',client_probe=probe)]
    runs=[];deleted=0;deleted_bytes=0
    for root,sanitized,actual in zip([args.ordinary,args.sanitized,args.package],[False,True,False],programs):
        result=check_run(root,{SCENARIO},sanitized)
        entry=dict(root=relative(root),sanitized=sanitized,program_sha256=result['program_sha256'],programs={n:relative(p) for n,p in actual.items()})
        validate_run(entry);runs.append(entry)
        files.extend(root.rglob('*.json'));files.extend(root.rglob('*.jsonl'))
        files.append(root/SCENARIO/'gui-raw-reads.log');files.extend(actual.values())
        gui=read(root/SCENARIO/'gui-programs.json')
        files.extend([Path(gui['probe']),Path(gui['guard'])])
        cleanup=read(root/'cleanup.json');deleted+=len(cleanup['deleted']);deleted_bytes+=sum(x['bytes'] for x in cleanup['deleted'])
    offline=args.offline;value=read(offline/'result.json')
    assert value['passed'] and len(value['cases'])==23 and value['network_attempts']==value['raw_remaining']==0
    assert value['runner_sha256']==sha(ROOT/'tests/run_ecc_successor_identity_tests.py')
    assert value['program_sha256']['service']==sha(ROOT/'build-ice-compat/getdaq-ecc-server')
    assert value['program_sha256']['fixture']==sha(ROOT/'build-ice-compat/ecc-successor-identity-tests')
    assert value['guard_sha256']==sha(ROOT/'build-ecc-successor-planning/offline-network-guard.so')
    files.extend([offline/'result.json',ROOT/'build-ice-compat/ecc-successor-identity-tests',ROOT/'build-ecc-successor-planning/offline-network-guard.so'])
    tests={'client-current-tests.log':55,'widget-current-tests.log':91}
    for name,count in tests.items():
        assert 'Totals: '+str(count)+' passed, 0 failed' in (PHASE/name).read_text();files.append(PHASE/name)
    files.extend([ROOT/'build-source-retirement/client-tests/ecc-system-client-tests',ROOT/'build-source-retirement/widget-tests/getdaq-tests'])
    codec=PHASE/'actual-records.log';assert 'PASS portable C++11 ECC codec:' in codec.read_text() and 'actual records 0;' not in codec.read_text()
    files.extend([codec,PHASE/'current-codec.log',ROOT/'build-ice-compat/data-supervisor/control-protocol/ecc-protocol-tests'])
    files.extend((ROOT/'tests').glob('*plan*transition*'))
    files.extend(ROOT/'tests'/name for name in ['check_ecc_plan_review_checkpoint.py','check_ecc_plan_transition_checkpoint.py','check_ecc_service_handoff_checkpoint.py','check_controller_handoff_evidence.py','run_description_recovery_tests.py','run_ecc_successor_identity_tests.py','build_controller_handoff_sanitizers.py','ecc_protocol_test.cpp','ecc_network_service_test.cpp','native_remote_service_fixture.h','test_ecc_system_client.cpp','test_getdaq.cpp'])
    files.extend([ROOT/'README.md',*[(ROOT/'docs'/name) for name in ['ECC_PLAN_TRANSITION_GUI.md','ECC_PLAN_TRANSITION.md','REFACTOR_STATUS.md','ARCHITECTURE.md','VALIDATION.md']]])
    files.extend(PHASE/name for name in ['linux-final-build.log','windows-final-build.log','windows-deploy.log','sanitized-network-build.log','sanitized-service-build.log'])
    value=dict(schema=1,phase='70.56',goal_complete=False,real_hardware=False,gui_plan_transition_integrated=True,
        continuous_board_reservation_transfer=False,partial_or_cold_transition_recovery=False,
        production_inputs=production,artifacts=hashes(files),runs=runs,packages=packages,sanitizer_builds=builds,
        qt_client_tests=55,qt_widget_tests=91,offline_identity_cases=23,network_scenarios=3,
        deleted_raw_files=deleted,deleted_raw_bytes=deleted_bytes,maximum_concurrent_test_bytes=2*512*1024**2,
        aggregate_user_limit_bytes=20*1024**3,linux_programs=len(linux['programs']),windows_programs=len(windows['programs']))
    verify_map(production);verify_map(value['artifacts']);assert not list(PHASE.rglob('*.graw'))
    (PHASE/'checkpoint.json').write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--index',action='store_true')
    for name in ['ordinary','sanitized','package','offline','host-package','windows-package']:parser.add_argument('--'+name,type=Path)
    args=parser.parse_args()
    if args.index:
        assert all(getattr(args,name.replace('-','_')) for name in ['ordinary','sanitized','package','offline','host-package','windows-package'])
        for name,value in vars(args).items():
            if isinstance(value,Path):setattr(args,name,value.resolve(strict=True))
        create(args)
    value=read(PHASE/'checkpoint.json')
    assert not value['goal_complete'] and value['gui_plan_transition_integrated'] and value['network_scenarios']==3
    count=verify_map(value['production_inputs'])+verify_map(value['artifacts'])
    for run in value['runs']:validate_run(run)
    assert not list(PHASE.rglob('*.graw'))
    print('PASS ECC plan review checkpoint:',count,'digests; 3 actual MainWindow cycles; whole goal incomplete')


if __name__=='__main__':main()
