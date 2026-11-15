#!/usr/bin/env python3
"""Real loopback TCP transport checks; delete synthetic raw files on every exit."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import signal
import socket
import subprocess
import tempfile
import time
import urllib.request


def sha(data): return hashlib.sha256(data).hexdigest()


def port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


def frame(event, little=False):
    # Independent GET rev4 header, 4KiB payload with embedded FDT-like words.
    endian = 'little' if little else 'big'
    data = bytearray([event & 255]) * 4096
    data[0] = 6 | (128 if little else 0)
    data[1:4] = (64).to_bytes(3, endian)
    data[4] = 7
    data[5:7] = (1).to_bytes(2, endian)
    data[7] = 4
    data[8:10] = (2).to_bytes(2, endian)
    data[10:12] = (4).to_bytes(2, endian)
    data[12:16] = (992).to_bytes(4, endian)
    data[22:26] = event.to_bytes(4, endian)
    data[26:28] = bytes([0, 0])
    data[128:132] = bytes(4)
    data[132:136] = bytes([255]) * 4
    return bytes(data)


def exact(sock, count):
    data = bytearray()
    while len(data) < count:
        part = sock.recv(count-len(data))
        assert part, 'export EOF before complete frame'
        data.extend(part)
    return bytes(data)


def interrupt(signum, unused):
    signal.signal(signal.SIGINT, signal.SIG_IGN)
    signal.signal(signal.SIGTERM, signal.SIG_IGN)
    raise KeyboardInterrupt('transport test interrupted')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('receiver', type=Path)
    parser.add_argument('evidence', type=Path)
    args = parser.parse_args()
    binary = args.receiver.resolve()
    args.evidence.mkdir(parents=True, exist_ok=True)
    assert shutil.disk_usage(args.evidence).free >= 64*1024*1024, 'insufficient free disk space'
    evidence = Path(tempfile.mkdtemp(prefix='receiver-transports-', dir=args.evidence))
    signal.signal(signal.SIGINT, interrupt)
    signal.signal(signal.SIGTERM, interrupt)
    hb, bye = bytes(4), bytes([255])*4
    a, b = frame(1), frame(2, True)
    # All streams in this invocation together stay far below the user 20GiB cap.
    cases = [
        ('fdt-fragmented-export', 'FDT', [hb+a+hb+b+bye], [a,b], 0, 2, 1),
        ('fdt-eof-without-goodbye', 'FDT', [hb+a], [a], 0, 1, 0),
        ('fdt-two-connections', 'FDT', [hb+a+bye,hb+b+bye], [a,b], 0, 2, 2),
        ('fdt-controls-only', 'FDT', [hb+bye], [], 0, 1, 1),
        ('fdt-after-goodbye', 'FDT', [a+bye+hb], [a], 1, 0, 1),
        ('fdt-partial-word-1', 'FDT', [a+hb[:1]], [a], 1, 0, 0),
        ('fdt-partial-word-2', 'FDT', [a+hb[:2]], [a], 1, 0, 0),
        ('fdt-partial-word-3', 'FDT', [a+hb[:3]], [a], 1, 0, 0),
        ('fdt-truncated-frame', 'FDT', [hb+a[:513]], [], 1, 1, 0),
        ('fdt-oversized-frame', 'FDT', [hb+bytes([6,255,255,255,7,0,1,4])], [], 1, 1, 0),
        ('raw-tcp-rejects-fdt', 'TCP', [hb+a], [], 1, 0, 0),
        ('zbuf-raw-tcp', 'ZBUF', [a+b], [a,b], 0, 0, 0),
    ]
    total = sum(len(wire) for case in cases for wire in case[2])
    assert total <= 20*1024**3
    results = []
    for name, protocol, connections, expected_frames, code, heartbeats, goodbyes in cases:
        folder = evidence/name
        folder.mkdir()
        data_port, status_port, export_port = port(), port(), port()
        process, sender, downstream = None, None, None
        cleanup = []
        stdout, stderr = folder/'stdout.txt', folder/'stderr.txt'
        def status():
            with urllib.request.urlopen('http://127.0.0.1:%d/status' % status_port, timeout=1) as response:
                return json.load(response)
        def wait(predicate):
            deadline = time.monotonic()+15
            while time.monotonic() < deadline:
                assert process.poll() is None, 'receiver exited before expected status: '+stderr.read_text()
                try:
                    value = status()
                    if predicate(value): return value
                except (OSError, ValueError): pass
                time.sleep(.02)
            raise AssertionError('receiver status timeout')
        try:
            with stdout.open('w') as out, stderr.open('w') as err:
                command = [str(binary),'--output',str(folder),'--input-protocol',protocol,
                           '--port',str(data_port),'--status-port',str(status_port),
                           '--queue-mib','1','--max-frame-mib','1']
                if len(connections)==1: command += ['--once']
                export = name=='fdt-fragmented-export'
                if export: command += ['--export-port',str(export_port)]
                process = subprocess.Popen(command,stdout=out,stderr=err)
                wait(lambda value: value['service']=='getdaq-receiver')
                if export:
                    downstream = socket.create_connection(('127.0.0.1',export_port),timeout=5)
                    downstream.settimeout(5)
                    wait(lambda value: value['export_clients']==1)
                for index, wire in enumerate(connections):
                    sender = socket.create_connection(('127.0.0.1',data_port),timeout=5)
                    # Split control words and frame headers, then vary later chunks.
                    if export:
                        for pos in range(0,len(wire),3): sender.sendall(wire[pos:pos+3])
                        for expected in expected_frames:
                            count = int.from_bytes(exact(downstream,8),'little')
                            assert count==len(expected) and exact(downstream,count)==expected
                        live = wait(lambda value: value['fdt_goodbyes']==1 and value['frames']==2)
                        assert live['connected'] and not live['stream_sealed'] and not live['sender_eof_confirmed']
                        assert not (folder/'.getdaq-receiver-seal.json').exists(), 'GOODBYE incorrectly seals before EOF'
                        (folder/'status-after-goodbye-before-eof.json').write_text(json.dumps(live,indent=2)+'\n')
                    else: sender.sendall(wire)
                    sender.close(); sender=None
                    if len(connections)>1:
                        wait(lambda value: value['connections']==index+1 and value['stream_sealed'])
                if len(connections)>1: process.terminate()
                process.wait(timeout=15)
            assert process.returncode==code, (name,process.returncode,stderr.read_text())
            final = json.loads(stdout.read_text().strip().splitlines()[-1])
            assert final['stream_sealed']==(code==0) and bool(final['error'])==(code!=0)
            assert final['frames']==len(expected_frames), (name,final)
            expected = b''.join(expected_frames)
            raw_files = sorted(folder.glob('*.graw'), key=lambda path:int(path.stem.rsplit('_',1)[1]))
            raw = b''.join(path.read_bytes() for path in raw_files)
            assert raw==expected, (name,'stored bytes differ')
            seal_path = folder/'.getdaq-receiver-seal.json'
            assert seal_path.exists()==(code==0)
            if code==0:
                seal=json.loads(seal_path.read_text())
                assert final['received_bytes']==final['sealed_bytes']==len(expected)
                assert seal['complete'] and seal['sender_eof_confirmed'] and int(seal['received_bytes'])==len(expected)
            if protocol=='FDT':
                assert final['wire_received_bytes']==sum(map(len,connections))
                assert final['fdt_heartbeats']==heartbeats and final['fdt_goodbyes']==goodbyes
                assert final['fdt_control_bytes']==4*(heartbeats+goodbyes)
            results.append({'case':name,'return_code':code,'wire_bytes':sum(map(len,connections)),
                            'raw_bytes':len(expected),'raw_sha256':sha(expected),'final_status':final})
            print('PASS',name)
        finally:
            if sender: sender.close()
            if downstream: downstream.close()
            if process and process.poll() is None:
                process.terminate()
                try: process.wait(timeout=5)
                except subprocess.TimeoutExpired: process.kill(); process.wait(timeout=5)
            for path in folder.glob('*.graw'):
                cleanup.append({'name':path.name,'bytes':path.stat().st_size,'sha256':sha(path.read_bytes())})
                path.unlink()
            (folder/'payload-cleanup.json').write_text(json.dumps({'deleted':cleanup,'raw_files_remaining':0},indent=2)+'\n')
    for protocol in ('ICE','fdt'):
        bad = subprocess.run([str(binary),'--output',str(evidence),'--input-protocol',protocol],capture_output=True,text=True)
        assert bad.returncode==2 and 'Input protocol must' in bad.stderr
    managed = subprocess.run([str(binary),'--output',str(evidence),'--input-protocol','FDT',
        '--once','--session-id','a'*64,'--sender-id','test','--configuration-sha256','b'*64,
        '--artifact-sha256','c'*64],capture_output=True,text=True)
    assert managed.returncode==2 and 'Managed receiver plans currently require TCP' in managed.stderr
    result={'schema':1,'receiver_sha256':sha(binary.read_bytes()),'runner_sha256':sha(Path(__file__).read_bytes()),
            'aggregate_wire_bytes':total,'aggregate_limit_bytes':20*1024**3,'cases':results,
            'generated_raw_deleted':not list(evidence.rglob('*.graw')),'real_hardware':False,
            'invalid_protocols_and_managed_fdt_rejected':True}
    (evidence/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    print('PASS transport integration:',len(results),'cases;',total,'wire bytes; raw files deleted;',evidence)


if __name__=='__main__': main()
