#!/usr/bin/env python3
import sys,json,pathlib,hashlib,socket
source,output,ecc,data,*ports=sys.argv[1:];ports=list(map(int,ports));out=pathlib.Path(output);out.mkdir();(out/'objects').mkdir()
def sha(b):return hashlib.sha256(b).hexdigest()
def port():
 s=socket.socket();s.bind(('127.0.0.1',0));p=s.getsockname()[1];s.close();return p
files=[];mapping=[]
for kind in ['describe','prepare','configure']:
 b=b'<?xml version="1.0"?><config/>';ident='group1-'+kind;digest=sha(b);(out/'objects'/digest).write_bytes(b);files.append(dict(id=ident,sha256=digest,bytes=len(b),objects=[digest]));mapping.append(dict(file=ident,path=kind+'-pedestals.xcfg',sha256=digest))
group={'id':'group1','name':'Original ECC','backend':'original-soap','experiment':'pedestals','master_repository':str(out),'repository_files':mapping,'board_index':0,'receiver_ip':'127.0.0.1','data_port':ports[3],'export_port':port(),'sentry_port':port(),'ecc':dict(node_id=ecc,address='127.0.0.1',port=ports[0],callback_port=ports[2]),'receivers':[dict(id='data',node_id=data,address='127.0.0.1',port=ports[1],sources=[dict(id='0',bind='127.0.0.1',data_port=ports[3])])]}
manifest=dict(schema=1,kind='getdaq.distributed-release',project_id='original-integration',hosts=[dict(id=ecc),dict(id=data)],groups=[group],files=files)
(out/'manifest.json').write_text(json.dumps(manifest,separators=(',',':')))
