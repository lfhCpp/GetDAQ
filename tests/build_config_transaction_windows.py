#!/usr/bin/env python3
"""Build the source-transaction core and process probe with the actual Windows Qt kit."""
import argparse,importlib.util,json,os,re,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('winbuild',ROOT/'tools/build-windows-from-wsl.py');win=importlib.util.module_from_spec(spec);spec.loader.exec_module(win)
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--kit',required=True,type=Path);ap.add_argument('--compiler',required=True,type=Path);ap.add_argument('--output',required=True,type=Path);a=ap.parse_args()
 kit=a.kit.resolve();compiler=a.compiler.resolve();base=a.output.resolve();base.relative_to(ROOT);base.mkdir(parents=True,exist_ok=True)
 assert subprocess.check_output([str(kit/'bin/qmake.exe'),'-query','QT_VERSION'],text=True).strip()=='5.14.2'
 before=win.suite.source_hashes()
 for f in ['tests/xcfg_shared.pro','tests/test_xcfg_shared.cpp','tests/config_transaction_probe.pro','tests/config_transaction_probe.cpp','tests/build_config_transaction_windows.py']:
  before[f]=win.suite.sha(ROOT/f)
 programs={};commands=[];runtime={}
 for name,project,target in [('core','xcfg_shared.pro','xcfg-shared-tests'),('probe','config_transaction_probe.pro','config-transaction-probe')]:
  out=base/('windows-'+name);out.mkdir(exist_ok=True);relative=Path(os.path.relpath(ROOT/'tests'/project,out)).as_posix()
  lines=['@echo off','setlocal','set "PATH='+win.windows(compiler/'bin')+';'+win.windows(kit/'bin')+';%PATH%"','pushd "'+win.windows(out)+'"','if errorlevel 1 exit /b 10',
         '"'+win.windows(kit/'bin/qmake.exe')+'" "'+relative+'" "DESTDIR=." "CONFIG+=strict_c++" -after "CONFIG+=release" "CONFIG-=debug"',
         'if errorlevel 1 goto failed','"'+win.windows(compiler/'bin/mingw32-make.exe')+'" -j2','if errorlevel 1 goto failed','popd','exit /b 0',':failed','set "getdaq_status=%errorlevel%"','popd','exit /b %getdaq_status%']
  bat=out/'build.cmd';bat.write_text('\n'.join(lines)+'\n');cmd=['/mnt/c/Windows/System32/cmd.exe','/c',win.windows(bat)];commands.append(cmd)
  with (out/'build.log').open('w') as log:subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,check=True)
  relative_target=re.search(r'^DESTDIR_TARGET\s*=\s*(.+)$',(out/'Makefile.Release').read_text(),re.M).group(1).strip().replace('\\','/')
  assert '$' not in relative_target and ':' not in relative_target
  exe=(out/relative_target).resolve();exe.relative_to(out);exe.chmod(exe.stat().st_mode|0o111);programs[name]={'path':str(exe),'sha256':win.suite.sha(exe),'pe_machine':win.pe_machine(exe)}
  for dll in [kit/'bin/Qt5Core.dll',kit/'bin/Qt5Test.dll']+[compiler/'bin'/n for n in ['libgcc_s_seh-1.dll','libstdc++-6.dll','libwinpthread-1.dll']]:
   dest=exe.parent/dll.name;shutil.copy2(dll,dest);assert win.suite.sha(dest)==win.suite.sha(dll);runtime[str(dest)]=win.suite.sha(dest)
 assert all(win.suite.sha(ROOT/p)==h for p,h in before.items())
 (base/'windows-core-build-receipt.json').write_text(json.dumps({'status':'complete','qt_version':'5.14.2','strict_cxx11':True,'programs':programs,'runtime_dll_sha256':runtime,'inputs':before,'commands':commands},indent=2)+'\n')
 print('PASS actual Qt 5.14.2 MinGW strict C++11 transaction core and probe build')
if __name__=='__main__':main()
