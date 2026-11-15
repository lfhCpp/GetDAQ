#!/usr/bin/env python3
"""Native copy/resume regression with owned roots, bounded payload and cleanup."""
import argparse,json,os,signal,subprocess,tempfile
from pathlib import Path
from run_receiver_observation_tests import interrupted,sha,stop_owned_group

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('fixture','catalog','evidence'):p.add_argument(name,type=Path)
    a=p.parse_args();a.evidence.mkdir(parents=True,exist_ok=True)
    folder=Path(tempfile.mkdtemp(prefix='catalog-resume-',dir=a.evidence)).resolve()
    temporary=folder/'temporary';temporary.mkdir(mode=0o700)
    shared=Path(tempfile.mkdtemp(prefix='getdaq-resume-',dir='/dev/shm'))
    signal.signal(signal.SIGINT,interrupted);signal.signal(signal.SIGTERM,interrupted)
    process=None;deleted=[];maximum=256*1024**2
    try:
        env=dict(os.environ,GETDAQ_CATALOG_TEST_ROOT=str(temporary),GETDAQ_TRANSFER_TEST_SHM_ROOT=str(shared))
        with (folder/'tests.log').open('w') as log:
            process=subprocess.Popen([str(a.fixture.resolve()),str(a.catalog.resolve())],stdout=log,stderr=subprocess.STDOUT,env=env,start_new_session=True)
            code=process.wait(timeout=180)
        assert code==0,(folder/'tests.log').read_text()[-3000:]
    finally:
        stop_owned_group(process)
        for root in (folder,shared):
            for file in root.rglob('*.graw'):
                if file.is_file() and not file.is_symlink():deleted.append({'path':str(file),'bytes':file.stat().st_size});file.unlink()
        remaining=[str(file) for root in (folder,shared) for file in root.rglob('*.graw')]
        (folder/'cleanup.json').write_text(json.dumps({'deleted':deleted,'remaining_raw':remaining},indent=2)+'\n')
        for path in sorted(shared.rglob('*'),key=lambda p:len(p.parts),reverse=True):
            if path.is_dir() and not path.is_symlink():
                try:path.rmdir()
                except OSError:pass
        try:shared.rmdir()
        except OSError:pass
    assert not remaining
    (folder/'result.json').write_text(json.dumps({'passed':True,'maximum_aggregate_test_bytes':maximum,'raw_remaining':0,'real_receiver':False,'real_hardware':False,'program_sha256':{'fixture':sha(a.fixture.resolve()),'catalog':sha(a.catalog.resolve())}},indent=2)+'\n')
    print('PASS native copy/resume; own raw removed;',folder)
if __name__=='__main__':main()
