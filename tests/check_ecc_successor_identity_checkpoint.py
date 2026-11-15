#!/usr/bin/env python3
"""Bind successor identity evidence to actual inputs/programs; never runs network operations."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
PHASE=ROOT/'build-ecc-successor-identity'


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def read(path):return json.loads(path.read_text())


def hashes(files):
    return {str(p.relative_to(ROOT)):sha(p) for p in files}


def verify_map(values,base=ROOT):
    for name,digest in values.items():
        path=Path(name);path=path if path.is_absolute() else base/path
        assert path.is_file() and not path.is_symlink() and sha(path)==digest,'Changed input/artifact: '+str(path)
    return len(values)


def collect():
    results={};evidence={};offline=0;network=0
    offline_families=['final-ordinary-encoding0','final-ordinary-encoding1','sanitized-encoding0','sanitized-encoding1','delivery-ordinary','planning-regression']
    network_families=['final-network-evidence-v2','sanitized-network-evidence','normal-service-regression']
    for family in offline_families+network_families:
        found=list((PHASE/family).glob('*/result.json'));assert len(found)==1,(family,found)
        path=found[0];data=read(path);assert data['passed']
        count=len(data.get('cases',data.get('runs',[])))
        assert data.get('raw_remaining',data.get('raw_bytes',0))==0
        if family in offline_families:
            assert data['network_attempts']==0
            assert count==(7 if family=='planning-regression' else 19);offline+=count
        else:
            assert count==(2 if family=='normal-service-regression' else 6);network+=count
            assert data['maximum_aggregate_test_bytes']<=512*1024**2
            if family=='sanitized-network-evidence':assert data['sanitized'] and data['leak_detection']
        results[family]={'file':str(path.relative_to(ROOT)),'sha256':sha(path),'cases':count}
        # A local evidence index, never a deployment copy. Exclude credentials.
        files=[p for p in path.parent.rglob('*') if p.is_file() and not p.is_symlink() and
               p.name not in ('credential','ecc-credential')]
        evidence.update(hashes(files))
    assert offline==102 and network==14
    spec=importlib.util.spec_from_file_location('suite',ROOT/'tools/build-suite.py')
    suite=importlib.util.module_from_spec(spec);spec.loader.exec_module(suite)
    linux=ROOT/'build-delivery/full/build-receipt.json';windows=ROOT/'build-windows-qt5142/windows-build-receipt.json'
    builds={};programs={};inputs={}
    for label,path,count in [('linux',linux,23),('windows',windows,6)]:
        receipt=read(path);assert receipt['status']=='complete' and len(receipt['programs'])==count
        verify_map(receipt['inputs']);inputs.update(receipt['inputs'])
        if label=='linux':assert receipt['inputs']==suite.source_hashes()
        for entry in receipt['programs'].values():
            p=Path(entry['path']);assert sha(p)==entry['sha256'];programs[str(p.relative_to(ROOT))]=sha(p)
        builds[label]={'file':str(path.relative_to(ROOT)),'sha256':sha(path),'inputs':len(receipt['inputs']),'programs':count}
    overlays={}
    for name in ['sanitized-service','sanitized-fixture','sanitized-network-fixture']:
        path=PHASE/name/'build-receipt.json';receipt=read(path)
        for field in ['dependencies','tracked_inputs','reused_objects']:verify_map(receipt[field])
        executable={'sanitized-service':'getdaq-ecc-server','sanitized-fixture':'ecc-successor-identity-tests','sanitized-network-fixture':'native-ecc-remote-receiver-tests'}[name]
        program=path.parent/executable;assert sha(program)==receipt['program_sha256'];programs.update(hashes([program]))
        overlays[name]={'file':str(path.relative_to(ROOT)),'sha256':sha(path),'compile_count':receipt['compile_count'],
            'reused_objects':len(receipt['reused_objects']),'dependencies':len(receipt['dependencies'])}
        evidence.update(hashes([path]))
    programs.update(hashes([ROOT/'build-ice-compat/getdaq-ecc-server',ROOT/'build-ice-compat/ecc-successor-identity-tests',
        ROOT/'build-ice-compat/native-ecc-remote-receiver-tests',ROOT/'build-ice-compat/ecc-network-service-tests',
        ROOT/'build-source-retirement/runtime-baseline/getdaq-receiver',ROOT/'build-ecc-successor-planning/offline-network-guard.so']))
    known_programs=set(programs.values())
    for entry in results.values():
        result=read(ROOT/entry['file']);record=result['program_sha256']
        for digest in record.values() if isinstance(record,dict) else [record]:assert digest in known_programs
    packages={}
    for name in ['getdaq-host-20261015-ecc-successor-identity-candidate','getdaq-windows-qt5142-20261015-ecc-successor-identity-candidate']:
        directory=ROOT/'build-packages'/name;manifest=read(directory/'manifest.json');verify_map(manifest['files'],directory)
        packages[name]={'directory':str(directory.relative_to(ROOT)),'manifest_sha256':sha(directory/'manifest.json'),'files':manifest['files']}
    sources=['apps/ecc/native/eccsuccessorplan.h','apps/ecc/native/eccsuccessorplan.cpp','apps/ecc/native/CMakeLists.txt',
        'apps/ecc/native/remotereceivermanager.h','apps/ecc/native/remotereceivermanager.cpp',
        'apps/ecc/native/ecccontroljobs.h','apps/ecc/native/ecccontroljobs.cpp','apps/ecc/service/eccservice.h',
        'apps/ecc/service/eccservice.cpp','apps/ecc/service/main.cpp','tests/ecc_successor_identity_test.cpp',
        'tests/run_ecc_successor_identity_tests.py','tests/native_ecc_remote_receiver_test.cpp',
        'tests/run_remote_controller_handoff_tests.py','tests/build_controller_handoff_sanitizers.py',str(Path(__file__).relative_to(ROOT))]
    docs=['README.md','docs/ECC_SUCCESSOR_IDENTITY.md','docs/ECC_SUCCESSOR_PLANNING.md','docs/REFACTOR_STATUS.md','docs/VALIDATION.md']
    assert not list(PHASE.rglob('*.graw'))
    return dict(schema=1,phase='70.52 persistent successor identity and startup binding',goal_complete=False,
        offline_cases=offline,network_cases=network,passed_results=results,programs=programs,production_inputs=inputs,
        builds=builds,sanitizer_builds=overlays,evidence=evidence,sources=hashes([ROOT/p for p in sources]),
        documentation=hashes([ROOT/p for p in docs]),packages=packages,raw_remaining=0,
        aggregate_test_budget_bytes=514*1024**2,aggregate_user_limit_bytes=20*1024**3,
        offline_sanitizer_leak_detection=False,network_sanitizer_leak_detection=True,
        data_services_receivers_old_libraries_and_system_qt_fully_instrumented=False,
        planned_jobs_network_startup_verified=True,planned_service_executable_startup_verified=False,
        windows_smoke_reused_from_identical_programs_and_dlls=True,
        remaining=['Planned ECC executable startup end-to-end and original ECC service handoff endpoint',
            'Continuous host board reservation transfer, changed receiver plans and GUI',
            'Cold/partial recovery; target platforms, physical hardware and sustained cross-host throughput acceptance'])


def verify(path):
    data=read(path);assert data['schema']==1 and not data['goal_complete']
    count=0
    for field in ['programs','production_inputs','evidence','sources','documentation']:count+=verify_map(data[field])
    for entry in list(data['builds'].values())+list(data['sanitizer_builds'].values()):count+=verify_map({entry['file']:entry['sha256']})
    for entry in data['packages'].values():
        directory=ROOT/entry['directory'];count+=verify_map({'manifest.json':entry['manifest_sha256']},directory)
        count+=verify_map(entry['files'],directory)
    for entry in data['passed_results'].values():
        count+=verify_map({entry['file']:entry['sha256']});assert read(ROOT/entry['file'])['passed']
    assert not list(PHASE.rglob('*.graw'))
    print('PASS',count,'digest checks;',data['offline_cases'],'offline,',data['network_cases'],'network cases; no owned raw; full project incomplete')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--write',action='store_true');args=parser.parse_args()
    path=PHASE/'checkpoint.json'
    if args.write:path.write_text(json.dumps(collect(),ensure_ascii=False,indent=2)+'\n')
    verify(path)
