#!/usr/bin/env python3
"""本机独立配置仓库/Qt上传客户端的HTTP联调；不接硬件或原始DAQ。"""
import argparse
import hashlib
import http.client
import json
import os
from pathlib import Path
import re
import selectors
import signal
import socketserver
import threading
import subprocess
import tempfile
import time


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('repository',type=Path);parser.add_argument('service',type=Path);parser.add_argument('client',type=Path)
    parser.add_argument('--evidence',type=Path,required=True)
    parser.add_argument('--manifest',type=Path,help='Existing validated publication; defaults to prior frozen phase evidence')
    args=parser.parse_args()
    programs=[p.resolve() for p in (args.repository,args.service,args.client)]
    args.evidence.mkdir(parents=True,exist_ok=True);root=Path(tempfile.mkdtemp(prefix='repository-network-',dir=args.evidence)).resolve();repository=root/'repository';credential=root/'credential-private';traces=[]
    def command(program,arguments,success=True):
        result=subprocess.run([str(program)]+list(map(str,arguments)),capture_output=True,text=True,timeout=180)
        traces.append({'program':str(program),'args':list(map(str,arguments)),'returncode':result.returncode,'stdout':result.stdout,'stderr':result.stderr})
        assert (result.returncode==0)==success,traces[-1]
        return json.loads(result.stdout) if success and result.stdout.lstrip().startswith('{') else None
    command(programs[0],['initialize',repository]);command(programs[1],['--generate-token',credential])
    token=credential.read_text().strip();assert re.fullmatch('[0-9a-f]{64}',token)
    service=subprocess.Popen([str(programs[1]),'--root',str(repository),'--credential',str(credential),'--port','0'],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,bufsize=1)
    selector=selectors.DefaultSelector();selector.register(service.stdout,selectors.EVENT_READ);lines=[];port=None
    try:
        deadline=time.monotonic()+20
        while port is None:
            assert time.monotonic()<deadline and service.poll() is None,'Service startup failed'
            if not selector.select(0.25):continue
            line=service.stdout.readline();lines.append(line);match=re.search(r'port\s+(\d+)',line)
            if match:port=int(match.group(1))
        def request(method,path,body=b'',authorized=True,repository_id=None,content_type='application/json',declared_length=None):
            connection=http.client.HTTPConnection('127.0.0.1',port,timeout=120);headers={'Content-Type':content_type,'Connection':'close'}
            if authorized:headers['Authorization']='Bearer '+token
            if repository_id is not None:headers['X-Repository-ID']=repository_id
            if declared_length is not None:headers['Content-Length']=str(declared_length)
            connection.request(method,path,body,headers);response=connection.getresponse();data=response.read();code=response.status;connection.close()
            assert token.encode() not in data;return code,json.loads(data)
        assert request('GET','/v1/identity',authorized=False)[0]==401
        code,identity=request('GET','/v1/identity');assert code==200;repository_id=identity['repository_id'];assert not identity['hardware_loaded_verified']
        endpoint=['--host','127.0.0.1','--port',str(port),'--credential',credential]
        assert command(programs[2],endpoint+['identity'])['repository_id']==repository_id
        reviewed=endpoint+['--repository-id',repository_id]
        repo=Path(__file__).resolve().parents[1]
        manifests=sorted((repo/'build-suite-tests/validation-configuration-publication/cli').glob('*/v1/publication.json'))
        assert args.manifest is not None or manifests,'Supply --manifest PACKAGE/publication.json or prior frozen evidence'
        manifest=args.manifest.resolve() if args.manifest else manifests[0];expected=json.loads(manifest.read_bytes());publication_sha=hashlib.sha256(manifest.read_bytes()).hexdigest()
        proxy_requests=[]
        class LoseCommitReply(socketserver.StreamRequestHandler):
            def handle(self):
                line=self.rfile.readline(16385).decode('ascii').strip().split(' ')
                if len(line)!=3:return
                method,path,_=line;headers={}
                while True:
                    raw=self.rfile.readline(16385)
                    if raw==b'\r\n':break
                    key,value=raw.decode('latin1').split(':',1);headers[key]=value.strip()
                body=self.rfile.read(int(headers.get('Content-Length','0')))
                remote=http.client.HTTPConnection('127.0.0.1',port,timeout=120)
                remote.request(method,path,body,headers);response=remote.getresponse();payload=response.read();code=response.status;remote.close()
                drop=path=='/v1/commit' and not any(item['lost'] for item in proxy_requests)
                proxy_requests.append({'method':method,'path':path,'status':code,'lost':drop})
                if drop:return
                self.wfile.write(f'HTTP/1.1 {code} Reply\r\nContent-Type: application/json\r\nContent-Length: {len(payload)}\r\nConnection: close\r\n\r\n'.encode()+payload)
        proxy=socketserver.ThreadingTCPServer(('127.0.0.1',0),LoseCommitReply);proxy.daemon_threads=True
        proxy_thread=threading.Thread(target=proxy.serve_forever,daemon=True);proxy_thread.start()
        proxy_endpoint=['--host','127.0.0.1','--port',str(proxy.server_address[1]),'--credential',credential,'--repository-id',repository_id]
        try:
            command(programs[2],proxy_endpoint+['upload',manifest],False)
            (root/'ack-loss-initial-requests.json').write_text(json.dumps(proxy_requests,indent=2)+'\n')
            (root/'ack-loss-initial-processes.json').write_text(json.dumps(traces,indent=2)+'\n')
            assert any(item['lost'] and item['status']==200 for item in proxy_requests),'Commit was not applied before ACK loss'
            receipt=command(programs[2],proxy_endpoint+['upload',manifest])
            assert sum(item['path']=='/v1/commit' for item in proxy_requests)==1,'Unknown registration was resubmitted rather than observed'
        finally:
            proxy.shutdown();proxy.server_close();proxy_thread.join(timeout=5)
        (root/'ack-loss-requests.json').write_text(json.dumps(proxy_requests,indent=2)+'\n')
        assert receipt['publication_sha256']==publication_sha and not receipt['hardware_loaded_verified']
        assert receipt['configuration_sha256']==expected['configuration_sha256']
        first=(repository/'versions'/publication_sha/'registration.json').read_bytes()
        assert command(programs[2],reviewed+['upload',manifest])==receipt
        assert (repository/'versions'/publication_sha/'registration.json').read_bytes()==first
        listed=command(programs[2],reviewed+['list']);assert listed['versions']==[{'publication_sha256':publication_sha,'content_verified':False}]
        code,verified=request('GET','/v1/versions/'+publication_sha);assert code==200 and verified==receipt
        wrong=endpoint+['--repository-id','0'*64];command(programs[2],wrong+['upload',manifest],False)
        assert request('POST','/v1/objects/'+'0'*64,b'wrong',repository_id=repository_id,content_type='application/octet-stream')[0]==400
        assert request('POST','/v1/objects/'+'0'*64,b'wrong',repository_id='0'*64,content_type='application/octet-stream')[0]==412
        # 在发送体前验证声明长度拒绝；发送全部超限体后立即关闭会因未读TCP数据产生RST。
        assert request('POST','/v1/objects/'+'0'*64,repository_id=repository_id,content_type='application/octet-stream',declared_length=65537)[0]==413
        before=list((repository/'versions').iterdir())
        missing=hashlib.sha256(b'missing plan').hexdigest();payload=json.dumps({'schema_version':1,'plan_objects':[missing]}).encode()
        assert request('POST','/v1/commit',payload,repository_id=repository_id)[0]==404
        assert list((repository/'versions').iterdir())==before
        command(programs[0],['list',repository],False) # 同一根只有一个持久写者。
        service.send_signal(signal.SIGTERM);service.wait(timeout=15);assert service.returncode==0
        # 重开仍保留已登记版本；不会创建另一个版本或改旧收据。
        assert command(programs[0],['version',repository,publication_sha])==receipt
        assert len(list((repository/'versions').iterdir()))==1
        data_file=repository/'versions'/publication_sha/expected['sources'][0]['file']['stored'];original=data_file.read_bytes();data_file.chmod(0o600);data_file.write_bytes(b'!'+original[1:]);data_file.chmod(0o400)
        command(programs[0],['version',repository,publication_sha],False)
        data_file.chmod(0o600);data_file.write_bytes(original);data_file.chmod(0o400)
        assert command(programs[0],['version',repository,publication_sha])==receipt
        (root/'processes.json').write_text(json.dumps(traces,indent=2)+'\n');(root/'service-output.txt').write_text(''.join(lines))
        (root/'scope.json').write_text(json.dumps({'physical_host_count':1,'hardware_connected':False,'receiver_executed':False,'raw_mfm_transferred':False,'configuration_http_executed':True,'publication_sha256':publication_sha,'repository_id':repository_id,'program_sha256':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in programs},'test_script_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'excluded_evidence_files':['credential-private']},indent=2)+'\n')
        assert token not in json.dumps(traces) and token not in ''.join(lines)
        print(f'PASS configuration repository HTTP/client: real publication upload/register/ACK-loss observation/dedup/query/reopen, authentication/identity/body bounds/lock/content refusal; evidence {root}')
    finally:
        selector.close()
        if service.poll() is None:service.terminate();service.wait(timeout=15)


if __name__=='__main__':main()
