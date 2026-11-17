"""Check an already-running original DataRouter without installing DataExporter."""
import argparse,os,pathlib,shutil,signal,socket,subprocess,tempfile,time
p=argparse.ArgumentParser();p.add_argument('--ecc',type=pathlib.Path,required=True);p.add_argument('--router',type=pathlib.Path,required=True);p.add_argument('--probe',type=pathlib.Path,required=True);a=p.parse_args();root=pathlib.Path(__file__).resolve().parents[1];work=pathlib.Path(tempfile.mkdtemp(prefix='getdaq-external-router-'));bins=work/'bin';bins.mkdir();(work/'marker.json').touch()
for name,src in [('getEccSoapServer',a.ecc),('dataRouter',a.router)]:shutil.copy2(src,bins/name)
assert not (bins/'DataExporter').exists()
for f in (root/'third_party/legacy-get/getbench/data/format').glob('*.xcfg'):shutil.copy2(f,work/f.name)
def port():
 s=socket.socket();s.bind(('127.0.0.1',0));n=s.getsockname()[1];s.close();return n
ports=[port() for _ in range(4)];children=[];env=os.environ.copy();env['GETDAQ_COBO_FORMATS']=str(root/'third_party/legacy-get/getbench/data/format/CoboFormats.xcfg')
try:
 commands=[['getEccSoapServer','--port',str(ports[0]),'--alarm-port',str(ports[1]),'--config-repo-url',str(work)],['dataRouter',f'127.0.0.1:{ports[2]}',f'127.0.0.1:{ports[3]}','TCP','FrameStorage']]
 for name,*args in commands:
  log=open(work/(name+'.log'),'wb');children.append(subprocess.Popen([str(bins/name),*args],cwd=work,env=env,stdout=log,stderr=subprocess.STDOUT));time.sleep(.5);assert children[-1].poll() is None
 subprocess.run([str(a.probe.resolve()),str(ports[0]),'127.0.0.1',str(ports[3])],check=True,timeout=10)
 b=bytearray([1])*4096;b[0]=6;b[1:4]=(64).to_bytes(3,'big');b[4]=7;b[5:7]=(1).to_bytes(2,'big');b[7]=4;b[8:10]=(2).to_bytes(2,'big');b[10:12]=(4).to_bytes(2,'big');b[12:16]=(992).to_bytes(4,'big');b[22:26]=(1).to_bytes(4,'big');b[26:28]=b'\0\0'
 with socket.create_connection(('127.0.0.1',ports[3]),timeout=3) as sender:sender.sendall(b);time.sleep(.2)
 deadline=time.monotonic()+3
 while time.monotonic()<deadline:
  files=list(work.glob('*.graw'))
  if len(files)==1 and files[0].read_bytes()==b:break
  time.sleep(.05)
 else:raise AssertionError('Original DataRouter did not write the original frame bytes')
 print('PASS external original DataRouter receives exact MFM bytes; no DataExporter installed or launched',work)
finally:
 for i,child in enumerate(children):child.send_signal(signal.SIGINT if i==0 else signal.SIGTERM)
 for child in children:
  try:child.wait(timeout=10)
  except subprocess.TimeoutExpired:child.kill();child.wait();raise AssertionError('Backend did not stop normally')
