#!/usr/bin/env python3
"""Local actual receiver lifecycle/session binding; all raw stays under owned evidence."""
import argparse,json,signal,subprocess,tempfile
from pathlib import Path
from run_receiver_observation_tests import interrupted,sha,stop_owned_group


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['fixture','receiver','evidence']:parser.add_argument(name,type=Path)
    parser.add_argument('--observe-reset',action='store_true',help='Observation spans actual completed Reset/source object release')
    args=parser.parse_args();args.fixture=args.fixture.resolve();args.receiver=args.receiver.resolve();args.evidence.mkdir(parents=True,exist_ok=True)
    source=Path(__file__).resolve().parents[1];folder=Path(tempfile.mkdtemp(prefix='local-ice-ecc-',dir=args.evidence));signal.signal(signal.SIGINT,interrupted);signal.signal(signal.SIGTERM,interrupted)
    deleted=[];results=[];process=None
    for minor in [0,1]:
        root=folder/str(minor);root.mkdir()
        try:
            with (root/'fixture.log').open('w') as log:
                command=[str(args.fixture),str(source/'tests/data/cobo-hardware.json'),str(source/'tests/data/mutant-hardware.json'),str(args.receiver),str(root),str(minor)]
                if args.observe_reset:command.append('observe-reset')
                process=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
                code=process.wait(timeout=600)
            assert code==0,(minor,(root/'fixture.log').read_text());results.append({'encoding':minor,'passed':True})
            if args.observe_reset:
                proof=json.loads((root/'source-reset-proof.json').read_text());assert proof['passed'] and proof['observation_spanned_completed_reset'] and proof['new_source_set_empty'] and proof['captured_operation_id']=='breakup'
        finally:
            stop_owned_group(process);process=None
            for path in root.rglob('*.graw'):deleted.append({'path':str(path.relative_to(folder)),'bytes':path.stat().st_size,'sha256':sha(path)});path.unlink()
            (folder/'payload-cleanup.json').write_text(json.dumps({'deleted':deleted,'remaining_raw':len(list(folder.rglob('*.graw')))},indent=2)+'\n')
    (folder/'result.json').write_text(json.dumps({'schema':1,'cases':results,'maximum_aggregate_test_bytes':2*1024**2,'aggregate_limit_bytes':20*1024**3,'raw_deleted':not list(folder.rglob('*.graw')),
        'fixture_sha256':sha(args.fixture),'receiver_sha256':sha(args.receiver),'runner_sha256':sha(Path(__file__)),'real_hardware':False,'observation_spanned_reset':args.observe_reset},indent=2)+'\n')
    print('PASS local native ECC ICE, both encodings, actual receivers/source sessions; raw deleted;',folder)


if __name__=='__main__':main()
