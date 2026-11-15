#!/usr/bin/env python3
"""独立核对真实Qt/ECC/receiver流程的启动配置与Run证据；不证明硬件加载。"""
import argparse
import hashlib
import json
from pathlib import Path


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('parent',type=Path);args=parser.parse_args()
    roots=list(args.parent.glob('ecc-network-system-*'));assert len(roots)==1,roots
    root=roots[0];results=json.loads((root/'results.json').read_text());assert len(results)==2 and all(r['exit_code']==0 for r in results)
    programs=json.loads((root/'program-sha256.json').read_text())
    for name,record in programs.items():assert sha(Path(record['path']))==record['sha256'],name
    for case in results:
        folder=root/case['scenario'];scope=json.loads((folder/'scope.json').read_text())
        assert scope['passed'] and scope['real_qt_runcontrol'] and scope['real_receivers'] and scope['physical_hosts']==1 and not scope['real_hardware']
        assert scope['startup_configuration_recorded'] and scope['synthetic_reviewed_publication']
        identity=json.loads((folder/'identity.json').read_text());snapshot=json.loads((folder/'controller/snapshot.json').read_text())
        run=folder/'gui-runs/hardware/NativeIntegration/run_0000';journal=json.loads((run/'journal.json').read_text())
        assert journal['phase']=='Complete'
        body=(run/'ecc-configuration.json').read_bytes();configuration=json.loads(body)
        assert journal['native_configuration'].encode()==body
        assert not configuration['hardware_loaded_verified'] and not configuration['input_binding_live_verified']
        for key in ['instance_id','snapshot_sha256','input_binding_sha256','configuration_sha256','artifact_sha256']:
            assert configuration[key]==identity[key],key
        selected=configuration['selection'];assert selected and not selected['hardware_loaded_verified']
        selectionFile=folder/'reviewed-selection.json'
        assert json.loads(selectionFile.read_bytes())==selected and sha(selectionFile)==snapshot['configuration_selection_sha256']
        directory=Path(selected['manifest_file']).parent
        assert sha(directory/'publication.json')==selected['publication_sha256']==snapshot['configuration_publication_sha256']
        assert sha(directory/'registration.json')==selected['registration_sha256']==snapshot['configuration_registration_sha256']
        assert json.loads((directory/'registration.json').read_bytes())['repository_id']==selected['repository_id']==snapshot['configuration_repository_id']
        assert sha(directory/'experiment.json')==selected['artifact_sha256']==configuration['artifact_sha256']
        assert not snapshot['actual_loaded_configuration_verified'] and snapshot['configuration_sha256']==configuration['configuration_sha256']
        stopped=json.loads((run/'receiver-status.json').read_text())
        receivers=[receiver for node in stopped['data']['nodes'] for receiver in node['lifecycle']['receivers']]
        assert len(receivers)==2
        for receiver in receivers:
            assert receiver['reaped'] and receiver['seal_verified'] and receiver['exit_code']==0
            assert receiver['received_bytes']=='16384' and receiver['frames']=='4'
        accepted=[]
        for line in (folder/'controller/network/operations.jsonl').read_text().splitlines():
            row=json.loads(line)
            if row['event']=='accepted':accepted.append(json.loads(row['request'])['command'])
        expected={'describe':1,'prepare':1,'configure':1,'start':1,'pause':1,'resume':1,'stop':1,'breakup':1,'reset':2}
        assert {name:accepted.count(name) for name in set(accepted)}==expected,accepted
        print('PASS',case['scenario'],'bound startup configuration, selection/publication/receipt SHA, single system commands and two sealed synthetic receivers')
    print('PASS independent configuration-view network validation; same physical host, no real hardware/loading/throughput claim')


if __name__=='__main__':main()
