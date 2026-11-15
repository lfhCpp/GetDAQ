#!/usr/bin/env python3
"""Independent CLI process versus actual loopback Describe servants."""
import copy
import json
import selectors
import subprocess
import sys
import tempfile
from pathlib import Path

def main():
    binary,fixture=(Path(p).resolve() for p in sys.argv[1:3])
    root=Path(__file__).resolve().parents[1]
    count=0
    with tempfile.TemporaryDirectory(prefix='getdaq-description-cli-') as tmp:
        folder=Path(tmp)
        def invoke(*args,success=True):
            nonlocal count
            result=subprocess.run([str(binary),*map(str,args)],capture_output=True,text=True,timeout=45)
            assert (result.returncode==0)==success,(args,result.stdout,result.stderr)
            count+=1
            return json.loads(result.stdout) if success and result.stdout.startswith('{') else result
        def start(policy,encoding,failure):
            directory=folder/('serve-'+str(count)); directory.mkdir()
            process=subprocess.Popen([str(fixture),str(root/'tests/data/cobo-hardware.json'),str(root/'tests/data/mutant-hardware.json'),
                '--serve',str(directory),str(policy),str(encoding),str(failure)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
            selector=selectors.DefaultSelector(); selector.register(process.stdout,selectors.EVENT_READ)
            try:
                assert selector.select(10),'Fixture did not become ready'
                assert json.loads(process.stdout.readline())['ready']
            except BaseException:
                process.kill(); process.communicate(); raise
            finally: selector.close()
            return process,directory
        def stop(process):
            output,error=process.communicate('stop\n',timeout=10)
            assert process.returncode==0,error
            return json.loads(output.strip())
        invoke('--help')
        invoke(success=False)
        invoke('wrong','missing.json','use-existing',success=False)
        policies=('use-existing','create-empty','replace-existing')
        for encoding in (0,1):
            for policy,name in enumerate(policies):
                process,directory=start(policy,encoding,0)
                try:
                    topology=directory/'topology.json'; audit=directory/'audit'
                    output=invoke('validate-topology',topology,name)
                    assert output=={'valid':True,'nodes':3,'hardware_connected':False,'ownership_declared':False,'hardware_stopped_verified':False}
                    invoke('check-system',topology,name,success=False)
                    invoke('describe-system',topology,name,audit,success=False)
                    assert not audit.exists()
                    invoke('describe-system',topology,name,audit,'--exclusive-stopped','--exclusive-stopped',success=False)
                    invoke('validate-topology',topology,'wrong',success=False)
                    output=invoke('check-system',topology,name,'--exclusive-stopped')
                    assert output['checked'] and not output['mutating_commands_sent'] and not output['register_write_sent']
                    assert not audit.exists()
                    extra=[]
                    if policy==2:
                        old=json.loads(topology.read_text()); old['cobos'].reverse()
                        current=directory/'current.json'; current.write_text(json.dumps(old))
                        extra=['--current-topology',current]
                        wrong=copy.deepcopy(old); wrong['cobos'][0]['node_proxy']=wrong['mutants'][0]['node_proxy']
                        invalid=directory/'invalid-current.json'; invalid.write_text(json.dumps(wrong))
                        invoke('validate-topology',topology,name,'--current-topology',invalid,success=False)
                    else: invoke('validate-topology',topology,name,'--current-topology',topology,success=False)
                    output=invoke('describe-system',topology,name,audit,'--exclusive-stopped',*extra)
                    assert output['phase']=='described' and output['nodes']==3 and not output['production_ecc_state_committed']
                    rows=[json.loads(row) for row in (audit/'system-description.jsonl').read_text().splitlines()]
                    assert rows[-1]['phase']=='described' and not rows[-1]['board_permissions_verified']
                    before=(audit/'system-description.jsonl').read_bytes()
                    invoke('describe-system',topology,name,audit,'--exclusive-stopped',*extra,success=False)
                    assert (audit/'system-description.jsonl').read_bytes()==before
                    invalid=json.loads(topology.read_text()); invalid['extra']=1
                    bad=directory/'invalid.json'; bad.write_text(json.dumps(invalid))
                    invoke('validate-topology',bad,name,success=False)
                finally: state=stop(process)
                assert state['writes']==4 and state['creates']==(0 if policy==0 else 3) and state['destroys']==(3 if policy==2 else 0),state
                assert [a for a in state['actions'] if '=0' in a]==['C0.initDone=0','C1.initDone=0','C0.mutantMode=0','C1.mutantMode=0']
        for policy,failure,expected in ((1,1,(0,0,0)),(1,2,(2,0,0)),(2,3,(0,1,0)),(0,4,(0,0,2))):
            process,directory=start(policy,1,failure)
            try:
                topology=directory/'topology.json'; audit=directory/'audit'
                # 错误的实际目标类型不影响离线校验，离线不尝试连接硬件。
                invoke('validate-topology',topology,policies[policy])
                result=invoke('describe-system',topology,policies[policy],audit,'--exclusive-stopped',success=False)
                if failure!=1:
                    assert 'UNKNOWN' in result.stderr
                    rows=[json.loads(row) for row in (audit/'system-description.jsonl').read_text().splitlines()]
                    assert rows[-1]['phase']=='failed' and not rows[-1]['automatic_retry'] and not rows[-1]['automatic_cleanup']
                else: assert not audit.exists()
            finally: state=stop(process)
            assert (state['creates'],state['destroys'],state['writes'])==expected,state
    print('PASS description CLI:',count,'contracts; independent processes, three policies/two encodings, offline versus live checks, current topology binding, ACK loss and no automatic cleanup')

if __name__=='__main__':
    main()
