#!/usr/bin/env python3
"""Cross-process topology parsing/alignment; only loopback simulated hardware."""
import copy
import hashlib
import json
import selectors
import subprocess
import sys
import tempfile
from pathlib import Path


def main():
    binary, fixture = (Path(p).resolve() for p in sys.argv[1:3])
    root = Path(__file__).resolve().parents[1]
    mutant = root / 'tests/data/mutant-hardware.json'
    cobo = root / 'tests/data/cobo-hardware.json'
    originals = list((root / 'source/get/GetBench-software-release-20210208/data/config').glob('hardwareDescription_*.xcfg'))
    before = {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in originals}
    contracts = 0
    with tempfile.TemporaryDirectory(prefix='getdaq-align-cli-') as temporary:
        folder = Path(temporary)
        def invoke(*args, success=True):
            nonlocal contracts
            completed = subprocess.run([str(binary), *map(str,args)],capture_output=True,text=True,timeout=45)
            assert (completed.returncode == 0) == success, (args, completed.stdout, completed.stderr)
            contracts += 1
            return json.loads(completed.stdout) if success and completed.stdout else completed
        def start(directory, failure):
            directory.mkdir()
            process = subprocess.Popen([str(fixture),str(mutant),str(cobo),'--serve',str(directory),str(failure)],
                                       stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
            selector = selectors.DefaultSelector()
            selector.register(process.stdout,selectors.EVENT_READ)
            try:
                assert selector.select(10), 'Fixture did not publish a ready response'
                assert json.loads(process.stdout.readline())['ready']
            except BaseException:
                process.kill(); process.communicate(); raise
            finally:
                selector.close()
            return process
        def stop(process):
            output,error = process.communicate('stop\n',timeout=10)
            assert process.returncode == 0, error
            return json.loads(output.strip())
        process = start(folder/'normal',0)
        try:
            topology = folder/'normal/topology.json'
            document = json.loads(topology.read_text())
            valid = invoke('validate-topology',topology)
            assert valid == {'valid':True,'mutants':3,'cobos':4,'hardware_connected':False,'proxy_types_verified':False}
            changes = [
                ('schema_version',2),('kind','getdaq.wrong'),('encoding','2.0'),('timeout_ms',99),
                ('timeout_ms',60001),('cobos',{}),('mutants',[]),('extra',1),
            ]
            for key,value in changes:
                changed = copy.deepcopy(document); changed[key] = value
                test = folder/'invalid.json'; test.write_text(json.dumps(changed))
                invoke('validate-topology',test,success=False)
            def invalid_change(change):
                changed = copy.deepcopy(document); change(changed)
                target = folder/'invalid.json'; target.write_text(json.dumps(changed))
                invoke('validate-topology',target,success=False)
            invalid_change(lambda x: x['mutants'][1].update(role='master'))
            invalid_change(lambda x: x['mutants'][1].update(role='unknown'))
            invalid_change(lambda x: x['mutants'][1].update(id=x['mutants'][0]['id']))
            invalid_change(lambda x: x['cobos'][0].update(enabled='yes'))
            invalid_change(lambda x: x['cobos'][0].update(hardware_description_file='missing.json'))
            invalid_change(lambda x: x['mutants'][0].pop('expected_node_name'))
            duplicate = folder/'duplicate.yaml'; duplicate.write_text(topology.read_text().replace('"schema_version":1','"schema_version":1,"schema_version":1'))
            invoke('validate-topology',duplicate,success=False)
            multiple = folder/'multiple.yaml'; multiple.write_text(topology.read_text()+'\n---\n{}\n')
            invoke('validate-topology',multiple,success=False)
            relative = folder/'relative.json'; changed = copy.deepcopy(document)
            (folder/'mutant.json').write_bytes(mutant.read_bytes()); (folder/'cobo.json').write_bytes(cobo.read_bytes())
            for row in changed['mutants']: row['hardware_description_file'] = 'mutant.json'
            for row in changed['cobos']: row['hardware_description_file'] = 'cobo.json'
            relative.write_text(json.dumps(changed)); invoke('validate-topology',relative)
            audit = folder/'normal-audit'
            invoke('align-system',topology,audit,success=False)
            assert not audit.exists(), 'Missing exclusive-stopped gate created files'
            invoke('align-system',topology,audit,'--wrong',success=False)
            completed = invoke('align-system',topology,audit,'--exclusive-stopped')
            assert completed['located'] == [{'cobo_id':'C0','mutant_id':'M0','amc_slot':1},
                                            {'cobo_id':'C1','mutant_id':'M1','amc_slot':12},
                                            {'cobo_id':'C2','mutant_id':'M2','amc_slot':6}]
            assert completed['inter_crate_steps'] == 24 and not completed['physical_alignment_verified'] and not completed['full_prepare']
            records = [json.loads(line) for line in (audit/'system.jsonl').read_text().splitlines()]
            assert records[0]['mutants'][0]['bound_description']['devices'] == json.loads(mutant.read_text())['devices']
            assert records[-1]['phase'] == 'complete' and not records[-1]['sync_pulse_sent']
            invoke('align-system',topology,audit,'--exclusive-stopped',success=False)
        finally:
            state = stop(process)
        assert state == {'links':61,'tdc':24,'multi':24,'writes':6}, state
        for failure in (1,3,11):
            directory = folder/('failure-'+str(failure)); process = start(directory,failure)
            try:
                audit = directory/'audit'
                failed = invoke('align-system',directory/'topology.json',audit,'--exclusive-stopped',success=False)
                assert 'UNKNOWN' in failed.stderr
                rows = [json.loads(line) for line in (audit/'system.jsonl').read_text().splitlines()]
                assert any(row['phase'] == 'unknown' for row in rows)
                assert rows[-1]['phase'] == 'failed' and not rows[-1]['automatic_restoration']
            finally:
                state = stop(process)
            if failure == 1: assert state == {'links':1,'tdc':0,'multi':0,'writes':0}, state
            if failure == 3: assert state == {'links':5,'tdc':1,'multi':0,'writes':0}, state
            if failure == 11: assert state == {'links':48,'tdc':24,'multi':4,'writes':0}, state
        for p,digest in before.items(): assert hashlib.sha256(p.read_bytes()).hexdigest() == digest
    print('PASS alignment CLI:',contracts,'contracts; independent process, relative paths, strict schema, owned audit, failures without retry/restoration; original descriptions unchanged')


if __name__ == '__main__':
    main()
