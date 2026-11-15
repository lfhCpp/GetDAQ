#!/usr/bin/env python3
"""Compare shared prefix inspection with the frozen pre-refactor MFM inspector."""
import argparse,json,subprocess
from pathlib import Path
from run_receiver_observation_tests import sha


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('output',type=Path);parser.add_argument('--sanitized',action='store_true');args=parser.parse_args()
    root=Path(__file__).resolve().parents[1];out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    archive=root/'build-source-readout/validation-source-readout-ecc';source=archive/'verified-sources/apps/datareceiver/mfmstream.cpp'
    inputs=json.loads((archive/'input-sha256.json').read_text());assert sha(source)==inputs['apps/datareceiver/mfmstream.cpp']
    old=source.read_text();a=old.index('namespace {');b=old.index('MfmStream::MfmStream',a);helpers=old[a:b]
    a=old.index('FrameInfo MfmStream::Inspect(');b=old.index('void MfmStream::Feed',a);inspect=old[a:b].replace('FrameInfo MfmStream::Inspect','FrameInfo Inspect')
    code='#include "'+str(root/'apps/datareceiver/mfmheader.h')+'"\n#include <algorithm>\n#include <iostream>\n#include <random>\n#include <vector>\nusing GetDAQ::FrameInfo;\nnamespace Frozen {\n'+helpers+inspect+'}\n'
    code+=r'''
void Put(std::vector<std::uint8_t> &p,unsigned at,std::uint64_t value,unsigned count,bool little) {
    for(unsigned i=0;i<count;++i)p[at+(little?i:count-i-1)]=std::uint8_t(value>>(8*i));
}
bool Same(const FrameInfo &a,const FrameInfo &b) {
    return a.bytes==b.bytes && a.type==b.type && a.revision==b.revision && a.source==b.source && a.cobo==b.cobo && a.asad==b.asad && a.event==b.event && a.hasEvent==b.hasEvent && a.littleEndian==b.littleEndian && a.coboFrame==b.coboFrame && a.mutantFrame==b.mutantFrame;
}
int main() {
    std::mt19937 random(1743);unsigned accepted=0,rejected=0;
    for(unsigned i=0;i<20000;++i) {
        const bool little=i&1,blob=(i%7)==0;const unsigned exponent=i%7,unit=1u<<exponent;
        const auto bytes=std::max(64u,unit*(1+unsigned(random()%32)));std::vector<std::uint8_t> p(bytes);
        for(auto &byte:p)byte=std::uint8_t(random());
        p[0]=std::uint8_t(exponent|(little?128:0)|(blob?64:0));
        Put(p,1,bytes/unit,3,little);Put(p,5,i%9==0?8:i%3==0?2:1,2,little);p[7]=std::uint8_t(i%9==0?0:i%3==0?5:4);
        const auto header=std::max(unit,((28+unit-1)/unit)*unit);Put(p,8,header/unit,2,little);Put(p,10,4,2,little);Put(p,12,(bytes-header)/4,4,little);p[27]=std::uint8_t(i%4);
        if(i%5==0)Put(p,1,0xffffff,3,little);
        else if(i%11==0)Put(p,8,0xffff,2,little);
        else if(i%13==0)Put(p,12,0xffffffff,4,little);
        else if(i%17==0)p[27]=9;
        bool oldOk=true,newOk=true;FrameInfo a,b;
        try {a=Frozen::Inspect(p.data(),p.size());}catch(...) {oldOk=false;}
        try {b=GetDAQ::InspectMfmHeader(p.data(),std::min<std::size_t>(28,p.size()),p.size());}catch(...) {newOk=false;}
        if(oldOk!=newOk || (oldOk && !Same(a,b))) {std::cerr<<"Mismatch "<<i<<'\n';return 1;}
        oldOk?++accepted:++rejected;
    }
    for(unsigned bytes:{8u,12u,16u,18u,27u,28u}) {
        std::vector<std::uint8_t> p(bytes,0);p[0]=64;p[3]=std::uint8_t(bytes);p[6]=8;
        const auto a=Frozen::Inspect(p.data(),p.size()),b=GetDAQ::InspectMfmHeader(p.data(),std::min<std::size_t>(28,p.size()),p.size());
        if(!Same(a,b))return 1;
        ++accepted;
    }
    std::cout<<"PASS frozen/shared MFM header 20006 cases; accepted "<<accepted<<", rejected "<<rejected<<'\n';return 0;
}
'''
    generated=out/'header-comparison.cpp';generated.write_text(code);program=out/'header-comparison';command=['c++','-std=c++11','-pedantic-errors','-Wall','-Wextra',str(generated),'-o',str(program)]
    command+=['-O1','-g','-fno-omit-frame-pointer','-fsanitize=address,undefined'] if args.sanitized else ['-O2']
    with (out/'compile.txt').open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
    with (out/'result.txt').open('w') as log:subprocess.run([str(program)],stdout=log,stderr=subprocess.STDOUT,check=True)
    receipt={'schema':1,'sanitized':args.sanitized,'case_count':20006,'original_source_sha256':sha(source),'shared_header_sha256':sha(root/'apps/datareceiver/mfmheader.h'),
        'generated_source_sha256':sha(generated),'program_sha256':sha(program),'builder_sha256':sha(Path(__file__)),'command':command,'full_original_library_executed':False}
    (out/'result.json').write_text(json.dumps(receipt,indent=2)+'\n');print((out/'result.txt').read_text().strip())


if __name__=='__main__':main()
