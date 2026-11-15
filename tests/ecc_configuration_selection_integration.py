#!/usr/bin/env python3
"""真实仓库→Qt选择导出→离线ECC部署→实际ECC配置缓存HTTP；合成包，不执行硬件/receiver。"""
import argparse
import hashlib
import http.client
import json
from pathlib import Path
import secrets
import selectors
import signal
import subprocess
import tempfile
import time


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['repository','repository_service','upload','ecc_select','ecc_service']:
        parser.add_argument(name,type=Path)
    parser.add_argument('--fixture',type=Path,required=True,help='Successful native fixed test evidence')
    parser.add_argument('--evidence',type=Path,required=True)
    args=parser.parse_args()
    programs={name:getattr(args,name).resolve() for name in ['repository','repository_service','upload','ecc_select','ecc_service']}
    args.evidence.mkdir(parents=True,exist_ok=True)
    root=Path(tempfile.mkdtemp(prefix='selection-system-',dir=args.evidence)).resolve()
    fixture=args.fixture.resolve();traces=[];children=[];handles=[]
    def command(name,arguments,success=True):
        p=subprocess.run([str(programs[name])]+list(map(str,arguments)),capture_output=True,text=True,timeout=180)
        traces.append({'program':name,'arguments':list(map(str,arguments)),'returncode':p.returncode,'stdout':p.stdout,'stderr':p.stderr})
        assert (p.returncode==0)==success,traces[-1]
        return json.loads(p.stdout) if success and p.stdout.lstrip().startswith('{') else None
    def start(name,arguments,marker):
        p=subprocess.Popen([str(programs[name])]+list(map(str,arguments)),stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,bufsize=1)
        children.append(p)
        observer=selectors.DefaultSelector();observer.register(p.stdout,selectors.EVENT_READ);handles.append(observer)
        lines=[];deadline=time.monotonic()+20
        while time.monotonic()<deadline and p.poll() is None:
            if not observer.select(0.25):continue
            line=p.stdout.readline();lines.append(line)
            if marker in line:
                return p,''.join(lines)
        if p.poll() is None:p.terminate();p.wait(timeout=15)
        return None,''.join(lines)
    def request(port,token,path):
        conn=http.client.HTTPConnection('127.0.0.1',port,timeout=10)
        conn.request('GET',path,headers={'Authorization':'Bearer '+token,'Connection':'close'})
        response=conn.getresponse();body=response.read();code=response.status;conn.close()
        assert code==200 and token.encode() not in body,(code,path)
        return json.loads(body)
    try:
        repository=root/'repository';credential=root/'credential-private'
        command('repository',['initialize',repository]);command('repository_service',['--generate-token',credential])
        service,lines=start('repository_service',['--root',repository,'--credential',credential,'--port','0'],'port')
        assert service,lines
        import re
        port=int(re.search(r'port\s+(\d+)',lines).group(1));token=credential.read_text().strip()
        identity=request(port,token,'/v1/identity')
        endpoint=['--host','127.0.0.1','--port',port,'--credential',credential,'--repository-id',identity['repository_id']]
        receipt=command('upload',endpoint+['upload',fixture/'publication/publication.json'])
        publication=receipt['publication_sha256'];selectedFile=root/'selection.json'
        selected=command('upload',endpoint+['selection',publication,selectedFile])
        assert selected['repository_id']==identity['repository_id'] and selected['manifest_file']==receipt['manifest_file']
        receiptFile=Path(receipt['manifest_file']).parent/'registration.json'
        assert selected['registration_sha256']==sha(receiptFile) and selectedFile.stat().st_mode&0o777==0o400
        original=selectedFile.read_bytes()
        command('upload',endpoint+['selection',publication,selectedFile],False)
        assert selectedFile.read_bytes()==original,'Selection overwrite'
        command('upload',endpoint+['selection','0'*64,root/'missing.json'],False)
        assert not (root/'missing.json').exists()
        template=json.loads((fixture/'service.json').read_text())
        template['audit_root']=str(root/'first-boot-controller') # 合成首次部署夹具，不恢复或清除原测试UNKNOWN。
        # 每次选择新的实际监听端口；不预探测/释放socket，不把绑定失败当成功。
        ecc=None
        for attempt in range(3):
            template['port']=30000+secrets.randbelow(25000)
            templateFile=root/('template-'+str(attempt)+'.json')
            templateFile.write_text(json.dumps(template,indent=2)+'\n');templateFile.chmod(0o600)
            deployed=command('ecc_select',[templateFile,selectedFile,root/('deployment-'+str(attempt))])
            configFile=Path(deployed['configuration_file'])
            assert deployed['selection']==selected and not deployed['hardware_connected'] and not deployed['ecc_state_changed']
            assert not Path(template['audit_root']).exists()
            command('ecc_service',['--config',configFile,'--check-config'])
            assert not Path(template['audit_root']).exists(),'Offline check created ECC root'
            ecc,eccLines=start('ecc_service',['--config',configFile],'control listener active')
            if ecc:break
            assert 'Cannot listen on ECC control endpoint' in eccLines,eccLines
        assert ecc,'No ECC listener after explicit collision retries'
        eccToken=(fixture/'credential').read_text().strip()
        cached=request(template['port'],eccToken,'/v1/configuration')
        activeIdentity=request(template['port'],eccToken,'/v1/identity')
        assert cached['instance_id']==activeIdentity['instance_id'] and cached['snapshot_sha256']==activeIdentity['snapshot_sha256']
        assert cached['input_binding_sha256']==deployed['input_binding_sha256'] and cached['selection']==selected
        assert not cached['input_binding_live_verified'] and not cached['hardware_loaded_verified']
        snapshot=json.loads((Path(template['audit_root'])/'snapshot.json').read_text())
        assert snapshot['configuration_selection_sha256']==sha(selectedFile)
        assert snapshot['configuration_registration_sha256']==selected['registration_sha256']
        assert not snapshot['actual_loaded_configuration_verified']
        ecc.send_signal(signal.SIGTERM);ecc.wait(timeout=20);assert ecc.returncode==0
        service.send_signal(signal.SIGTERM);service.wait(timeout=20);assert service.returncode==0
        secretsToExclude=[token,eccToken]
        assert all(secret not in json.dumps(traces) and secret not in eccLines+lines for secret in secretsToExclude)
        (root/'processes.json').write_text(json.dumps(traces,indent=2)+'\n')
        (root/'ecc-configuration.json').write_text(json.dumps(cached,indent=2)+'\n')
        (root/'service-output.txt').write_text(lines+eccLines)
        (root/'scope.json').write_text(json.dumps({'schema':1,'physical_host_count':1,'configuration_http_executed':True,
            'ecc_configuration_http_executed':True,'synthetic_publication':True,'hardware_connected':False,
            'receiver_executed':False,'raw_mfm_transferred':False,'selection_sha256':sha(selectedFile),
            'program_sha256':{str(p):sha(p) for p in programs.values()},'test_script_sha256':sha(Path(__file__)),
            'excluded_evidence_files':['credential-private']},indent=2)+'\n')
        print('PASS actual repository/Qt selection export/offline ECC deployment/ECC configuration cache; no hardware/receiver/DAQ; evidence',root)
    finally:
        for handle in handles:handle.close()
        for child in children:
            if child.poll() is None:child.terminate();child.wait(timeout=20)


if __name__=='__main__':main()
