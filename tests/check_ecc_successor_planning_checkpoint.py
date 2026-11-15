#!/usr/bin/env python3
"""Index or verify offline ECC planning evidence without executing any network operation."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
PHASE=ROOT/"build-ecc-successor-planning"


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path):return json.loads(path.read_text())


def verify_files(values,base=ROOT):
    for name,digest in values.items():
        path=Path(name);path=path if path.is_absolute() else base/path
        if not path.is_file() or sha(path)!=digest:raise RuntimeError("Recorded source/artifact changed: "+str(path))
    return len(values)


def collect():
    evidence={};results={};total=0
    service=ROOT/"build-delivery/full/native/getdaq-ecc-server"
    guard=PHASE/"offline-network-guard.so";runner=ROOT/"tests/run_ecc_successor_planning_tests.py"
    sanitized=PHASE/"sanitized/getdaq-ecc-server"
    for family in ["ordinary-encoding0-evidence","ordinary-encoding1-evidence","sanitized-encoding0-evidence","sanitized-encoding1-evidence"]:
        matches=list((PHASE/family).glob('*/result.json'));assert len(matches)==1
        path=matches[0];data=read(path);is_sanitized=family.startswith('sanitized')
        assert data['passed'] and data['raw_bytes']==0 and data['network_attempts']==0
        assert len(data['cases'])==7 and all(c['passed'] for c in data['cases'])
        assert data['sanitized']==is_sanitized
        assert data['program_sha256']==sha(sanitized if is_sanitized else service)
        assert data['runner_sha256']==sha(runner) and data['guard_sha256']==sha(guard)
        assert sha(Path(data['original_snapshot_file']))==data['original_snapshot_sha256']
        if is_sanitized:assert data['leak_detection'] is False
        total+=len(data['cases']);results[family]={'file':str(path.relative_to(ROOT)),'sha256':sha(path)}
        for file in path.parent.rglob('*'):
            if file.is_file():evidence[str(file.relative_to(ROOT))]=sha(file)
    assert total==28
    build=ROOT/'build-delivery/full/build-receipt.json';receipt=read(build)
    assert receipt['status']=='complete' and len(receipt['programs'])==23
    verify_files(receipt['inputs'])
    programs={}
    for value in receipt['programs'].values():
        path=Path(value['path']);assert sha(path)==value['sha256'];programs[str(path.relative_to(ROOT))]=sha(path)
    overlay=PHASE/'sanitized/build-receipt.json';san=read(overlay)
    assert san['compile_count']==20 and len(san['reused_objects'])==62 and len(san['dependencies'])==374
    verify_files(san['dependencies']);verify_files(san['tracked_inputs']);verify_files(san['reused_objects'])
    assert sha(sanitized)==san['program_sha256']
    for path in [sanitized,guard,ROOT/'build-ice-compat/getdaq-ecc-server',ROOT/'build-ice-compat/ecc-network-service-tests']:
        programs[str(path.relative_to(ROOT))]=sha(path)
    for path in [overlay,PHASE/'strict-syntax-results.json',PHASE/'linux-suite-build.log',PHASE/'network-fixture-build.log']:
        evidence[str(path.relative_to(ROOT))]=sha(path)
    assert all(row['exit_code']==0 and '-pedantic-errors' in row['command'] for row in read(PHASE/'strict-syntax-results.json'))
    package=ROOT/'build-packages/getdaq-host-20261015-ecc-successor-planning-candidate';manifest=read(package/'manifest.json')
    assert len(manifest['files'])==145;verify_files(manifest['files'],package)
    assert sha(package/'bin/getdaq-ecc-server')==sha(service)
    docs=['README.md','REWRITE_PROPOSAL.md','docs/SOURCE_REVIEW_AND_PLAN.md','docs/REFACTOR_STATUS.md','docs/VALIDATION.md','docs/ECC_CONFIGURATION_SELECTION.md','docs/ECC_SUCCESSOR_PLANNING.md']
    sources=['apps/ecc/native/nativeecccoordinator.h','apps/ecc/native/nativeecccoordinator.cpp','apps/ecc/service/main.cpp',
        'tests/run_ecc_successor_planning_tests.py','tests/offline_planning_network_guard.c','tests/build_controller_handoff_sanitizers.py','tests/check_ecc_successor_planning_checkpoint.py']
    assert not list(PHASE.rglob('*.graw'))
    return {'schema':1,'phase':'70.51 pure successor snapshot planning shared with actual startup','goal_complete':False,'offline_cases':28,
        'passed_results':results,'programs':programs,'production_receipt_sha256':sha(build),'production_inputs':receipt['inputs'],
        'sanitizer_receipt_sha256':sha(overlay),'sanitizer_compile_count':20,'sanitizer_reused_objects':62,'sanitizer_dependencies':374,
        'evidence':evidence,'sources':{name:sha(ROOT/name) for name in sources},'documentation':{name:sha(ROOT/name) for name in docs},
        'package':{'directory':str(package.relative_to(ROOT)),'manifest_sha256':sha(package/'manifest.json'),'files':manifest['files']},
        'raw_remaining':0,'network_attempts':0,'leak_detection_enabled':False,'old_libraries_and_system_qt_fully_instrumented':False,
        'live_network_startup_regression_executed':False,'windows_whole_source_receipt_refreshed_this_phase':False,
        'pending':['Live normal startup/workflow regression when automatic approval review is available',
          'Persistent successor epoch and complete ECC activation/data-plan switching','Continuous host board-reservation transfer and GUI',
          'Cold/partial recovery and target-platform/physical hardware/sustained throughput acceptance']}


def verify(path):
    data=read(path);assert data['schema']==1 and not data['goal_complete'] and data['offline_cases']==28
    count=0
    for key in ['programs','evidence','sources','documentation','production_inputs']:count+=verify_files(data[key])
    count+=verify_files({'build-delivery/full/build-receipt.json':data['production_receipt_sha256']})
    package=ROOT/data['package']['directory'];count+=verify_files({'manifest.json':data['package']['manifest_sha256']},package)
    count+=verify_files(data['package']['files'],package)
    assert not list(PHASE.rglob('*.graw'))
    print('PASS',count,'recorded digest checks; 28 offline planning cases; no raw; complete project and live startup remain unverified')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--write',action='store_true');args=parser.parse_args()
    file=PHASE/'checkpoint.json'
    if args.write:file.write_text(json.dumps(collect(),ensure_ascii=False,indent=2)+'\n')
    verify(file)
