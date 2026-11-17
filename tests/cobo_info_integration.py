"""Real loopback Ice RPC checks; no physical hardware or write commands."""
import json,socket,subprocess,sys,tempfile
from pathlib import Path
helper,fixture=map(lambda p:Path(p).resolve(),sys.argv[1:])
with tempfile.TemporaryDirectory(prefix='getdaq-board-info-') as temp:
    log=Path(temp)/'operations.txt'
    with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
    proc=subprocess.Popen([str(fixture),str(port),str(log)],stdout=subprocess.PIPE,text=True)
    try:
        assert proc.stdout.readline().strip()=='READY'
        proxy=f'HwNode:tcp -h 127.0.0.1 -p {port}'
        def query(*args):return json.loads(subprocess.check_output([str(helper),*args],text=True,timeout=15))
        assert query('devices',proxy)=={'node':'CoBo[0]','devices':['ctrl']}
        for name,value in [('hardwareVersion','5'),('firmwareRelease','249'),('firmwareDate','538380306')]:
            assert query('read-register',proxy,'ctrl',name)['value']==value
        for name in ['firmwareEnable','intEnable']:assert query('read-field',proxy,'ctrl','pipeCtrl',name)['value']=='0'
        before=log.read_text()
        for args in [('write-register',proxy,'ctrl','pipeCtrl','1'),('read-register',proxy,'ctrl','notAllowed')]:
            result=subprocess.run([str(helper),*args],capture_output=True,timeout=5);assert result.returncode!=0
        assert log.read_text()==before,'Rejected command reached the board'
        calls=[line.split() for line in before.splitlines()]
        assert all(name in ['name','getListOfDevices','findDevice','readRegister','readField'] and encoding=='1.0' for name,encoding in calls)
        assert len(calls)==12,(len(calls),calls)
        print('PASS six real Ice metadata queries; 12 read-only RPCs; encoding 1.0; invalid/write commands rejected before connecting')
    finally:
        proc.terminate();proc.wait(timeout=5)
