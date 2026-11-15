#!/usr/bin/env python3
"""Independently bind parent requests, node retirement proofs and the successor cycle."""
import argparse
import hashlib
import json
from pathlib import Path


def read(path):return json.loads(path.read_text())
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def digest(value):return hashlib.sha256(value.encode()).hexdigest()


def check(case):
    partial=case.name=='plan-transition-partial-0'
    old=read(case/'original-fenced-status.json')
    assert old['control_needs_reconciliation'] and not old['operation_active']
    parent_op=read(case/('partial-retirement-operation.json' if partial else 'handoff-operation.json'))
    request=parent_op['request'];assert request['command']=='retire-data-plans'
    assert request['instance_id']==old['instance_id'] and request['snapshot_sha256']==old['snapshot_sha256']
    request_file=case/'controller/network/operations'/request['operation_id']/'request.json'
    assert read(request_file)==request
    assert digest('getdaq.ecc-control-request:1\n'+request_file.read_text())==parent_op['request_key']
    rows=[json.loads(line) for line in (case/'controller/network/operations.jsonl').read_text().splitlines()]
    accepted=[r for r in rows if r.get('event')=='accepted' and r.get('operation_id')==request['operation_id']]
    assert len(accepted)==1 and json.loads(accepted[0]['request'])==request
    assert all(r.get('event')!='accepted' or r['operation_id']==request['operation_id'] for r in rows[rows.index(accepted[0]):])
    if partial:
        proof=read(case/'partial-plan-result.json');assert proof['passed'] and proof['cold_original_fenced'] and not proof['automatic_retry']
        assert parent_op['phase']=='failed'
        assert len(list((case/'node-0/deployment/control/operations').glob('*/plan-retirement.json')))==1
        assert not list((case/'node-1/deployment/control/operations').glob('*/plan-retirement.json'))
        assert not (case/'successor-controller').exists()
        return dict(partial=True,nodes_retired=1,new_cycle=False)
    assert parent_op['phase']=='complete'
    receipt=read(case/'successor-inputs/old-handoff.json')
    assert receipt['kind']=='getdaq.remote-plan-retirement' and receipt['schema']==1
    assert receipt['original_scope_sha256']==old['data']['controller_scope_sha256'] and receipt['original_epoch']==old['data']['controller_epoch']
    assert receipt['successor_scope_sha256']==request['successor_scope_sha256'] and receipt['successor_epoch']==request['successor_epoch']
    plan_file=case/'successor-inputs/successor.json';plan=read(plan_file);activation=read(Path(str(plan_file)+'.activation.json'))
    assert sha(plan_file)==request['successor_plan_sha256']
    new=read(case/'successor-new-cycle-status.json');identity=read(case/'successor-identity.json');startup=read(case/'successor-startup-status.json')
    assert new['instance_id']==identity['instance_id']!=old['instance_id']
    original_artifact=read(case/'experiment.json');next_artifact=read(case/'successor-inputs/experiment.json')
    assert original_artifact['configuration_sha256']!=next_artifact['configuration_sha256']==identity['configuration_sha256']
    assert original_artifact['settings']!=next_artifact['settings']
    assert identity['data_binding_sha256']==request['successor_data_binding_sha256']
    assert new['snapshot_sha256']==plan['snapshot_sha256'] and not new['control_needs_reconciliation']
    assert new['data']['controller_scope_sha256']==request['successor_scope_sha256'] and new['data']['controller_epoch']==request['successor_epoch']
    assert all(node['lifecycle']['state']=='fresh' and not node['lifecycle']['receivers'] for node in startup['data']['nodes'])
    old_nodes={n['agent_id']:n for n in old['data']['nodes']};new_nodes={n['agent_id']:n for n in new['data']['nodes']}
    targets={n['agent_id']:n for n in request['successor_receiver_plans']}
    assert set(old_nodes)==set(new_nodes)==set(targets)=={n['agent_id'] for n in receipt['nodes']}
    for node in receipt['nodes']:
        agent=node['agent_id'];child=json.loads(node['request']);before=old_nodes[agent];after=new_nodes[agent]
        assert child['command']=='retire-plan' and child['instance_id']==before['instance_id']!=after['instance_id']
        assert child['expected_session_id']==before['lifecycle']['session_id']!=after['lifecycle']['session_id']
        assert child['successor_plan_sha256']==targets[agent]['plan_sha256']==after['plan_sha256']!=before['plan_sha256']
        assert child['successor_plan_file']==targets[agent]['plan_file']
        assert child['successor_controller_snapshot_sha256']==request['successor_scope_sha256'] and child['successor_controller_epoch']==request['successor_epoch']
        proof_file=case/agent/'deployment/control/operations'/child['operation_id']/'plan-retirement.json';proof=read(proof_file)
        assert sha(proof_file)==node['proof_sha256'] and json.loads(proof['request'])==child and proof['request_key']==node['request_key']
        assert proof['lifecycle']==before['lifecycle']
        assert sha(proof_file.parent/'successor-plan.json')==targets[agent]['plan_sha256']
        next_plan=read(proof_file.parent/'successor-plan.json')
        assert next_plan['configuration_sha256']==identity['configuration_sha256'] and next_plan['artifact_sha256']==identity['artifact_sha256']
        assert next_plan['sources'][0]['queue_mib']==next_plan['sources'][0]['max_frame_mib']==next_plan['sources'][0]['rotate_mib']==2
        rows=[json.loads(line) for line in (case/agent/'deployment/control/operations.jsonl').read_text().splitlines()]
        commits=[r for r in rows if r.get('event')=='complete' and r.get('operation_id')==child['operation_id']]
        assert len(commits)==1 and commits[0]['plan_retirement_proof_sha256']==node['proof_sha256']
        assert after['lifecycle']['state']=='stopped' and all(r['reaped'] and r['seal_verified'] and r['exit_code']==0 and int(r['frames'])==2 for r in after['lifecycle']['receivers'])
    if case.name=='plan-transition-1':
        assert read(case/'successor-inputs/tool-evidence/receipt.json')==receipt
    if case.name=='plan-transition-window-1':
        window=read(case/'gui-switch-evidence/window-proof.json')
        assert window['passed'] and window['actual_main_window_and_runcontrol']
        assert window['cancel_submissions']==window['cancel_intents']==0 and window['confirmed_submissions']==1
        assert window['server_review_lease_blocks_startup'] and window['old_control_buttons_disabled'] and window['receipt_exported'] and not window['raw_transferred_by_gui']
        assert window['raw_read_guard_selfcheck']
        # The probe deliberately attempts one owned one-byte raw read to prove
        # that the guard is active. Any production GUI raw read adds a line.
        assert (case/'gui-raw-reads.log').read_bytes()==b'raw-read-denied\n'
        programs=read(case/'gui-programs.json')
        assert sha(Path(programs['probe']))==programs['probe_sha256']
        assert sha(Path(programs['guard']))==programs['guard_sha256']
        result=read(case.parent/'result.json')
        assert programs['wrapper_sha256']==result['program_sha256']['client_probe']
        assert read(case/'gui-switch-evidence/request.json')==request and read(case/'gui-switch-evidence/receipt.json')==receipt
        assert request['successor_plan_file']==str(plan_file.resolve())
        reviewed=request_file.parent/'reviewed-successor-plan.json'
        assert reviewed.read_bytes()==plan_file.read_bytes()
    assert activation['kind']=='getdaq.ecc-successor-activation' and activation['plan_sha256']==sha(plan_file)
    assert read(case/'service-handoff-result.json')['consumed_plan_restart_refused']
    return dict(partial=False,nodes_retired=len(receipt['nodes']),new_cycle=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('root',type=Path);args=parser.parse_args()
    result=read(args.root/'result.json');assert result['passed'] and not list(args.root.rglob('*.graw'))
    summaries={p.name:check(p) for p in sorted(args.root.glob('plan-transition-*')) if p.is_dir()}
    assert summaries
    print('PASS independent ECC plan transition evidence:',json.dumps(summaries,sort_keys=True))


if __name__=='__main__':main()
