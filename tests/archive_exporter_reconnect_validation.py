#!/usr/bin/env python3
"""Preserve export reconnection evidence with native transport regression."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import archive_receiver_transports_validation as transport


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(target,current=None):
    transport.verify(target,current)
    runner=sha(target/'verified-sources/tests/exporter_reconnect_integration.py')
    before=json.loads((target/'before/evidence/result.json').read_text())
    assert before['legacy_demonstration'] and before['legacy_idle_slot_exhaustion_confirmed']
    assert before['status']['export_clients']==8 and before['raw_bytes']==0
    assert before['receiver_sha256']==sha(target/'before/getdaq-receiver') and before['runner_sha256']==runner
    assert before['generated_raw_deleted'] and not before['real_hardware']
    for label in ('ordinary','sanitized'):
        result=json.loads((target/label/'reconnect-evidence/result.json').read_text())
        assert result['receiver_sha256']==sha(target/label/'getdaq-receiver') and result['runner_sha256']==runner
        assert not result['legacy_demonstration'] and not result['real_hardware']
        assert result['raw_bytes']==4096 and result['export_received_bytes']==4104
        assert result['generated_raw_deleted'] and result['aggregate_limit_bytes']==20*1024**3
        assert len(result['checks'])==4
        idle=result['checks'][0]['status']
        assert idle['export_clients']==idle['frames']==idle['received_bytes']==0 and idle['export_disconnections']==20
        assert result['checks'][1]['status']['export_clients']==0
        assert result['checks'][2]['check']=='write-eof' and result['checks'][3]['check']=='unsolicited-data'
        final=result['final_status'];assert final['frames']==1 and final['stream_sealed'] and final['sender_eof_confirmed'] and not final['error']
        assert final['received_bytes']==final['sealed_bytes']==4096
        cleanup=json.loads((target/label/'reconnect-evidence/payload-cleanup.json').read_text())
        assert cleanup['raw_files_remaining']==0 and len(cleanup['deleted'])==1
        assert cleanup['deleted'][0]['bytes']==4096 and cleanup['deleted'][0]['sha256']==result['raw_sha256']
    print('PASS exporter reconnect evidence: original issue confirmed; ordinary/sanitized recovery; raw files deleted')


def main():
    root=Path(__file__).resolve().parents[1]
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--create',action='store_true');parser.add_argument('--historical-only',action='store_true')
    args=parser.parse_args();target=root/'build-receiver/validation-export-reconnect'
    if not args.create:verify(target,None if args.historical_only else root);return
    assert not target.exists() and not args.historical_only
    files,inputs={},{}
    def add(path,name):
        assert path.is_file() and path.suffix!='.graw' and 'credential' not in path.name.lower()
        assert name not in files and '..' not in Path(name).parts;files[name]=path
    def evidence(parent,name):
        roots=list(parent.iterdir());assert len(roots)==1 and roots[0].is_dir()
        assert not list(roots[0].rglob('*.graw'))
        for path in roots[0].rglob('*'):
            if path.is_file():add(path,name+'/'+str(path.relative_to(roots[0])))
    for label,build in [('ordinary',root/'build-receiver'),('sanitized',Path('/tmp/getdaq-export-reconnect-asan'))]:
        for name,digest in json.loads((build/'source-sha256.json').read_text()).items():
            assert sha(root/name)==digest and (name not in inputs or inputs[name]==digest);inputs[name]=digest
        receipt=json.loads((build/'build-receipt.json').read_text())
        for name in receipt['program_sha256']:add(build/name,label+'/'+name)
        for name in ['build-receipt.json','source-sha256.json','verified-compile-commands.json','results-configure-verified.txt','results-build-verified.txt']:
            add(build/name,label+'/'+name)
        for source,dest in [('results-export-core-final.txt','results-transports-core-current.txt'),
                            ('results-export-transport-final.txt','results-transports-integration-current.txt'),
                            ('results-export-raw-regression-final.txt','results-transports-raw-regression-final.txt'),
                            ('results-export-reconnect-final.txt','results-export-reconnect-final.txt')]:
            add(build/source,label+'/'+dest)
        evidence(root/('build-receiver/export-transport-'+label),label+'/evidence')
        evidence(root/('build-receiver/export-reconnect-'+label+'-final'),label+'/reconnect-evidence')
    evidence(root/'build-receiver/export-reconnect-before-final','before/evidence')
    previous=root/'build-receiver/validation-transports'
    add(previous/'ordinary/getdaq-receiver','before/getdaq-receiver')
    add(previous/'files-sha256.json','before/original-archive-files-sha256.json')
    for name in ('exporter.cpp','exporter.h'):
        add(previous/('verified-sources/apps/datareceiver/'+name),'before/source/'+name)
    add(root/'build-receiver/results-export-reconnect-before-final.txt','before/results-final.txt')
    add(root/'build-receiver/results-export-reconnect-current.txt','ordinary/results-initial-rst-observation.txt')
    add(root/'build-suite/bin/getdaq-sentry','programs/getdaq-sentry-ordinary')
    for name in ['tests/archive_exporter_reconnect_validation.py','tests/exporter_reconnect_integration.py',
                 'tests/archive_receiver_transports_validation.py','tests/receiver_transport_integration.py',
                 'tests/receiver_integration.py','docs/EXPORTER_RECONNECTION.md','docs/DATA_SERVICE.md',
                 'docs/RECEIVER_TRANSPORTS.md','docs/PROTOCOLS.md','docs/ARCHITECTURE.md',
                 'docs/REFACTOR_STATUS.md','docs/VALIDATION.md','README.md']:
        inputs[name]=sha(root/name)
    for name in inputs:add(root/name,'verified-sources/'+name)
    target.mkdir()
    for name,path in files.items():
        dest=target/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,dest)
    (target/'input-sha256.json').write_text(json.dumps(inputs,indent=2)+'\n')
    manifest={str(path.relative_to(target)):sha(path) for path in sorted(target.rglob('*')) if path.is_file()}
    (target/'files-sha256.json').write_text(json.dumps(manifest,indent=2)+'\n')
    verify(target,root)


if __name__=='__main__':main()
