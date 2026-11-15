#!/usr/bin/env python3
"""Build and verify the Qt-free native ICE data package and three required GET interfaces."""
import argparse
import json
from pathlib import Path
import subprocess
from run_receiver_observation_tests import sha


def main():
    root=Path(__file__).resolve().parents[1];parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('build',type=Path);args=parser.parse_args();build=args.build.resolve();build.mkdir(parents=True,exist_ok=True)
    command=['cmake','-S',str(root/'third_party/ice-compat'),'-B',str(build),'-DGETDAQ_BUILD_ICE_RECEIVER=ON','-DGETDAQ_ICE_DATA_ONLY=ON',
        '-DCMAKE_DISABLE_FIND_PACKAGE_Qt5=TRUE','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DCMAKE_BUILD_TYPE=Release']
    with (build/'data-only-configure.txt').open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
    recipes=json.loads((build/'compile_commands.json').read_text());repo=[Path(r['file']).resolve() for r in recipes if root in Path(r['file']).resolve().parents and build not in Path(r['file']).resolve().parents]
    assert all('/apps/iceingress/' in str(p) or '/apps/icecontrol/' in str(p) or '/apps/sourcereader/' in str(p) or '/apps/datareceiver/' in str(p) or '/tests/ice_' in str(p) or p.name in ['controlled_ice_source_fixture.cpp','source_fault_commit_test.cpp','ring_ice_source_fixture.cpp','source_ring_reader_test.cpp','source_ring_readout_test.cpp','source_mfm_ring_test.cpp'] for p in repo)
    assert sum('/generated/mdaq/daq/DataProcessor.cpp' in r['file'] for r in recipes)==1
    assert not any('mocs_compilation' in r['file'] or '/apps/envoy/' in r['file'] or '/apps/ecc/' in r['file'] for r in recipes)
    tracked=set(repo)|{Path(__file__).resolve()}
    for folder in ['apps/iceingress','apps/icecontrol','apps/sourcereader','apps/datareceiver','common/storage','common/text']:tracked.update((root/folder).glob('*.h'))
    for name in ['third_party/ice-compat/CMakeLists.txt','apps/iceingress/CMakeLists.txt','apps/icecontrol/CMakeLists.txt','apps/sourcereader/CMakeLists.txt','apps/iceingress/IceIngress.ice','source/mordicus/MDaq-release-20210208/src/mdaq/daq/DataProcessor.ice']:tracked.add(root/name)
    before={str(p.relative_to(root)):sha(p) for p in sorted(tracked)}
    compile_command=['cmake','--build',str(build),'--target','getdaq-ice-receiver','getdaq-ice-send','controlled-ice-source-fixture','getdaq-source-control-servant','source-fault-commit-tests','ring-ice-source-fixture','source-ring-reader-tests','source-ring-readout-tests','source-mfm-ring-tests','--parallel','4']
    with (build/'data-only-build.txt').open('w') as log:subprocess.run(compile_command,stdout=log,stderr=subprocess.STDOUT,check=True)
    assert before=={str(p.relative_to(root)):sha(p) for p in sorted(tracked)}
    cache=(build/'CMakeCache.txt').read_text();assert 'GETDAQ_ICE_DATA_ONLY:BOOL=ON' in cache and 'CMAKE_DISABLE_FIND_PACKAGE_Qt5:UNINITIALIZED=TRUE' in cache
    programs=['getdaq-ice-receiver','getdaq-ice-send','controlled-ice-source-fixture'];linked={name:subprocess.check_output(['ldd',str(build/name)],text=True) for name in programs}
    assert all(not any(word in value for word in ['libQt','libyaml','libboost','libcurl']) for value in linked.values())
    receipt={'schema':1,'qt_configure_disabled':True,'qt_linked':False,'repo_source_count':len(repo),'get_interface_count':3,'compiler_recipe_count':len(recipes),
        'commands':[command,compile_command],'program_sha256':{name:sha(build/name) for name in programs},'runtime_dependencies':linked}
    (build/'data-only-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');(build/'data-only-source-sha256.json').write_text(json.dumps(before,indent=2)+'\n')
    print('PASS Qt-free native ICE data package;',len(repo),'repository compiles;',len(before),'inputs; three required original GET interfaces; no Qt configure/link')


if __name__=='__main__':main()
