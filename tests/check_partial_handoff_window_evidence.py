#!/usr/bin/env python3
"""Check actual Qt confirmation, immutable client context, and native child history."""
import argparse
import json
from pathlib import Path
from check_partial_handoff_completion_evidence import completion, digest, read, rows, sha


def check(folder,delivery_receipt=None):
    result=read(folder/'result.json')
    assert result['passed'] and not result['real_hardware'] and result['raw_remaining']==0
    scenarios={r['scenario'] for r in result['runs']};assert len(scenarios)==len(result['runs'])
    assert scenarios=={'plan-transition-completion-window-0','plan-transition-completion-window-1'}
    if delivery_receipt is not None:
        assert not result['sanitized']
        delivery=read(delivery_receipt);assert delivery['status']=='complete' and len(delivery['programs'])==24
        for role,name in {'ecc':'getdaq-ecc-server','receiver':'getdaq-receiver','data_service':'getdaq-supervisor'}.items():
            program=delivery['programs'][name];assert sha(Path(program['path']))==program['sha256']==result['program_sha256'][role]
    assert not list(folder.rglob('*.graw'))
    cleanup=read(folder/'cleanup.json');assert cleanup['deleted'] and not cleanup['remaining_raw']
    assert sum(r['bytes'] for r in cleanup['deleted'])<=result['maximum_aggregate_test_bytes']<=20*1024**3
    summaries=[]
    for run in result['runs']:
        assert run['exit_code']==0
        case=folder/run['scenario'];evidence=case/'gui-switch-evidence';client=case/'gui-client-records'
        proof=read(evidence/'window-proof.json')
        assert proof['passed'] and proof['actual_main_window_and_runcontrol'] and proof['partial_completion']
        assert proof['cancel_intents']==proof['cancel_submissions']==0 and proof['confirmed_submissions']==1
        assert proof['completion_cancel_default'] and proof['completion_confirmed_submissions']==2 and proof['completion_observation_signals']==1
        assert proof['saved_original_pre_retirement_context'] and proof['original_failure_retained'] and proof['old_control_buttons_disabled']
        assert proof['server_review_lease_blocks_startup'] and proof['receipt_exported'] and not proof['raw_transferred_by_gui'] and proof['raw_read_guard_selfcheck']
        assert (case/'gui-raw-reads.log').read_bytes()==b'raw-read-denied\n'
        programs=read(case/'gui-programs.json')
        assert sha(Path(programs['probe']))==programs['probe_sha256'] and sha(Path(programs['guard']))==programs['guard_sha256']
        assert programs['wrapper_sha256']==result['program_sha256']['client_probe']
        request=read(evidence/'request.json');old=read(case/'partial-retirement-operation.json');done=read(case/'completion-operation.json')
        first=read(evidence/'first-completion-operation.json')
        assert old==read(evidence/'original-operation.json') and old['phase']=='failed' and old['request']==request
        assert first['phase']=='failed' and first['request']==read(evidence/'first-completion-request.json')
        assert done['phase']=='complete' and done==read(evidence/'completion-operation.json') and done['request']==read(evidence/'completion-request.json')
        original=read(client/'plan-retirement-context.json')
        assert original==read(evidence/'original-status.json') and not original['control_needs_reconciliation'] and not original['data']['needs_reconciliation']
        assert not original['operation_active'] and not original['controller']['outcome_unknown'] and not original['controller']['needs_reconciliation']
        ids={request['operation_id'],first['request']['operation_id'],done['request']['operation_id']};assert len(ids)==3
        assert {p.stem for p in (client/'requests').glob('*.json')}==ids
        journal=rows(client/'observations.jsonl');contexts=[r for r in journal if r['kind']=='retirement-context'];assert len(contexts)==1
        assert [r['sequence'] for r in journal]==list(range(len(journal)))
        intents=[r for r in journal if r['kind']=='intent']
        assert [json.loads(r['payload'])['operation_id'] for r in intents]==[request['operation_id'],first['request']['operation_id'],done['request']['operation_id']]
        assert contexts[0]['sequence']<intents[0]['sequence']
        assert contexts[0]['request_key']==digest('getdaq.ecc-control-request:1\n'+(evidence/'request.json').read_text())
        assert contexts[0]['payload']==sha(client/'plan-retirement-context.json')
        for operation in (first,done):
            child=operation['request'];assert child['command']=='complete-data-handoff' and child['failed_operation_id']==request['operation_id']
            for field in ('instance_id','snapshot_sha256','client_id','client_epoch'):assert child[field]==request[field]
            assert set(child)=={'schema','operation_id','command','instance_id','snapshot_sha256','client_id','client_epoch','failed_operation_id'}
            assert read(client/'requests'/(child['operation_id']+'.json'))==child
        history=rows(case/'controller/network/operations.jsonl')
        assert not any(r['event']=='complete' and r.get('operation_id')==request['operation_id'] for r in history)
        for operation in (old,first,done):
            op=operation['request']['operation_id'];assert len([r for r in history if r['event']=='accepted' and r['operation_id']==op])==1
            assert len([r for r in history if r['event']==operation['phase'] and r['operation_id']==op])==1
        audit=case/'controller/operations'/done['request']['operation_id']/'data-handoff'
        completion(case,evidence/'receipt.json',audit)
        receipt=read(evidence/'receipt.json');assert receipt['original_scope_sha256']==original['data']['controller_scope_sha256'] and receipt['original_epoch']==original['data']['controller_epoch']
        frozen={n['agent_id']:n for n in original['data']['nodes']}
        for node in receipt['nodes']:
            child=json.loads(node['request']);previous=frozen[node['agent_id']]
            assert child['instance_id']==previous['instance_id'] and child['plan_sha256']==previous['plan_sha256']
            assert child['expected_session_id']==previous['lifecycle']['session_id']
        assert (evidence/'receipt.json').read_bytes()==(case/'successor-inputs/old-handoff.json').read_bytes()
        assert (evidence/'receipt.json').read_bytes()==(client/('plan-retirement-'+request['operation_id']+'.json')).read_bytes()
        native=rows(case/'controller/ecc.jsonl')
        assert not any(r['phase']=='intent' and r['id'] in ids for r in native)
        before=read(case/'original-fenced-status.json');after=read(case/'successor-new-cycle-status.json')
        assert before['control_needs_reconciliation'] and before['data']['needs_reconciliation']
        assert before['instance_id']==request['instance_id']!=after['instance_id'] and not after['control_needs_reconciliation'] and not after['data']['needs_reconciliation']
        previous={n['agent_id']:n for n in before['data']['nodes']}
        for node in after['data']['nodes']:
            life=node['lifecycle'];prior=previous[node['agent_id']]
            assert node['instance_id']!=prior['instance_id'] and node['plan_sha256']!=prior['plan_sha256']
            assert life['session_id']!=prior['lifecycle']['session_id'] and life['state']=='stopped' and life['receivers']
            assert all(r['reaped'] and r['seal_verified'] and r['exit_code']==0 for r in life['receivers'])
        assert read(case/'completion-ecc-exit.json')=={'normal_exit':True,'exit_code':0}
        summaries.append({'scenario':run['scenario'],'actual_main_window':True,'original_failure_preserved':True,'new_cycle_sealed':True})
    for path in folder.rglob('*.log'):
        text=path.read_text(errors='replace');assert not any(x in text for x in ('ERROR: AddressSanitizer','ERROR: LeakSanitizer','runtime error:')),str(path)
    return summaries


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('evidence',type=Path)
    parser.add_argument('--delivery-receipt',type=Path,help='Bind both encoding cases and all three native programs to the new 24-program delivery build')
    args=parser.parse_args()
    print('PASS Qt partial handoff completion:',json.dumps(check(args.evidence.resolve(),args.delivery_receipt.resolve() if args.delivery_receipt else None),sort_keys=True))
