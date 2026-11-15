#!/usr/bin/env python3
"""FDT cycle through real authenticated data service; keep all raw in owned cleanup root."""
import argparse
import json
from pathlib import Path
import signal
import socket
from types import SimpleNamespace
import tempfile
import subprocess
import urllib.request
from run_supervisor_control_tests import Rig, frame, wait_for, require, process_identity
from receiver_transport_integration import port
from run_receiver_observation_tests import interrupted, sha


class FdtRig(Rig):
    def start_service(self):
        if self.process is None:
            for source in self.plan['sources']:source['input_protocol']='FDT'
            self.config_file.write_text(json.dumps(self.config,indent=2)+'\n')
            check=subprocess.run([str(self.args.service),'--config',str(self.config_file),'--check-config'],capture_output=True,text=True,timeout=10)
            require(check.returncode==0,'FDT offline service configuration rejected')
        super().start_service()

    def receiver_status(self, index):
        source = self.plan['sources'][index]
        with urllib.request.urlopen('http://127.0.0.1:'+str(source['status_port'])+'/status', timeout=.5) as response:
            return json.load(response)


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('build',type=Path);parser.add_argument('receiver',type=Path);parser.add_argument('evidence',type=Path)
    args=parser.parse_args();build=args.build.resolve();args.evidence.mkdir(parents=True,exist_ok=True)
    folder=Path(tempfile.mkdtemp(prefix='receiver-cycle-http-',dir=args.evidence))
    signal.signal(signal.SIGINT,interrupted);signal.signal(signal.SIGTERM,interrupted)
    options=SimpleNamespace(service=build/'getdaq-supervisor',fixture=build/'supervisor-control-tests',query=build/'getdaq-supervisor-query',receiver=args.receiver.resolve(),file_naming='native')
    cases=[];deleted=[];wire_bytes=0
    for name in ['segments','active-timeout','empty-cycle','unused','mixed-unused']:
        selected=[]
        while len(selected)<5:
            value=port()
            if value not in selected:selected.append(value)
        rig=FdtRig.__new__(FdtRig)
        try:
            rig.__init__(options,folder/name,selected);rig.start();before=rig.status()['lifecycle'];expected=[b'',b'']
            if name=='segments':
                for event in range(1,4):
                    rig.connect_senders();segment=[frame(event),frame(event,True)];expected=[a+b for a,b in zip(expected,segment)];rig.eof(segment)
                    wire_bytes += sum(len(value) for value in segment)
                    def drained():
                        value=rig.query('receivers')['observation']
                        statuses = [rig.receiver_status(index) for index in range(2)]
                        complete = all(status['connections']==event and status['sender_eof_confirmed'] and not status['connected'] for status in statuses)
                        return value if complete and not value['busy'] and all(row['state']=='running' for row in value['receivers']) else None
                    wait_for(drained)
                    require(rig.status()['lifecycle']['session_id']==before['session_id'],'FDT HTTP replaced session on EOF')
                rig.submit(rig.make_request('finish','finish'));finished=rig.completed('finish')
                for i,receiver in enumerate(finished['result']['receivers']):
                    output=Path(receiver['output_directory']);seal=json.loads((output/'.getdaq-receiver-seal.json').read_text())
                    names=sorted([item['name'] for item in seal['files']],key=lambda name:int(name.rsplit('_',1)[1][:-5]))
                    require(b''.join((output/name).read_bytes() for name in names)==expected[i],'FDT HTTP segment bytes differ')
                    require(seal['connections']==3 and receiver['pid']==before['receivers'][i]['pid'],'FDT HTTP cycle identity/EOF count differs')
            elif name=='active-timeout':
                rig.connect_senders()
                for index, connection in enumerate(rig.sockets):
                    payload = frame(index+1)
                    connection.sendall(payload);wire_bytes += len(payload)
                # A completed connect can still be in the listener backlog.
                # Verify that both receiver processes accepted and parsed data.
                wait_for(lambda: all(rig.receiver_status(index)['connected'] and rig.receiver_status(index)['frames']==1 for index in range(2)))
                # Do not send EOF until the accepted Finish has actually timed out.
                rig.submit(rig.make_request('finish','finish'));failed=rig.completed('finish','failed')
                require(failed['result']['state']=='failed' and all(process_identity(pid)==identity for pid,identity in rig.children.items()),'Finish timeout killed FDT receiver')
                require(all(not (Path(row['output_directory'])/'.getdaq-receiver-seal.json').exists() for row in before['receivers']),'Active FDT timeout fabricated seal')
                rig.eof([b'',b''])
            elif name in ['unused','mixed-unused']:
                unused_ids = ['C0','C1']
                if name=='mixed-unused':
                    source=rig.plan['sources'][0]
                    connection=socket.create_connection(('127.0.0.1',source['data_port']),timeout=2)
                    rig.sockets.append(connection)
                    payload=frame(1);connection.sendall(payload);wire_bytes += len(payload)
                    connection.shutdown(socket.SHUT_WR);connection.close();rig.sockets=[]
                    wait_for(lambda:rig.receiver_status(0)['sender_eof_confirmed'])
                    unused_ids=['C1']
                request=rig.make_request('finish','finish');request['unused_source_ids']=unused_ids
                rig.submit(request);finished=rig.completed('finish')
                for index, receiver in enumerate(finished['result']['receivers']):
                    unused='C'+str(index) in unused_ids
                    output=Path(receiver['output_directory']);seal=json.loads((output/'.getdaq-receiver-seal.json').read_text())
                    require(receiver.get('source_unused_confirmed',False)==unused and seal['sender_eof_confirmed']==(not unused),'Explicit unused source became sender EOF')
                    require(seal['connections']==(0 if unused else 1) and receiver['seal_verified'],'Unused source termination receipt differs')
                    if unused:require(not seal['files'] and seal['received_bytes']=='0','Unused receipt has raw payload')
                # The native query decoder must retain the new lifecycle field.
                require(rig.query('receivers')['observation']['lifecycle']['receivers'][1]['source_unused_confirmed'],'Native client lost explicit unused proof')
            else:
                rig.submit(rig.make_request('finish','finish'));failed=rig.completed('finish','failed')
                require(all(not row['seal_verified'] for row in failed['result']['receivers']),'No-input FDT claimed successful cycle')
            cases.append({'case':name,'passed':True})
        finally:
            rig.close()
            for path in (folder/name).rglob('*.graw'):
                deleted.append({'path':str(path.relative_to(folder)),'bytes':path.stat().st_size,'sha256':sha(path)});path.unlink()
            (folder/'payload-cleanup.json').write_text(json.dumps({'deleted':deleted,'remaining_raw':len(list(folder.rglob('*.graw')))},indent=2)+'\n')
    require(wire_bytes <= 1024**2, 'FDT HTTP fixture exceeded its aggregate payload budget')
    result={'schema':1,'cases':cases,'wire_bytes':wire_bytes,'maximum_wire_bytes':1024**2,'aggregate_limit_bytes':20*1024**3,'generated_raw_deleted':not list(folder.rglob('*.graw')),
            'program_sha256':{name:sha(build/name) for name in ['getdaq-supervisor','getdaq-supervisor-query','supervisor-control-tests']},
            'receiver_sha256':sha(args.receiver.resolve()),'runner_sha256':sha(Path(__file__)),'real_hardware':False}
    (folder/'result.json').write_text(json.dumps(result,indent=2)+'\n');print('PASS',len(cases),'actual authenticated FDT cycle cases; raw payload deleted;',folder)


if __name__=='__main__':main()
