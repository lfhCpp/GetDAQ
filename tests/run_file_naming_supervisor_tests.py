#!/usr/bin/env python3
"""Sequential native lifecycle checks, including GET names and metadata-only catalog."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import tempfile


def sha(data):return hashlib.sha256(data).hexdigest()


def interrupted(signum,unused):
    signal.signal(signal.SIGINT,signal.SIG_IGN);signal.signal(signal.SIGTERM,signal.SIG_IGN)
    raise KeyboardInterrupt('managed filename test interrupted')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('fixture',type=Path);parser.add_argument('receiver',type=Path);parser.add_argument('evidence',type=Path)
    args=parser.parse_args();fixture=args.fixture.resolve();receiver=args.receiver.resolve();args.evidence.mkdir(parents=True,exist_ok=True)
    folder=Path(tempfile.mkdtemp(prefix='managed-file-naming-',dir=args.evidence))
    signal.signal(signal.SIGINT,interrupted);signal.signal(signal.SIGTERM,interrupted)
    scenarios=['fdt-unused','fdt-mixed-unused','fdt-cycle','fdt-active-finish','fdt-empty-finish','fdt-partial-start','zbuf-managed','protocol-identity','observe-clean','observe-error','observe-signal','observe-lost','get-naming','naming-identity','normal','empty','truncated','signal-no-eof','timeout','partial-start','abort-connected','identity','file-set','storage','journal','lease','reopen','validation','catalog']
    results=[];deleted=[]
    wrapper=folder/'ignore-file-naming.py'
    wrapper.write_text('#!/usr/bin/python3\nimport os,sys\nreal='+repr(str(receiver))+'\nargs=sys.argv[1:]\nif "--file-naming" in args:\n i=args.index("--file-naming");del args[i:i+2]\nos.execv(real,[real]+args)\n')
    wrapper.chmod(0o750)
    protocol_wrapper=folder/'ignore-input-protocol.py'
    protocol_wrapper.write_text('#!/usr/bin/python3\nimport os,sys\nreal='+repr(str(receiver))+'\nargs=sys.argv[1:]\nif "--input-protocol" in args:\n i=args.index("--input-protocol");del args[i:i+2]\nos.execv(real,[real]+args)\n')
    protocol_wrapper.chmod(0o750)
    # Fixture payloads are fixed: at most two receivers, two cycles, 12 * 4KiB each.
    maximum_wire_bytes=len(scenarios)*2*2*12*4096
    assert maximum_wire_bytes<=20*1024**3
    try:
        for scenario in scenarios:
            target=folder/scenario;binary=wrapper if scenario=='naming-identity' else protocol_wrapper if scenario=='protocol-identity' else receiver
            command=[str(fixture),str(binary),str(target),scenario];process=None
            try:
                with (folder/(scenario+'.log')).open('w') as log:
                    process=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
                    code=process.wait(timeout=60)
                assert code==0,(scenario,(folder/(scenario+'.log')).read_text())
                result=json.loads((target/'fixture-result.json').read_text());assert result['passed'] and not result['real_hardware']
                if scenario=='get-naming':
                    plan=json.loads((target/'deployment/plan.json').read_text())
                    assert all(source['file_naming']=='get' for source in plan['sources'])
                    catalog=json.loads((target/'catalog-result.json').read_text())
                    assert catalog['complete'] and catalog['raw_payload_bytes_read']=='0'
                    raw=list(target.rglob('*.graw'));assert raw and all('T' in p.name and ':' in p.name for p in raw)
                results.append({'scenario':scenario,'command':command,'exit_code':code})
                print('PASS',scenario)
            finally:
                # Own fixture session only; never signal a product or unrelated process.
                if process:
                    try:os.killpg(process.pid,signal.SIGTERM)
                    except ProcessLookupError:pass
                    if process.poll() is None:
                        try:process.wait(timeout=3)
                        except subprocess.TimeoutExpired:
                            try:os.killpg(process.pid,signal.SIGKILL)
                            except ProcessLookupError:pass
                            process.wait(timeout=3)
                    else:
                        # The fixture normally reaps all children. Check remaining owned group before cleanup.
                        try:os.killpg(process.pid,0)
                        except ProcessLookupError:pass
                        else:os.killpg(process.pid,signal.SIGKILL)
    finally:
        for path in folder.rglob('*.graw'):
            with path.open('rb') as raw:
                digest=hashlib.sha256()
                for chunk in iter(lambda:raw.read(1024*1024),b''):digest.update(chunk)
            deleted.append({'path':str(path.relative_to(folder)),'bytes':path.stat().st_size,'sha256':digest.hexdigest()})
            path.unlink()
        (folder/'payload-cleanup.json').write_text(json.dumps({'deleted':deleted,'raw_files_remaining':len(list(folder.rglob('*.graw')))},indent=2)+'\n')
    result={'schema':1,'fixture_sha256':sha(fixture.read_bytes()),'receiver_sha256':sha(receiver.read_bytes()),'runner_sha256':sha(Path(__file__).read_bytes()),
            'cases':results,'maximum_wire_bytes':maximum_wire_bytes,'aggregate_limit_bytes':20*1024**3,'generated_raw_deleted':not list(folder.rglob('*.graw')),'real_hardware':False}
    (folder/'result.json').write_text(json.dumps(result,indent=2)+'\n');print('PASS',len(results),'sequential lifecycle cases; synthetic raw files deleted;',folder)


if __name__=='__main__':main()
