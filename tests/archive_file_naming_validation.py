#!/usr/bin/env python3
"""Freeze GET naming, plan binding and cleanup evidence without raw data or tokens."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(target,current=None):
    manifest=json.loads((target/'files-sha256.json').read_text())
    assert {str(p.relative_to(target)) for p in target.rglob('*') if p.is_file()}==set(manifest)|{'files-sha256.json'}
    for name,digest in manifest.items():assert sha(target/name)==digest,name
    assert not list(target.rglob('*.graw'))
    inputs=json.loads((target/'input-sha256.json').read_text())
    for name,digest in inputs.items():
        assert sha(target/'verified-sources'/name)==digest
        if current:assert sha(current/name)==digest,name
    for label in ('ordinary','sanitized'):
        for kind,count,total in [('receiver',8,18),('supervisor',49,81)]:
            folder=target/label/kind;receipt=json.loads((folder/'build-receipt.json').read_text())
            assert receipt['sanitized']==(label=='sanitized') and receipt['compile_count']==count
            assert len(json.loads((folder/'source-sha256.json').read_text()))==total
            for name,digest in receipt['program_sha256'].items():assert sha(folder/name)==digest
            for recipe in json.loads((folder/'verified-compile-commands.json').read_text()):
                assert '-std=c++11' in recipe['command'] and '-pedantic-errors' in recipe['command']
                if label=='sanitized':assert '-fsanitize=address,undefined' in recipe['command']
        receiver=target/label/'receiver'
        result=json.loads((target/label/'naming-evidence/result.json').read_text())
        assert result['receiver_sha256']==sha(receiver/'getdaq-receiver') and result['runner_sha256']==sha(target/'verified-sources/tests/receiver_file_naming_integration.py')
        assert result['aggregate_wire_bytes']==13676684<=result['aggregate_limit_bytes']==20*1024**3
        assert result['generated_raw_deleted'] and not result['real_hardware'] and len(result['cases'])==4
        assert sum(len(case['files']) for case in result['cases'])==22
        for case in result['cases']:
            cleanup=json.loads((target/label/'naming-evidence'/case['case']/'payload-cleanup.json').read_text())
            assert cleanup['raw_files_remaining']==0 and {x['name']:x['sha256'] for x in cleanup['deleted']}=={x['name']:x['sha256'] for x in case['files']}
            assert case['final_status']['file_naming']=='get' and case['final_status']['stream_sealed'] and case['final_status']['sender_eof_confirmed']
        managed=json.loads((target/label/'managed-evidence/result.json').read_text())
        assert len(managed['cases'])==17 and all(case['exit_code']==0 for case in managed['cases'])
        assert managed['receiver_sha256']==sha(receiver/'getdaq-receiver') and managed['fixture_sha256']==sha(target/label/'supervisor/data-supervisor-tests')
        assert managed['maximum_wire_bytes']==3342336 and managed['generated_raw_deleted']
        assert managed['runner_sha256']==sha(target/'verified-sources/tests/run_file_naming_supervisor_tests.py')
        http=json.loads((target/label/'http-evidence/result.json').read_text())
        assert http['passed'] and http['generated_raw_deleted'] and not http['real_hardware']
        assert all(row['exit_code']==0 for row in http['results'])
        assert http['program_sha256']['service']==sha(target/label/'supervisor/getdaq-supervisor')
        assert http['program_sha256']['fixture']==sha(target/label/'supervisor/supervisor-control-tests')
        assert http['program_sha256']['receiver']==sha(receiver/'getdaq-receiver')
        assert 'PASS 27 native catalog checks' in (target/label/'supervisor/results-file-naming-catalog-final.txt').read_text()
        assert (receiver/'results-get-naming-native-regression.txt').read_text().count('PASS:')==5
    reference=json.loads((target/'reference/result.json').read_text())
    assert len(reference['timestamp_cases'])==27 and reference['native_plan_cases']==6 and len(reference['actual_filename_comparisons'])==21
    assert reference['program_sha256']==sha(target/'reference/reference-probe') and reference['raw_payload_bytes']==0
    cleanup=json.loads((target/'legacy-catalog-fixture-cleanup.json').read_text())
    assert cleanup['deleted_paths']==221 and cleanup['deleted_logical_bytes']==5369806911 and len(cleanup['roots'])==43 and not cleanup['real_runs_affected']
    assert all(row['receipt']['raw_files_remaining']==0 for row in cleanup['roots'])
    integrated=json.loads((target/'integrated-normal/receipt.json').read_text());assert not integrated['sanitized'] and not integrated['real_hardware']
    for name,digest in integrated['program_sha256'].items():assert sha(target/'integrated-normal'/name)==digest
    print('PASS file naming archive:',len(manifest),'files;',len(inputs),'inputs; four strict builds; no raw payload/tokens; historical only',current is None)


def main():
    root=Path(__file__).resolve().parents[1]
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--create',action='store_true');parser.add_argument('--historical-only',action='store_true')
    args=parser.parse_args();target=root/'build-file-naming-supervisor/validation-file-naming'
    if not args.create:verify(target,None if args.historical_only else root);return
    assert not target.exists() and not args.historical_only
    files,inputs={},{};secrets=[]
    def add(path,name,evidence=False):
        assert path.is_file() and path.suffix!='.graw' and name not in files and '..' not in Path(name).parts
        if evidence and any('credential' in part.lower() for part in Path(name).parts):return
        files[name]=path
    def tree(parent,name):
        roots=[p for p in parent.iterdir() if p.is_dir()];assert len(roots)==1
        folder=roots[0];assert not list(folder.rglob('*.graw'))
        for token in folder.rglob('credential'):
            if token.is_file() and not token.is_symlink():
                value=token.read_bytes().strip()
                if len(value)>=32:secrets.append(value)
        for path in folder.rglob('*'):
            if path.is_file():add(path,name+'/'+str(path.relative_to(folder)),True)
    for label,receiver,supervisor in [('ordinary',root/'build-receiver',root/'build-file-naming-supervisor'),('sanitized',Path('/tmp/getdaq-file-naming-receiver-asan'),Path('/tmp/getdaq-file-naming-supervisor-asan'))]:
        for kind,build in [('receiver',receiver),('supervisor',supervisor)]:
            prefix=label+'/'+kind+'/'
            receipt=json.loads((build/'build-receipt.json').read_text())
            for name,digest in json.loads((build/'source-sha256.json').read_text()).items():
                assert sha(root/name)==digest and (name not in inputs or inputs[name]==digest);inputs[name]=digest
            for name,digest in receipt['program_sha256'].items():assert sha(build/name)==digest;add(build/name,prefix+name)
            for name in ['build-receipt.json','source-sha256.json','verified-compile-commands.json','results-configure-verified.txt','results-build-verified.txt']:add(build/name,prefix+name)
            logs=['results-get-naming-core-current.txt','results-get-naming-integration-current.txt','results-get-naming-native-regression.txt','results-get-naming-transport-regression.txt','results-get-naming-export-regression.txt'] if kind=='receiver' else ['results-managed-file-naming-final-v2.txt','results-file-naming-http-current.txt','results-file-naming-codec-final.txt','results-file-naming-catalog-final.txt']
            for name in logs:add(build/name,prefix+name)
            if kind=='supervisor':
                generated=json.loads((build/'generated-sha256.json').read_text());add(build/'generated-sha256.json',prefix+'generated-sha256.json')
                for name,digest in generated.items():assert sha(build/name)==digest;add(build/name,prefix+'generated/'+name)
        tree(root/('build-receiver/get-naming-'+label),label+'/naming-evidence')
        tree(root/('build-file-naming-supervisor/managed-naming-'+label+'-final-v2'),label+'/managed-evidence')
        tree(root/('build-file-naming-supervisor/http-'+label),label+'/http-evidence')
        tree(root/('build-receiver/get-naming-transport-'+label),label+'/transport-evidence')
        tree(root/('build-receiver/get-naming-export-'+label),label+'/export-evidence')
    tree(root/'build-file-naming-supervisor/reference-final','reference')
    for name in ['legacy-catalog-fixture-cleanup.json','catalog-fixture-cleanup-verification.json']:add(root/'build-file-naming-supervisor'/name,name)
    receipt_path=root/'build-file-naming-supervisor/integrated-normal-receipt.json';receipt=json.loads(receipt_path.read_text());add(receipt_path,'integrated-normal/receipt.json')
    for name,digest in receipt['source_sha256'].items():assert sha(root/name)==digest;inputs[name]=digest
    for name in receipt['program_sha256']:add(root/'build-ice-compat'/name,'integrated-normal/'+name)
    add(root/'build-ice-compat/results-file-naming-current-build.txt','integrated-normal/results-build.txt')
    for name in ['tests/archive_file_naming_validation.py','tests/receiver_file_naming_integration.py','tests/run_file_naming_supervisor_tests.py',
                 'tests/run_file_naming_service_tests.py','tests/run_supervisor_control_tests.py','tests/file_naming_original_reference.py',
                 'tests/receiver_integration.py','tests/receiver_transport_integration.py','tests/exporter_reconnect_integration.py',
                 'docs/RECEIVER_FILE_NAMING.md','docs/DATA_SERVICE.md','docs/ARCHITECTURE.md','docs/REFACTOR_STATUS.md','docs/VALIDATION.md','README.md',
                 'source/get/GetBench-software-release-20210208/src/get/utl/Time.cpp','source/get/GetBench-software-release-20210208/src/get/daq/FrameStorage.cpp']:
        inputs[name]=sha(root/name)
    old=root/'build-ecc-json/validation-native-json/verified-sources/apps/datasupervisor/supervisorio.cpp';add(old,'reference/frozen-supervisorio.cpp')
    for name in inputs:add(root/name,'verified-sources/'+name)
    # Scan all selected text and programs for exact actual fixture tokens before copying.
    for path in files.values():
        data=path.read_bytes();assert not any(secret in data for secret in secrets),path
    target.mkdir()
    for name,path in files.items():
        dest=target/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,dest)
    (target/'input-sha256.json').write_text(json.dumps(inputs,indent=2)+'\n')
    manifest={str(path.relative_to(target)):sha(path) for path in sorted(target.rglob('*')) if path.is_file()}
    (target/'files-sha256.json').write_text(json.dumps(manifest,indent=2)+'\n');verify(target,root)


if __name__=='__main__':main()
