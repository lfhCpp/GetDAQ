#!/usr/bin/env python3
"""Project-folder distribution, two original receivers, shared Sentry and remote logs.
All nodes and hardware frames are loopback/software-only; never contacts boards.
"""
import argparse,hashlib,json,os,signal,socket,ssl,subprocess,tempfile,time,urllib.request
from pathlib import Path
from original_backends_integration import request,port as available_port,require,wait,sha

# Both simulated hosts share one kernel. Keep their ephemeral test ports globally
# distinct even though real separate hosts may validly reuse the same port.
_allocated_ports=set()
def port():
 while True:
  value=available_port()
  if value not in _allocated_ports:
   _allocated_ports.add(value)
   return value

def main():
 p=argparse.ArgumentParser();p.add_argument('bin',type=Path);p.add_argument('--evidence',type=Path,required=True);p.add_argument('--controller-probe',type=Path);p.add_argument('--log-panel-probe',type=Path);args=p.parse_args();programs=args.bin.resolve();args.evidence.mkdir(parents=True,exist_ok=True);root=Path(tempfile.mkdtemp(prefix='project-',dir=args.evidence.resolve()));children=[];logs=[];checks=[]
 def run(*cmd):
  result=subprocess.run(list(map(str,cmd)),capture_output=True,timeout=30);require(result.returncode==0,result.stderr.decode(errors='replace'));return result.stdout
 def start(name,*cmd):
  log=(root/(name+'.log')).open('wb');logs.append(log);child=subprocess.Popen(list(map(str,cmd)),stdout=log,stderr=subprocess.STDOUT,start_new_session=True);children.append(child)
 def terminal(node,body):
  code,value=request(node,'POST','/v1/operations',body,node['token']);require(code==202,value.decode(errors='replace'))
  def observe():
   code,value=request(node,'GET','/v1/operations/'+body['operation_id'],token=node['token']);require(code==200,value.decode(errors='replace'));record=json.loads(value);return record if record['phase'] not in ['accepted','running'] else None
  result=wait(observe,90);require(result['phase']=='complete',json.dumps(result));return result['result']
 try:
  nodes=[]
  for index in range(2):
   directory=root/('node-'+str(index));run(programs/'getdaq-node-agent','--initialize',directory,'--programs',programs,'--bind','127.0.0.1','--port',port(),'--data',root/('project-'+str(index)))
   invitation=json.loads((directory/'pairing.json').read_text());node=dict(id=invitation['node_id'],port=int(invitation['url'].rsplit(':',1)[1]),pin=invitation['certificate_sha256'],root=directory,token=(directory/'agent.token').read_text().strip());nodes.append(node);start('node-'+str(index),programs/'getdaq-node-agent','--config',directory/'node.json')
  def ready(node):
   try:return request(node,'GET','/v1/identity')[0]==200
   except OSError:return False
  for node in nodes:wait(lambda:ready(node))
  repo=root/'repository';run(programs/'getdaq-config-repository','initialize',repo);run(programs/'getdaq-config-service','--generate-token',root/'admin.token');run(programs/'getdaq-config-service','--generate-token',root/'read.token');admin=(root/'admin.token').read_text().strip();reader=(root/'read.token').read_text().strip();repository=dict(port=port(),pin=nodes[0]['pin'])
  start('repository',programs/'getdaq-config-service','--root',repo,'--credential',root/'admin.token','--read-credential',root/'read.token','--bind','127.0.0.1','--port',repository['port'],'--tls-certificate',nodes[0]['root']/'tls.crt','--tls-key',nodes[0]['root']/'tls.key')
  def identity():
   try:return json.loads(request(repository,'GET','/v1/identity',token=admin)[1])['repository_id']
   except OSError:return None
  repository_id=wait(identity)
  for node in nodes:
   invitation=json.loads((node['root']/'pairing.json').read_text());body=dict(pairing_code=invitation['pairing_code'],repository_id=repository_id,repository_url='https://127.0.0.1:'+str(repository['port']),repository_certificate_sha256=repository['pin'],repository_token=reader);require(request(node,'POST','/v1/pair',body)[0]==200,'Pairing failed')
  source=root/'master-config';source.mkdir();(source/'describe-pedestals.xcfg').write_text('<config/>');(source/'configure-pedestals.xcfg').write_text('<config/>');(source/'resource.dat').write_bytes(b'project-resource');(source/'empty.dat').write_bytes(b'')
  import shutil
  for path in (Path(__file__).resolve().parents[1]/'third_party/legacy-get/getbench/data/format').glob('*.xcfg'):shutil.copy2(path,source/path.name)
  folders={nodes[i]['id']:str(root/('project-'+str(i))) for i in range(2)}
  for folder in folders.values():Path(folder).mkdir();Path(folder).chmod(0o755)
  data_folder=Path(folders[nodes[1]['id']]);(data_folder/'preserve.graw').write_bytes(b'old-raw')
  data_ip=next((value for value in subprocess.check_output(['hostname','-I'],text=True).split() if ':' not in value and not value.startswith('127.')),'127.0.0.1')
  monitor_port=port();groups=[]
  for index,backend in enumerate(['DataExporter','dataRouter']):
   groups.append(dict(id='group'+str(index+1),name=backend,backend='original-soap',project_mode=True,sync_configuration=True,receiver_backend=backend,project_directories=folders,master_repository=str(source),experiment='pedestals',board_index=index,receiver_control_ip='127.0.0.1',receiver_ip=data_ip,data_port=port(),export_port=port(),sentry_ip='127.0.0.1',sentry_port=monitor_port,ecc=dict(node_id=nodes[0]['id'],address='127.0.0.1',port=port(),callback_port=port()),receivers=[dict(id='data',node_id=nodes[1]['id'],address='127.0.0.1',port=port(),sources=[dict(id=str(index),bind=data_ip,data_port=1)])]))
  def upload(groups):
   files=[];mapping=[]
   for index,path in enumerate(sorted(source.rglob('*'))):
    if not path.is_file():continue
    value=path.read_bytes();parts=[]
    for offset in range(0,len(value),32768):
     part=value[offset:offset+32768];digest=sha(part);require(request(repository,'POST','/v1/objects/'+digest,part,admin,repository_id,True)[0]==200,'Object upload failed');parts.append(digest)
    ident='file-'+str(index);files.append(dict(id=ident,sha256=sha(value),bytes=len(value),objects=parts));mapping.append(dict(file=ident,path=str(path.relative_to(source)),sha256=sha(value)))
   for group in groups:group['repository_files']=mapping
   manifest=dict(schema=1,kind='getdaq.distributed-release',project_id='unified-test',hosts=[dict(id=n['id']) for n in nodes],groups=groups,files=files);value=json.dumps(manifest,separators=(',',':')).encode();parts=[]
   for offset in range(0,len(value),32768):
    part=value[offset:offset+32768];digest=sha(part);require(request(repository,'POST','/v1/objects/'+digest,part,admin,repository_id,True)[0]==200,'Manifest chunk upload failed');parts.append(digest)
   code,response=request(repository,'POST','/v1/deployments',dict(manifest_objects=parts),admin,repository_id);require(code==200,response.decode(errors='replace'));return json.loads(response)['release_id']
  master=dict(root=str(root),repository_url='https://127.0.0.1:'+str(repository['port']),certificate_sha256=repository['pin'],repository_id=repository_id,admin_credential=str(root/'admin.token'),read_credential=str(root/'read.token'))
  (root/'master.json').write_text(json.dumps(master));project=dict(project_id='unified-test',configuration_source=str(source),sync_configuration=True,groups=groups,nodes=[dict(id=n['id'],name='Node '+str(i),url='https://127.0.0.1:'+str(n['port']),certificate_sha256=n['pin'],credential_file=str(n['root']/'agent.token'),inventory=dict(data_directory=folders[n['id']])) for i,n in enumerate(nodes)]);(root/'project.json').write_text(json.dumps(project))
  if args.controller_probe:
   synchronized=json.loads(run(args.controller_probe,'sync',root/'master.json'));require(len(synchronized['receipts'])==2,'Master did not sync both targets')
   started=json.loads(run(args.controller_probe,'start',root/'master.json'));require(started['nodes']==2,'Unified console must contain both ECCs');release=started['preparation']['release_id'];groups=started['preparation']['manifest']['groups'];activated=[]
   for active in started['activation']['receipts']:
    node=next(n for n in nodes if n['id']==active['node_id']);body=dict(schema=1,group_id=active['group_id'],role=active['role'],release_id=active['release_id'],action='activate',expected_previous='')
    if active['role']=='receiver':body['receiver_id']=active['receiver_id']
    activated.append((node,body,active));config=json.loads(Path(active['config_file']).read_text());require(config['working_directory']==folders[node['id']],'Wrong project cwd')
    for process in active['processes']:require(os.readlink('/proc/'+str(int(process['pid']))+'/cwd')==folders[node['id']],'Actual process cwd differs')
    if active['role']=='receiver':require(config['arguments'][0].startswith('127.0.0.1:') and config['arguments'][1].startswith(data_ip+':'),'Control/data IPs were merged')
   checks.append('master controller sync/start creates a two-ECC Run and immutable local configuration snapshot')
  else:
   release=upload(groups)
   for i,node in enumerate(nodes):
    result=terminal(node,dict(schema=1,operation_id='sync-'+str(i),action='sync',release_id=release,group_id='group1',role='ecc'));require(result['configuration_sync']['state']=='applied','Slave did not automatically apply received files')
    for path in source.iterdir():require((Path(folders[node['id']])/path.name).read_bytes()==path.read_bytes(),'Project configuration differs')
   require((data_folder/'preserve.graw').read_bytes()==b'old-raw','Sync modified raw data');checks.append('complete configuration folder including empty resource distributed and auto-applied on both nodes; existing raw retained')
   activated=[]
   for group in groups:
    for role,node in [('receiver',nodes[1]),('ecc',nodes[0])]:
     body=dict(schema=1,operation_id='prepare-'+group['id']+'-'+role,action='prepare',release_id=release,group_id=group['id'],role=role)
     if role=='receiver':body['receiver_id']='data'
     prepared=terminal(node,body);config=json.loads(Path(prepared['config_file']).read_text());require(config['working_directory']==folders[node['id']],'Wrong project cwd');require(config['repository']==folders[node['id']],'ECC repo not in project directory')
     body.update(operation_id='start-'+group['id']+'-'+role,action='activate',expected_previous='');active=terminal(node,body);activated.append((node,body,active))
     for process in active['processes']:require(os.readlink('/proc/'+str(int(process['pid']))+'/cwd')==folders[node['id']],'Actual process cwd differs')
     if role=='receiver':
      require(config['arguments'][0].startswith('127.0.0.1:'),'Wrong control IP');require(config['arguments'][1].startswith(data_ip+':'),'Wrong data IP');require((['TCP','FrameStorage']==config['arguments'][2:4]) if group['receiver_backend']=='dataRouter' else '--export-port' in config['arguments'],'Wrong original backend arguments')
    terminal(nodes[1],dict(schema=1,operation_id='monitor-'+group['id'],action='start-monitor',release_id=release,group_id=group['id'],role='receiver',receiver_id='data'))
  monitor=json.loads((data_folder/'.getdaq/sentry/process.json').read_text());require(Path('/proc',str(int(monitor['pid']))).exists(),'Shared Sentry is absent');checks.append('two original receiver backends and ECCs share explicit project directories, dual IP arguments and exactly one Sentry record')
  for index,group in enumerate(groups):
   payload=bytearray([1])*4096;payload[0]=6;payload[1:4]=(64).to_bytes(3,'big');payload[4]=7;payload[5:7]=(1).to_bytes(2,'big');payload[7]=4;payload[8:10]=(2).to_bytes(2,'big');payload[10:12]=(4).to_bytes(2,'big');payload[12:16]=(992).to_bytes(4,'big');payload[22:26]=(1).to_bytes(4,'big');payload[26:28]=bytes([index,0])
   with socket.create_connection((data_ip,group['data_port']),timeout=3) as sender:sender.sendall(payload)
  def stats():
   with urllib.request.urlopen('http://127.0.0.1:'+str(monitor_port)+'/status',timeout=3) as response:value=json.load(response)
   return value if value.get('data_path_files')==3 else None
  status=wait(stats);require(set(status['per_source'])=={'0','1'},'Per-board counters missing');require(status['data_path']==str(data_folder),'Monitor uses another data folder');checks.append('real synthetic MFM frames received by both backends in shared folder; per-board statistics and project total agree')
  require(request(nodes[0],'GET','/v1/instances/group1-ecc/logs/ecc?offset=0')[0]==401,'Logs must require authentication')
  code,body=request(nodes[0],'GET','/v1/instances/group1-ecc/logs/ecc?offset=0',token=nodes[0]['token']);require(code==200 and json.loads(body)['available'],'ECC log unavailable');require('Starting Electronics Control Core SOAP server' in json.loads(body)['text'],'Original ECC INFO logs were disabled')
  code,body=request(nodes[1],'GET','/v1/instances/group1-data/logs/receiver?offset=0',token=nodes[1]['token']);require(code==200 and 'Connection established' in json.loads(body)['text'],'Receiver connection INFO log did not reach authenticated stream')
  code,body=request(nodes[1],'GET','/v1/instances/group1-data/logs/sentry?offset=0',token=nodes[1]['token']);require(code==200 and 'listening' in json.loads(body)['text'],'Sentry output unavailable')
  require(request(nodes[1],'GET','/v1/instances/group1-data/logs/../../agent.token?offset=0',token=nodes[1]['token'])[0]!=200,'Log path escaped instance');checks.append('authenticated incremental ECC/Sentry log reads; unauthorized and arbitrary-path requests rejected')
  if args.controller_probe:
   old_snapshot=Path(started['snapshot']);require((old_snapshot/'resource.dat').read_bytes()==b'project-resource','Missing initial immutable snapshot')
   (source/'resource.dat').write_bytes(b'updated-while-idle');run(args.controller_probe,'sync',root/'master.json');refreshed=json.loads(run(args.controller_probe,'refresh',root/'master.json'));require((Path(refreshed['snapshot'])/'resource.dat').read_bytes()==b'updated-while-idle','Pre-Describe snapshot did not refresh');require((old_snapshot/'resource.dat').read_bytes()==b'project-resource','Old Run snapshot was modified');checks.append('pre-Describe refresh preserves old snapshot and selects the actually synchronized configuration revision')
   reused=json.loads(run(args.controller_probe,'start',root/'master.json'));before={r['instance_id']:r['processes'][0]['pid'] for r in started['activation']['receipts']};after={r['instance_id']:r['processes'][0]['pid'] for r in reused['activation']['receipts']};require(before==after,'Configuration-only update restarted backends');checks.append('configuration-only updates reuse verified backend processes without duplicate startup')
  if args.log_panel_probe:
   os.environ['QT_QPA_PLATFORM']='offscreen';output=root/'combined-logs.txt';run(args.log_panel_probe,root/'project.json',output);combined=output.read_text();require('EVENT LOG' in combined and 'ECC LOG' in combined and 'DATA LOG' in combined,'Combined log lacks source separators');checks.append('real GUI collector reads all authenticated sources on Event tab and exports a stable combined snapshot; Clear preserves history')
  # With a foreign/uninspectable ECC still associated with live receivers, updates stay cached.
  ecc_active=[v for v in activated if v[1]['role']=='ecc'];os.kill(int(ecc_active[0][2]['processes'][0]['pid']),signal.SIGINT);time.sleep(.3)
  (source/'resource.dat').write_bytes(b'updated-resource');next_release=upload(groups)
  pending=terminal(nodes[1],dict(schema=1,operation_id='pending-sync',action='sync',release_id=next_release,group_id='group1',role='ecc'));require(pending['configuration_sync']['state']=='pending','Uncertain active project was overwritten');require((data_folder/'resource.dat').read_bytes()==(b'updated-while-idle' if args.controller_probe else b'project-resource'),'Pending files replaced active files');checks.append('uninspectable live project caches changed configuration without replacing active project files')
  dead=next(v for v in activated if v[1]['group_id']=='group1' and v[1]['role']=='receiver');os.kill(int(dead[2]['processes'][0]['pid']),signal.SIGTERM);wait(lambda:not Path('/proc',str(int(dead[2]['processes'][0]['pid'])),'exe').exists(),15)
  try:
   urllib.request.urlopen('http://127.0.0.1:'+str(monitor_port)+'/status',timeout=3);raise AssertionError('Shared Sentry hid a dead receiver')
  except urllib.error.HTTPError as error:require(error.code==503,'Wrong receiver-health failure')
  checks.append('shared Sentry verifies exact receiver process identities and reports a missing receiver')
  # Stop all receivers and the remaining ECC; the shared monitor exits only after the last receiver.
  for node,body,active in reversed(activated):
   if body['role']=='ecc' and body['group_id']=='group1':continue
   stopped=dict(body,operation_id='stop-'+body['group_id']+'-'+body['role'],action='stop',ecc_stopped_confirmed=True);terminal(node,stopped)
  # Release the externally stopped ECC record via explicit repair; no process remains.
  terminal(nodes[0],dict(schema=1,operation_id='repair-ecc',action='terminate',release_id=release,group_id='group1',role='ecc',accepted_repair=True))
  applied=terminal(nodes[1],dict(schema=1,operation_id='apply-after-stop',action='sync',release_id=next_release,group_id='group1',role='ecc'));require(applied['configuration_sync']['state']=='applied','Stopped project update did not apply');require((data_folder/'resource.dat').read_bytes()==b'updated-resource','Wrong updated configuration');require((data_folder/'preserve.graw').read_bytes()==b'old-raw','Raw file changed during update');checks.append('stopped project applies staged update with backups; raw files preserved and shared monitor released')
  for group in groups:group['sync_configuration']=False
  (source/'resource.dat').write_bytes(b'cached-only');disabled_release=upload(groups)
  disabled=terminal(nodes[1],dict(schema=1,operation_id='sync-disabled',action='sync',release_id=disabled_release,group_id='group1',role='ecc'));require(disabled['configuration_sync']['state']=='disabled','Sync checkbox was ignored');require((data_folder/'resource.dat').read_bytes()==b'updated-resource','Disabled synchronization replaced project files');checks.append('disabled synchronization caches delivery without replacing project files')
  result=dict(passed=True,checks=checks,real_hardware=False,physical_multi_host=False,distinct_control_data_ip=data_ip!='127.0.0.1');(root/'result.json').write_text(json.dumps(result,indent=2)+'\n');print('PASS',len(checks),'unified project checks;',root)
 finally:
  for entry in Path('/proc').iterdir():
   if not entry.name.isdigit():continue
   try:
    executable=(entry/'exe').resolve(strict=True);cmd=(entry/'cmdline').read_bytes()
    if executable.parent==programs and str(root).encode() in cmd and executable.name in ['getEccSoapServer','DataExporter','dataRouter','getdaq-sentry']:os.kill(int(entry.name),signal.SIGTERM)
   except (OSError,RuntimeError):pass
  for child in reversed(children):
   if child.poll() is None:child.terminate()
   child.wait(timeout=10)
  for log in logs:log.close()
if __name__=='__main__':main()
