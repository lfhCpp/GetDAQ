#!/usr/bin/env python3
"""Compare actual unchanged legacy binary/XML execution with production conversion.
Small private configuration fixtures only; no DAQ payload or hardware access.
"""
import argparse,hashlib,json,subprocess,tempfile
from pathlib import Path
import xml.etree.ElementTree as ET

TYPES={'integer':'int','hexadecimal':'hex','real number':'real','boolean':'bool','string':'string'}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('current',type=Path);ap.add_argument('legacy',type=Path);ap.add_argument('legacy_export',type=Path);ap.add_argument('evidence',type=Path);args=ap.parse_args()
    args.evidence.mkdir(parents=True,exist_ok=True);programs={n:getattr(args,n).resolve() for n in ('current','legacy','legacy_export')};traces=[];checks=[]
    def run(name,argv,ok=True):
        r=subprocess.run([str(programs[name])]+[str(x) for x in argv],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,timeout=30)
        traces.append({'program':name,'args':[str(x) for x in argv],'exit_code':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
        assert (r.returncode==0)==ok,(name,argv,r.stdout,r.stderr);return r.stdout
    def queries(xml):
        result=[]
        def walk(node,path):
            for c in node:
                p=path+[c.tag+('['+c.attrib['id']+']' if c.attrib.get('id') else '')]
                if c.attrib.get('type'):result.append('.'.join(p))
                else:walk(c,p)
        walk(ET.fromstring(xml),[]);return result
    def oldvalues(path,paths):
        d=json.loads(run('legacy',[path]+paths));assert d['loaded'];assert all(q['ok'] for q in d['queries']),d
        return [(q['path'],TYPES[q['type']],q['value'],q['unit'],q['range'],q['in_range']) for q in d['queries']]
    with tempfile.TemporaryDirectory(prefix='getdaq-binary-config-') as tmp:
        root=Path(tmp);samples=Path(__file__).resolve().parents[1]/'source/cconfig/CompoundConfig-release-20210208/test/data'
        for name in ('daq','daq2'):
            source=samples/(name+'.bcfg');xml=root/(name+'.xcfg');run('current',['import-binary',source,xml]);paths=queries(xml.read_bytes());baseline=oldvalues(source,paths);assert oldvalues(xml,paths)==baseline
            exported=root/(name+'-new.bcfg');rootname=ET.fromstring(xml.read_bytes()).tag;run('current',['export-binary',xml,rootname,'declarations',exported]);assert oldvalues(exported,paths)==baseline
            reimport=root/(name+'-again.xcfg');run('current',['import-binary',exported,reimport]);assert oldvalues(reimport,paths)==baseline
            checks.append({'case':name,'scalar_queries':len(paths),'legacy_sample_to_xml_and_new_binary_equal':True,'sample_sha256':sha(source)})
        cases={
          'all-types':'<Setup><i type="int" range="]-10,5[">-3</i><h type="hex" range="{0x0,0xffffffff}">0xffffffff</h><r type="real" range="[-inf,+inf]">1.25</r><b type="bool" range="{false,true}">true</b><s type="string" unit="字节"><![CDATA[  a\nb  ]]></s></Setup>',
          'integer-boundaries':'<Setup><a type="int">-2147483648</a><b type="int">2147483647</b><s type="string"><![CDATA[]]></s></Setup>',
          'defaults':'<Setup><node id="*"><n type="int" unit="ns" range="[0,10]">1</n></node><node id="2"><n type="int">2</n></node></Setup>',
          'string-range':'<Setup><s type="string" range="{&quot;a\\n&quot;,&quot;b&quot;}"><![CDATA[a\n]]></s><t type="real" range="{1.0,2.5}">1.0</t></Setup>'}
        for name,text in cases.items():
            xml=root/(name+'.xcfg');xml.write_text(text);paths=queries(xml.read_bytes());baseline=oldvalues(xml,paths);oldbin=root/(name+'-old.bcfg');run('legacy_export',[xml,'root',oldbin]);assert oldvalues(oldbin,paths)==baseline
            converted=root/(name+'-old.xcfg');run('current',['import-binary',oldbin,converted]);assert oldvalues(converted,paths)==baseline
            newbin=root/(name+'-new.bcfg');run('current',['export-binary',xml,'Setup','declarations',newbin]);assert oldvalues(newbin,paths)==baseline
            checks.append({'case':name,'scalar_queries':len(paths),'old_export_new_import_and_new_export_old_parse_equal':True})
        ill=samples/'daq2_ill.bcfg';dest=root/'ill.xcfg';run('current',['import-binary',ill,dest],False);assert not dest.exists();checks.append({'case':'historical-ill-refused','new_file_absent':True})
        assert not list(root.rglob('*.graw'))
    result={'passed':True,'checks':checks,'program_sha256':{n:sha(p) for n,p in programs.items()},'scope':'actual unchanged historical binary/XML runtime and current production CLI; no hardware, DAQ raw, or whole-library equivalence','temporary_configurations_removed':True,'raw_created':False}
    (args.evidence/'result.json').write_text(json.dumps(result,indent=2,ensure_ascii=False)+'\n');(args.evidence/'traces.json').write_text(json.dumps(traces,indent=2,ensure_ascii=False)+'\n');print('PASS actual binary/XML comparison: %d cases; temporary configurations removed'%len(checks))
if __name__=='__main__':main()
