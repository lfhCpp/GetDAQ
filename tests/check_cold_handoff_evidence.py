#!/usr/bin/env python3
"""Bind new persistent contexts and offline reviews to real handoff histories."""
import argparse
import hashlib
import json
from pathlib import Path
from check_partial_handoff_completion_evidence import check as check_completion, read, rows, sha


def context(case,receipt_file):
    receipt=read(receipt_file);audit=Path(receipt['audit_directory']);audit.resolve().relative_to(case.resolve())
    document=read(audit/'handoff-context.json');intent=read(audit/'intent.json');history=rows(audit/'remote.jsonl')
    assert document['schema']==1 and document['kind']=='getdaq.remote-handoff-context'
    assert document['audit_directory']==str(audit) and document['intent_sha256']==sha(audit/'intent.json')
    assert document['binding_sha256']==intent['binding_sha256']
    assert document['original_scope_sha256']==receipt['original_scope_sha256'] and document['original_epoch']==receipt['original_epoch']
    assert history[0]['phase']=='intent' and history[0]['handoff_context_sha256']==sha(audit/'handoff-context.json')
    assert not history[0]['automatic_retry']
    nodes={n['agent_id']:n for n in document['nodes']};assert len(nodes)==2
    for node in receipt['nodes']:
        stopped=nodes[node['agent_id']];request=json.loads(node['request']);life=stopped['lifecycle']
        assert stopped['instance_id']==request['instance_id'] and stopped['plan_sha256']==request['plan_sha256']==life['plan_sha256']
        assert request['controller_epoch']==document['original_epoch'] and request['controller_snapshot_sha256']==document['original_scope_sha256']
        assert request['parent_audit_reference']==str(audit) and life['state']=='stopped' and not life['needs_reconciliation']
        expected=hashlib.sha256(('getdaq.remote-data-operation:1\n'+document['original_scope_sha256']+'\n'+document['original_epoch']+'\n'+str(audit)+'\n'+request['command']+'\n'+node['agent_id']).encode()).hexdigest()
        assert expected==request['operation_id']
        assert life['receivers'] and all(r['reaped'] and r['seal_verified'] and r['exit_code']==0 and r['pid']>0 and r['session_id']==life['session_id'] for r in life['receivers'])
        if request['command']=='retire-plan':assert request['expected_session_id']==life['session_id']
    return document,nodes


def check(folder,tool,guard):
    result=read(folder/'result.json');assert result['passed'] and result['raw_remaining']==0 and not list(folder.rglob('*.graw'))
    if 'runs' in result:
        # Existing verifier also checks all original child histories, immutable
        # requests, native no-write audit, successor instances/cycles and seals.
        check_completion(folder)
        assert {r['scenario'] for r in result['runs']}=={'plan-transition-complete-0','plan-transition-complete-1'}
        for run in result['runs']:
            case=folder/run['scenario'];receipt_file=case/'successor-inputs/old-handoff.json';document,nodes=context(case,receipt_file)
            programs=read(case/'cold-review-programs.json')
            assert programs['program_sha256']==sha(tool) and programs['guard_sha256']==sha(guard)
            assert programs['wrapper_sha256']==result['program_sha256']['handoff_tool']
            log=case/'cold-review-network.log';assert not log.exists() or log.stat().st_size==0
            offline=read(case/'cold-offline-result.json')
            assert all(offline[k] for k in ['passed','all_services_stopped','expired_credentials_removed','offline_configuration_cannot_start_control','normal_configuration_still_requires_credentials','original_input_binding_retained','native_audit_unchanged'])
            assert not list(case.rglob('credential')) and not (case/'ecc-credential').exists()
            native=rows(case/'controller/ecc.jsonl');assert len([r for r in native if r['phase']=='opened'])==1
            for name in ['cold-review-original','cold-review-completed','cold-review-no-credentials']:
                report=read(case/'successor-inputs'/name/'review.json')
                assert report['kind']=='getdaq.cold-handoff-review' and report['schema']==1
                assert report['original_intent_sha256']==document['intent_sha256'] and report['binding_sha256']==document['binding_sha256']
                assert report['original_scope_sha256']==document['original_scope_sha256'] and report['original_epoch']==document['original_epoch']
                assert all(report[k] is False for k in ['current_remote_identity_checked','control_granted','hardware_unknown_cleared','cold_recovery_executed','raw_payload_read'])
                assert {n['agent_id'] for n in report['nodes']}==set(nodes)
                for node in report['nodes']:
                    old=nodes[node['agent_id']];assert node['instance_id']==old['instance_id'] and node['plan_sha256']==old['plan_sha256'] and node['session_id']==old['lifecycle']['session_id']
                    expected='explicit_rejection' if name=='cold-review-original' and node['agent_id']=='node-1' else 'uncertain_or_accepted'
                    assert node['historical_delivery']==expected
                    if expected=='explicit_rejection':assert node['http_rejection'] in (409,412)
                    else:assert node['http_rejection'] is None
    else:
        check_completion(folder)
        assert len(result['cases'])==4
        for run in result['cases']:context(folder/run['case'],folder/run['case']/'control-handoff-result.json')
    return len(result.get('runs',result.get('cases',[])))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('evidence',type=Path)
    parser.add_argument('--tool',type=Path,required=True);parser.add_argument('--guard',type=Path,required=True);args=parser.parse_args()
    print('PASS cold handoff context/offline evidence:',check(args.evidence.resolve(),args.tool.resolve(),args.guard.resolve()),'scenarios; cold control recovery not claimed')
