#!/usr/bin/env python3
"""Real DataExporter: bounded slow-client preview, exact raw bytes, comparative timing."""
import argparse,hashlib,json,os,signal,socket,struct,subprocess,tempfile,threading,time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('bin',type=Path);p.add_argument('--frames',type=int,default=8192);p.add_argument('--viewer-probe',type=Path);p.add_argument('--evidence',type=Path,required=True);a=p.parse_args();a.evidence.mkdir(parents=True,exist_ok=True);root=Path(__file__).resolve().parents[1];used=set()
def port():
 while True:
  with socket.socket() as s:s.bind(('127.0.0.1',0));v=s.getsockname()[1]
  if v not in used:used.add(v);return v
def run_case(preview):
 work=Path(tempfile.mkdtemp(prefix='slow-' if preview else 'baseline-',dir=a.evidence));control,data,export=[port() for _ in range(3)];env=dict(os.environ,GETDAQ_COBO_FORMATS=str(root/'third_party/legacy-get/getbench/data/format/CoboFormats.xcfg'));log=(work/'receiver.log').open('wb');proc=subprocess.Popen([str(a.bin.resolve()/'DataExporter'),f'127.0.0.1:{control}',f'127.0.0.1:{data}','--export-port',str(export),'--instance',str(work/'instance')],cwd=work,env=env,stdout=log,stderr=subprocess.STDOUT);clients=[];probe=None;fast_count=[0];failed=[];rss=[];finished=threading.Event()
 frame=bytearray([1])*4096;frame[0]=6;frame[1:4]=(64).to_bytes(3,'big');frame[4]=7;frame[5:7]=(1).to_bytes(2,'big');frame[7]=4;frame[8:10]=(2).to_bytes(2,'big');frame[10:12]=(4).to_bytes(2,'big');frame[12:16]=(992).to_bytes(4,'big');frame[22:26]=(1).to_bytes(4,'big');frame[26:28]=bytes([0,0])
 for i in range(992):frame[128+4*i:132+4*i]=((i//512)<<23 | (i%512)<<14 | i%4096).to_bytes(4,"big")
 frame=bytes(frame)
 def sample_rss():
  while not finished.wait(.005):
   try:
    stat=Path("/proc",str(proc.pid),"status").read_text();rss.append(next(int(x.split()[1])*1024 for x in stat.splitlines() if x.startswith("VmRSS:")))
   except (OSError,StopIteration):pass
 monitor=threading.Thread(target=sample_rss,daemon=True);monitor.start()
 def connected(port):
  deadline=time.monotonic()+8
  while time.monotonic()<deadline:
   try:return socket.create_connection(('127.0.0.1',port),timeout=.5)
   except OSError:
    if proc.poll() is not None:raise RuntimeError('DataExporter exited')
    time.sleep(.05)
  raise TimeoutError('Listener did not start')
 def fast_read(sock):
  def exact(n):
   result=bytearray()
   while len(result)<n:
    part=sock.recv(n-len(result))
    if not part:raise EOFError
    result.extend(part)
   return bytes(result)
  try:
   while not finished.is_set():
    n=struct.unpack('<Q',exact(8))[0]
    body=exact(n) if n==len(frame) else b''
    if n!=len(frame) or body[:22]+body[26:]!=frame[:22]+frame[26:]:raise ValueError('TCP frame boundary or bytes corrupted')
    fast_count[0]+=1
  except (OSError,EOFError):pass
  except Exception as e:failed.append(str(e))
 try:
  raw=connected(data)
  if preview:
   slow=connected(export);slow.setsockopt(socket.SOL_SOCKET,socket.SO_RCVBUF,1024);fast=connected(export);fast.settimeout(2);clients=[slow,fast];thread=threading.Thread(target=fast_read,args=(fast,),daemon=True);thread.start()
  probe=None
  if preview and a.viewer_probe:
   probe=subprocess.Popen([str(a.viewer_probe.resolve()),f'127.0.0.1:{export}'],stdout=subprocess.PIPE,stderr=subprocess.PIPE);time.sleep(.2)
  time.sleep(.15);start=time.perf_counter();expected=len(frame)*a.frames
  expected_hash=hashlib.sha256()
  for offset in range(0,a.frames,128):
   packet=bytearray()
   for i in range(offset,min(offset+128,a.frames)):
    current=bytearray(frame);current[22:26]=(i+1).to_bytes(4,'big');packet.extend(current)
   expected_hash.update(packet);raw.sendall(packet)
  raw.shutdown(socket.SHUT_WR);raw.close();deadline=time.monotonic()+15
  while time.monotonic()<deadline:
   stat=Path('/proc',str(proc.pid),'status').read_text();rss.append(next(int(x.split()[1])*1024 for x in stat.splitlines() if x.startswith('VmRSS:')))
   if sum(f.stat().st_size for f in work.glob('*.graw'))>=expected-8192:break
   time.sleep(.01)
  duration=time.perf_counter()-start;time.sleep(1.2);proc.send_signal(signal.SIGINT);proc.wait(timeout=10);assert proc.returncode==0
  files=list(work.glob('*.graw'));actual=b''.join(f.read_bytes() for f in sorted(files));assert len(actual)==expected and hashlib.sha256(actual).digest()==expected_hash.digest(),'Raw write differs under preview pressure'
  assert not failed,failed
  if preview:assert fast_count[0]>0,'Healthy preview client starved'
  assert max(rss,default=0)<768*1024*1024,'Receiver RSS exceeded bounded preview budget'
  probe_result=None
  if preview and a.viewer_probe:
   output,errors=probe.communicate(timeout=10);assert probe.returncode==0,(output,errors);probe_result=json.loads(output);assert probe_result['decoded']
  text=(work/'receiver.log').read_text(errors='replace');return {'seconds':duration,'raw_bytes':expected,'peak_rss':max(rss,default=0),'fast_frames':fast_count[0],'preview_drop_reported':'Preview frames discarded:' in text,'online_probe':probe_result}
 finally:
  finished.set()
  for s in clients:s.close()
  if proc.poll() is None:proc.terminate();proc.wait(timeout=10)
  if probe is not None and probe.poll() is None:probe.terminate();probe.wait(timeout=5)
  log.close()
baseline=run_case(False);overload=run_case(True);assert overload['preview_drop_reported'],'Slow client did not exercise preview dropping';result={'baseline':baseline,'slow_and_fast_client':overload,'raw_bytes_identical':True,'preview_framing_intact':True,'hardware':False};(a.evidence/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
