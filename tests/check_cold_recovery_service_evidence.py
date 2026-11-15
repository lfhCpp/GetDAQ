#!/usr/bin/env python3
"""Independently bind recovery tasks/windows/crash evidence to exact original children and the new sealed ECC cycle."""
import argparse,json
from pathlib import Path
from check_partial_handoff_completion_evidence import read,rows,sha,digest,completion
from check_cold_handoff_evidence import context

def check(folder):
    result=read(folder/'result.json');assert result['passed'] and result['scope']=='network' and not result['real_hardware']
    assert not result['native_data_lifecycle_test_double'] and result['raw_remaining']==0 and not list(folder.rglob('*.graw'))
    assert {'fixture','recovery_service','recovery_window','handoff_tool'}<=set(result['program_sha256'])
    cleanup=read(folder/'cleanup.json');assert cleanup['deleted'] and not cleanup['remaining_raw']
    assert sum(x['bytes'] for x in cleanup['deleted'])<=result['maximum_aggregate_test_bytes']<=20*1024**3
    cases=[]
    for run in result['runs']:
        assert run['exit_code']==0
        name=run['scenario'];assert name.startswith(('plan-transition-cold-complete-','cold-handoff-service-'))
        case=folder/name;inputs=case/'successor-inputs';tasks=inputs/'recovery-tasks'
        original=read(case/'partial-retirement-operation.json');request=original['request'];assert original['phase']=='failed'
        assert read(case/'handoff-operation.json')==original
        policy=read(tasks/'policy.json');config=read(inputs/'recovery-service.json')
        assert policy['configuration_sha256']==sha(inputs/'recovery-service.json') and config['executable_sha256']==result['program_sha256']['handoff_tool']
        assert {x['path']:x['sha256'] for x in policy['inputs']}[str(inputs/'successor.json')]==request['successor_plan_sha256']
        ui=read(inputs/'cold-window-submit.json');assert all(ui[k] for k in ['passed','real_production_window','cancel_created_no_id','loaded_intent_observed_without_post','wrong_service_refused'])
        operation=json.loads(ui['operation']);r=operation['request'];attempt=r['attempt_id'];assert r['action']=='submit' and r['task_id']==attempt
        assert r['policy_sha256']==sha(tasks/'policy.json') and operation['phase']=='complete'
        job=tasks/('job-'+attempt);assert read(job/'operation.json')==operation
        assert read(job/'request.json')==r and operation['request_key']==digest('getdaq.cold-handoff-request:1\n'+(job/'request.json').read_text())
        external=tasks/('attempt-'+attempt);proof=external/'receipt.json'
        assert sha(proof)==operation['receipt_sha256'] and proof.read_text()==operation['receipt']
        assert (inputs/'cold-window-submit.json.receipt.json').read_bytes()==proof.read_bytes()==(inputs/'old-handoff.json').read_bytes()
        intent=read(external/'cold-request.json');assert intent['operation_id']==attempt and intent['original_request_sha256']==sha(inputs/'cold-original-evidence/request.json')
        assert intent['successor_plan_sha256']==request['successor_plan_sha256'] and read(external/'original-request.json')==request
        assert (external/'original-status.json').read_bytes()==(inputs/'cold-original-evidence/original-status.json').read_bytes()
        completion(case,proof,case/'controller/operations'/attempt/'data-handoff',case/'controller/operations/cold-uncertain/data-handoff')
        context(case,proof)
        inspection=read(inputs/'cold-window-inspect.json');assert inspection['passed'] and inspection['cancel_created_no_id'] and inspection['wrong_service_refused']
        inspected=json.loads(inspection['operation']);assert inspected['request']['action']=='inspect' and inspected['request']['attempt_id']==attempt
        assert inspected['receipt']==operation['receipt'] and (inputs/'cold-window-inspect.json.receipt.json').read_bytes()==proof.read_bytes()
        for p in tasks.glob('job-*'):
            op=read(p/'operation.json');assert read(p/'request.json')==op['request']
            assert op['request_key']==digest('getdaq.cold-handoff-request:1\n'+(p/'request.json').read_text())
            assert op['request']['policy_sha256']==sha(tasks/'policy.json') and (p/'diagnostic.log').stat().st_size<=65536
        assert read(tasks/'job-cold-while-live/operation.json')['phase']=='failed'
        failed='cold-missing-plan' if request['command']=='retire-data-plans' else 'cold-uncertain'
        assert read(tasks/('job-'+failed)/'operation.json')['phase']=='failed' and not (case/'controller/operations/cold-while-live').exists()
        restart=read(inputs/'recovery-service-result.json');assert all(restart[k] for k in ['passed','authentication_required','restart_new_instance','old_request_observed_without_replay','different_request_same_id_refused','stale_instance_refused']) and not restart['raw_in_qt']
        crash=inputs/'crash-case';crash_result=read(crash/'crash-result.json')
        assert all(crash_result[k] for k in ['passed','real_service_sigkill','live_child_preserved','restart_unknown_no_replay','new_submit_held','explicit_inspect_proved_original','original_unknown_preserved'])
        crash_tasks=crash/'recovery-tasks';unknown=read(crash_tasks/'job-crash-original/operation.json')
        assert unknown['phase']=='running' and unknown['request']['action']=='submit' and not (crash_tasks/'job-must-not-replay').exists()
        assert (crash_tasks/'attempt-crash-original/receipt.json').read_bytes()==proof.read_bytes()
        crash_journal=rows(case/'controller/operations/crash-original/data-handoff/remote.jsonl')
        assert crash_journal[-1]['phase']=='complete' and not any(x['phase']=='node_explicit_submit' for x in crash_journal)
        assert len([x for x in crash_journal if x['phase']=='node_original_observed'])==2
        inspected_crash=[read(p) for p in crash_tasks.glob('job-inspect-*/operation.json')]
        assert len(inspected_crash)==1 and inspected_crash[0]['phase']=='complete' and inspected_crash[0]['receipt']==operation['receipt']
        assert not (crash/'gate').exists() and int((crash/'child.pid').read_text())>0
        for file,before,field in [(case/'controller/ecc.jsonl',case/'cold-original-native.jsonl','phase'),(case/'controller/network/operations.jsonl',case/'cold-original-network.jsonl','event')]:
            raw=file.read_bytes();prefix=before.read_bytes();assert raw.startswith(prefix)
            extra=[json.loads(x) for x in raw[len(prefix):].decode().splitlines() if x];assert len(extra)==1 and extra[0][field]=='opened'
        history=rows(case/'controller/network/operations.jsonl')
        assert len([x for x in history if x.get('operation_id')==request['operation_id'] and x['event']=='failed'])==1
        assert not any(x.get('operation_id')==request['operation_id'] and x['event']=='complete' for x in history)
        old=read(case/'original-fenced-status.json');new=read(case/'successor-new-cycle-status.json');plan=read(inputs/'successor.json')
        assert old['control_needs_reconciliation'] and old['instance_id']!=new['instance_id'] and not new['control_needs_reconciliation'] and not new['data']['needs_reconciliation']
        assert new['data']['controller_scope_sha256']==plan['data_controller_scope_sha256'] and new['data']['controller_epoch']==plan['receiver_controller_epoch']
        retirement=request['command']=='retire-data-plans';old_nodes={n['agent_id']:n for n in old['data']['nodes']}
        for node in new['data']['nodes']:
            previous=old_nodes[node['agent_id']];assert (node['instance_id']!=previous['instance_id'])==retirement and (node['plan_sha256']!=previous['plan_sha256'])==retirement
            life=node['lifecycle'];assert life['state']=='stopped' and life['session_id']!=previous['lifecycle']['session_id']
            assert all(x['reaped'] and x['seal_verified'] and x['exit_code']==0 for x in life['receivers'])
        cases.append(name)
    for log in folder.rglob('*.log'):
        text=log.read_text(errors='replace');assert not any(x in text for x in ['ERROR: AddressSanitizer','ERROR: LeakSanitizer','AddressSanitizer:DEADLYSIGNAL','runtime error:','LeakSanitizer has encountered a fatal error']),log
    return cases
if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('evidence',type=Path);a=ap.parse_args()
    print('PASS independent recovery service/window/crash/original-child/new-cycle evidence:',check(a.evidence.resolve()))
