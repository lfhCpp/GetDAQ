#!/usr/bin/env python3
"""Freeze optional ICE ingress programs and bounded cleaned protocol evidence."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def sha(path):
    value=hashlib.sha256()
    with path.open('rb') as file:
        for block in iter(lambda:file.read(1024*1024),b''):value.update(block)
    return value.hexdigest()


def read(path):return json.loads(path.read_text())


def one(parent,pattern):
    matches=list(parent.glob(pattern));assert len(matches)==1,(str(parent),pattern,len(matches))
    return matches[0]


def verify(target,current=None):
    manifest=read(target/'files-sha256.json');inputs=read(target/'input-sha256.json')
    assert {str(p.relative_to(target)) for p in target.rglob('*') if p.is_file()}==set(manifest)|{'files-sha256.json'}
    for name,digest in manifest.items():
        path=target/name;assert not path.is_symlink() and path.suffix!='.graw' and sha(path)==digest,name
        assert name.startswith('verified-sources/') or not any(word in name.lower() for word in ['credential','token','bearer'])
    for name,digest in inputs.items():
        assert sha(target/'verified-sources'/name)==digest,name
        if current:assert sha(current/name)==digest,name
    for label in ['ordinary','sanitized']:
        folder=target/label;receipt=read(folder/'build/build-receipt.json');result=read(folder/'evidence/result.json')
        assert receipt['strict_cpp11_compile_count']==18 and receipt['sanitized']==(label=='sanitized') and not receipt['qt_used'] and not receipt['local_vendor_libraries_instrumented']
        assert 'PASS ICE receiver option' in receipt['option_test_stdout']
        commands=read(folder/'build/commands.json');compiled=[r for r in commands if '-c' in r]
        assert len(compiled)==18 and all('-std=c++11' in r and '-pedantic-errors' in r for r in compiled)
        if label=='sanitized':assert all('-fsanitize=address,undefined' in r for r in compiled)
        for name,digest in receipt['program_sha256'].items():assert sha(folder/'programs'/name)==digest
        assert result['receiver_sha256']==sha(folder/'programs/getdaq-ice-receiver') and result['fixture_sha256']==sha(folder/'programs/ice-ingress-fixture')
        assert result['runner_sha256']==sha(target/'verified-sources/tests/run_ice_ingress_tests.py')
        assert len(result['cases'])==15 and result['generated_raw_deleted'] and not result['real_hardware']
        assert result['maximum_wire_bytes']==32*1024**2<result['aggregate_limit_bytes']==20*1024**3
        for row in result['cases']:
            assert not row['final_status']['sender_eof_confirmed'] and row['receiver_exit']==(0 if row['success'] else 1)
            if row['success']:assert row['final_status']['sender_end_confirmed'] and row['final_status']['completion_receipt_published']
        cases={row['case']:row for row in result['cases']}
        assert cases['complete-0']['raw_bytes']==cases['complete-1']['raw_bytes']==8192
        assert cases['byte-storage-1']['raw_bytes']==65537 and cases['byte-storage-1']['final_status']['frames']==0
        assert cases['export-1']['final_status']['export_bytes']==8208
        for name in ['legacy-0','peer-close-1','message-limit-1','reconnect-count-0']:
            assert not cases[name]['final_status']['sender_end_confirmed'] and not cases[name]['success']
        cleanup=read(folder/'evidence/payload-cleanup.json')
        assert cleanup['remaining_raw']==0 and len(cleanup['deleted'])==11 and sum(row['bytes'] for row in cleanup['deleted'])==81920+65537
        for name,digest in read(folder/'build/generated-sha256.json').items():assert sha(folder/'generated'/Path(name).name)==digest
    print('PASS ICE ingress archive:',len(manifest),'files;',len(inputs),'inputs; actual programs/ordered source end/false EOF; no raw/credentials; historical only',current is None)


def main():
    root=Path(__file__).resolve().parents[1];base=root/'build-ice-ingress';target=base/'validation-ice-ingress'
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--create',action='store_true');parser.add_argument('--historical-only',action='store_true');args=parser.parse_args()
    if not args.create:verify(target,None if args.historical_only else root);return
    assert not target.exists() and not args.historical_only
    files={};inputs={}
    def add(path,name):
        assert path.is_file() and not path.is_symlink() and path.suffix!='.graw' and name not in files
        assert '..' not in Path(name).parts and (name.startswith('verified-sources/') or not any(w in name.lower() for w in ['credential','token','bearer']))
        files[name]=path
    for label,build,parent in [('ordinary',base/'ordinary-build',base/'ordinary-export-final'),
                               ('sanitized',Path('/tmp/getdaq-ice-ingress-asan'),base/'sanitized-final')]:
        for name,digest in read(build/'source-sha256.json').items():
            assert sha(root/name)==digest and (name not in inputs or inputs[name]==digest),name
            inputs[name]=digest
        for path in build.glob('*.json'):add(path,label+'/build/'+path.name)
        for path in build.glob('*.compile.txt'):add(path,label+'/build/'+path.name)
        for path in build.glob('*.link.txt'):add(path,label+'/build/'+path.name)
        for name,digest in read(build/'build-receipt.json')['program_sha256'].items():assert sha(build/name)==digest;add(build/name,label+'/programs/'+name)
        for path,digest in read(build/'generated-sha256.json').items():assert sha(Path(path))==digest;add(Path(path),label+'/generated/'+Path(path).name)
        evidence=one(parent,'ice-ingress-*');assert not list(evidence.rglob('*.graw'))
        for path in evidence.rglob('*'):
            relative=str(path.relative_to(evidence))
            if path.is_file() and not path.is_symlink() and not any(w in relative.lower() for w in ['credential','token','bearer']):add(path,label+'/evidence/'+relative)
    for name in ['tests/archive_ice_ingress_validation.py','tests/run_ice_ingress_tests.py','tests/receiver_transport_integration.py','tests/run_receiver_observation_tests.py',
                 'source/mordicus/MDaq-release-20210208/src/mdaq/daq/IceDataSender.cpp','source/mordicus/MDaq-release-20210208/src/mdaq/daq/IceDataReceiver.cpp',
                 'docs/ICE_DATA_INGRESS.md','docs/DATA_SERVICE.md','docs/RECEIVER_TRANSPORTS.md','docs/REFACTOR_STATUS.md','docs/ARCHITECTURE.md','docs/VALIDATION.md','README.md']:
        inputs[name]=sha(root/name)
    for name in inputs:add(root/name,'verified-sources/'+name)
    target.mkdir()
    for name,path in files.items():dest=target/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,dest)
    (target/'input-sha256.json').write_text(json.dumps(inputs,indent=2)+'\n')
    manifest={str(path.relative_to(target)):sha(path) for path in sorted(target.rglob('*')) if path.is_file()}
    (target/'files-sha256.json').write_text(json.dumps(manifest,indent=2)+'\n');verify(target,root)


if __name__=='__main__':main()
