#!/usr/bin/env python3
"""Archive actual tmpfs ENOSPC and simulated I/O errors without synthetic raw files."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(target,current=None):
    manifest=json.loads((target/'files-sha256.json').read_text());assert {str(p.relative_to(target)) for p in target.rglob('*') if p.is_file()}==set(manifest)|{'files-sha256.json'}
    for name,digest in manifest.items():assert sha(target/name)==digest,name
    assert not list(target.rglob('*.graw'))
    inputs=json.loads((target/'input-sha256.json').read_text())
    for name,digest in inputs.items():
        assert sha(target/'verified-sources'/name)==digest
        if current:assert sha(current/name)==digest,name
    for label in ('ordinary','sanitized'):
        folder=target/label;receipt=json.loads((folder/'build-receipt.json').read_text())
        assert receipt['sanitized']==(label=='sanitized') and receipt['compile_count']==8 and not receipt['raw_path_uses_qt']
        for name,digest in receipt['program_sha256'].items():assert sha(folder/name)==digest
        enospc=json.loads((folder/'enospc/result.json').read_text())
        assert enospc['receiver_sha256']==sha(folder/'getdaq-receiver') and enospc['runner_sha256']==sha(target/'verified-sources/tests/receiver_enospc_test.py')
        assert enospc['generated_raw_deleted'] and enospc['private_mount_namespace'] and not enospc['power_loss_tested']
        assert enospc['aggregate_maximum_wire_bytes']==12584192<=enospc['aggregate_limit_bytes']==20*1024**3
        assert len(enospc['cases'])==2
        for case in enospc['cases']:
            path=folder/'enospc'/case['case'];cleanup=json.loads((path/'payload-cleanup.json').read_text())
            assert cleanup['raw_files_remaining']==0 and {f['name']:f['sha256'] for f in cleanup['deleted']}=={f['name']:f['sha256'] for f in case['files']}
            assert case['actual_kernel_enospc'] and case['no_fault_interposer'] and not case['shared_disk_filled']
            assert case['return_code']==1 and 'No space left on device' in case['final_status']['error'] and not case['final_status']['stream_sealed']
            assert case['tmpfs_capacity_bytes']==8*1024**2 and case['partial_file_bytes']<=8*1024**2
            if case['case']=='bytes':assert case['free_bytes_at_failure']==0 and case['free_inodes_at_failure']>0
            else:assert case['free_inodes_at_failure']==0 and case['free_bytes_at_failure']>0
        faults=json.loads((folder/'io-fault/result.json').read_text())
        assert faults['receiver_sha256']==sha(folder/'getdaq-receiver') and faults['runner_sha256']==sha(target/'verified-sources/tests/receiver_io_fault_test.py')
        assert faults['interposer_source_sha256']==sha(target/'verified-sources/tests/receiver_io_faults.c') and faults['interposer_sha256']==sha(folder/'io-fault/receiver-io-faults.so')
        assert faults['aggregate_wire_bytes']==897024 and faults['generated_raw_deleted'] and not faults['actual_device_failure'] and len(faults['cases'])==10
        assert sum(case['expected_success'] for case in faults['cases'])==1
        for case in faults['cases']:
            success=case['expected_success'];assert case['return_code']==(0 if success else 1) and case['seal_published']==success
            assert case['final_status']['stream_sealed']==success and bool(case['final_status']['error'])==(not success)
            cleanup=json.loads((folder/'io-fault'/case['case']/'payload-cleanup.json').read_text())
            assert cleanup['raw_files_remaining']==0 and len(cleanup['deleted'])==1
            assert cleanup['deleted'][0]['bytes']==case['raw_bytes'] and cleanup['deleted'][0]['sha256']==case['raw_sha256']
        assert (folder/'results-io-core-current.txt').read_text().startswith('PASS:')
        assert (folder/'results-io-native-regression.txt').read_text().count('PASS:')==5
        file_limit=json.loads((folder/'file-limit/result.json').read_text());assert file_limit['receiver_sha256']==sha(folder/'getdaq-receiver') and file_limit['generated_raw_deleted'] and len(file_limit['cases'])==4
    print('PASS receiver I/O archive:',len(manifest),'files;',len(inputs),'inputs; real ENOSPC distinguished from simulated errors; no raw data; historical only',current is None)


def main():
    root=Path(__file__).resolve().parents[1]
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--create',action='store_true');parser.add_argument('--historical-only',action='store_true')
    args=parser.parse_args();target=root/'build-receiver/validation-io-reliability'
    if not args.create:verify(target,None if args.historical_only else root);return
    assert not target.exists() and not args.historical_only
    files,inputs={},{}
    def add(path,name):
        assert path.is_file() and path.suffix!='.graw' and name not in files and '..' not in Path(name).parts and 'credential' not in path.name.lower();files[name]=path
    def tree(parent,name,pattern):
        roots=list(parent.glob(pattern));assert len(roots)==1 and not list(roots[0].rglob('*.graw'))
        for p in roots[0].rglob('*'):
            if p.is_file():add(p,name+'/'+str(p.relative_to(roots[0])))
    for label,build in [('ordinary',root/'build-receiver'),('sanitized',Path('/tmp/getdaq-receiver-io-asan'))]:
        receipt=json.loads((build/'build-receipt.json').read_text())
        for name,digest in json.loads((build/'source-sha256.json').read_text()).items():
            assert sha(root/name)==digest and (name not in inputs or inputs[name]==digest);inputs[name]=digest
        for name,digest in receipt['program_sha256'].items():assert sha(build/name)==digest;add(build/name,label+'/'+name)
        for name in ['build-receipt.json','source-sha256.json','verified-compile-commands.json','results-configure-verified.txt','results-build-verified.txt','results-enospc-current.txt','results-io-fault-current.txt','results-io-core-current.txt','results-io-native-regression.txt','results-io-file-limit-regression.txt']:
            add(build/name,label+'/'+name)
        tree(root/('build-receiver/enospc-'+label+'-final'),label+'/enospc','receiver-enospc-*')
        tree(root/('build-receiver/io-fault-'+label),label+'/io-fault','receiver-io-fault-*')
        tree(root/('build-receiver/io-file-limit-'+label),label+'/file-limit','receiver-file-limit-*')
    for name in ['tests/archive_receiver_io_validation.py','tests/receiver_enospc_test.py','tests/receiver_io_fault_test.py','tests/receiver_io_faults.c','tests/receiver_integration.py','tests/receiver_file_limit_test.py',
                 'docs/RECEIVER_IO_RELIABILITY.md','docs/RECEIVER_WRITE_FAILURES.md','docs/DATA_SERVICE.md','docs/REFACTOR_STATUS.md','docs/VALIDATION.md','README.md']:
        inputs[name]=sha(root/name)
    for name in inputs:add(root/name,'verified-sources/'+name)
    add(root/'build-suite/bin/getdaq-sentry','programs/getdaq-sentry-ordinary')
    target.mkdir()
    for name,path in files.items():
        dest=target/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,dest)
    (target/'input-sha256.json').write_text(json.dumps(inputs,indent=2)+'\n')
    manifest={str(p.relative_to(target)):sha(p) for p in sorted(target.rglob('*')) if p.is_file()}
    (target/'files-sha256.json').write_text(json.dumps(manifest,indent=2)+'\n');verify(target,root)


if __name__=='__main__':main()
