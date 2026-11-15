#!/usr/bin/env python3
"""收集发布包阶段同版证据；明确排除凭据，不覆盖旧归档。"""
import hashlib
import json
from pathlib import Path
import shutil


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    root=Path(__file__).resolve().parents[1]
    target=root/'build-suite-tests/validation-configuration-publication'
    assert not target.exists(),'Keep previous evidence immutable'
    inputs={}
    copies={}
    def add(source,name):
        assert source.is_file(),source
        assert name not in copies,name
        copies[name]=source
    qt=[(root/'build-suite-tests','results-publication-tests-final-current.txt',59),
        (root/'build-publication-qtcore','results-publication-tests-final-current.txt',5),
        (Path('/tmp/getdaq-publication-qtcore-asan'),'results-publication-tests-final-current.txt',5),
        (root/'build-xcfg-shared','results-publication-regression-current.txt',18)]
    for i,(build,result,count) in enumerate(qt):
        receipt=json.loads((build/'build-receipt.json').read_text())
        assert receipt['sanitized']==(i==2) and not receipt['system_qt_instrumented']
        assert digest(build/receipt['target'])==receipt['program_sha256']
        for name,sha in json.loads((build/'source-sha256.json').read_text()).items():
            assert digest(root/name)==sha,name
            assert name not in inputs or inputs[name]==sha,name
            inputs[name]=sha
        for name,sha in json.loads((build/'generated-sha256.json').read_text()).items():
            assert digest(build/name)==sha,name
            add(build/name,f'qt-build/{i}/{name}')
        assert f'Totals: {count} passed, 0 failed, 0 skipped' in (build/result).read_text()
        for name in [receipt['target'],'build-receipt.json','source-sha256.json','generated-sha256.json','Makefile','results-verified-build.txt',result]: add(build/name,f'qt-build/{i}/{name}')
    native=Path('/tmp/getdaq-publication-native-asan')
    for name,sha in json.loads((native/'source-sha256.json').read_text()).items():
        assert digest(root/name)==sha,name
        assert name not in inputs or inputs[name]==sha,name
        inputs[name]=sha
    for name,sha in json.loads((native/'dependency-sha256.json').read_text()).items(): assert digest(root/name)==sha,name
    commands=json.loads((native/'commands.json').read_text()); compiles=[r['command'] for r in commands if '-c' in r['command']]
    assert compiles and all(all(flag in row for flag in ('-std=c++11','-pedantic-errors','-fsanitize=address,undefined')) for row in compiles)
    for name in ['getdaq-ecc-server','fixed-ecc-configuration-tests','native-ecc-coordinator-tests','source-sha256.json','dependency-sha256.json','commands.json','generated-sha256.json']:
        add(native/name,'native-sanitized/'+name)
    for path in native.glob('*.compile.txt'): add(path,'native-sanitized/'+path.name)
    for path in native.glob('*.link.txt'): add(path,'native-sanitized/'+path.name)
    for path in native.glob('moc-*.cpp'): add(path,'native-sanitized/'+path.name)
    for name in ['GetDAQ','getdaq-config','getdaq-sentry','getdaq-simulator']: add(root/'build-suite/bin'/name,'programs/'+name)
    for name in ['getdaq-ecc-server','fixed-ecc-configuration-tests','getdaq-system-config']: add(root/'build-ice-compat'/name,'programs/'+name)
    cli_roots=[]
    for folder in ['configuration-publication-final-current-evidence','configuration-publication-final-sanitized-evidence']:
        scopes=list((root/'build-suite-tests'/folder).glob('configuration-publication-*/scope.json')); assert len(scopes)==1,scopes
        scope=json.loads(scopes[0].read_text())
        assert scope['offline_only'] and not scope['hardware_connected'] and not scope['receiver_started'] and scope['process_cases']==14
        for path,sha in scope['program_sha256'].items(): assert digest(Path(path))==sha,path
        evidence=scopes[0].parent; cli_roots.append(evidence)
        for path in sorted(evidence.rglob('*')):
            if path.is_file() and path.name not in scope['excluded_evidence_files']: add(path,'cli/'+folder+'/'+str(path.relative_to(evidence)))
    for mode in ['ordinary','sanitized']:
        matches=list((root/'build-ice-compat').glob('publication-fixed-final-'+mode+'-*')); assert len(matches)==1,matches
        evidence=matches[0]; folder=evidence.name
        scope=json.loads((evidence/'scope.json').read_text())
        program=root/'build-ice-compat/fixed-ecc-configuration-tests' if mode=='ordinary' else native/'fixed-ecc-configuration-tests'
        assert scope['returncode']==0 and digest(program)==scope['program_sha256']
        add(evidence/'scope.json','fixed-input/'+folder+'/scope.json')
        assert 'PASS fixed input/publication binding' in (evidence/'result.txt').read_text()
        add(evidence/'result.txt','fixed-input/'+folder+'/result.txt')
        for name in ['snapshot.json','ecc.jsonl']:
            add(evidence/'evidence/published-controller'/name,'fixed-input/'+folder+'/'+name)
    regressions=list((root/'build-ice-compat').glob('publication-native-regression-final-*'))
    assert len(regressions)==1,regressions
    regression=regressions[0]
    scope=json.loads((regression/'scope.json').read_text()); assert scope['returncode']==0 and digest(native/'native-ecc-coordinator-tests')==scope['program_sha256']
    add(regression/'scope.json','native-regression/scope.json')
    text=(regression/'result.txt').read_text(); assert 'PASS' in text and 'FAIL' not in text,text[-1000:]
    add(regression/'result.txt','native-regression/result.txt')
    for path in regression.glob('*.jsonl'): add(path,'native-regression/'+path.name)
    for folder,name in [('build-suite','results-publication-unified-build-final-current.txt'),('build-ice-compat','results-publication-native-build-final.txt'),
                        ('build-experiment','results-publication-core-build-final.txt'),('build-experiment','results-publication-core-tests-final.txt')]: add(root/folder/name,'results/'+name)
    for name in ['results-publication-cli-final-current.txt','results-publication-cli-final-sanitized.txt','results-publication-system-cli-current.txt','results-publication-aget-cli-current.txt','results-publication-readout-cli-current.txt','results-publication-mutant-cli-current.txt']:
        add(root/'build-suite-tests'/name,'results/'+name)
    for build,result in [(root/'build-experiment','results-publication-core-tests-final.txt'),(Path('/tmp/getdaq-publication-core-asan'),'results-publication-tests-final.txt')]:
        assert '100% tests passed, 0 tests failed' in (build/result).read_text()
        add(build/'system-experiment-tests','pure-core/'+build.name+'/system-experiment-tests')
        add(build/result,'pure-core/'+build.name+'/'+result)
    extras=['README.md','docs/CONFIGURATION_PUBLICATION.md','docs/CONFIGURATION_EDITOR.md','docs/ARCHITECTURE.md','docs/REFACTOR_STATUS.md','docs/ECC_NETWORK_SERVICE.md','docs/SYSTEM_EXPERIMENT_CONFIGURATION.md','docs/VALIDATION.md',
            'tests/configuration_publication_integration.py','tests/archive_configuration_publication_validation.py','tests/system_experiment_test.cpp','tests/data/cobo-hardware.json','tests/data/mutant-hardware.json']
    for name in extras: inputs[name]=digest(root/name)
    for name in sorted(inputs): add(root/name,'verified-sources/'+name)
    for name,source in copies.items():
        destination=target/name; destination.parent.mkdir(parents=True,exist_ok=True); shutil.copy2(source,destination)
    (target/'input-sha256.json').write_text(json.dumps(inputs,indent=2)+'\n')
    files={str(p.relative_to(target)):digest(p) for p in sorted(target.rglob('*')) if p.is_file()}
    (target/'files-sha256.json').write_text(json.dumps(files,indent=2)+'\n')
    for name,sha in files.items(): assert digest(target/name)==sha,name
    for name,sha in inputs.items(): assert digest(root/name)==sha,name
    assert not list(target.rglob('credential-private'))
    print(f'PASS {len(files)} publication evidence files; {len(inputs)} current input hashes; Qt/native/pure-core programs, 28 process cases and manifests verified; credentials excluded')


if __name__=='__main__': main()
