#!/usr/bin/env python3
"""Ubuntu24 userspace build and bounded runtime validation, with explicit evidence scope.

Use directly on Ubuntu24 as a non-root user, or through the project WSL helper.
--stage plan only prints commands and does not claim they have executed.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import time
import uuid

ROOT=Path(__file__).resolve().parents[1]
EXPECTED_QTTEST_TOTALS={'core':151,'widgets':119}


def sha(path):
    digest=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda:stream.read(1024*1024),b''):digest.update(block)
    return digest.hexdigest()


def write_json(path,value):
    temporary=path.with_name(path.name+'.tmp')
    temporary.write_text(json.dumps(value,indent=2)+'\n');temporary.replace(path)


def require(okay,message):
    if not okay:raise RuntimeError(message)


def cleanup_test_mode_data(home):
    home=Path(home)
    if home.is_symlink() or not home.is_dir():
        raise RuntimeError('QtTest HOME is not an owned directory: '+str(home))
    test_data=home/'.qttest'
    if not test_data.exists() and not test_data.is_symlink():return
    if test_data.is_symlink() or not test_data.is_dir():
        raise RuntimeError('Refusing to clean an unexpected QtTest data path: '+str(test_data))
    shutil.rmtree(test_data)
    if test_data.exists() or test_data.is_symlink():
        raise RuntimeError('QtTest private data cleanup did not complete: '+str(test_data))


def validation_environment(work):
    paths={name:work/name for name in ['home','cache','runtime','tmp','logs','evidence']}
    work_root=work.resolve()
    for path in paths.values():
        if path.exists() or path.is_symlink():
            if path.is_symlink() or not path.is_dir() or path.resolve().parent!=work_root:
                raise RuntimeError('Validation-owned directory path is unsafe: '+str(path))
        else:
            path.mkdir(mode=0o700)
        path.chmod(0o700)
    cleanup_test_mode_data(paths['home'])
    return dict(os.environ,HOME=str(paths['home']),QT_QPA_PLATFORM='offscreen',
                XDG_CACHE_HOME=str(paths['cache']),XDG_RUNTIME_DIR=str(paths['runtime']),
                TMPDIR=str(paths['tmp']))


def build_plan(work,jobs):
    entries=[]
    def command(label,arguments,cwd=None):entries.append({'label':label,'arguments':[str(x) for x in arguments],'cwd':str(cwd or ROOT)})
    command('full-suite',[sys.executable,ROOT/'tools/build-suite.py','--profile','full','--build-root',work/'delivery','--jobs',jobs])
    for name,project in [('core','xcfg_shared.pro'),('widgets','tests.pro'),('migration-probe','source_migration_probe.pro'),('transaction-probe','config_transaction_probe.pro'),('failed-handoff-window-probe','failed_handoff_window_probe.pro')]:
        directory=work/name
        flags=[] if name=='widgets' else ['CONFIG+=strict_c++']
        command(name+'-qmake',['qmake',ROOT/'tests'/project,*flags],directory)
        command(name+'-make',['make','-j'+str(jobs)],directory)
    injector_dir=work/'handoff-injector';injector=injector_dir/'libhandoff-fault-injector.so'
    command('handoff-injector-build',['g++','-shared','-fPIC',ROOT/'tests/handoff_fault_injector.cpp','-ldl','-o',injector],injector_dir)
    directory=work/'native-contracts'
    command('native-cmake',['cmake','-S',ROOT/'tests/platform','-B',directory,'-DBUILD_TESTING=ON','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-pedantic-errors'])
    command('native-build',['cmake','--build',directory,'--parallel',jobs])
    return entries


def runtime_plan(work, programs=None):
    def program_path(name, fallback):
        if programs is None:
            return work/'delivery/full'/fallback
        require(name in programs, 'Build receipt omits runtime program: '+name)
        return Path(programs[name]['path'])

    return [
        {'label':'core','arguments':[str(work/'core/xcfg-shared-tests'),'-o',str(work/'core/result.txt')+',txt']},
        {'label':'widgets','arguments':[str(work/'widgets/getdaq-tests'),'-o',str(work/'widgets/result.txt')+',txt']},
        {'label':'native-contracts','arguments':['ctest','--output-on-failure'],'cwd':str(work/'native-contracts')},
        {'label':'source-migration','arguments':[sys.executable,str(ROOT/'tests/run_source_migration_tests.py'),str(work/'migration-probe/source-migration-probe'),str(work/'evidence/migration')]},
        {'label':'source-transactions','arguments':[sys.executable,str(ROOT/'tests/run_config_transaction_tests.py'),str(work/'transaction-probe/config-transaction-probe'),str(work/'evidence/transactions')]},
        {'label':'receiver-transports','arguments':[sys.executable,str(ROOT/'tests/receiver_transport_integration.py'),str(work/'delivery/full/receiver/getdaq-receiver'),str(work/'evidence/transports')]},
        {'label':'receiver-load-safety','arguments':[sys.executable,str(ROOT/'tests/receiver_load_safety_test.py')]},
        {'label':'receiver-four-stream','arguments':[sys.executable,str(ROOT/'tests/receiver_sustained_load.py'),
            str(work/'delivery/full/receiver/getdaq-receiver'),'--evidence',str(work/'evidence/receiver-four-stream'),
            '--streams','4','--mib-per-stream','256','--rotate-mib','128']},
        {'label':'failed-handoff-window','arguments':[sys.executable,str(ROOT/'tests/run_failed_handoff_completion_tests.py'),
            '--service',str(program_path('getdaq-supervisor','native/getdaq-supervisor')),
            '--receiver',str(program_path('getdaq-receiver','receiver/getdaq-receiver')),
            '--client',str(program_path('getdaq-supervisor-query','bin/getdaq-supervisor-query')),
            '--injector',str(work/'handoff-injector/libhandoff-fault-injector.so'),
            '--window-probe',str(work/'failed-handoff-window-probe/failed-handoff-window-probe'),
            '--evidence',str(work/'evidence/failed-handoff-window')]},
    ]


def expected_full_programs(suite, jobs):
    build_args=argparse.Namespace(profile='full',cmake='cmake',qmake='qmake',make='make',jobs=jobs)
    steps=suite.plan(build_args,ROOT/'build-delivery'/'expected-full')
    return {name for step in steps for name in step['programs']}


def ctest_test_count(build_directory):
    result = subprocess.run(['ctest', '-N'], cwd=build_directory, check=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    match = re.search(r'Total Tests:\s*(\d+)', result.stdout)
    require(match is not None, 'CTest did not report the registered native test count')
    count = int(match.group(1))
    require(count > 0, 'No native contract tests are registered')
    return count


def validate_qttest_result(name, result_text):
    expected=EXPECTED_QTTEST_TOTALS.get(name)
    require(expected is not None, 'Unknown QtTest suite: '+name)
    require(f'Totals: {expected} passed, 0 failed, 0 skipped' in result_text and
            'FAIL!' not in result_text,
            'Incomplete QtTest result: '+name+'; expected '+str(expected)+' passing cases')


def attempt_results(paths, started):
    """Select only evidence created by this invocation when work roots are reused."""
    return [path for path in paths if path.stat().st_mtime >= started]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work',type=Path,default=Path('/build/getdaq-platform'))
    parser.add_argument('--jobs',type=int,default=2)
    parser.add_argument('--stage',choices=['plan','build','run','all'],default='all')
    args=parser.parse_args();work=args.work.resolve()
    require(1<=args.jobs<=8,'Use 1..8 build workers')
    if args.stage=='plan':
        print(json.dumps({'execution_claimed':False,'build':build_plan(work,args.jobs),'runtime':runtime_plan(work),
                          'raw_test_budget_bytes':20*1024**3,'real_hardware':False},indent=2));return 0
    release={}
    for line in Path('/etc/os-release').read_text().splitlines():
        if '=' in line:
            key,value=line.split('=',1);release[key]=value.strip('"')
    require(release.get('ID')=='ubuntu' and release.get('VERSION_ID')=='24.04','Run this validation in actual Ubuntu24 userspace; plan mode is available elsewhere')
    require(os.getuid()!=0,'Run builds/tests as a non-root user; root would invalidate permission tests')
    require(platform.machine()=='x86_64','This target matrix currently covers x86_64 only')
    require(work!=ROOT and ROOT not in work.parents,'Keep validation outputs outside the read-only source tree')
    work.mkdir(parents=True,exist_ok=True);marker=work/'validation-owner.json'
    if marker.exists():
        owner=json.loads(marker.read_text());require(owner.get('schema')=='getdaq.ubuntu24-validation:1' and owner.get('work')==str(work),'Existing output ownership marker differs')
    else:
        require(not any(work.iterdir()),'Use a new output directory or an existing validation-owned directory')
        write_json(marker,{'schema':'getdaq.ubuntu24-validation:1','work':str(work),'source':str(ROOT),'owner_uid':os.getuid()})
    require(shutil.disk_usage(work).free>=4*1024**3,'At least 4GiB free space is needed for this build matrix')
    environment=validation_environment(work)
    spec=importlib.util.spec_from_file_location('suite',ROOT/'tools/build-suite.py')
    suite=importlib.util.module_from_spec(spec);spec.loader.exec_module(suite)
    expected_programs=expected_full_programs(suite,args.jobs)
    require('getdaq-merge' in expected_programs,'Full suite plan omits the native MFM merger')
    require('getdaq-matrix' in expected_programs,'Full suite plan omits the native CoBo matrix converter')
    inputs=suite.source_hashes()
    for directory in [ROOT/'tests',ROOT/'tools']:
        for path in directory.rglob('*'):
            if path.is_file() and (path.suffix in ['.cpp','.h','.pro','.py','.json','.xml','.xcfg'] or path.name=='CMakeLists.txt'):
                inputs[str(path.relative_to(ROOT))]=sha(path)
    attempt=uuid.uuid4().hex;attempt_started=time.time()
    receipt={'schema':'getdaq.ubuntu24-platform:1','attempt':attempt,'status':'running','stage':args.stage,
             'source_root':str(ROOT),'work':str(work),'uid':os.getuid(),'release':release,'host_kernel':platform.release(),
             'started_unix':attempt_started,
             'independent_machine_accepted':False,'real_hardware_accepted':False,'sustained_throughput_accepted':False,
             'raw_test_budget_bytes':20*1024**3,'system_qt_instrumented':False,'commands':[],'inputs':inputs}
    receipt['versions']={label:subprocess.check_output(command,text=True).splitlines()[0].strip() for label,command in [
        ('gcc',['g++','--version']),('cmake',['cmake','--version']),('qt',['qmake','-query','QT_VERSION'])]}
    if args.stage=='run':
        receipt['native_ctest_expected_count']=ctest_test_count(work/'native-contracts')
    destination=work/('platform-'+attempt+'.json');write_json(destination,receipt)
    def run(entry):
        record=dict(entry);record['cwd']=record.get('cwd',str(ROOT));directory=Path(record['cwd']);directory.mkdir(parents=True,exist_ok=True)
        log=work/'logs'/(attempt+'-'+record['label']+'.log');started=time.monotonic()
        print('Running Ubuntu24 validation:',record['label'],flush=True)
        try:
            with log.open('w') as output:
                result=subprocess.run(record['arguments'],cwd=directory,env=environment,stdout=output,stderr=subprocess.STDOUT)
        finally:
            if record['label'] in ('core','widgets'):
                cleanup_test_mode_data(environment['HOME'])
        record.update(exit_code=result.returncode,seconds=round(time.monotonic()-started,3),log=str(log))
        receipt['commands'].append(record);write_json(destination,receipt)
        require(result.returncode==0,'Validation failed: '+record['label']+'; inspect '+str(log))
    try:
        if args.stage in ['all','build']:
            for entry in build_plan(work,args.jobs):run(entry)
            full=json.loads((work/'delivery/full/build-receipt.json').read_text())
            actual_programs=set(full['programs'])
            require(full['status']=='complete' and actual_programs==expected_programs,
                    'Full suite build receipt differs from current plan; missing='+
                    ','.join(sorted(expected_programs-actual_programs))+'; extra='+
                    ','.join(sorted(actual_programs-expected_programs)))
            receipt['programs']=full['programs']
            receipt['native_ctest_expected_count']=ctest_test_count(work/'native-contracts')
            tests={}
            for name,target in [('core','xcfg-shared-tests'),('widgets','getdaq-tests'),('migration-probe','source-migration-probe'),('transaction-probe','config-transaction-probe'),('failed-handoff-window-probe','failed-handoff-window-probe')]:
                binary=work/name/target;require(binary.is_file(),'Expected test program missing: '+str(binary))
                tests[name]={'path':str(binary),'sha256':sha(binary),'makefile_sha256':sha(work/name/'Makefile')}
            require(all(sha(ROOT/name)==digest for name,digest in inputs.items()),'Inputs changed while building test programs')
            write_json(work/'test-build-receipt.json',{'status':'complete','inputs':inputs,'programs':tests})
        if args.stage in ['all','run']:
            full=json.loads((work/'delivery/full/build-receipt.json').read_text())
            require(full['status']=='complete' and full['inputs']==suite.source_hashes(),'Production build is stale; rebuild from the current sources')
            tests=json.loads((work/'test-build-receipt.json').read_text())
            require(tests['status']=='complete' and tests['inputs']==inputs,'Test build is stale; rebuild before claiming current-source validation')
            for record in list(full['programs'].values())+list(tests['programs'].values()):
                require(sha(Path(record['path']))==record['sha256'],'A validation executable changed after compilation')
            receipt['programs']=full['programs'];receipt['test_programs']=tests['programs']
            for entry in runtime_plan(work, full['programs']):run(entry)
            # Keep this gate aligned with the current source test inventory.
            # The Windows helper-availability guard follows the failed-handoff window action test.
            for name in ('core','widgets'):
                text=(work/name/'result.txt').read_text()
                validate_qttest_result(name,text)
            native=next(record for record in receipt['commands'] if record['label']=='native-contracts')
            native_text=Path(native['log']).read_text()
            expected_native_count=receipt.get('native_ctest_expected_count')
            match=re.search(r'100% tests passed, 0 tests failed out of (\d+)',native_text)
            require(expected_native_count and match and int(match.group(1))==expected_native_count and
                    'No tests were found' not in native_text,
                    'Native contract result must contain every currently registered CTest case')
            transport=attempt_results((work/'evidence/transports').glob('receiver-transports-*/result.json'),attempt_started)
            require(bool(transport),'Receiver transport evidence is missing')
            total=0
            for path in transport:
                value=json.loads(path.read_text());require(value['generated_raw_deleted'],'Transport raw cleanup incomplete')
                total+=value['aggregate_wire_bytes']
            load_results=attempt_results((work/'evidence/receiver-four-stream').glob('receiver-load-*/result.json'),attempt_started)
            require(len(load_results)==1,'Four-stream receiver load evidence is missing or ambiguous')
            load_path=load_results[0];load=json.loads(load_path.read_text())
            cleanup=json.loads(load_path.with_name('payload-cleanup.json').read_text())
            require(load['receiver_sha256']==sha(work/'delivery/full/receiver/getdaq-receiver') and
                    load['streams'] and len(load['streams'])==4 and load['total_bytes']==4*256*1024**2,
                    'Four-stream receiver result differs from current full-suite binary or planned payload')
            require(cleanup['automatic_cleanup'] and cleanup['deleted_bytes']==load['total_bytes'] and
                    cleanup['remaining_generated_payload_files']==0,'Four-stream receiver payload cleanup is incomplete')
            total+=load['total_bytes']
            handoff_results=attempt_results((work/'evidence/failed-handoff-window').glob('failed-handoff-*/result.json'),attempt_started)
            require(len(handoff_results)==1,'Failed-handoff Qt window evidence is missing or ambiguous')
            handoff_path=handoff_results[0];handoff=json.loads(handoff_path.read_text())
            deleted_handoff=sum(item['bytes'] for item in handoff['deleted_raw'])
            require(handoff['passed'] and handoff['raw_remaining']==0 and deleted_handoff<=64*1024**2 and
                    handoff['window_probe_sha256']==sha(work/'failed-handoff-window-probe/failed-handoff-window-probe'),
                    'Failed-handoff Qt window evidence is incomplete or mismatched')
            require(not list(handoff_path.parent.rglob('*.graw')),'Failed-handoff Qt window synthetic raw was not cleaned')
            total+=deleted_handoff
            require(total<=20*1024**3,'Aggregate receiver wire budget exceeded')
            require(not list(work.rglob('*.graw')),'Owned test raw remains; inspect exact fixture cleanup before continuing')
            receipt['transport_wire_bytes']=sum(json.loads(path.read_text())['aggregate_wire_bytes'] for path in transport)
            receipt['receiver_load_wire_bytes']=load['total_bytes'];receipt['aggregate_test_wire_bytes']=total;receipt['raw_remaining']=0
            receipt['failed_handoff_window_raw_bytes']=deleted_handoff
            receipt['failed_handoff_window_scenario']=handoff['scenario']
        require(all(sha(ROOT/name)==digest for name,digest in inputs.items()),'Build/test inputs changed during validation')
        receipt['status']='complete';write_json(destination,receipt)
        print('PASS Ubuntu24 userspace',args.stage,'scope; no hardware or independent-machine acceptance;',destination)
        return 0
    except BaseException as error:
        receipt['status']='failed';receipt['failure']=str(error);write_json(destination,receipt);raise


if __name__=='__main__':
    try:sys.exit(main())
    except (OSError,RuntimeError,ValueError,subprocess.SubprocessError) as error:
        print('Ubuntu24 platform validation failed: '+str(error),file=sys.stderr);sys.exit(1)
