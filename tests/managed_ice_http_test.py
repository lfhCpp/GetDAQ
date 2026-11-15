#!/usr/bin/env python3
"""Real authenticated mixed ICE/TCP data host operations; no controller-generated ICE end."""
import argparse
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import tempfile
from types import SimpleNamespace
from run_supervisor_control_tests import Rig, require, process_identity
from receiver_transport_integration import frame, port
from run_receiver_observation_tests import interrupted, sha, stop_owned_group


class IceRig(Rig):
    def start_service(self):
        if self.process is None:
            self.plan['ice_executable']=str(self.args.ice)
            self.plan['ice_executable_sha256']=sha(self.args.ice)
            for index,source in enumerate(self.plan['sources']):
                if not (self.args.mixed and index==0):source['input_protocol']='ICE'
            self.config_file.write_text(json.dumps(self.config,indent=2)+'\n')
            check=subprocess.run([str(self.args.service),'--config',str(self.config_file),'--check-config'],capture_output=True,text=True,timeout=10)
            require(check.returncode==0,'ICE offline config rejected: '+check.stdout+check.stderr)
        super().start_service()

    def remember(self,lifecycle):
        for receiver in lifecycle['receivers']:
            if receiver['pid']>0 and receiver['pid'] not in self.children:
                identity=process_identity(receiver['pid'])
                if identity:
                    source=next(s for s in self.plan['sources'] if s['id']==receiver['id'])
                    expected=self.args.ice if source.get('input_protocol','TCP')=='ICE' else self.args.receiver
                    require(identity[0]==str(expected),'Mixed fixture assigned an unrelated process')
                    self.children[receiver['pid']]=identity


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['build','native','ice','source','evidence']:parser.add_argument(name,type=Path)
    args=parser.parse_args()
    for name in ['build','native','ice','source']:setattr(args,name,getattr(args,name).resolve())
    args.evidence.mkdir(parents=True,exist_ok=True);folder=Path(tempfile.mkdtemp(prefix='managed-ice-http-',dir=args.evidence))
    signal.signal(signal.SIGINT,interrupted);signal.signal(signal.SIGTERM,interrupted)
    cases=[];deleted=[];child=None
    for name,minor in [('catalog-normal',0),('catalog-normal',1),('catalog-mixed',1),('catalog-empty',0),('missing-end',1)]:
        case=name+'-'+str(minor);selected=[]
        while len(selected)<5:
            value=port()
            if value not in selected:selected.append(value)
        options=SimpleNamespace(service=args.build/'getdaq-supervisor',fixture=args.build/'supervisor-control-tests',query=args.build/'getdaq-supervisor-query',
            receiver=args.native,ice=args.ice,file_naming='get',mixed=name=='catalog-mixed')
        rig=IceRig.__new__(IceRig)
        try:
            rig.__init__(options,folder/case,selected);require(rig.request('GET','/v1/receivers',auth=False)[0]==401,'Unauthenticated receivers exposed')
            rig.start();before=rig.status()['lifecycle'];expected=[]
            for index,source in enumerate(rig.plan['sources']):
                payload=b'' if name=='catalog-empty' else frame(1)+frame(2,True);expected.append(payload)
                if source.get('input_protocol','TCP')=='TCP':
                    with socket.create_connection(('127.0.0.1',source['data_port']),timeout=2) as connection:connection.sendall(payload);connection.shutdown(socket.SHUT_WR)
                else:
                    mode='empty' if name=='catalog-empty' else 'legacy' if name=='missing-end' and index==1 else 'complete'
                    with (rig.root/('source-'+str(index)+'.log')).open('w') as log:
                        child=subprocess.Popen([str(args.source),'127.0.0.1',str(source['data_port']),before['session_id'],mode,str(minor)],stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
                        require(child.wait(timeout=10)==0,'HTTP ICE source fixture failed')
                    stop_owned_group(child);child=None
            rig.submit(rig.make_request('finish','finish'));result=rig.completed('finish','failed' if name=='missing-end' else 'complete')
            if name=='missing-end':
                receiver=before['receivers'][1];require(process_identity(receiver['pid'])==rig.children[receiver['pid']],'Finish timeout killed/replaced ICE source')
                require(not (Path(receiver['output_directory'])/'.getdaq-ice-receiver-seal.json').exists(),'Missing ICE end produced receipt')
            else:
                for index,receiver in enumerate(result['result']['receivers']):
                    ice=rig.plan['sources'][index].get('input_protocol','TCP')=='ICE';output=Path(receiver['output_directory'])
                    require(receiver.get('source_end_confirmed',False)==ice and receiver['seal_verified'],'HTTP end proof differs')
                    seal=json.loads((output/('.getdaq-ice-receiver-seal.json' if ice else '.getdaq-receiver-seal.json')).read_text())
                    require(seal['sender_eof_confirmed']==(not ice),'ICE HTTP marker became TCP EOF')
                    require(b''.join(path.read_bytes() for path in sorted(output.glob('*.graw')))==expected[index],'HTTP raw bytes changed')
                query=rig.query('receivers')['observation']['lifecycle']
                require(query['receivers'][1]['source_end_confirmed'],'Native HTTP query dropped ICE end')
                request=rig.make_request('catalog','catalog');request.update(catalog_experiment='Test',catalog_run_number=0,catalog_session_id=before['session_id'])
                rig.submit(request);catalog=rig.completed('catalog');directory=catalog['catalog']['directory']
                checked=subprocess.run([str(args.build/'getdaq-catalog'),'--inspect-catalog',directory],capture_output=True,text=True,timeout=10)
                require(checked.returncode==0,'HTTP produced uninspectable ICE catalog: '+checked.stderr)
            cases.append({'case':case,'passed':True})
        finally:
            stop_owned_group(child);child=None;rig.close()
            for path in (folder/case).rglob('*.graw'):
                deleted.append({'path':str(path.relative_to(folder)),'bytes':path.stat().st_size,'sha256':sha(path)});path.unlink()
            # Evidence must not freeze live secrets. This is a fixture-only credential.
            if getattr(rig,'token_file',None) and rig.token_file.exists():rig.token_file.unlink()
            (folder/'payload-cleanup.json').write_text(json.dumps({'deleted':deleted,'remaining_raw':len(list(folder.rglob('*.graw')))},indent=2)+'\n')
    maximum=4*1024**2;require(maximum<20*1024**3,'Aggregate budget')
    (folder/'result.json').write_text(json.dumps({'schema':1,'cases':cases,'maximum_aggregate_test_bytes':maximum,'aggregate_limit_bytes':20*1024**3,'raw_deleted':not list(folder.rglob('*.graw')),'real_hardware':False,
        'program_sha256':{name:sha(args.build/name) for name in ['getdaq-supervisor','getdaq-supervisor-query','getdaq-catalog']},'ice_sha256':sha(args.ice),'native_sha256':sha(args.native),'source_sha256':sha(args.source),'runner_sha256':sha(Path(__file__))},indent=2)+'\n')
    print('PASS',len(cases),'authenticated ICE/mixed HTTP and catalog cases; raw deleted;',folder)


if __name__=='__main__':main()
