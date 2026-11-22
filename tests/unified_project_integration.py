#!/usr/bin/env python3
"""Project-folder distribution, two original receivers, shared Sentry and remote logs.
All nodes and hardware frames are loopback/software-only; never contacts boards.
"""
import argparse,hashlib,json,os,signal,socket,ssl,subprocess,tempfile,time,urllib.request,shutil,random
from pathlib import Path
from original_backends_integration import request,port as available_port,require,wait,sha

# Both simulated hosts share one kernel. Keep their ephemeral test ports globally
# distinct even though real separate hosts may validly reuse the same port.
_allocated_ports=set()
def port():
 while True:
  # Keep planned listeners outside Linux's usual outgoing ephemeral range.
  # TLS calls during preparation must not claim a reserved future listener port.
  value=random.randrange(12000,30000)
  try:
   with socket.socket() as probe:probe.bind(('127.0.0.1',value))
  except OSError:continue
  if value not in _allocated_ports:
   _allocated_ports.add(value)
   return value

def main():
 p=argparse.ArgumentParser();p.add_argument('bin',type=Path);p.add_argument('--evidence',type=Path,required=True);p.add_argument('--controller-probe',type=Path);p.add_argument('--log-panel-probe',type=Path);p.add_argument('--slow-sentry-startup',action='store_true');args=p.parse_args();programs=args.bin.resolve();args.evidence.mkdir(parents=True,exist_ok=True);root=Path(tempfile.mkdtemp(prefix='project-',dir=args.evidence.resolve()));children=[];logs=[];checks=[]
 def run(*cmd):
  result=subprocess.run(list(map(str,cmd)),capture_output=True,timeout=30);require(result.returncode==0,result.stderr.decode(errors='replace'));return result.stdout
 child_env=os.environ.copy()
 if args.slow_sentry_startup:
  library=root/"sentry-startup-delay.so";subprocess.run(["cc","-shared","-fPIC",str(Path(__file__).with_name("sentry_startup_delay.c")),"-o",str(library)],check=True);child_env["LD_PRELOAD"]=str(library)
 def start(name,*cmd):
  log=(root/(name+'.log')).open('wb');logs.append(log);child=subprocess.Popen(list(map(str,cmd)),stdout=log,stderr=subprocess.STDOUT,start_new_session=True,env=child_env);children.append(child)
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
  (root/'nodes').mkdir(mode=0o700)
  for n in nodes:
   credential=root/'nodes'/(n['id']+'.token');credential.write_text(n['token']+'\n');credential.chmod(0o600)
  (root/'master.json').write_text(json.dumps(master));project=dict(project_id='unified-test',configuration_source=str(source),sync_configuration=True,groups=groups,nodes=[dict(id=n['id'],name='Node '+str(i),url='https://127.0.0.1:'+str(n['port']),certificate_sha256=n['pin'],credential_file=str(root/'nodes'/(n['id']+'.token')),inventory=dict(data_directory=folders[n['id']])) for i,n in enumerate(nodes)]);(root/'project.json').write_text(json.dumps(project))
  if args.controller_probe:
   synchronized=json.loads(run(args.controller_probe,'sync',root/'master.json'));require(len(synchronized['receipts'])==2,'Master did not sync both targets')
   # Reproduce a partial startup: receiver succeeds, ECC port is occupied.
   # Re-preparing the same release must inspect terminal results and create a
   # fresh operation rather than reuse the old operation ID with a new recipe.
   blocker=socket.socket();blocker.bind(('127.0.0.1',groups[0]['ecc']['port']));blocker.listen()
   def attempt_start():
    return subprocess.run([str(args.controller_probe),'start-selected',str(root/'master.json'),'group1'],capture_output=True,timeout=90)
   try:
    failed=attempt_start();require(failed.returncode!=0 and b'Port is already in use' in failed.stderr,'Fixture did not fail before starting ECC')
    pending_file=next(p for p in (root/'pending-activations').glob('*.json') if json.loads(p.read_text()).get('phase')=='pending')
    first_context=json.loads(pending_file.read_text());first_job=Path(first_context['directory']);require('preparation_binding' in first_context,'New context lacks prepared recipe binding')
    receiver_before=json.loads((nodes[1]['root']/'instances/group1-data.json').read_text())['processes'][0]['pid']
    failed_again=attempt_start();require(failed_again.returncode!=0 and b'Port is already in use' in failed_again.stderr,'New preparation was blocked by the old startup intent')
    context=json.loads(pending_file.read_text());require(context['directory']!=str(first_job),'Changed preparation reused an operation ID');require((first_job/'activation-context.json').exists(),'Prior context audit was lost')
    require(json.loads((nodes[1]['root']/'instances/group1-data.json').read_text())['processes'][0]['pid']==receiver_before,'Partial retry duplicated the running receiver')
    checks.append('new prepared recipe supersedes verified terminal startup tasks; successful partial receiver is reused')
   finally:blocker.close()
   # Old installed versions did not save preparation_binding. Recover them too,
   # but never bypass unknown/running results or a mismatching remote journal.
   context=json.loads(pending_file.read_text());context.pop('preparation_binding',None);pending_file.write_text(json.dumps(context));old_job=Path(context['directory'])
   old_records=[json.loads(p.read_text()) for p in old_job.glob('*.json') if json.loads(p.read_text()).get('request')]
   failure=next(r for r in old_records if r['request']['role']=='ecc');op=nodes[0]['root']/'operations'/(failure['request']['operation_id']+'.json');original=op.read_bytes()
   def activation_count():
    return sum(json.loads(p.read_text()).get('action')=='activate' for n in nodes for p in (n['root']/'operations').glob('*.json'))
   for phase in ['unknown','running','accepted']:
    record=json.loads(original);record['phase']=phase;op.write_text(json.dumps(record));before=activation_count();rejected=attempt_start()
    require(rejected.returncode!=0 and (b'unknown result' in rejected.stderr if phase=='unknown' else b'still pending' in rejected.stderr),'Unresolved original startup was bypassed')
    require(activation_count()==before and json.loads(pending_file.read_text())['directory']==str(old_job),'Unresolved startup created another activation');op.write_bytes(original)
   record=json.loads(original);record['request_sha256']='0'*64;op.write_text(json.dumps(record));before=activation_count();rejected=attempt_start();require(rejected.returncode!=0 and b'differs from its saved request' in rejected.stderr,'Changed remote journal was accepted');require(activation_count()==before,'Journal mismatch started another operation');op.write_bytes(original)
   checks.append('legacy startup recovery blocks unknown, running, accepted and mismatching remote results without replay')
   single=json.loads(run(args.controller_probe,'start-selected',root/'master.json','group1'));require(single['nodes']==1,'One selected ECC must not require unrelated draft groups');require(single['preparation']['prepared_groups']==['group1'],'Unselected group was prepared');require(all(r['group_id']=='group1' for r in single['activation']['receipts']),'Unselected group was activated');checks.append('one selected ECC prepares, starts and binds without activating unrelated draft groups')
   require(json.loads(pending_file.read_text())['directory']!=str(old_job),'Legacy pending task was reused after recipe changed')
   for old in old_records:
    file=next(p for p in old_job.glob('*.json') if json.loads(p.read_text()).get('request',{}).get('operation_id')==old['request']['operation_id']);saved=json.loads(file.read_text());require(saved['request']==old['request'] and saved['request_sha256']==old['request_sha256'],'Recovery rewrote original operation identity')
   require(json.loads((nodes[1]['root']/'instances/group1-data.json').read_text())['processes'][0]['pid']==receiver_before,'Legacy recovery replaced a valid receiver');require((data_folder/'preserve.graw').read_bytes()==b'old-raw','Recovery altered existing raw data');checks.append('legacy failed startup recovers after re-prepare with old request hashes, partial processes and raw files preserved')

   started=json.loads(run(args.controller_probe,'start-selected',root/'master.json','group1','group2'));require(started['nodes']==2,'Unified console must contain both ECCs');release=started['preparation']['release_id'];groups=started['preparation']['manifest']['groups'];activated=[]
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

   for node in nodes:node['inventory']=json.loads(request(node,'GET','/v1/inventory',token=node['token'])[1])
   # Write only public node fields; keep fixture Path objects out of JSON.
   project['nodes']=[dict(id=n['id'],name='Node '+str(i),url='https://127.0.0.1:'+str(n['port']),certificate_sha256=n['pin'],credential_file=str(root/'nodes'/(n['id']+'.token')),inventory=n['inventory']) for i,n in enumerate(nodes)]
   (root/'project.json').write_text(json.dumps(project))
   os.environ['QT_QPA_PLATFORM']='offscreen';output=root/'combined-logs.txt';run(args.log_panel_probe,root/'project.json',output);combined=output.read_text();require('EVENT LOG' in combined and 'ECC LOG' in combined and 'DATA LOG' in combined,'Combined log lacks source separators');checks.append('real GUI collector skips previous-session tails, collects new authenticated output on Event tab and exports a stable snapshot; Clear preserves this session history')
  if args.controller_probe:
   plan=json.loads(run(args.controller_probe,'exit-plan',root/'master.json'));require(not plan['errors'] and len(plan['targets'])==4,'Exit inspection did not find both managed ECC/data pairs');checks.append('exit inspection finds authenticated managed ECC and receiver instances across both hosts')
   stopped_plan=json.loads(run(args.controller_probe,'exit-stop',root/'master.json','group2'));require(len(stopped_plan['targets'])==2,'Exit stop selected unrelated groups')
   inventory=json.loads(request(nodes[0],'GET','/v1/inventory',token=nodes[0]['token'])[1]);require(any(i['instance_id']=='group1-ecc' and i['actual_pids'] for i in inventory['instances']),'Exit stop touched another ECC')
   for node in nodes:
    inventory=json.loads(request(node,'GET','/v1/inventory',token=node['token'])[1]);require(all(not i.get('actual_pids') for i in inventory['instances'] if i.get('group_id')=='group2'),'Exit stop left a selected backend running')
   require((data_folder/'preserve.graw').read_bytes()==b'old-raw','Exit stop changed existing raw files');checks.append('exit stops selected ECC before receiver, preserves the other group/raw data and updates managed receipts')
   restarted=json.loads(run(args.controller_probe,'start-selected',root/'master.json','group2'))
   replacement={r['instance_id']:r for r in restarted['activation']['receipts']}
   activated=[(node,dict(body,release_id=replacement.get(active['instance_id'],active)['release_id']),replacement.get(active['instance_id'],active)) for node,body,active in activated]
   checks.append('normally stopped exit services restart without clearing node pairing or deployment cache')
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
  # Simulate a software update while keeping the original instance audit.
  # No boards exist in this fixture; its explicit inspection confirmation is real.
  variant=root/'runtime-upgrade/bin';variant.mkdir(parents=True)
  for name in ['getEccSoapServer','DataExporter','dataRouter','getdaq-sentry','getdaq-node-agent']:shutil.copy2(programs/name,variant/name)
  children[0].terminate();children[0].wait(timeout=10)
  cfg=nodes[0]['root']/'node.json';settings=json.loads(cfg.read_text());settings['program_directory']=str(variant);cfg.write_text(json.dumps(settings));cfg.chmod(0o600)
  start('node-0-upgraded',variant/'getdaq-node-agent','--config',cfg);wait(lambda:ready(nodes[0]))
  released=terminal(nodes[0],dict(schema=1,operation_id='inspect-old-installation',action='release-terminated',release_id=release,group_id='group1',role='ecc',accepted_repair=True));require(released['phase']=='released-after-inspection','Old runtime could not be inspected after manager update');checks.append('inspected terminated original ECC can be released after agent runtime-directory change; prior executable hash and process absence remain verified')
  legacy_pointer=nodes[0]['root']/'deployments'/next_release/'group1-ecc/prepared.json';legacy_pointer.parent.mkdir(parents=True,exist_ok=True);legacy_pointer.parent.chmod(0o700);legacy_pointer.write_bytes(b'legacy frozen audit pointer\n');legacy_pointer.chmod(0o400)
  prepare=dict(schema=1,operation_id='same-release-prepare-a',action='prepare',release_id=next_release,group_id='group1',role='ecc')
  first=terminal(nodes[0],prepare);first_bytes=Path(first['config_file']).read_bytes();second=terminal(nodes[0],dict(prepare,operation_id='same-release-prepare-b'))
  require(first['config_file']!=second['config_file'] and Path(first['config_file']).read_bytes()==first_bytes,'Repeated preparation rewrote an immutable runtime artifact');require(legacy_pointer.read_bytes()==b'legacy frozen audit pointer\n' and legacy_pointer.stat().st_mode&0o777==0o400,'Legacy read-only pointer was changed');require((legacy_pointer.parent/'current-preparation.json').exists(),'Missing mutable preparation selector');checks.append('same experiment release prepares repeatedly after update with separate immutable runtime recipes and unchanged prior audit')
  stale=dict(schema=1,operation_id='stale-runtime-recipe',action='activate',release_id=next_release,group_id='group1',role='ecc',expected_previous=release,preparation_id=first['preparation_id'])
  code,body=request(nodes[0],'POST','/v1/operations',stale,nodes[0]['token']);require(code==202,'Stale recipe request not journaled')
  def rejected():
   code,body=request(nodes[0],'GET','/v1/operations/stale-runtime-recipe',token=nodes[0]['token']);value=json.loads(body);return value if value.get('phase') not in ['accepted','running'] else None
  rejection=wait(rejected);require(rejection['phase']=='failed' and 'recipe changed' in rejection.get('message',''),'Stale prepared recipe was activated')
  active=terminal(nodes[0],dict(stale,operation_id='restart-new-runtime',preparation_id=second['preparation_id']));require(active['program'].startswith(str(variant)),'Restart used old installation')
  terminal(nodes[0],dict(schema=1,operation_id='stop-restarted-ecc',action='stop',release_id=next_release,group_id='group1',role='ecc'))
  third=terminal(nodes[0],dict(prepare,operation_id='same-release-prepare-c'));terminal(nodes[0],dict(stale,operation_id='restart-same-release-again',expected_previous=next_release,preparation_id=third['preparation_id']));terminal(nodes[0],dict(schema=1,operation_id='stop-again',action='stop',release_id=next_release,group_id='group1',role='ecc'));checks.append('stale activation recipe is rejected; original ECC starts, normally stops and restarts with the same release in the new runtime')
  for group in groups:group['sync_configuration']=False
  (source/'resource.dat').write_bytes(b'cached-only');disabled_release=upload(groups)
  disabled=terminal(nodes[1],dict(schema=1,operation_id='sync-disabled',action='sync',release_id=disabled_release,group_id='group1',role='ecc'));require(disabled['configuration_sync']['state']=='disabled','Sync checkbox was ignored');require((data_folder/'resource.dat').read_bytes()==b'updated-resource','Disabled synchronization replaced project files');checks.append('disabled synchronization caches delivery without replacing project files')
  # Files can be synchronized in Ready/Running; no hardware transition is sent.
  from http.server import BaseHTTPRequestHandler,HTTPServer
  import threading
  observed_state=[4];queries=[]
  class EccObservation(BaseHTTPRequestHandler):
   def do_POST(self):
    queries.append(self.rfile.read(int(self.headers.get('Content-Length','0'))));body=('<Envelope><State>'+str(observed_state[0])+'</State><Transition>0</Transition><ErrorCode>0</ErrorCode></Envelope>').encode();self.send_response(200);self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
   def log_message(self,*args):pass
  observation=HTTPServer(('127.0.0.1',0),EccObservation);observing=threading.Thread(target=observation.serve_forever,daemon=True);observing.start()
  try:
   groups[0]['ecc']['port']=observation.server_port;groups[0]['sync_configuration']=True
   for state in [4,5]:
    observed_state[0]=state;(source/'resource.dat').write_bytes(('files-in-state-'+str(state)).encode());live_release=upload(groups);applied=terminal(nodes[0],dict(schema=1,operation_id='sync-state-'+str(state),action='sync',release_id=live_release,group_id='group1',role='ecc',configuration_warning_accepted=True));require(applied['configuration_sync']['state']=='applied','Non-Idle state blocked configuration sync')
   require(all(b'GetState' in query for query in queries),'File synchronization sent a hardware command');checks.append('Ready and Running accept acknowledged file synchronization without Idle or hardware transitions')
  finally:observation.shutdown();observation.server_close()
  # Explicit re-pair revokes the old credential while retaining all instance/raw records.
  before_files={p.name:p.read_bytes() for p in (nodes[0]['root']/'instances').glob('*.json')};old_token=nodes[0]['token'];run(programs/'getdaq-node-agent','--config',nodes[0]['root']/'node.json','--request-repair');wait(lambda:(nodes[0]['root']/'pairing.json').exists() and not (nodes[0]['root']/'re-pair.request').exists())
  require(request(nodes[0],'GET','/v1/inventory',token=old_token)[0]==401,'Old master credential was not revoked');invitation=json.loads((nodes[0]['root']/'pairing.json').read_text());pair=dict(pairing_code=invitation['pairing_code'],repository_id=repository_id,repository_url='https://127.0.0.1:'+str(repository['port']),repository_certificate_sha256=repository['pin'],repository_token=reader);code,body=request(nodes[0],'POST','/v1/pair',pair);require(code==200,'Re-pair failed');nodes[0]['token']=json.loads(body)['credential'];require(request(nodes[0],'GET','/v1/inventory',token=nodes[0]['token'])[0]==200,'New master cannot observe instances');require(before_files=={p.name:p.read_bytes() for p in (nodes[0]['root']/'instances').glob('*.json')},'Re-pair altered deployed instance records');require((data_folder/'preserve.graw').read_bytes()==b'old-raw','Re-pair changed raw data');checks.append('explicit slave re-pair revokes consumed credentials, enrolls with a fresh code, and retains backend/raw records')
  if args.controller_probe:
   project=json.loads((root/'project.json').read_text());project_node=next(n for n in project['nodes'] if n['id']==nodes[0]['id']);project_node['root']=str(nodes[0]['root']);(root/'project.json').write_text(json.dumps(project));saved_credential=Path(project_node['credential_file'])
   require(saved_credential.read_text().strip()!=nodes[0]['token'],'Fixture must retain a stale project token')
   run(args.controller_probe,'prepare-selected',root/'master.json','group1');require(saved_credential.read_text().strip()==nodes[0]['token'],'Local stale project credential was not refreshed');checks.append('selected preparation refreshes a stale local project credential after explicit re-pair')
   run(programs/'getdaq-node-agent','--config',nodes[0]['root']/'node.json','--request-repair');wait(lambda:(nodes[0]['root']/'pairing.json').exists() and not (nodes[0]['root']/'re-pair.request').exists());require(not (nodes[0]['root']/'master.json').exists(),'Fixture must be unpaired')
   run(args.controller_probe,'prepare-selected',root/'master.json','group1');nodes[0]['token']=saved_credential.read_text().strip();require(request(nodes[0],'GET','/v1/inventory',token=nodes[0]['token'])[0]==200,'Rejoined local credential rejected');require(json.loads((nodes[0]['root']/'master.json').read_text())['repository_id']==repository_id,'Local node joined another project');require((data_folder/'preserve.graw').read_bytes()==b'old-raw','Local rejoin changed raw files');checks.append('selected preparation rejoins an unpaired locally owned node without stopping backends or losing raw data')
   foreign=json.loads((root/'master.json').read_text());foreign['repository_id']='b'*64;(root/'other-master.json').write_text(json.dumps(foreign));before_pair=(nodes[0]['root']/'master.json').read_bytes();before_token=saved_credential.read_bytes();rejected=subprocess.run([str(args.controller_probe),'join-local',str(root/'other-master.json'),str(nodes[0]['root']/'node.json')],capture_output=True,timeout=30);require(rejected.returncode!=0 and b'already joined to another master' in rejected.stderr,'Local rejoin replaced an existing foreign pairing');require((nodes[0]['root']/'master.json').read_bytes()==before_pair and saved_credential.read_bytes()==before_token,'Rejected migration altered pairing/credentials');checks.append('local credential recovery refuses to replace a different master pairing')
  result=dict(passed=True,checks=checks,real_hardware=False,physical_multi_host=False,distinct_control_data_ip=data_ip!='127.0.0.1');(root/'result.json').write_text(json.dumps(result,indent=2)+'\n');print('PASS',len(checks),'unified project checks;',root)
 finally:
  for entry in Path('/proc').iterdir():
   if not entry.name.isdigit():continue
   try:
    executable=(entry/'exe').resolve(strict=True);cmd=(entry/'cmdline').read_bytes()
    if executable.parent in [programs,root/'runtime-upgrade/bin'] and str(root).encode() in cmd and executable.name in ['getEccSoapServer','DataExporter','dataRouter','getdaq-sentry']:os.kill(int(entry.name),signal.SIGTERM)
   except (OSError,RuntimeError):pass
  for child in reversed(children):
   if child.poll() is None:child.terminate()
   child.wait(timeout=10)
  for log in logs:log.close()
if __name__=='__main__':main()
