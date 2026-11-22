#!/usr/bin/env python3
"""Build original SOAP ECC, DataExporter and DataRouter natively with MinGW."""
import argparse,hashlib,importlib.util,json,os,shutil,struct,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
PROGRAMS=['getEccSoapServer','DataExporter','dataRouter','getdaq-cobo-info']

def execute(command):subprocess.run([str(x) for x in command],check=True)
def verify_pe(path):
    with path.open('rb') as stream:
        header=stream.read(64)
        if header[:2]!=b'MZ':raise ValueError('Expected native Windows executable: '+str(path))
        stream.seek(struct.unpack_from('<I',header,60)[0]);signature=stream.read(6)
        if signature[:4]!=b'PE\0\0' or struct.unpack_from('<H',signature,4)[0]!=0x8664:raise ValueError('Expected x86_64 PE executable: '+str(path))
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build-dir',type=Path,default=ROOT/'build-windows-backends');p.add_argument('--bin-dir',type=Path,default=ROOT/'bin');p.add_argument('--compiler-bin',type=Path);p.add_argument('--cmake',default='cmake');p.add_argument('--jobs',type=int,default=2);p.add_argument('--prepare-only',action='store_true');a=p.parse_args()
    if os.name!='nt':p.error('Use native Windows Python; from WSL invoke py.exe -3 with Windows paths')
    if not 1<=a.jobs<=16:p.error('--jobs must be 1..16')
    compiler=a.compiler_bin.resolve() if a.compiler_bin else Path(shutil.which('g++.exe') or '').parent
    if not (compiler/'g++.exe').is_file() or not (compiler/'mingw32-make.exe').is_file():p.error('Select the MinGW bin directory matching the Qt Creator kit')
    os.environ['PATH']=str(compiler)+os.pathsep+os.environ.get('PATH','');os.environ['CC']=str(compiler/'gcc.exe')
    cmake=shutil.which(a.cmake)
    if not cmake:p.error('Install CMake and add it to PATH, or supply --cmake C:/path/to/cmake.exe')
    build=a.build_dir.resolve();output=a.bin_dir.resolve();build.mkdir(parents=True,exist_ok=True);output.mkdir(parents=True,exist_ok=True)
    execute([sys.executable,ROOT/'tools/verify-original-backends.py'])
    common=['-G','MinGW Makefiles','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_C_COMPILER='+(compiler/'gcc.exe').as_posix(),'-DCMAKE_CXX_COMPILER='+(compiler/'g++.exe').as_posix(),'-DCMAKE_MAKE_PROGRAM='+(compiler/'mingw32-make.exe').as_posix(),'-DPython3_EXECUTABLE='+sys.executable.replace('\\','/')]
    ice=build/'ice';execute([cmake,'-S',ROOT/'third_party/ice-compat','-B',ice,*common]);execute([cmake,'--build',ice,'--parallel',a.jobs,'--target','compat-GET','getdaq-cobo-info'])
    spec=importlib.util.spec_from_file_location('original_backends',ROOT/'tools/build-original-backends.py');original=importlib.util.module_from_spec(spec);spec.loader.exec_module(original)
    original.prepare(build/'original',ice)
    execute([cmake,'-S',build/'original','-B',build/'original/build',*common])
    if a.prepare_only:return
    execute([cmake,'--build',build/'original/build','--parallel',a.jobs,'--target','getEccSoapServer','DataExporter','dataRouter'])
    records={}
    for name in PROGRAMS:
        source=(ice if name=='getdaq-cobo-info' else build/'original/build')/(name+'.exe');verify_pe(source);destination=output/source.name
        temp=destination.with_suffix('.exe.installing');shutil.copy2(source,temp);os.replace(temp,destination)
        records[name]={'sha256':hashlib.sha256(destination.read_bytes()).hexdigest(),'pe_machine':'x86_64'}
    # These are compiler runtimes, not separately installed GET/Boost libraries.
    runtime={}
    for name in ['libgcc_s_seh-1.dll','libstdc++-6.dll','libwinpthread-1.dll']:
        source=compiler/name
        if source.is_file():
            digest=hashlib.sha256(source.read_bytes()).hexdigest()
            if not (output/name).is_file() or hashlib.sha256((output/name).read_bytes()).hexdigest()!=digest:shutil.copy2(source,output/name)
            runtime[name]=digest
    licenses=output.parent/'share/licenses/windows-backends'
    for source_root,name in [(ROOT/'third_party/legacy-get/licenses','original-get'),(ROOT/'third_party/licenses','dependencies'),(compiler.parent/'licenses','mingw')]:
        if source_root.is_dir():
            for source in source_root.rglob('*'):
                if source.is_file():
                    destination=licenses/name/source.relative_to(source_root);destination.parent.mkdir(parents=True,exist_ok=True)
                    if not destination.exists() or destination.read_bytes()!=source.read_bytes():shutil.copy2(source,destination)
    receipt={'schema':'getdaq.windows-original-backends:1','status':'complete','programs':records,'compiler_runtime':runtime,'hardware_validated':False,'automatic_master_slave_management':False}
    (output/'windows-original-backends.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
    print('PASS native Windows original backends:',output)
if __name__=='__main__':
    try:main()
    except (OSError,ValueError,subprocess.SubprocessError) as error:sys.exit(str(error))
