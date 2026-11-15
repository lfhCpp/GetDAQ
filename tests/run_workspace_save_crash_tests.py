#!/usr/bin/env python3
"""Terminate a real Save All writer after its first synced file, then recover by record."""
import argparse
import json
import signal
import subprocess
import tempfile
from pathlib import Path
from run_receiver_observation_tests import interrupted, sha, stop_owned_group


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('probe',type=Path)
    p.add_argument('evidence',type=Path)
    args=p.parse_args()
    args.evidence.mkdir(parents=True,exist_ok=True)
    folder=Path(tempfile.mkdtemp(prefix='save-crash-',dir=args.evidence)).resolve()
    signal.signal(signal.SIGINT,interrupted)
    signal.signal(signal.SIGTERM,interrupted)
    probe=args.probe.resolve()
    process=None
    try:
        process=subprocess.Popen([str(probe),'crash',str(folder/'sources')],stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE,start_new_session=True,text=True)
        out,err=process.communicate(timeout=30)
        (folder/'crash.log').write_text(out+err)
        assert process.returncode==77,'Writer did not terminate at the actual after-file sync boundary'
        record=Path(out.strip())
        assert folder in record.parents and (record/'manifest.json').is_file()
        assert not (record/'complete.json').exists()
        assert b'>3<' in (folder/'sources/a.xcfg').read_bytes()
        assert b'>2<' in (folder/'sources/b.xcfg').read_bytes()
        process=subprocess.Popen([str(probe),'recover',str(record)],stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE,start_new_session=True,text=True)
        out,err=process.communicate(timeout=30)
        (folder/'recovery.log').write_text(out+err)
        assert process.returncode==0,err
        assert (record/'complete.json').is_file()
        assert all(b'>3<' in (folder/'sources'/n).read_bytes() for n in ('a.xcfg','b.xcfg'))
        assert not list((folder/'sources').glob('.getdaq-workspace-save-*.json'))
        (folder/'result.json').write_text(json.dumps({'passed':True,'actual_writer_exit_code':77,
            'mixed_sources_observed':True,'separate_recovery_process':True,
            'completion_and_marker_cleanup':True,'raw_remaining':0,
            'probe_sha256':sha(probe),'maximum_test_bytes':4*1024**2,
            'power_loss_verified':False,'abrupt_child_lsan_verified':False},indent=2)+'\n')
    finally:
        stop_owned_group(process)
        assert not list(folder.rglob('*.graw'))
    print('PASS actual writer interruption, mixed sources, separate process recovery;',folder)


if __name__=='__main__':
    main()
