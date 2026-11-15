#!/usr/bin/env python3
"""无Qt检查器对旧Qt冻结记录与篡改的独立进程验证。"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('checker',type=Path)
    parser.add_argument('--evidence',type=Path,required=True)
    args=parser.parse_args(); checker=args.checker.resolve(); repo=Path(__file__).resolve().parents[1]
    args.evidence.mkdir(parents=True,exist_ok=True)
    root=Path(tempfile.mkdtemp(prefix='effective-record-',dir=args.evidence)).resolve(); traces=[]
    def run(kind,path,success=True):
        result=subprocess.run([str(checker),kind,str(path)],capture_output=True,text=True,timeout=40)
        traces.append({'kind':kind,'path':str(path),'returncode':result.returncode,'stdout':result.stdout,'stderr':result.stderr})
        assert (result.returncode==0)==success,traces[-1]
        return json.loads(result.stdout) if success else None
    previous=repo/'build-xcfg-shared/validation-effective-snapshot/cli-evidence'
    for name in ['before.json','after.json','changed.json']:
        source=previous/name; data=source.read_bytes(); record=json.loads(data)
        local=root/name; local.write_bytes(data); result=run('snapshot',local)
        assert result['effective_sha256']==record['effective_sha256'] and result['snapshot_sha256']==record['snapshot_sha256']
        assert result['effective_snapshot_semantics_recomputed'] and not result['hardware_loaded_verified']
    before=json.loads((root/'before.json').read_bytes())
    mutations=[]
    v=copy.deepcopy(before);v['scalars'][0]['normalized_value']='wrong';mutations.append(('normalized',v))
    v=copy.deepcopy(before);v['scalars'][0]['value_source']['file']='unknown.xcfg';mutations.append(('origin',v))
    v=copy.deepcopy(before);v['scalars'][0]['value_source']['line']=0;mutations.append(('line',v))
    v=copy.deepcopy(before);v['scalars'][0]['path']='Setup/Unknown/gain';mutations.append(('parent',v))
    v=copy.deepcopy(before);v['scalars'][0]['range']='{}';mutations.append(('empty-range',v))
    v=copy.deepcopy(before);v['scalars'].append(v['scalars'][0]);mutations.append(('duplicate-path',v))
    v=copy.deepcopy(before);v['effective_sha256']='0'*64;mutations.append(('digest',v))
    for name,v in mutations:
        path=root/(name+'.json');path.write_text(json.dumps(v));run('snapshot',path,False)
    duplicate=root/'duplicate-key.json';data=(root/'before.json').read_bytes();duplicate.write_bytes(b'{"schema_version":1,'+data.lstrip()[1:]);run('snapshot',duplicate,False)
    invalid=root/'invalid-utf8.json';invalid.write_bytes(data+b'\xff');run('snapshot',invalid,False)
    packages=list((repo/'build-suite-tests/validation-configuration-publication/cli').glob('*/v1/publication.json'))
    assert len(packages)==2,packages
    references=[]
    for package in packages:
        result=run('publication',package);expected=json.loads(package.read_bytes())
        assert result['configuration_sha256']==expected['configuration_sha256'] and result['publication_sha256']==hashlib.sha256(package.read_bytes()).hexdigest()
        assert result['effective_snapshot_semantics_recomputed'] and not result['hardware_loaded_verified']
        run('experiment',package.parent/'experiment.json')
        references.append({'path':str(package),'sha256':hashlib.sha256(package.read_bytes()).hexdigest()})
    dependencies=subprocess.check_output(['ldd',str(checker)],text=True)
    assert not any(name in dependencies for name in ['libQt','libIce','libyaml','libcurl','libboost']),dependencies
    (root/'dependencies.txt').write_text(dependencies)
    (root/'processes.json').write_text(json.dumps(traces,indent=2)+'\n')
    (root/'scope.json').write_text(json.dumps({'process_cases':len(traces),'program_sha256':hashlib.sha256(checker.read_bytes()).hexdigest(),'legacy_package_references':references,'hardware_connected':False,'receiver_executed':False,'legacy_snapshot_format':1},indent=2)+'\n')
    print(f'PASS pure checker: {len(traces)} process cases; frozen Qt schema1 digests, legacy publications, tampering and no Qt/Ice/yaml/curl/Boost dynamic dependency; evidence {root}')


if __name__=='__main__':main()
