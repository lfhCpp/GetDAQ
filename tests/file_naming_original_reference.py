#!/usr/bin/env python3
"""Compare received GET timestamp/name code and frozen old plan encoding with current code."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    root=Path(__file__).resolve().parents[1]
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('supervisor_build',type=Path);parser.add_argument('receiver_build',type=Path);parser.add_argument('evidence',type=Path);parser.add_argument('--payload-evidence',type=Path,required=True)
    args=parser.parse_args();supervisor=args.supervisor_build.resolve();receiver=args.receiver_build.resolve();args.evidence.mkdir(parents=True,exist_ok=True)
    folder=Path(tempfile.mkdtemp(prefix='file-naming-reference-',dir=args.evidence))
    old_root=root/'build-ecc-json/validation-native-json'
    old_plan=old_root/'verified-sources/apps/datasupervisor/supervisorio.cpp'
    manifest=json.loads((old_root/'files-sha256.json').read_text());assert manifest[str(old_plan.relative_to(old_root))]==sha(old_plan)
    text=old_plan.read_text();start=text.index('std::string EncodeOptions(');end=text.rfind('\n} }');assert start<end
    plan_body=text[start:end].replace('EncodeOptions(','LegacyEncodeOptions(',1)
    timestamp_source=root/'source/get/GetBench-software-release-20210208/src/get/utl/Time.cpp'
    text=timestamp_source.read_text();start=text.index('std::string buildTimeStamp(');end=text.index('//_________________________________________________________________________________________________',start)
    timestamp_body=text[start:end].strip()
    storage_source=root/'source/get/GetBench-software-release-20210208/src/get/daq/FrameStorage.cpp'
    text=storage_source.read_text();begin=text.index('void FrameStorage::createNewFile(')
    start=text.index('\tstd::ostringstream oss;',begin);end=text.index('\n\tstorage.dataFile.clear()',start);filename_body=text[start:end]
    legacy=folder/'legacy.cpp'
    legacy.write_text('#include "supervisorio.h"\n#include "systemexperimentconfiguration.h"\n#include <ctime>\n#include <cstdio>\n#include <sstream>\n#include <iomanip>\n#include <utility>\n'
        'namespace IceUtil {typedef long long Int64;struct Time {Int64 value;Int64 toMicroSeconds() const {return value;}};}\n'
        'namespace get {namespace utl {\n'+timestamp_body+'\n}}\nnamespace GetDAQ {namespace SupervisorIO {\n'+plan_body+'\n}}\n'
        'std::string LegacyName(unsigned cobo,unsigned asad,const std::string &stamp,uint64_t index) {\n'
        'std::string fileDirectory=".",filePrefix,fileTimestamp=stamp;auto sourceIdx=std::make_pair(cobo,asad);\n'
        'struct {uint64_t fileIndex;std::string fileName;} storage;storage.fileIndex=index;\n'+filename_body+'\nreturn storage.fileName.substr(2);}\n')
    probe=folder/'probe.cpp'
    probe.write_text(r'''#include "supervisorio.h"
#include "framestore.h"
#include <iostream>
#include <stdexcept>
namespace IceUtil {typedef long long Int64;struct Time {Int64 value;Int64 toMicroSeconds() const {return value;}};}
namespace get {namespace utl {std::string buildTimeStamp(const IceUtil::Time &);}}
namespace GetDAQ {namespace SupervisorIO {std::string LegacyEncodeOptions(const DataSupervisorOptions &,const std::string &);}}
std::string LegacyName(unsigned,unsigned,const std::string &,uint64_t);
int main(int argc,char **argv) {
 try {
  using namespace GetDAQ;using namespace GetDAQ::SupervisorIO;
  if(argc==3 && std::string(argv[1])=="time") {auto usec=std::stoull(argv[2]);IceUtil::Time time;time.value=static_cast<long long>(usec);
   const auto before=get::utl::buildTimeStamp(time),after=FrameStore::FormatGetTimestamp(usec);
   if(before!=after) throw std::runtime_error("Timestamp differs");std::cout<<after<<'\n';return 0;}
  if(argc==6 && std::string(argv[1])=="name") {std::cout<<LegacyName(std::stoul(argv[2]),std::stoul(argv[3]),argv[4],std::stoull(argv[5]))<<'\n';return 0;}
  if(argc==2 && std::string(argv[1])=="plan") {
   unsigned cases=0;
   for(unsigned count:{1u,2u,16u}) for(bool catalog:{false,true}) {
    DataSupervisorOptions options;options.deploymentId="legacy-default";options.auditRoot="/srv/control";options.receiverExecutable="/opt/getdaq/receiver";
    options.configurationSha256=std::string(64,'a');options.artifactSha256=std::string(64,'b');
    if(catalog) options.catalogRoot="/srv/catalog";
    for(unsigned i=0;i<count;++i) {ReceiverSourcePlan source;source.id="C"+std::to_string(i);source.outputRoot="/srv/data";
     source.receiverAddress="192.0.2.10:"+std::to_string(23000+3*i);source.bindAddress="0.0.0.0";source.dataPort=23000+3*i;source.statusPort=23001+3*i;
     source.exportPort=23002+3*i;source.queueMiB=1+i;source.maxFrameMiB=1;options.sources.push_back(source);}
    const auto before=LegacyEncodeOptions(options,std::string(64,'c')),after=EncodeOptions(options,std::string(64,'c'));
    if(before!=after) throw std::runtime_error("Native canonical plan differs");++cases;
   }
   std::cout<<cases<<'\n';return 0;
  }
  return 2;
 } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
''')
    libraries=[supervisor/'libgetdaq-data-supervisor-core.a',supervisor/'libgetdaq-supervisor-protocol.a',
               supervisor/'control-protocol/libgetdaq-control-protocol.a',supervisor/'experiment/libgetdaq-experiment-core.a',receiver/'libgetdaq-data-core.a']
    command=['c++','-std=c++11','-O2','-pedantic-errors','-I'+str(root/'apps/datasupervisor'),'-I'+str(root/'common/experiment'),'-I'+str(root/'apps/datareceiver'),str(legacy),str(probe),*[str(path) for path in libraries],'-pthread','-o',str(folder/'reference-probe')]
    with (folder/'compile.txt').open('w') as log:subprocess.run(command,check=True,stdout=log,stderr=subprocess.STDOUT)
    times=[]
    for zone in ('UTC','Asia/Shanghai','America/New_York'):
        env=dict(os.environ,TZ=zone)
        for usec in (0,1,999999,1000000,1517300000123456,1609459199999999,1710054000000000,1730613600123000,1791417600000000):
            out=subprocess.check_output([str(folder/'reference-probe'),'time',str(usec)],env=env,text=True).strip()
            times.append({'zone':zone,'microseconds':str(usec),'timestamp':out})
    assert subprocess.check_output([str(folder/'reference-probe'),'plan'],text=True).strip()=='6'
    roots=list(args.payload_evidence.glob('receiver-file-naming-*'));assert len(roots)==1
    payload=json.loads((roots[0]/'result.json').read_text());compared=[]
    for case in payload['cases']:
        for item in case['files']:
            match=re.fullmatch(r'(CoBo([0-9]+)_AsAd([0-3])|MuTanT)_([^_]+)_([0-9]+)\.graw',item['name'])
            if not match:continue # Unknown MFM is preserved by the new receiver; old storage dropped it.
            cobo=100 if match[1]=='MuTanT' else int(match[2]);asad=0 if match[1]=='MuTanT' else int(match[3])
            old=subprocess.check_output([str(folder/'reference-probe'),'name',str(cobo),str(asad),match[4],match[5]],text=True).strip()
            assert old==item['name'];compared.append(item['name'])
    assert len(compared)==21
    record={'schema':1,'timestamp_cases':times,'native_plan_cases':6,'actual_filename_comparisons':compared,'raw_payload_bytes':0,
            'source_sha256':{str(path.relative_to(root)):sha(path) for path in [timestamp_source,storage_source,old_plan]},
            'generated_sha256':{path.name:sha(path) for path in [legacy,probe]},'program_sha256':sha(folder/'reference-probe'),
            'libraries_sha256':{str(path):sha(path) for path in libraries},'commands':[command],'runner_sha256':sha(Path(__file__))}
    (folder/'result.json').write_text(json.dumps(record,indent=2)+'\n')
    print('PASS original references: 27 timestamps, 6 unchanged native plans, 21 actual GET-style filenames;',folder)


if __name__=='__main__':main()
