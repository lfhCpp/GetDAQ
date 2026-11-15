#!/usr/bin/env python3
"""Freeze native TCP/ZBUF/FDT evidence without generated raw payload."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(target, current=None):
    manifest = json.loads((target/'files-sha256.json').read_text())
    assert {str(p.relative_to(target)) for p in target.rglob('*') if p.is_file()} == set(manifest)|{'files-sha256.json'}
    for name,digest in manifest.items(): assert sha(target/name)==digest, name
    assert not list(target.rglob('*.graw'))
    inputs = json.loads((target/'input-sha256.json').read_text())
    for name,digest in inputs.items():
        assert sha(target/'verified-sources'/name)==digest
        if current: assert sha(current/name)==digest, name
    runner = sha(target/'verified-sources/tests/receiver_transport_integration.py')
    for label in ('ordinary','sanitized'):
        folder=target/label
        receipt=json.loads((folder/'build-receipt.json').read_text())
        assert receipt['sanitized']==(label=='sanitized') and receipt['compile_count']==8
        assert not receipt['raw_path_uses_qt']
        for name,digest in receipt['program_sha256'].items(): assert sha(folder/name)==digest
        recipes=json.loads((folder/'verified-compile-commands.json').read_text())
        assert len(recipes)==8
        for recipe in recipes:
            assert '-std=c++11' in recipe['command'] and '-pedantic-errors' in recipe['command']
            if label=='sanitized': assert '-fsanitize=address,undefined' in recipe['command']
        result=json.loads((folder/'evidence/result.json').read_text())
        assert result['receiver_sha256']==sha(folder/'getdaq-receiver') and result['runner_sha256']==runner
        assert result['aggregate_wire_bytes']==49739 and result['aggregate_wire_bytes']<=result['aggregate_limit_bytes']==20*1024**3
        assert result['generated_raw_deleted'] and not result['real_hardware'] and len(result['cases'])==12
        assert result['invalid_protocols_and_managed_fdt_rejected']
        assert len({case['case'] for case in result['cases']})==12
        for case in result['cases']:
            evidence=folder/'evidence'/case['case']
            cleanup=json.loads((evidence/'payload-cleanup.json').read_text())
            assert cleanup['raw_files_remaining']==0
            assert sum(item['bytes'] for item in cleanup['deleted'])==case['raw_bytes']
            for item in cleanup['deleted']: assert len(item['sha256'])==64
            success=case['return_code']==0
            final=case['final_status']
            assert final['stream_sealed']==success and bool(final['error'])==(not success)
            assert (evidence/'.getdaq-receiver-seal.json').exists()==success
            if success: assert final['received_bytes']==final['sealed_bytes']==case['raw_bytes']
            if 'fdt_control_bytes' in final:
                assert final['wire_received_bytes']==case['wire_bytes']
                assert final['fdt_control_bytes']==4*(final['fdt_heartbeats']+final['fdt_goodbyes'])
        live=json.loads((folder/'evidence/fdt-fragmented-export/status-after-goodbye-before-eof.json').read_text())
        assert live['connected'] and live['fdt_goodbyes']==1 and not live['stream_sealed'] and not live['sender_eof_confirmed']
        assert (folder/'results-transports-core-current.txt').read_text().startswith('PASS:')
        assert (folder/'results-transports-raw-regression-final.txt').read_text().count('PASS:')==5
    print('PASS receiver transport archive:',len(manifest),'files;',len(inputs),'inputs; no raw payload; historical only',current is None)


def main():
    root=Path(__file__).resolve().parents[1]
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--create',action='store_true')
    parser.add_argument('--historical-only',action='store_true')
    args=parser.parse_args()
    target=root/'build-receiver/validation-transports'
    if not args.create: verify(target,None if args.historical_only else root); return
    assert not target.exists() and not args.historical_only
    files,inputs={},{}
    def add(path,name):
        assert path.is_file() and name not in files and '..' not in Path(name).parts
        assert path.suffix!='.graw' and 'credential' not in path.name.lower()
        files[name]=path
    for label,build in [('ordinary',root/'build-receiver'),('sanitized',Path('/tmp/getdaq-receiver-transports-asan'))]:
        for name,digest in json.loads((build/'source-sha256.json').read_text()).items():
            assert sha(root/name)==digest
            assert name not in inputs or inputs[name]==digest
            inputs[name]=digest
        receipt=json.loads((build/'build-receipt.json').read_text())
        for name in receipt['program_sha256']: add(build/name,label+'/'+name)
        for name in ['build-receipt.json','source-sha256.json','verified-compile-commands.json',
                     'results-configure-verified.txt','results-build-verified.txt',
                     'results-transports-core-current.txt','results-transports-integration-current.txt',
                     'results-transports-raw-regression-final.txt']:
            add(build/name,label+'/'+name)
        roots=list((root/('build-receiver/transports-'+label)).glob('receiver-transports-*'))
        assert len(roots)==1 and not list(roots[0].rglob('*.graw'))
        for path in roots[0].rglob('*'):
            if path.is_file(): add(path,label+'/evidence/'+str(path.relative_to(roots[0])))
    for name in ['tests/receiver_transport_integration.py','tests/receiver_integration.py',
                 'tests/archive_receiver_transports_validation.py','docs/RECEIVER_TRANSPORTS.md',
                 'docs/DATA_SERVICE.md','docs/PROTOCOLS.md','docs/ARCHITECTURE.md',
                 'docs/REFACTOR_STATUS.md','docs/VALIDATION.md','README.md','third_party/README.md',
                 'source/get/GetBench-software-release-20210208/src/get/daq/DataRouter.cpp',
                 'source/get/GetBench-software-release-20210208/src/get/daq/FdtDataReceiver.cpp',
                 'source/mordicus/MDaq-release-20210208/src/mdaq/daq/FdtDataSender.hpp',
                 'source/mordicus/MDaq-release-20210208/src/mdaq/utl/net/ZBufConnection.cpp']:
        inputs[name]=sha(root/name)
    for name in inputs: add(root/name,'verified-sources/'+name)
    add(root/'build-suite/bin/getdaq-sentry','programs/getdaq-sentry-ordinary')
    # Preserve the early wrong executable path failure as evidence, not a successful gate.
    add(root/'build-receiver/results-transports-raw-regression-current.txt','ordinary/results-early-missing-sentry.txt')
    target.mkdir()
    for name,path in files.items():
        dest=target/name; dest.parent.mkdir(parents=True,exist_ok=True); shutil.copy2(path,dest)
    (target/'input-sha256.json').write_text(json.dumps(inputs,indent=2)+'\n')
    manifest={str(p.relative_to(target)):sha(p) for p in sorted(target.rglob('*')) if p.is_file()}
    (target/'files-sha256.json').write_text(json.dumps(manifest,indent=2)+'\n')
    verify(target,root)


if __name__=='__main__': main()
