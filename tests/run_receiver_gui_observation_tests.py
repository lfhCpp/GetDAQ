#!/usr/bin/env python3
"""Real ECC/data-service observation integration; prune only owned synthetic raw files."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile
import signal
from run_receiver_observation_tests import interrupted, sha, stop_owned_group


def main():
    source = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('native_build', type=Path)
    parser.add_argument('receiver', type=Path)
    parser.add_argument('data_service', type=Path)
    parser.add_argument('evidence', type=Path)
    parser.add_argument('--scenarios',nargs=3,choices=['normal-0','normal-1','truncated','zbuf-0','zbuf-1'],default=['normal-0','normal-1','truncated'])
    args = parser.parse_args(); native = args.native_build.resolve()
    args.evidence.mkdir(parents=True, exist_ok=True)
    folder = Path(tempfile.mkdtemp(prefix='receiver-gui-service-', dir=args.evidence))
    signal.signal(signal.SIGINT, interrupted); signal.signal(signal.SIGTERM, interrupted)
    programs = {'fixture': native/'ecc-network-service-tests', 'ecc_service': native/'getdaq-ecc-server',
                'receiver': args.receiver.resolve(), 'data_service': args.data_service.resolve()}
    # Fixed local fixture: two sources, two cycles, <=64 x4KiB per source/cycle/case.
    maximum_wire_bytes = 3*2*2*64*4096
    assert maximum_wire_bytes <= 20*1024**3
    command = ['python3', str(source/'tests/run_ecc_network_service_tests.py'), str(programs['fixture']),
               str(programs['receiver']), str(programs['data_service']), str(programs['ecc_service']),
               '--output-parent', str(folder), '--jobs', '1', '--scenarios']+args.scenarios
    process = None; deleted = []
    try:
        with (folder/'runner.log').open('w') as log:
            process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
            code = process.wait(timeout=600)
        assert code == 0, (folder/'runner.log').read_text()
        root = next(folder.glob('ecc-network-system-*'))
        results = json.loads((root/'results.json').read_text()); assert len(results) == 3 and all(row['exit_code'] == 0 for row in results)
        for name in args.scenarios:
            fresh = json.loads((root/name/'fresh-receiver-observation.json').read_text())
            ready = json.loads((root/name/'ready-receiver-observation.json').read_text())
            assert len(fresh['nodes']) == len(ready['nodes']) == 2
            assert all(node['available'] and not node['observation']['receivers'] for node in fresh['nodes'])
            assert all(node['available'] and not node['observation']['busy'] and len(node['observation']['receivers']) == 1 and
                       node['observation']['receivers'][0]['state'] == 'running' for node in ready['nodes'])
            if name.startswith('zbuf-'):
                snapshot=json.loads((root/name/'controller/snapshot.json').read_text())
                assert all(sender['protocol']=='ZBUF' for sender in snapshot['senders'])
                plans=list((root/name).glob('node-*/deployment/plan.json'));assert len(plans)==2
                assert all(source['input_protocol']=='ZBUF' for plan in plans for source in json.loads(plan.read_text())['sources'])
        if 'normal-0' in args.scenarios:
            partial = json.loads((root/'normal-0/partial-receiver-observation.json').read_text())
            timed = json.loads((root/'normal-0/timed-receiver-observation.json').read_text())
            assert not partial['nodes'][0]['available'] and partial['nodes'][1]['available']
            assert all(not node['available'] and node['observation'] is None for node in timed['nodes'])
    finally:
        stop_owned_group(process)
        for path in folder.rglob('*.graw'):
            deleted.append({'path': str(path.relative_to(folder)), 'bytes': path.stat().st_size, 'sha256': sha(path)})
            path.unlink()
        (folder/'payload-cleanup.json').write_text(json.dumps({'deleted': deleted, 'raw_files_remaining': len(list(folder.rglob('*.graw')))}, indent=2)+'\n')
    receipt = {'schema': 1, 'passed': True, 'results': results, 'maximum_wire_bytes': maximum_wire_bytes,
               'aggregate_limit_bytes': 20*1024**3, 'generated_raw_deleted': not list(folder.rglob('*.graw')),
               'real_hardware': False, 'qt_gui_executed': False, 'program_sha256': {name: sha(path) for name,path in programs.items()},
               'runner_sha256': sha(Path(__file__)), 'underlying_runner_sha256': sha(source/'tests/run_ecc_network_service_tests.py')}
    (folder/'result.json').write_text(json.dumps(receipt, indent=2)+'\n')
    print('PASS actual ECC/data-service/receiver observation, normal cycles and truncated tail; raw payload deleted;', folder)


if __name__ == '__main__':
    main()
