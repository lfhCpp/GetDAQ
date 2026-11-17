import pathlib,subprocess,socket,time,urllib.request,signal,os,json,tempfile
import argparse
p=argparse.ArgumentParser(description='Original SOAP ECC / DataExporter smoke with a synthetic MFM frame');p.add_argument('bin',type=pathlib.Path);args=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[1]; out=args.bin.resolve();work=pathlib.Path(tempfile.mkdtemp(prefix='getdaq-original-smoke-'))
def port():
 s=socket.socket();s.bind(('127.0.0.1',0));p=s.getsockname()[1];s.close();return p
ports=[port() for _ in range(5)]
ps=[]
try:
 env=os.environ.copy();env['GETDAQ_COBO_FORMATS']=str(root/'third_party/legacy-get/getbench/data/format/CoboFormats.xcfg')
 for name,args in [('DataExporter',[f'127.0.0.1:{ports[2]}',f'127.0.0.1:{ports[3]}','--export-port',str(ports[4]),'--instance',str(work/'marker.json')]),('getEccSoapServer',['--port',str(ports[0]),'--alarm-port',str(ports[1]),'--config-repo-url',str(work)])]:
  (work/'marker.json').touch(); log=open(work/(name+'.log'),'w');ps.append(subprocess.Popen([str(out/name),*args],cwd=work,stdout=log,stderr=subprocess.STDOUT,env=env))
  time.sleep(1);assert ps[-1].poll() is None,(name,ps[-1].poll(),(work/(name+'.log')).read_text())
 for p in [ports[2],ports[3],ports[4]]:
  with socket.create_connection(('127.0.0.1',p),timeout=2):pass
 import struct
 payload=bytearray([1])*4096;payload[0]=6;payload[1:4]=(64).to_bytes(3,'big');payload[4]=7;payload[5:7]=(1).to_bytes(2,'big');payload[7]=4;payload[8:10]=(2).to_bytes(2,'big');payload[10:12]=(4).to_bytes(2,'big');payload[12:16]=(992).to_bytes(4,'big');payload[22:26]=(1).to_bytes(4,'big');payload[26:28]=b'\0\0'
 with socket.create_connection(('127.0.0.1',ports[4]),timeout=3) as exported:
  exported.settimeout(3);time.sleep(.2)
  with socket.create_connection(('127.0.0.1',ports[3]),timeout=3) as sender:sender.sendall(payload);time.sleep(.2)
  def exact(n):
   b=b''
   while len(b)<n:
    part=exported.recv(n-len(b));assert part;b+=part
   return b
  size=struct.unpack('=Q',exact(8))[0];assert size==len(payload),(size,len(payload));assert exact(size)==payload
 time.sleep(.2);raw=list(work.glob('*.graw'));assert len(raw)==1,raw;assert raw[0].read_bytes()==payload
 print('PASS synthetic MFM frame written and exported byte-for-byte')
 body=b'<SOAP-ENV:Envelope xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/" xmlns:ecc="urn:http://localhost:8083/"><SOAP-ENV:Body><ecc:GetState/></SOAP-ENV:Body></SOAP-ENV:Envelope>'
 req=urllib.request.Request(f'http://127.0.0.1:{ports[0]}/',body,{'Content-Type':'text/xml','SOAPAction':'""'})
 print(urllib.request.urlopen(req,timeout=3).read().decode());print('PASS real original ECC state and DataExporter listeners',work)
finally:
 for p in ps:p.send_signal(signal.SIGINT)
 for p in ps:
  try:p.wait(timeout=8)
  except subprocess.TimeoutExpired:p.kill();p.wait();print('FAILED graceful stop')
  print('exit',p.returncode)
