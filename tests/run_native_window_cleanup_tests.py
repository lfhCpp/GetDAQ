#!/usr/bin/env python3
"""Real MainWindow cleanup across ECC/data processes; bounded synthetic payload, own cleanup."""
import argparse,json,os,signal,subprocess,tempfile
from pathlib import Path
from run_receiver_observation_tests import interrupted,sha,stop_owned_group

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['fixture','receiver','data_service','ecc_service','probe','evidence']:parser.add_argument(name,type=Path)
    args=parser.parse_args();root=Path(__file__).resolve().parents[1]
    args.evidence.mkdir(parents=True,exist_ok=True);folder=Path(tempfile.mkdtemp(prefix='real-window-cleanup-',dir=args.evidence)).resolve()
    runtime=folder/'runtime';runtime.mkdir(mode=0o700)
    env=dict(os.environ,QT_QPA_PLATFORM='offscreen',XDG_RUNTIME_DIR=str(runtime))
    maximum=4*1024**2;assert maximum<20*1024**3
    signal.signal(signal.SIGINT,interrupted);signal.signal(signal.SIGTERM,interrupted)
    programs={name:getattr(args,name).resolve() for name in ['fixture','receiver','data_service','ecc_service','probe']}
    rows=[];deleted=[];process=None
    try:
        for name in ['qt-window-cleanup-0','qt-window-cleanup-1']:
            case=folder/name
            command=[str(programs['fixture']),str(root/'tests/data/cobo-hardware.json'),str(root/'tests/data/mutant-hardware.json'),str(programs['receiver']),str(programs['data_service']),str(programs['ecc_service']),str(case),'--scenario',name,str(programs['probe'])]
            with (folder/(name+'.log')).open('w') as log:
                try:
                    process=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT,env=env,start_new_session=True);code=process.wait(timeout=480)
                    rows.append({'scenario':name,'exit_code':code});assert code==0,(folder/(name+'.log')).read_text()[-4000:]
                    proof=json.loads((case/'gui-runs/hardware/NativeIntegration/run_0000/window-cleanup-proof.json').read_text())
                    assert proof['passed'] and proof['actual_main_window'] and proof['cancel_requests']==0 and proof['confirmed_requests']==1 and proof['duration_incomplete']
                finally:stop_owned_group(process);process=None
    finally:
        stop_owned_group(process)
        for path in folder.rglob('*.graw'):
            if path.is_file() and not path.is_symlink():deleted.append({'path':str(path.relative_to(folder)),'bytes':path.stat().st_size,'sha256':sha(path)});path.unlink()
        (folder/'payload-cleanup.json').write_text(json.dumps({'deleted':deleted,'remaining_raw':[str(p.relative_to(folder)) for p in folder.rglob('*.graw')]},indent=2)+'\n')
        (folder/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
    assert sum(row['bytes'] for row in deleted)<=maximum and not list(folder.rglob('*.graw'))
    (folder/'scope.json').write_text(json.dumps({'passed':True,'actual_main_window':True,'cases':2,'maximum_aggregate_test_bytes':maximum,'raw_deleted':True,'real_hardware':False,'programs':{name:{'path':str(p),'sha256':sha(p)} for name,p in programs.items()}},indent=2)+'\n')
    print('PASS real MainWindow confirm/cancel -> ECC -> two data hosts -> original receivers; Run incomplete; raw removed;',folder)

if __name__=='__main__':main()
