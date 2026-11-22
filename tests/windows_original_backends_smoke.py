#!/usr/bin/env python3
"""Native Windows loopback acceptance; never addresses physical hardware."""
import argparse,hashlib,json,os,signal,socket,subprocess,tempfile,time,urllib.request,shutil
from pathlib import Path
import xml.etree.ElementTree as ET

_ports=set()
def available_port():
    while True:
        with socket.socket() as s:s.bind(('127.0.0.1',0));port=s.getsockname()[1]
        if port not in _ports:
            _ports.add(port)
            return port
def wait(check,seconds=15):
    until=time.monotonic()+seconds
    while time.monotonic()<until:
        result=check()
        if result:return result
        time.sleep(.05)
    raise RuntimeError('Loopback condition did not become ready')
def listening(port):
    try:
        with socket.create_connection(('127.0.0.1',port),timeout=.1):return True
    except OSError:return False
def receive(sock,size):
    data=b''
    while len(data)<size:
        part=sock.recv(size-len(data))
        if not part:raise RuntimeError('Preview disconnected early')
        data+=part
    return data

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--bin',type=Path,required=True);p.add_argument('--formats',type=Path,required=True);p.add_argument('--compiler-bin',type=Path);p.add_argument('--evidence',type=Path,required=True);a=p.parse_args()
    if os.name!='nt':p.error('Run with native Windows Python and Windows executables')
    programs=a.bin.resolve();a.evidence.mkdir(parents=True,exist_ok=True);root=a.evidence.resolve();env=os.environ.copy()
    if a.compiler_bin:env['PATH']=str(a.compiler_bin)+';'+env.get('PATH','')
    env['GETDAQ_COBO_FORMATS']=str(a.formats.resolve());children=[];logs=[];checks=[]
    def start(name,args,folder,tag=None):
        folder.mkdir(exist_ok=True)
        for config in a.formats.parent.glob('*.xcfg'):shutil.copy2(config,folder/config.name)
        log=(root/((tag or name)+'.log')).open('wb');logs.append(log)
        process=subprocess.Popen([str(programs/name),*args],cwd=folder,env=env,stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NEW_PROCESS_GROUP);children.append(process);return process
    def soap(port,operation):
        body=('<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" xmlns:ecc="urn:http://localhost:8083/"><s:Body><ecc:'+operation+'/></s:Body></s:Envelope>').encode()
        with urllib.request.urlopen(urllib.request.Request('http://127.0.0.1:'+str(port)+'/',body,{'Content-Type':'text/xml','SOAPAction':'""'}),timeout=4) as response:return ET.fromstring(response.read())
    try:
        for kind in ['describe','prepare','configure']:(root/(kind+'-loopback.xcfg')).write_text('<Setup/>')
        instances=[]
        for index in [0,1]:
            port,alarm=available_port(),available_port();ecc=start('getEccSoapServer.exe',['--port',str(port),'--alarm-port',str(alarm),'--config-repo-url',str(root)],root/('ecc-'+str(index)),tag='ecc-'+str(index));instances.append((port,ecc))
            wait(lambda:listening(port));reply=soap(port,'GetState');state={name:reply.find('.//'+name).text for name in ['State','Transition','ErrorCode']};assert state=={'State':'1','Transition':'0','ErrorCode':'0'},state
            configs=soap(port,'GetConfigIDs');assert 'loopback' in ''.join(configs.itertext())
        soap(instances[0][0],'ServerExit');assert instances[0][1].wait(timeout=10)==0
        assert soap(instances[1][0],'GetState').find('.//State').text=='1'
        soap(instances[1][0],'ServerExit');assert instances[1][1].wait(timeout=10)==0
        checks.append('two original SOAP ECC instances start independently in Idle, enumerate Windows configuration paths and exit cleanly without hardware commands')
        for name in ['DataExporter.exe','dataRouter.exe']:
            control,data,preview=available_port(),available_port(),available_port();folder=root/name.removesuffix('.exe')
            args=['127.0.0.1:'+str(control),'127.0.0.1:'+str(data)]+(['--export-port',str(preview)] if name=='DataExporter.exe' else ['TCP','FrameStorage'])
            service=start(name,args,folder);wait(lambda:listening(control));client=None
            if name=='DataExporter.exe':
                wait(lambda:listening(preview));client=socket.create_connection(('127.0.0.1',preview),timeout=5);client.settimeout(5);time.sleep(.15)
            payload=bytearray([1])*4096;payload[0]=6;payload[1:4]=(64).to_bytes(3,'big');payload[4]=7;payload[5:7]=(1).to_bytes(2,'big');payload[7]=4;payload[8:10]=(2).to_bytes(2,'big');payload[10:12]=(4).to_bytes(2,'big');payload[12:16]=(992).to_bytes(4,'big');payload[22:26]=(1).to_bytes(4,'big');payload[26:28]=bytes([0,0])
            frames=[]
            for event in range(1,65):
                frame=bytearray(payload);frame[22:26]=event.to_bytes(4,'big');frames.append(bytes(frame))
            expected=b''.join(frames)
            with socket.create_connection(('127.0.0.1',data),timeout=5) as sender:
                sender.sendall(frames[0])
                if client:
                    size=int.from_bytes(receive(client,8),'little');assert size==len(payload),size;assert receive(client,size)==frames[0];client.close();checks.append('DataExporter TCP preview forwards exact synthetic MFM frame')
                sender.sendall(b''.join(frames[1:]))
            def raw_ready():
                files=list(folder.glob('*.graw'));return files[0] if files and files[0].stat().st_size>=len(expected) else None
            raw=wait(raw_ready);assert raw.read_bytes()==expected;assert ':' not in raw.name;checks.append(name+' stores 64 exact MFM frames with a Windows-safe filename, including frames after preview disconnect')
            service.send_signal(signal.CTRL_BREAK_EVENT);assert service.wait(timeout=10)==0;checks.append(name+' stops gracefully with Ctrl+Break')
        result={'passed':True,'platform':'Windows','checks':checks,'real_hardware':False,'sha256':{name:hashlib.sha256((programs/name).read_bytes()).hexdigest() for name in ['getEccSoapServer.exe','DataExporter.exe','dataRouter.exe']}}
        (root/'result.json').write_text(json.dumps(result,indent=2)+'\n');print('PASS',len(checks),'native Windows backend checks')
    finally:
        for child in children:
            if child.poll() is None:child.terminate();child.wait(timeout=10)
        for log in logs:log.close()
if __name__=='__main__':main()
