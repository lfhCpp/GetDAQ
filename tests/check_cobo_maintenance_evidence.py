#!/usr/bin/env python3
"""Bind selected-board maintenance journals, target IDs and preserved control history."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath


def read(path):return json.loads(path.read_text())
def rows(path):return [json.loads(line) for line in path.read_text().splitlines()]
def digest(value):return hashlib.sha256(value.encode()).hexdigest()
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def chain(path):
    value=digest('getdaq.hardware-audit-chain:1\n')
    for line in path.read_bytes().splitlines(keepends=True):value=hashlib.sha256((value+'\n').encode()+line).hexdigest()
    return value


def maintenance(path,archive,original):
    records=rows(path);intent=records[0];assert intent['operation']=='reprepare-cobo' and not intent['raw_data_handled'] and not intent['automatic_retry']
    complete=[r for r in records if r['phase']=='complete']
    closed=[r for r in records if r['phase']=='old_subscription_closed']
    if closed:
        close=path.parent/'old-subscription-close.jsonl';assert len(closed)==1 and chain(close)==closed[0]['command_chain_sha256']
        request=rows(close)[0];assert request['operation']=='unsubscribe-owned-alarm' and not request['reset_all_subscribers']
    if complete:
        assert len(complete)==1 and closed
        assert not complete[0]['other_board_writes_sent'] and not complete[0]['system_alignment_repeated']
        assert not complete[0]['physical_alignment_verified'] and not complete[0]['configure_or_start_sent']
        prepared=rows(path.parent/'board-prepare/prepare.jsonl')
        assert any(r['phase']=='complete' for r in prepared)
        source=PurePosixPath(intent['original_prepare_directory']).relative_to(original)
        parent=rows(archive/source/'system.jsonl')
        commits=[r for r in parent if r['phase']=='maintenance_complete' and r['audit_directory']==str(original/path.parent.relative_to(archive).as_posix())]
        assert len(commits)==1 and commits[0]['cobo_id']==intent['cobo_id'] and commits[0]['maintenance_chain_sha256']==chain(path)
    return intent,bool(complete)


def check(record_file):
    record=read(record_file);assert record['passed'] and not record['real_hardware'] and not record['real_receiver_executed'] and record['raw_remaining']==0
    assert not list(record_file.parent.rglob('*.graw'))
    summaries=[]
    for run in record['runs']:
        scope=run.get('scope',record['scope']);meta=run['archive'];original=PurePosixPath(meta['original_root'])
        if record['scope']=='existing-completed-ordinary-runs':
            directory='preparation' if scope=='preparation' else 'coordinator-'+run['scenario'][-1]
        else:directory=run['scenario']
        archive=record_file.parent/directory
        for name,value in meta['files'].items():
            relative=Path(name);assert not relative.is_absolute() and '..' not in relative.parts
            assert sha(archive/relative)==value,name
        if scope=='preparation':
            proofs=[maintenance(p,archive,original) for p in sorted(archive.glob('*/maintenance.jsonl'))]
            assert len(proofs)==6 and sum(done for intent,done in proofs)==2
            assert all(intent['cobo_id']=='C0' for intent,done in proofs)
            summaries.append(dict(scope=scope,maintenance_attempts=6,complete=2,held=4))
        else:
            assert record.get('data_lifecycle_test_double',run.get('data_lifecycle_test_double',False))
            journal=rows(archive/'ecc.jsonl');intents=[r for r in journal if r['phase']=='intent']
            assert [r['command'] for r in intents]==[1,2,11,3,4,5,8,11,3,4,5,8,9,9]
            for operation,target in [('maintenance','C0'),('after-breakup','C1')]:
                found=[r for r in intents if r['id']==operation];assert len(found)==1
                entry=found[0];assert entry['command']==11 and entry['from']==entry['to']==3
                operation_dir=archive/'operations'/operation
                binding=rows(operation_dir/'operation.jsonl')[0]
                assert binding['cobo_id']==target and binding['request_key']==entry['request_key']
                base=digest('getdaq.native-ecc-operation:1\n'+binding['snapshot_sha256']+'\n'+operation+'\n11\n'+str(original/'operations'/operation))
                assert digest('getdaq.native-ecc-maintenance:1\n'+base+'\n'+target)==entry['request_key']
                intent,done=maintenance(operation_dir/'maintenance/maintenance.jsonl',archive,original)
                assert done and intent['cobo_id']==target
                assert len([r for r in journal if r['phase']=='complete' and r['id']==operation])==1
            assert journal[-1]['phase']=='opened' and journal[-1]['state']==1
            summaries.append(dict(scope=scope,maintenance_operations=2,explicit_cycles=2,cold_reopen_held=True))
    return summaries


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('result',type=Path);args=parser.parse_args()
    print('PASS maintenance evidence:',json.dumps(check(args.result),sort_keys=True))


if __name__=='__main__':main()
