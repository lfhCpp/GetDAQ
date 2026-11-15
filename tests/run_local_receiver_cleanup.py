#!/usr/bin/env python3
"""Run one real local ECC/receiver fixture; remove only its verified new synthetic payload."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile
import signal
from run_receiver_observation_tests import interrupted, sha, stop_owned_group


def main():
    source=Path(__file__).resolve().parents[1]
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('fixture',type=Path);parser.add_argument('receiver',type=Path);parser.add_argument('evidence',type=Path)
    parser.add_argument('--scenario',choices=['normal-0','normal-1','fdt-0','fdt-empty-0'],default='normal-0')
    args=parser.parse_args();args.evidence.mkdir(parents=True,exist_ok=True)
    folder=Path(tempfile.mkdtemp(prefix='local-receiver-check-',dir=args.evidence))
    signal.signal(signal.SIGINT,interrupted);signal.signal(signal.SIGTERM,interrupted)
    baseline={p.resolve() for p in Path('/tmp').glob('getdaq-native-receivers-*')}
    process=None;deleted=[];roots=[]
    command=[str(args.fixture.resolve()),str(source/'tests/data/cobo-hardware.json'),str(source/'tests/data/mutant-hardware.json'),str(args.receiver.resolve()),str(folder/'audits.jsonl'),'--scenario',args.scenario]
    try:
        with (folder/'fixture.log').open('w') as log:
            process=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
            code=process.wait(timeout=180)
        assert code==0,(folder/'fixture.log').read_text()
    finally:
        stop_owned_group(process)
        indexed=set()
        marker=folder/'audits.jsonl.fixture-root.json'
        if marker.is_file():
            directory=Path(json.loads(marker.read_text())['directory']).resolve()
            assert directory.parent==Path('/tmp') and directory.name.startswith('getdaq-native-receivers-')
            indexed.add(directory)
        if (folder/'audits.jsonl').is_file():
            for line in (folder/'audits.jsonl').read_text().splitlines():
                try:directory=Path(json.loads(line)['directory'])
                except (ValueError,KeyError):continue
                indexed.update(p.resolve() for p in directory.parents if p.parent==Path('/tmp') and p.name.startswith('getdaq-native-receivers-'))
        candidates=[p for p in Path('/tmp').glob('getdaq-native-receivers-*') if p.resolve() not in baseline]
        # This fixture itself chooses a mkdtemp root. Verify its fixed plan before touching payload.
        for root in candidates:
            if root.resolve() not in indexed:continue # 不能仅凭同名夹具签名触碰其他会话。
            plan_path=root/'supervisor/plan.json'
            if root.is_symlink() or not plan_path.is_file():continue
            plan=json.loads(plan_path.read_text())
            if plan.get('deployment_id')!='actual-receiver-fixture' or plan.get('executable')!=str(args.receiver.resolve()):continue
            if any(row.get('output_root')!=str(root/'outputs') for row in plan.get('sources',[])):continue
            roots.append(str(root))
            for path in root.rglob('*.graw'):
                assert path.is_file() and not path.is_symlink()
                deleted.append({'path':str(path),'bytes':path.stat().st_size,'sha256':sha(path)});path.unlink()
        (folder/'payload-cleanup.json').write_text(json.dumps({'verified_fixture_roots':roots,'deleted':deleted,'remaining_raw':sum(len(list(Path(root).rglob('*.graw'))) for root in roots)},indent=2)+'\n')
    assert len(roots)==1 and sum(row['bytes'] for row in deleted)<=1024**2
    receipt={'schema':1,'passed':True,'command':command,'maximum_wire_bytes':1024**2,'aggregate_limit_bytes':20*1024**3,
             'generated_raw_deleted':True,'real_hardware':False,'fixture_sha256':sha(args.fixture.resolve()),'receiver_sha256':sha(args.receiver.resolve()),'runner_sha256':sha(Path(__file__))}
    (folder/'result.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print('PASS local manager protocol mismatch rejection and actual lifecycle; generated raw deleted;',folder)


if __name__=='__main__':main()
