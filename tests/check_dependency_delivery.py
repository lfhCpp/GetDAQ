#!/usr/bin/env python3
"""Bind source preflight, current build receipts, unchanged programs and delivery docs."""
import argparse
import importlib.util
import json
from pathlib import Path
from check_ecc_plan_transition_checkpoint import read, sha, relative, hashes, verify_map

ROOT=Path(__file__).resolve().parents[1]
PHASE=ROOT/'build-dependency-delivery'
HOST=ROOT/'build-packages/getdaq-host-20261016-offline-dependencies-candidate'
WINDOWS=ROOT/'build-packages/getdaq-windows-qt5142-20261016-offline-dependencies-candidate'


def suite_module():
    spec=importlib.util.spec_from_file_location('suite',ROOT/'tools/build-suite.py')
    value=importlib.util.module_from_spec(spec);spec.loader.exec_module(value);return value


def create():
    assert not (PHASE/'checkpoint.json').exists(),'Preserve original evidence'
    suite=suite_module();linux=read(ROOT/'build-delivery/full/build-receipt.json');windows=read(ROOT/'build-windows-qt5142/windows-build-receipt.json')
    assert linux['status']==windows['status']=='complete' and linux['inputs']==suite.source_hashes()
    verify_map(windows['inputs']);production=dict(linux['inputs']);production.update(windows['inputs'])
    files=[ROOT/'build-delivery/full/build-receipt.json',ROOT/'build-windows-qt5142/windows-build-receipt.json']
    for profile,name,count in [('full','embedded-dependencies',109),('control','control-dependencies',95),('data','data-dependencies',0),('data-host','data-host-dependencies',1)]:
        file=PHASE/(name+'.json');value=read(file)
        assert value==suite.verify_dependencies(profile)
        assert value['passed'] and not value['network_used'] and len(value['checked_files'])==count
        verify_map(value['checked_files']);files.append(file)
        if profile=='full':assert linux['embedded_dependencies']==value
        if profile=='control':assert windows['embedded_dependencies']==value
    historical=read(ROOT/'build-ecc-plan-review/checkpoint.json');unchanged=read(PHASE/'unchanged-programs.json')
    assert unchanged['passed'] and unchanged['programs']==30 and not unchanged['cxx_changes']
    actual={}
    for receipt in [linux,windows]:
        for name,program in receipt['programs'].items():
            p=Path(program['path']);assert sha(p)==program['sha256']==historical['artifacts'][relative(p)]
            files.append(p);actual[relative(p)]=program['sha256']
    for folder in [HOST,WINDOWS]:
        manifest=read(folder/'manifest.json');assert manifest['status']=='complete' and not (folder/'BUILD_INCOMPLETE').exists()
        files.append(folder/'manifest.json')
        assert {relative(p) for p in folder.rglob('*') if p.is_file() and p.name!='manifest.json'}=={relative(folder/n) for n in manifest['files']}
        for name,digest in manifest['files'].items():
            assert sha(folder/name)==digest;files.append(folder/name)
        for name,program in manifest['programs'].items():
            assert program['sha256'] in actual.values()
    tests=(PHASE/'dependency-tests.log').read_text();assert 'Ran 12 tests' in tests and tests.rstrip().endswith('OK')
    assert 'PASS 1036 local Markdown file links' in (PHASE/'documentation-links.log').read_text()
    files.extend(PHASE/name for name in ['dependency-tests.log','documentation-links.log','unchanged-programs.json','linux-build.log','windows-build.log','windows-deploy.log','data-build-plan.json'])
    files.extend(ROOT/name for name in ['tools/verify-dependencies.py','tools/build-suite.py','tools/build-windows-from-wsl.py','tools/deploy-windows-from-wsl.py','tools/check-documentation-links.py','tests/test_dependency_verification.py','tests/check_dependency_delivery.py','tests/check_ecc_plan_transition_checkpoint.py','tests/check_ecc_plan_transition_evidence.py','tests/check_ecc_service_handoff_checkpoint.py','tests/check_controller_handoff_evidence.py','third_party/dependencies.json','third_party/yaml-cpp/SHA256SUMS','third_party/archives/SHA256SUMS','third_party/README.md','README.md','REWRITE_PROPOSAL.md'])
    files.extend((ROOT/'docs').glob('*.md'))
    value=dict(schema=1,phase='70.57',goal_complete=False,real_hardware=False,new_raw_bytes=0,
               production_inputs=production,artifacts=hashes(files),unchanged_programs=actual,
               cpp_programs_unchanged_from='build-ecc-plan-review/checkpoint.json',dependency_tests=12,
               local_markdown_links=1036,packages=[relative(HOST),relative(WINDOWS)],
               linux_programs=24,windows_programs=6)
    verify_map(value['production_inputs']);verify_map(value['artifacts'])
    (PHASE/'checkpoint.json').write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--index',action='store_true');args=parser.parse_args()
    if args.index:create()
    value=read(PHASE/'checkpoint.json');assert not value['goal_complete'] and value['new_raw_bytes']==0
    suite=suite_module();linux=read(ROOT/'build-delivery/full/build-receipt.json')
    assert linux['inputs']==suite.source_hashes()
    count=verify_map(value['production_inputs'])+verify_map(value['artifacts'])
    verify_map(value['unchanged_programs'])
    assert not list(PHASE.rglob('*.graw'))
    print('PASS dependency delivery checkpoint:',count,'digests; 12 checks; 30 unchanged C++ programs; whole goal incomplete')


if __name__=='__main__':main()
