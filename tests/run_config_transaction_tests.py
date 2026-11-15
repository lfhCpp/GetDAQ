#!/usr/bin/env python3
"""Real cooperating processes must reject partial source generations; own XML only, no DAQ data."""
import argparse,hashlib,json,os,shutil,subprocess,tempfile,time
from pathlib import Path

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('probe',type=Path);ap.add_argument('evidence',type=Path);ap.add_argument('--windows',action='store_true');ap.add_argument('--sanitized',action='store_true');a=ap.parse_args()
 if a.windows and a.sanitized:ap.error('Windows Qt kit checks do not claim sanitizer support')
 a.evidence.mkdir(parents=True,exist_ok=True);folder=Path(tempfile.mkdtemp(prefix='transactions-',dir=a.evidence)).resolve();probe=a.probe.resolve(strict=True);before=sha(probe);runner_sha=sha(Path(__file__).resolve())
 env=dict(os.environ,XDG_CACHE_HOME=str(folder/'cache'));processes=[];runs=[];native_owned=None
 def native(p):
  value=str(p)
  if not a.windows:return value
  if (len(value)>1 and value[1]==':') or value.startswith('\\\\'):return value
  return subprocess.check_output(['wslpath','-w',value],text=True).strip()
 executable=str(probe)
 source_parent=folder
 if a.sanitized:
  env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
  linked=subprocess.check_output(['ldd',str(probe)],text=True);assert 'libasan' in linked and 'libubsan' in linked
 if a.windows:
  r=subprocess.run([executable,'temp','unused'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,env=env,text=True,timeout=15);assert r.returncode==0,r.stderr
  native_owned=Path(subprocess.check_output(['wslpath','-u',r.stdout.strip()],text=True).strip());assert native_owned.is_dir() and native_owned.name.startswith('getdaq-config-transactions-')
  source_parent=native_owned
 def setup(name):
  d=source_parent/name;d.mkdir();(d/'root.xcfg').write_text('<Setup><A include="a.xcfg"/><B include="b.xcfg"/></Setup>');(d/'a.xcfg').write_text('<A><n type="int">1</n></A>');(d/'b.xcfg').write_text('<B><n type="int">1</n></B>');return d
 def call(mode,d,arg=None,expected=0):
  cmd=[executable,mode,native(d)]+([native(arg)] if arg else []);p=subprocess.run(cmd,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=20);runs.append(dict(mode=mode,arguments=cmd[2:],exit_code=p.returncode,output=p.stdout));assert p.returncode==expected,p.stdout;return p.stdout
 def start(mode,d,name):
  gate=folder/(name+'.gate');gate.write_text('owned gate\n');log=(folder/(name+'.log')).open('w');p=subprocess.Popen([executable,mode,native(d),native(gate)],env=env,stdout=log,stderr=subprocess.STDOUT);log.close();processes.append(p)
  ready=Path(str(gate)+'.ready');deadline=time.monotonic()+15
  while True:
   if ready.exists():
    try:record=json.loads(ready.read_text());break
    except (ValueError,OSError):pass
   assert p.poll() is None or ready.exists(),(folder/(name+'.log')).read_text();assert time.monotonic()<deadline,'Gate readiness deadline';time.sleep(.01)
  return p,gate,record
 try:
  d=setup('active-writer');writer,gate,w=start('hold-write',d,'active-writer');assert '>2<' in (d/'a.xcfg').read_text() and '>1<' in (d/'b.xcfg').read_text()
  assert 'transaction busy' in call('read',d,expected=3);gate.unlink();assert writer.wait(timeout=15)==0;assert json.loads(call('read',d))==dict(a='2',b='2')
  d=setup('active-reader');preloaded,write_gate,p=start('preloaded-write',d,'preloaded');reader,read_gate,r=start('hold-read',d,'active-reader');assert p['cache']==r['cache'];assert json.loads(call('read',d))==dict(a='1',b='1')
  write_gate.unlink();assert preloaded.wait(timeout=15)==4;assert 'transaction busy' in (folder/'preloaded.log').read_text();assert '>1<' in (d/'a.xcfg').read_text();read_gate.unlink();assert reader.wait(timeout=15)==0
  d=setup('crashed-writer');writer,gate,c=start('crash-write',d,'crashed-writer');assert writer.wait(timeout=15)==77;assert '>2<' in (d/'a.xcfg').read_text() and '>1<' in (d/'b.xcfg').read_text()
  assert 'unfinished Save All' in call('read',d,expected=3);call('recover',d,c['recovery']);assert json.loads(call('read',d))==dict(a='2',b='2');gate.unlink()
  d=setup('crashed-reader');reader,read_gate,r=start('crash-read',d,'crashed-reader');assert reader.wait(timeout=15)==78;read_gate.unlink()
  writer,gate,w=start('hold-write',d,'after-crashed-reader');gate.unlink();assert writer.wait(timeout=15)==0;assert json.loads(call('read',d))==dict(a='2',b='2')
  if not a.windows:
   lock_root=folder/'cache/getdaq-configuration-transactions-v1'
   lock_root.chmod(0o755)
   assert 'owner/permissions differ' in call('read',d,expected=3)
   lock_root.chmod(0o700)
   saved=lock_root.with_name(lock_root.name+'-held');lock_root.rename(saved);lock_root.symlink_to(saved,target_is_directory=True)
   try:assert 'Invalid configuration transaction cache directory' in call('read',d,expected=3)
   finally:lock_root.unlink();saved.rename(lock_root)
   assert json.loads(call('read',d))==dict(a='2',b='2')
  assert sha(probe)==before and not list(folder.rglob('*.graw')) and not list(source_parent.rglob('*.graw'))
  if native_owned is not None:shutil.copytree(native_owned,folder/'native-source-evidence')
  assert sha(Path(__file__).resolve())==runner_sha
  if a.sanitized:
   texts=[x['output'] for x in runs]+[x.read_text(errors='replace') for x in folder.glob('*.log')]
   assert not any(marker in text for text in texts for marker in ['ERROR: AddressSanitizer','ERROR: LeakSanitizer','AddressSanitizer:DEADLYSIGNAL','runtime error:','LeakSanitizer has encountered a fatal error'])
  result={'passed':True,'sanitized':a.sanitized,'abrupt_child_lsan_verified':False,'runner_sha256':runner_sha,'probe_sha256':before,'actual_windows_runtime':a.windows,'cache_permissions_and_symlink_refusal_checked':not a.windows,'active_partial_generation_refused':True,'preloaded_writer_held_by_reader':True,'parallel_readers_supported':True,'dead_reader_reservation_reconciled':True,'different_application_names_same_cache':True,'crashed_writer_remains_pending':True,'explicit_original_candidate_recovery':True,'raw_files_created':0,'cooperative_same_user_cache_only':True,'uncooperative_external_writers_protected':False,'runs':runs}
  (folder/'result.json').write_text(json.dumps(result,indent=2)+'\n');print('PASS cooperating source transaction processes; no DAQ payload:',folder)
 finally:
  for p in processes:
   if p.poll() is None:
    p.terminate()
    try:p.wait(timeout=3)
    except subprocess.TimeoutExpired:p.kill();p.wait(timeout=3)
  if native_owned is not None:shutil.rmtree(native_owned)
if __name__=='__main__':main()
