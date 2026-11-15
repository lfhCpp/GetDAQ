#!/usr/bin/env python3
"""Read-only verification of changed-plan evidence and the current delivery provenance."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
PHASE=ROOT/'build-data-plan-transition'
CASES={'success','ack-loss','held-lease','copy-proof','tamper-seal','tamper-payload-size',
       'changed-next-plan','retirement-cold','input-tamper-after-start',
       'audit-tamper-after-start','lease-replaced-after-start','archive-lock-after-start'}
HELD={'input-tamper-after-start','audit-tamper-after-start','lease-replaced-after-start'}


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def read(path):return json.loads(path.read_text())
def relative(path):return str(path.resolve().relative_to(ROOT))
def mapping(paths):return {relative(p):sha(p) for p in sorted(set(paths))}


def verify_map(values):
    for name,digest in values.items():
        path=Path(name);path=path if path.is_absolute() else ROOT/path
        assert path.is_file() and not path.is_symlink() and sha(path)==digest,'Changed artifact: '+str(path)
    return len(values)


def evidence(root,expected_sanitized=None):
    result=read(root/'result.json');cleanup=read(root/'cleanup.json')
    assert result['passed'] and not result['real_hardware'] and result['raw_remaining']==cleanup['raw_remaining']==0
    assert not list(root.rglob('*.graw')),'Owned raw remains'
    assert sum(item['bytes'] for item in cleanup['deleted'])<=2*1024**2
    assert result['maximum_aggregate_test_bytes']<=20*1024**3
    assert {item['case'] for item in result['cases']}==CASES and all(item['passed'] for item in result['cases'])
    if expected_sanitized is not None:
        assert result['sanitized']==result['leak_detection']==expected_sanitized
    assert result['runner_sha256']==sha(ROOT/'tests/run_data_plan_transition_tests.py')
    if result['raw_read_guard_enabled']:
        assert (root/'guard-selfcheck.log').read_text()=='raw-read-denied\n'
        guardlog=root/'supervisor-raw-reads.log';assert not guardlog.exists() or not guardlog.read_bytes()
    for item in result['cases']:
        case=root/item['case'];opfile=case/'retire-plan-result.json'
        if not opfile.exists():
            assert item['case']=='changed-next-plan' and not item['retirement_accepted'];continue
        operation=read(opfile);assert operation['phase']=='complete' and operation['command']=='retire-plan'
        directory=case/'deployment/control/operations/retire-plan'
        proof_file=directory/'plan-retirement.json';proof=read(proof_file);request=read(directory/'request.json')
        assert proof['proof_file']==str(proof_file) and json.loads(proof['request'])==request
        assert proof['request_key']==operation['request_key']
        assert sha(proof_file)==operation['plan_retirement']['proof_sha256']
        assert sha(directory/'cycle-evidence.json')==proof['cycle_evidence_sha256']
        assert (directory/'successor-plan.json').read_text()==proof['successor_plan']
        assert sha(directory/'successor-plan.json')==request['successor_plan_sha256']
        lifecycle=proof['lifecycle'];assert lifecycle['state']=='stopped' and lifecycle['session_id']==request['expected_session_id']
        assert lifecycle['receivers'] and all(r['reaped'] and r['exit_code']==0 and r['seal_verified'] for r in lifecycle['receivers'])
        rows=[json.loads(line) for line in (case/'deployment/control/operations.jsonl').read_text().splitlines()]
        accepted=[r for r in rows if r.get('event')=='accepted' and r.get('operation_id')=='retire-plan']
        complete=[r for r in rows if r.get('event')=='complete' and r.get('operation_id')=='retire-plan']
        assert len(accepted)==len(complete)==1 and json.loads(accepted[0]['request'])==request
        assert complete[0]['plan_retirement_proof_sha256']==sha(proof_file)
        assert json.loads(complete[0]['result'])==lifecycle
        status_file=case/'successor-final-status.json'
        if status_file.exists():
            status=read(status_file);marker=read(Path(str(proof_file)+'.activation.json'))
            assert marker['proof_sha256']==sha(proof_file) and marker['successor_plan_sha256']==request['successor_plan_sha256']
            assert status['instance_id']!=request['instance_id'] and status['plan_sha256']==request['successor_plan_sha256']
            assert status['controller_snapshot_sha256']==request['successor_controller_snapshot_sha256']
            assert status['controller_epoch']==request['successor_controller_epoch']
            assert status['control_needs_reconciliation']==(item['case'] in HELD)
            if item['case'] not in HELD:
                assert status['lifecycle']['state']=='stopped'
                arguments=read(case/'new-receiver-arguments.json')
                for words in arguments.values():
                    for flag in ['--queue-mib','--max-frame-mib','--rotate-mib']:assert words[words.index(flag)+1]=='2'
        if item['case']=='archive-lock-after-start':
            assert (case/'archived-original/0/.getdaq-receiver-seal.json').is_file()
            assert (case/'archived-original/1/.getdaq-receiver-seal.json').is_file()
            assert all(not Path(r['output_directory']).exists() for r in lifecycle['receivers'])
    return len(result['cases'])


def suite_module():
    spec=importlib.util.spec_from_file_location('suite',ROOT/'tools/build-suite.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module


def create(args):
    assert not (PHASE/'checkpoint.json').exists(),'Preserve an existing checkpoint'
    roots=[args.ordinary.resolve(),args.sanitized.resolve(),args.package_evidence.resolve()]
    for root,sanitized in zip(roots,[False,True,False]):evidence(root,sanitized)
    suite=suite_module();host=read(ROOT/'build-delivery/full/build-receipt.json')
    assert host['status']=='complete' and host['inputs']==suite.source_hashes()
    windows=read(ROOT/'build-windows-qt5142/windows-build-receipt.json');assert windows['status']=='complete'
    verify_map(windows['inputs'])
    sources=dict(host['inputs']);sources.update(windows['inputs'])
    # Include verification sources, docs and receipts, never credentials or payloads.
    extra=[Path(__file__),ROOT/'tests/run_data_plan_transition_tests.py',ROOT/'tests/deny_raw_payload_reads.c',
           ROOT/'tests/build_data_supervisor_validation.py',ROOT/'docs/DATA_PLAN_TRANSITION.md',
           ROOT/'docs/REFACTOR_STATUS.md',ROOT/'docs/VALIDATION.md',ROOT/'docs/DATA_CONTROL_HANDOFF.md']
    extra.extend([ROOT/'README.md',ROOT/'docs/ARCHITECTURE.md'])
    files=extra+[ROOT/'build-delivery/full/build-receipt.json',ROOT/'build-windows-qt5142/windows-build-receipt.json']
    for folder in ['ordinary','sanitized']:
        base=PHASE/folder;receipt=read(base/'build-receipt.json')
        files.extend(base/name for name in ['build-receipt.json','source-sha256.json','generated-sha256.json','verified-compile-commands.json'])
        inputs=read(base/'source-sha256.json');verify_map(inputs);sources.update(inputs)
        files.extend(base.rglob('getdaq-supervisor'))
        files.extend(base.rglob('getdaq-supervisor-query'))
        assert receipt['sanitized']==(folder=='sanitized')
    for root,base in zip(roots,[PHASE/'ordinary',PHASE/'sanitized',args.host_package.resolve()/'bin']):
        result=read(root/'result.json')
        for name,executable in [('service','getdaq-supervisor'),('query','getdaq-supervisor-query')]:
            assert result['program_sha256'][name]==sha(base/executable),'Evidence did not run the indexed program'
        receiver=ROOT/'build-source-retirement/runtime-baseline/getdaq-receiver'
        assert result['program_sha256']['receiver']==sha(receiver);files.append(receiver)
    for root in roots:
        files.extend(root.rglob('*.json'))
        files.extend(root.glob('*raw-reads.log'));files.extend(root.glob('guard-selfcheck.log'))
    regression=args.regression.resolve();data=read(regression/'result.json')
    assert data['passed'] and len(data['cases'])==9 and not list(regression.rglob('*.graw'))
    files.extend(regression.rglob('*.json'))
    for name in ['verified-ordinary-codec.log','verified-sanitized-codec.log']:
        path=PHASE/name;assert 'PASS strict plan/request scope' in path.read_text();files.append(path)
    packages=[]
    for path in [args.host_package.resolve(),args.windows_package.resolve()]:
        manifest=read(path/'manifest.json');assert manifest['status']=='complete'
        files.append(path/'manifest.json')
        for name,digest in manifest['files'].items():
            assert sha(path/name)==digest;files.append(path/name)
        packages.append(relative(path))
    checkpoint=dict(schema=1,phase='70.54',goal_complete=False,real_hardware=False,
        parent_ecc_different_plan_integrated=False,gui_different_plan_integrated=False,
        source_sha256=sources,artifact_sha256=mapping(files),evidence_roots=[relative(p) for p in roots],
        regression_root=relative(regression),packages=packages,scenarios=36,same_plan_regression_scenarios=9,
        linux_programs=len(host['programs']),windows_programs=len(windows['programs']))
    verify_map(checkpoint['source_sha256']);verify_map(checkpoint['artifact_sha256'])
    (PHASE/'checkpoint.json').write_text(json.dumps(checkpoint,ensure_ascii=False,indent=2)+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--index',action='store_true')
    for name in ['ordinary','sanitized','package-evidence','regression','host-package','windows-package']:parser.add_argument('--'+name,type=Path)
    args=parser.parse_args()
    if args.index:
        assert all(getattr(args,name.replace('-','_')) for name in ['ordinary','sanitized','package-evidence','regression','host-package','windows-package'])
        create(args)
    checkpoint=read(PHASE/'checkpoint.json');assert not checkpoint['goal_complete'] and not checkpoint['parent_ecc_different_plan_integrated']
    count=verify_map(checkpoint['source_sha256'])+verify_map(checkpoint['artifact_sha256'])
    for name,sanitized in zip(checkpoint['evidence_roots'],[False,True,False]):evidence(ROOT/name,sanitized)
    assert not list((ROOT/checkpoint['regression_root']).rglob('*.graw'))
    print('PASS changed-plan checkpoint:',count,'digests;',checkpoint['scenarios'],'changed-plan scenes; 9 same-plan regressions; goal incomplete')


if __name__=='__main__':main()
