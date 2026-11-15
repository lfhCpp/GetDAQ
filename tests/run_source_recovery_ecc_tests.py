#!/usr/bin/env python3
"""Actual failed ICE stream retirement, complete original cleanup and explicit acquisition recovery."""
import argparse,json,signal,subprocess,tempfile
from pathlib import Path
from run_receiver_observation_tests import interrupted,sha,stop_owned_group


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['fixture','receiver','evidence']:parser.add_argument(name,type=Path)
    args=parser.parse_args();args.fixture=args.fixture.resolve();args.receiver=args.receiver.resolve();args.evidence.mkdir(parents=True,exist_ok=True)
    root=Path(__file__).resolve().parents[1];folder=Path(tempfile.mkdtemp(prefix='source-recovery-ecc-',dir=args.evidence));signal.signal(signal.SIGINT,interrupted);signal.signal(signal.SIGTERM,interrupted)
    results=[];deleted=[];process=None
    for minor in [0,1]:
        case=folder/str(minor);case.mkdir()
        try:
            with (case/'fixture.log').open('w') as log:
                process=subprocess.Popen([str(args.fixture),str(root/'tests/data/cobo-hardware.json'),str(root/'tests/data/mutant-hardware.json'),str(args.receiver),str(case),str(minor),'source-recovery'],stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
                assert process.wait(timeout=600)==0,(case/'fixture.log').read_text()
            source=json.loads((case/'sources.json').read_text());assert len(source['sources'])==2
            assert all(s['phase']=='available' and s['state']=='unknown' and s['failure_stop_acknowledged'] and not s['source_end_acknowledged'] for s in source['sources'])
            proof=json.loads((case/'source-recovery-denial-proof.json').read_text())
            assert proof['passed'] and proof['encoding_minor']==minor and proof['denied_recovery_attempts']==2
            assert proof['unknown_source_disconnect_rejected'] and proof['original_start_complete'] and proof['original_receivers_retained']
            assert proof['recovery_hardware_writes']==0 and proof['complete_retirement_not_reached']
            recovered=json.loads((case/'source-recovery-result.json').read_text())
            assert recovered['passed'] and recovered['encoding_minor']==minor and recovered['original_start_complete']
            assert recovered['original_raw_unsealed'] and recovered['retired_source_unknown_preserved'] and recovered['recovery_hardware_writes']==0
            assert recovered['fresh_cycle_end_and_seal_verified']
            old=json.loads((case/'old-retired-data.json').read_text())
            assert old['state']=='retired' and all(not r['seal_verified'] and not r.get('source_end_confirmed',False) for r in old['receivers'])
            for r in old['receivers']:assert not (Path(r['output_directory'])/'.getdaq-ice-receiver-seal.json').exists()
            results.append({'encoding':minor,'passed':True,'source':source,'recovery':recovered})
        finally:
            stop_owned_group(process);process=None
            for path in case.rglob('*.graw'):deleted.append({'path':str(path.relative_to(folder)),'bytes':path.stat().st_size,'sha256':sha(path)});path.unlink()
            (folder/'payload-cleanup.json').write_text(json.dumps({'deleted':deleted,'remaining_raw':len(list(folder.rglob('*.graw')))},indent=2)+'\n')
    (folder/'result.json').write_text(json.dumps({'schema':1,'cases':results,'maximum_aggregate_test_bytes':2*1024**2,'aggregate_limit_bytes':20*1024**3,'raw_deleted':not list(folder.rglob('*.graw')),
        'fixture_sha256':sha(args.fixture),'receiver_sha256':sha(args.receiver),'runner_sha256':sha(Path(__file__)),'real_hardware':False},indent=2)+'\n')
    print('PASS source recovery ECC both encodings; old raw/UNKNOWN retained, full cleanup and fresh normal End/seal; raw deleted;',folder)


if __name__=='__main__':main()
