#!/usr/bin/env python3
"""Bind explicit completion to unchanged original child intents and final service history."""
import argparse
import hashlib
import json
from pathlib import Path


def read(path):return json.loads(path.read_text())
def rows(path):
    # Existing remote journals embed canonical status objects whose encoding
    # ends with a newline. Parse complete JSON records, including legacy
    # records spanning physical lines, without altering the retained bytes.
    text=path.read_text();decoder=json.JSONDecoder();result=[];position=0
    while position<len(text):
        while position<len(text) and text[position].isspace():position+=1
        if position==len(text):break
        value,end=decoder.raw_decode(text,position)
        assert isinstance(value,dict) and (end==len(text) or text[end].isspace())
        result.append(value);position=end
    return result
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def digest(value):return hashlib.sha256(value.encode()).hexdigest()


def completion(case,receipt_file,audit,ambiguous_audit=None,missing_agent='node-1'):
    receipt=read(receipt_file);original=Path(receipt['audit_directory']).resolve()
    original.relative_to(case.resolve());audit.resolve().relative_to(case.resolve())
    initial=read(original/'intent.json');review=read(audit/'intent.json');journal=rows(audit/'remote.jsonl')
    assert review['kind']=='getdaq.remote-handoff-completion-intent'
    assert review['binding_sha256']==initial['binding_sha256']
    assert review['original_audit_directory']==str(original) and review['original_intent_sha256']==sha(original/'intent.json')
    assert review['requests']==initial['requests'] and len(review['requests'])==2
    assert rows(original/'remote.jsonl')[-1]['phase']=='unknown'
    assert journal[-1]['phase']=='complete' and journal[-1]['receipt_sha256']==sha(receipt_file)
    assert journal[-1]['original_control_remains_fenced'] and not journal[-1]['raw_data_forwarded']
    expected={r['operation_id']:r for r in initial['requests']};assert len(expected)==2
    assert len(receipt['nodes'])==2 and len({r['agent_id'] for r in receipt['nodes']})==2
    for node in receipt['nodes']:
        assert node['agent_id'] in ('node-0','node-1')
        request=json.loads(node['request']);assert request==expected[node['operation_id']]
        assert request['parent_audit_reference']==str(original)
        assert request['instance_id']==node['instance_id'] and request['controller_snapshot_sha256']==receipt['original_scope_sha256']
        assert request['controller_epoch']==receipt['original_epoch']
        assert request['successor_controller_snapshot_sha256']==receipt['successor_scope_sha256'] and request['successor_controller_epoch']==receipt['successor_epoch']
        control=case/node['agent_id']/'deployment/control';operation=control/'operations'/node['operation_id']
        assert (operation/'request.json').read_text()==node['request']
        key=digest('getdaq.supervisor-request:1\n'+node['request']);assert key==node['request_key']
        history=rows(control/'operations.jsonl')
        accepted=[r for r in history if r['event']=='accepted' and r['operation_id']==node['operation_id']]
        completed=[r for r in history if r['event']=='complete' and r['operation_id']==node['operation_id']]
        assert len(accepted)==len(completed)==1 and accepted[0]['request']==node['request']
        assert accepted[0]['request_key']==completed[0]['request_key']==key
        retired=request['command']=='retire-plan';assert retired or request['command']=='handoff-control'
        proof_file=operation/('plan-retirement.json' if retired else 'handoff-proof.json');proof=read(proof_file)
        assert sha(proof_file)==node['proof_sha256'] and proof['request_key']==key
        life=proof['lifecycle'];assert life['state']=='stopped' and life['receivers']
        assert all(r['reaped'] and r['seal_verified'] and r['exit_code']==0 for r in life['receivers'])
    observed=[r for r in journal if r['phase']=='node_original_observed']
    submitted=[r for r in journal if r['phase']=='node_explicit_submit']
    if receipt['kind']=='getdaq.remote-control-handoff':
        assert len(observed)==2 and not submitted and all(r['no_post_sent'] for r in observed)
        ambiguous=rows((ambiguous_audit if ambiguous_audit else audit.parent/'ambiguous-404')/'remote.jsonl')
        assert ambiguous[-1]['phase']=='unknown' and not any(r['phase']=='node_explicit_submit' for r in ambiguous)
    else:
        assert receipt['kind']=='getdaq.remote-plan-retirement'
        assert len(observed)==1 and observed[0]['agent_id']!=missing_agent and observed[0]['no_post_sent']
        assert len(submitted)==1 and submitted[0]['agent_id']==missing_agent
        missing=[r for r in journal if r['phase']=='node_original_not_accepted']
        assert len(missing)==1 and missing[0]['prior_rejection'] in (1,409,412)


def check(folder,package=None,selected_scenarios=None):
    result=read(folder/'result.json');assert result['passed'] and result['raw_remaining']==0 and not result['real_hardware']
    assert not list(folder.rglob('*.graw'))
    cleanup=read(folder/'cleanup.json');assert cleanup['deleted']
    assert sum(r['bytes'] for r in cleanup['deleted'])<=result['maximum_aggregate_test_bytes']<=20*1024**3
    summaries=[]
    if 'cases' in result:
        assert package is None and selected_scenarios is None
        assert {r['case'] for r in result['cases']}=={'handoff-complete-0','handoff-complete-1','handoff-retire-complete-0','handoff-retire-complete-1'}
        for entry in result['cases']:
            case=folder/entry['case'];proof=read(case/'handoff-proof.json')
            assert proof['passed'] and proof['original_manager_fenced'] and proof['old_raw_retained'] and not proof['raw_forwarded']
            completion(case,case/'control-handoff-result.json',case/'controller/operations/complete/data-handoff')
            summaries.append(dict(case=entry['case'],original_children_accepted_once=True))
    else:
        runs=result['runs']
        assert len({r['scenario'] for r in runs})==len(runs)
        if selected_scenarios is not None:
            assert set(selected_scenarios)<={r['scenario'] for r in runs}
            runs=[r for r in runs if r['scenario'] in selected_scenarios]
        scenarios={r['scenario'] for r in runs}
        if package is None:
            assert scenarios=={'plan-transition-complete-0','plan-transition-complete-1'}
        else:
            assert not result['sanitized'] and scenarios=={'plan-transition-complete-1'}
            manifest=read(package/'manifest.json')
            for key,name in {'ecc':'getdaq-ecc-server','receiver':'getdaq-receiver','data_service':'getdaq-supervisor','handoff_tool':'getdaq-ecc-handoff'}.items():
                assert sha(package/'bin'/name)==manifest['programs'][name]['sha256']==result['program_sha256'][key]
        for entry in runs:
            case=folder/entry['scenario'];old=read(case/'partial-retirement-operation.json');done=read(case/'completion-operation.json')
            assert old['phase']=='failed' and old['request']['command']=='retire-data-plans'
            assert done['phase']=='complete' and done['request']['command']=='complete-data-handoff'
            assert done['request']['failed_operation_id']==old['request']['operation_id']
            for field in ('instance_id','snapshot_sha256','client_id','client_epoch'):
                assert done['request'][field]==old['request'][field]
            assert set(done['request'])=={'schema','operation_id','command','instance_id','snapshot_sha256','client_id','client_epoch','failed_operation_id'}
            state=done['result']
            assert state['state'] in (1,2) and state['transition']==state['error']==0 and not state['outcome_unknown'] and state['needs_reconciliation']
            reviewed=case/'controller/network/operations'/old['request']['operation_id']/'reviewed-successor-plan.json'
            assert sha(reviewed)==old['request']['successor_plan_sha256']
            op=done['request']['operation_id'];request_file=case/'controller/network/operations'/op/'request.json'
            assert read(request_file)==done['request'] and digest('getdaq.ecc-control-request:1\n'+request_file.read_text())==done['request_key']
            history=rows(case/'controller/network/operations.jsonl')
            assert len([r for r in history if r['event']=='accepted' and r['operation_id']==op])==1
            assert len([r for r in history if r['event']=='complete' and r['operation_id']==op])==1
            assert not any(r['event']=='complete' and r.get('operation_id')==old['request']['operation_id'] for r in history)
            completion(case,case/'successor-inputs/old-handoff.json',case/'controller/operations'/op/'data-handoff')
            native=rows(case/'controller/ecc.jsonl')
            assert not any(r['phase']=='intent' and r['id']==op for r in native)
            original=read(case/'original-fenced-status.json');fresh=read(case/'successor-new-cycle-status.json')
            assert original['control_needs_reconciliation'] and original['data']['needs_reconciliation']
            assert original['instance_id']!=fresh['instance_id'] and not fresh['control_needs_reconciliation'] and not fresh['data']['needs_reconciliation']
            old_nodes={n['agent_id']:n for n in original['data']['nodes']}
            for node in fresh['data']['nodes']:
                before=old_nodes[node['agent_id']];life=node['lifecycle']
                assert node['instance_id']!=before['instance_id'] and node['plan_sha256']!=before['plan_sha256']
                assert life['session_id']!=before['lifecycle']['session_id'] and life['state']=='stopped' and life['receivers']
                assert all(r['reaped'] and r['seal_verified'] and r['exit_code']==0 for r in life['receivers'])
            exited=read(case/'completion-ecc-exit.json');assert exited['normal_exit'] and exited['exit_code']==0
            if entry['scenario'].endswith('-1'):
                evidence=case/'successor-inputs/completion-evidence'
                assert read(evidence/'request.json')==done['request'] and (evidence/'receipt.json').read_bytes()==(case/'successor-inputs/old-handoff.json').read_bytes()
            summaries.append(dict(scenario=entry['scenario'],original_failure_preserved=True,new_plan_cycle_sealed=True))
    for log in folder.rglob('*.log'):
        text=log.read_text(errors='replace')
        assert not any(error in text for error in ('ERROR: AddressSanitizer','ERROR: LeakSanitizer','runtime error:')),str(log)
    return summaries


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('evidence',type=Path)
    parser.add_argument('--package',type=Path,help='Verify exactly the packaged encoding-1 case and bind all four packaged program hashes')
    args=parser.parse_args()
    print('PASS explicit partial handoff completion:',json.dumps(check(args.evidence.resolve(),args.package.resolve() if args.package else None),sort_keys=True))
