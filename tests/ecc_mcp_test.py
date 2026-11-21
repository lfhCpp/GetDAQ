#!/usr/bin/env python3
"""Current SOAP MCP regression. Uses only local HTTP fixtures, never boards."""
import argparse,json,os,selectors,subprocess,tempfile,threading,time
import xml.etree.ElementTree as ET
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from pathlib import Path

SOAP='http://schemas.xmlsoap.org/soap/envelope/'
class Handler(BaseHTTPRequestHandler):
 def log_message(self,*args):pass
 def reply(self,value,mime='application/json'):
  data=value.encode() if isinstance(value,str) else json.dumps(value).encode();self.send_response(200);self.send_header('Content-Type',mime);self.send_header('Content-Length',str(len(data)));self.end_headers()
  try:self.wfile.write(data)
  except (BrokenPipeError,ConnectionResetError):pass
 def do_GET(self):
  assert self.path=='/status';self.server.observations+=1
  self.reply(dict(disk='fixture',process='dataRouter',data_path='/fixture/raw',data_written_gb=.125,data_path_files=3,disk_avail_gb=90,disk_total_gb=100))
 def do_POST(self):
  data=self.rfile.read(int(self.headers['Content-Length']));root=ET.fromstring(data);body=root.find('{'+SOAP+'}Body');command=list(body)[0].tag.split('}')[-1];self.server.requests.append((command,data))
  if command in self.server.delay:time.sleep(self.server.delay[command])
  if command!='GetState':self.server.state={'Describe':2,'Prepare':3,'Configure':4,'Start':5,'Stop':4,'Pause':6,'Resume':5,'Breakup':3,'Undo':1}.get(command,self.server.state)
  payload=f'<State>{self.server.state}</State><Transition>0</Transition>' if command=='GetState' else ''
  self.reply(f'<s:Envelope xmlns:s="{SOAP}"><s:Body><ResponseState><ErrorCode>0</ErrorCode><ErrorMessage/>{payload}</ResponseState></s:Body></s:Envelope>','text/xml')
class Client:
 def __init__(self,program,config):
  self.proc=subprocess.Popen([str(program),'--config',str(config)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE);self.id=0
 def send(self,obj):self.proc.stdin.write(json.dumps(obj).encode()+b'\n');self.proc.stdin.flush()
 def read(self):
  ready=selectors.DefaultSelector();ready.register(self.proc.stdout,selectors.EVENT_READ);assert ready.select(8),'MCP response timed out';ready.close();line=self.proc.stdout.readline();assert line,'MCP process exited: '+self.proc.stderr.read().decode();return json.loads(line)
 def request(self,method,params=None):
  self.id+=1;self.send(dict(jsonrpc='2.0',id=self.id,method=method,params=params or {}));return self.read()
 def init(self):
  result=self.request('initialize',dict(protocolVersion='2025-11-25',capabilities={},clientInfo=dict(name='test',version='1')));assert result['result']['protocolVersion']=='2025-11-25';self.send(dict(jsonrpc='2.0',method='notifications/initialized'))
 def call(self,name,**args):return self.request('tools/call',dict(name=name,arguments=args))
 def close(self):
  if self.proc.poll() is None:self.proc.stdin.close();self.proc.wait(timeout=5)
  assert self.proc.returncode==0,self.proc.stderr.read().decode()
def value(response):
 result=response['result'];assert not result.get('isError'),response;return result['structuredContent']
def main():
 p=argparse.ArgumentParser();p.add_argument('program',type=Path);p.add_argument('--evidence',type=Path);a=p.parse_args();checks=[];server=ThreadingHTTPServer(('127.0.0.1',0),Handler);server.state=4;server.requests=[];server.observations=0;server.delay={};thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start();clients=[]
 try:
  with tempfile.TemporaryDirectory(prefix='getdaq-soap-mcp-') as directory:
   root=Path(directory);yaml=root/'envoy.yml';yaml.write_text(f'''experiment: pedestals
run_number: 0
getdaq:
  config_dir: ./configuration
  output_dir: ./records
  timeout_ms: 300
  step_timeout_ms: 300
  nodes:
    - id: 0
      role: cobo
      host: 127.0.0.1
      ecc_port: {server.server_port}
      sentry_port: {server.server_port}
      router_host: 192.0.2.50
      router_port: 46005
      describe_id: cobo0
      prepare_id: pedestals
      configure_id: pedestals
''');cfg=root/'mcp.json';options=dict(schema=2,run_config=str(yaml),allow_control=False,request_timeout_ms=500,operation_directory=str(root/'operations'));cfg.write_text(json.dumps(options));client=Client(a.program.resolve(),cfg);clients.append(client)
   assert client.request('tools/list')['error']['code']==-32002;client.init();tools=value_tools=client.request('tools/list')['result']['tools'];assert 'ecc_submit_operation' not in [t['name'] for t in tools];assert value(client.call('ecc_status'))['items'][0]['state']==4;assert value(client.call('ecc_receivers'))['items'][0]['data_path']=='/fixture/raw';assert value(client.call('ecc_configuration'))['nodes'][0]['prepare_id']=='pedestals';assert client.call('ecc_submit_operation')['error'];assert all(c=='GetState' for c,_ in server.requests);checks.append('MCP negotiation and read-only default; GUI YAML reuses original SOAP/Sentry clients')
   assert client.call('ecc_status',node_id='0')['error']['code']==-32602;assert client.call('ecc_status',node_id=77)['error']['code']==-32602;assert client.call('ecc_status',shell='rm anything')['error'];assert client.call('unknown_tool')['error']['code']==-32602
   client.send(dict(jsonrpc='2.0',method='tools/call',params=dict(name='ecc_submit_operation',arguments={})));assert client.request('ping')['result']=={};client.close();checks.append('invalid nodes/arguments/tools rejected; notifications cannot execute tools')
   options['allow_control']=True;cfg.write_text(json.dumps(options));client=Client(a.program.resolve(),cfg);clients.append(client);client.init();assert 'ecc_submit_operation' in [t['name'] for t in client.request('tools/list')['result']['tools']]
   args=dict(command='start',operation_id='run-0-start',node_id=0);assert client.call('ecc_submit_operation',**args,confirmation='no')['error'];assert not any(c=='Start' for c,_ in server.requests)
   args['confirmation']='I_CONFIRM_start_run-0-start';assert value(client.call('ecc_submit_operation',**args))['phase']=='acknowledged';assert value(client.call('ecc_submit_operation',**args))['phase']=='acknowledged';assert sum(c=='Start' for c,_ in server.requests)==1;assert value(client.call('ecc_status'))['items'][0]['state']==5
   receipt=root/'operations/run-0-start.json';assert receipt.exists();assert os.name=='nt' or receipt.stat().st_mode&0o077==0;client.close();client=Client(a.program.resolve(),cfg);clients.append(client);client.init();assert value(client.call('ecc_submit_operation',**args))['phase']=='acknowledged';assert sum(c=='Start' for c,_ in server.requests)==1;assert client.call('ecc_submit_operation',**dict(args,command='stop',confirmation='I_CONFIRM_stop_run-0-start'))['error'];checks.append('explicit confirmation, durable private receipts and no replay across retries/restart')
   value(client.call('ecc_submit_operation',node_id=0,command='stop',operation_id='run-0-stop',confirmation='I_CONFIRM_stop_run-0-stop'));assert value(client.call('ecc_status'))['items'][0]['state']==4
   server.delay['Start']=1.0
   delayed=dict(node_id=0,command='start',operation_id='timeout-start',confirmation='I_CONFIRM_start_timeout-start')
   client.id+=1;client.send(dict(jsonrpc='2.0',id=client.id,method='tools/call',params=dict(name='ecc_submit_operation',arguments=delayed)))
   deadline=time.monotonic()+2
   while not (root/'operations/timeout-start.json').exists() and time.monotonic()<deadline:time.sleep(.005)
   assert (root/'operations/timeout-start.json').exists()
   second=Client(a.program.resolve(),cfg);clients.append(second);second.init();blocked=second.call('ecc_submit_operation',**delayed);assert 'in progress' in blocked['error']['message'];second.close()
   response=client.read();assert response['result']['isError'];assert response['result']['structuredContent']['phase']=='unknown';again=client.call('ecc_submit_operation',**delayed);assert again['result']['structuredContent']['phase']=='unknown';assert sum(c=='Start' for c,_ in server.requests)==2;checks.append('SOAP timeout remains unknown and is never automatically replayed');checks.append('independent MCP processes sharing receipts cannot submit concurrent duplicate commands')
   interrupted=root/'operations/interrupted.json';interrupted.write_text(json.dumps(dict(operation_id='interrupted',phase='accepted')));assert value(client.call('ecc_operation',operation_id='interrupted'))['phase']=='unknown';assert client.call('ecc_operation',operation_id='../escape')['error'];client.close();checks.append('interrupted intent is inspectable as unknown; operation paths cannot escape')
   malformed=subprocess.run([str(a.program.resolve()),'--config',str(cfg)],input=b'{bad\n'+b'x'*(1024*1024+1)+b'\n',capture_output=True,timeout=5);assert malformed.returncode==0;responses=[json.loads(x) for x in malformed.stdout.splitlines()];assert [r['error']['code'] for r in responses]==[-32700,-32600];checks.append('stdout contains only JSON-RPC; malformed and oversized frames return bounded errors')
   assert not (root/'records').exists();assert not any(b'/catalog' in data for _,data in server.requests);assert all(b'192.0.2.50' in data for command,data in server.requests if command!='GetState');checks.append('no fabricated GUI Run/catalog or raw-file work; configured receiver mapping retained')
 finally:
  for c in clients:
   if c.proc.poll() is None:c.proc.terminate();c.proc.wait(timeout=5)
  server.shutdown();server.server_close()
 result=dict(passed=True,checks=checks,physical_hardware=False)
 if a.evidence:a.evidence.parent.mkdir(parents=True,exist_ok=True);a.evidence.write_text(json.dumps(result,indent=2)+'\n')
 print('PASS',len(checks),'original SOAP MCP checks')
if __name__=='__main__':main()
