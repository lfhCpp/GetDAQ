#!/usr/bin/env python3
"""Received old function bodies versus actual native run audit/actions.
Only host orchestration and successful CoBo Start/Resume commands are compared.
Producer-first stopping, explicit layout/version checks, temperature semantics,
data transport and physical timing are separately documented/tested.
"""
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path
from alignment_original_reference import extract

def main():
    root=Path(__file__).resolve().parents[1]
    index=Path(sys.argv[1]) if len(sys.argv)>1 else root/'build-ice-compat/system-acquisition-audits.jsonl'
    folder=root/'source/get/GetBench-software-release-20210208/src/get/rc'
    sources=[folder/'SystemManager.cpp',folder/'CoBoNode.cpp']
    before={p:hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}
    system,cobo=(p.read_text() for p in sources)
    system_bodies='\n'.join(extract(system,'SystemManager::'+name) for name in ('start','pause','resume','stop'))
    cobo_bodies='\n'.join(extract(cobo,'CoBoNode::'+name) for name in ('runInit','resetCoBoReadPointers','impl_start','postStart'))
    cobo_bodies+='\n'+extract(cobo,'CoBoNode::switchToFastControl',0)+'\n'+extract(cobo,'CoBoNode::switchToFastControl',1)
    common=r'''
#include <cstdint>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using std::size_t;
#define LOG_INFO() std::ostringstream()
#define LOG_DEBUG() std::ostringstream()
#define NODE_INFO() std::ostringstream()
#define NODE_DEBUG() std::ostringstream()
#define NODE_ERROR() std::ostringstream()
namespace mdaq { namespace utl { struct CmdException:std::runtime_error{
    std::string reason; explicit CmdException(const std::string&s):std::runtime_error(s),reason(s){}
};}}
using mdaq::utl::CmdException;
// Only the old numeric-to-string environment conversion is replaced; no Boost dependency.
namespace boost { template<class T,class U>T lexical_cast(const U&v){
    std::stringstream stream;stream<<v;T out;stream>>out;return out;
}}
namespace CCfg { struct Object{explicit Object(const char*){}};
struct CConfig{explicit CConfig(Object*){} CConfig(){} }; }
namespace get { namespace rc {
std::vector<std::string> actions;
'''
    sys_program=common+r'''
struct CoBoNode{
    std::string id;bool active;
    CoBoNode(unsigned i,bool a):id("C"+std::to_string(i)),active(a){}
    void runInit(){if(active)actions.push_back(id+".init");}
    void start(){if(active)actions.push_back(id+".start");}
    void stop(){if(active)actions.push_back(id+".stop");}
};
struct MutantNode{
    std::string id;bool trigger,init;
    MutantNode(unsigned i,bool t,bool r):id("M"+std::to_string(i)),trigger(t),init(r){}
    void runInit(){actions.push_back(id+".init");} // Function call exists even when counter clear is disabled.
    void impl_start(){if(trigger)actions.push_back(id+".start");}
    void impl_stop(){actions.push_back(id+".stop");}
    unsigned moduleId(){return unsigned(id.back()-'0');}
};
struct Node{struct List{template<class T>struct Iterator{
    std::vector<T>&v;size_t i=0;explicit Iterator(std::vector<T>&x):v(x){}
    bool hasNext()const{return i<v.size();}void operator++(){++i;}T*operator->(){return &v.at(i);}
};};};
template<class T>struct Nodes{
    std::vector<T>v;size_t size()const{return v.size();}
    void doLoop_start(CCfg::CConfig){for(auto&n:v)n.start();}
    void doLoop_stop(CCfg::CConfig){for(auto&n:v)n.stop();}
};
using MutantSeq=std::vector<MutantNode*>;
struct SystemManager{
    Nodes<CoBoNode>c;Nodes<MutantNode>m;
    template<class T>Nodes<T>&getList();
    template<class T>typename Node::List::Iterator<T>iterate(){return typename Node::List::Iterator<T>(getList<T>().v);}
    MutantNode&mutantMaster(){return m.v.front();}
    MutantSeq mutantSlaves(){MutantSeq out;for(size_t i=1;i<m.size();++i)out.push_back(&m.v[i]);return out;}
    void start();void pause();void resume();void stop();
};
template<>Nodes<CoBoNode>&SystemManager::getList<CoBoNode>(){return c;}
template<>Nodes<MutantNode>&SystemManager::getList<MutantNode>(){return m;}
'''+system_bodies+r'''
}}
int main(int argc,char**argv){
    if(argc!=7)return 2;using namespace get::rc;SystemManager s;
    unsigned nm=std::stoul(argv[2]),nc=std::stoul(argv[3]),mask=std::stoul(argv[4]);
    for(unsigned i=0;i<nc;++i)s.c.v.emplace_back(i,(mask&(1u<<i))!=0);
    for(unsigned i=0;i<nm;++i)s.m.v.emplace_back(i,i!=0||std::stoi(argv[5])!=0,i!=0||std::stoi(argv[6])!=0);
    std::string op=argv[1];if(op=="start")s.start();else if(op=="resume")s.resume();else if(op=="pause")s.pause();else if(op=="stop")s.stop();else return 3;
    for(const auto&a:actions)std::cout<<a<<'\n';
}
'''
    co_program=common+r'''
struct Registers{
    std::map<std::string,uint64_t>memory;bool shared=false;
    void writeRegister(const std::string&r,uint64_t v){actions.push_back("ctrl."+r+"="+std::to_string(v));}
    void writeField(const std::string&r,const std::string&f,uint64_t v){actions.push_back("ctrl."+r+"."+f+"="+std::to_string(v));}
    uint64_t readRegister(const std::string&r){if(shared&&r.find("_asad")!=std::string::npos)throw CmdException("shared fixture");return memory.at(r);}
};
struct Proc{
    Registers&r;explicit Proc(Registers&x):r(x){}
    Proc&write(const std::string&n,uint64_t v){r.writeRegister(n,v);return *this;}
    Proc&write(const std::string&n,const std::string&f,uint64_t v){r.writeField(n,f,v);return *this;}
    Proc&sleep_ms(unsigned){return *this;}
};
struct Sender{void daqStart(){actions.push_back("sender-start");}};
struct Hardware{Registers&r;Sender*dataSenderCtrlPrx;explicit Hardware(Registers&x,Sender*s):r(x),dataSenderCtrlPrx(s){}
    Proc regProc(const char*){return Proc(r);}
};
struct Pulser{void startPeriodicPulser(){actions.push_back("pulser-start");}};
struct CoBoNode{
    bool active=true,useExternalTimeReset_=false;uint16_t agetMask_=0;
    struct{bool asadPulserPeriodicModeEnabled=false;}configData_;
    Registers registers;Sender sender;Pulser pulser;Hardware hardware;
    CoBoNode():hardware(registers,&sender){}
    bool isCoBoActive(){return active;}Registers*ctrl(){return &registers;}
    Hardware&hwNode(){return hardware;}Pulser*asadPulserMgr(){return &pulser;}
    void readAsAdTemperatures(){} // Raw monitoring/conversion is outside this command comparison.
    void runInit();void resetCoBoReadPointers();void impl_start();void postStart();
    void switchToFastControl(uint16_t);void switchToFastControl();
};
'''+cobo_bodies+r'''
}}
int main(int argc,char**argv){
    if(argc!=10)return 2;using namespace get::rc;CoBoNode c;
    std::string op=argv[1];c.active=std::stoi(argv[2])!=0;c.useExternalTimeReset_=std::stoi(argv[3])!=0;
    c.configData_.asadPulserPeriodicModeEnabled=std::stoi(argv[4])!=0;c.registers.shared=std::stoi(argv[5])!=0;c.agetMask_=uint16_t(std::stoul(argv[6]));
    for(unsigned i=0;i<4;++i)c.registers.memory["memStart_asad"+std::to_string(i)]=std::stoull(argv[7+i]);
    c.registers.memory["memStart"]=std::stoull(argv[7]);
    if(op=="start")c.runInit();c.impl_start();for(const auto&a:actions)std::cout<<a<<'\n';
}
'''
    # 1 operation + 5 controls + 4 observed memory starts = ten arguments after executable.
    co_program=co_program.replace('if(argc!=10)return 2','if(argc!=11)return 2')
    records=[json.loads(row) for row in index.read_text().splitlines()]
    if len(records)!=12: raise AssertionError('Need 12 final successful native phase records')
    with tempfile.TemporaryDirectory(prefix='getdaq-run-reference-') as tmp:
        tmp=Path(tmp)
        for name,program in [('system',sys_program),('cobo',co_program)]:
            source=tmp/(name+'.cpp');source.write_text(program)
            subprocess.run(['g++','-std=c++11','-pedantic-errors',str(source),'-o',str(tmp/name)],check=True)
        checks=0
        for record in records:
            args=[record['operation'],str(record['mutants']),str(record['cobos']),str(record['active']),str(int(record['master_trigger'])),str(int(record['master_init']))]
            expected=subprocess.check_output([str(tmp/'system')]+args,text=True).splitlines()
            audit=Path(record['directory'])
            if not audit.is_absolute(): audit=index.resolve().parent/audit
            rows=[json.loads(row) for row in (audit/'system-run.jsonl').read_text().splitlines()]
            stages={'cobo-run-init':'init','cobo-arm':'start','mutant-run-init':'init','mutant-run':'start','mutant-stop-production':'stop','cobo-stop-pulser':'stop'}
            actual=[r['id']+'.'+stages[r['stage']] for r in rows if r.get('phase')=='step_begin' and r.get('stage') in stages]
            if actual!=expected: raise AssertionError((record['operation'],actual,expected))
            checks+=1
            if record['operation'] not in ('start','resume'): continue
            for i,board in enumerate(record['boards']):
                args=[record['operation']]+[str(int(board[k])) for k in ('enabled','external','periodic','shared')]+[str(board['aget_mask'])]+list(map(str,board['mem_start']))
                expected=subprocess.check_output([str(tmp/'cobo')]+args,text=True).splitlines()
                prefix='C'+str(i)+'.'
                actual=[a[len(prefix):] for a in record['actions'] if a.startswith(prefix) and '.ctrl.asadSelect.' not in a]
                if actual!=expected: raise AssertionError(('CoBo',record['operation'],i,actual,expected))
                checks+=1
        print('PASS 10 extracted old SystemManager/CoBoNode bodies vs 12 native phases and successful board Start/Resume:',checks,'comparisons')
        print('Explicit exclusions: temperatures, physical delays, runtime layout fallback and immutable memory check, new producer-first stop/flush sequencing, inactive sender connection policy, real hardware/data transport.')
    for p,digest in before.items():
        if hashlib.sha256(p.read_bytes()).hexdigest()!=digest: raise AssertionError('Received source changed')
        print('SOURCE_SHA256',digest,p.relative_to(root))
if __name__=='__main__': main()
