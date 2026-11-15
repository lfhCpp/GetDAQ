#!/usr/bin/env python3
"""Extract old SystemManager bodies and compare successful native TCP audit stages.

Board operations/config objects are recording stubs. Existing alignment/Prepare
references separately test their internal order. Data links and physical effects
are outside this comparison; no new planner generates the old expected sequence.
"""
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path
from alignment_original_reference import extract


def main():
    root = Path(__file__).resolve().parents[1]
    index = Path(sys.argv[1]) if len(sys.argv) > 1 else root / 'build-ice-compat/system-preparation-audits.jsonl'
    source = root / 'source/get/GetBench-software-release-20210208/src/get/rc/SystemManager.cpp'
    original = source.read_bytes()
    digest = hashlib.sha256(original).hexdigest()
    names = ('prepare', 'configure', 'setCoBoInitDone', 'setCoBoMutantMode',
             'setCoBoLevel2Trigger', 'setWscaClk2', 'sendSyncPulses')
    bodies = '\n'.join(extract(original.decode(), 'SystemManager::' + n) for n in names)
    program = r'''
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using std::size_t;
#define LOG_INFO() std::ostringstream()
#define LOG_DEBUG() std::ostringstream()
namespace CCfg { struct CConfig {
    bool level2; explicit CConfig(bool l=true):level2(l){}
    CConfig operator()(const char*,int=0)const{return *this;}
    bool getBoolValue(bool)const{return level2;}
}; }
namespace get { namespace rc {
struct CmdException:std::runtime_error{explicit CmdException(const std::string&s):std::runtime_error(s){}};
std::vector<std::string> actions; bool preparing=false;
struct DataLinkSet{void fromXml(const std::string&){};};
std::ostream &operator<<(std::ostream&o,const DataLinkSet&){return o;}
struct CoBoNode {
    std::string id; bool enabled;
    CoBoNode(unsigned i,bool e):id("C"+std::to_string(i)),enabled(e){}
    void prepare(CCfg::CConfig){if(enabled)actions.push_back(id+".prepare");}
    void configure(CCfg::CConfig){if(enabled)actions.push_back(id+".configure");}
    void setInitDone(bool b){actions.push_back(id+".init="+std::to_string(b&&enabled));}
    void setMutantMode(bool b){actions.push_back(id+".mode="+std::to_string(b&&enabled));}
    void setLevel2Trigger(bool b){actions.push_back(id+".level2="+std::to_string(b&&enabled));}
    void setDataLinks(const DataLinkSet&){} void daqConnect(){}
};
struct MutantNode {
    std::string id; explicit MutantNode(unsigned i):id("M"+std::to_string(i)){}
    void setModuleCount(size_t){}
    void prepare(CCfg::CConfig){actions.push_back(id+".prepare");}
    void configure(CCfg::CConfig){actions.push_back(id+".configure");}
    CCfg::CConfig nodeCfg(CCfg::CConfig c){return c;}
    void setWscaClk2(bool b){if(!preparing)actions.push_back(id+".wsca="+std::to_string(b));}
    void restoreClockShelfMasks(){actions.push_back(id+".restore");}
    void setSyncPulse(bool b){actions.push_back(id+".sync="+std::to_string(b));}
    bool isReadoutEnabled(){return true;}
    void setDataLinks(const DataLinkSet&){} void daqConnect(){}
};
struct Node{struct List{template<class T>struct Iterator{
    std::vector<T>&v;size_t i=0;explicit Iterator(std::vector<T>&x):v(x){}
    bool hasNext()const{return i<v.size();} void operator++(){++i;} T*operator->(){return &v.at(i);}
};};};
template<class T>struct Nodes{
    std::vector<T> v;size_t size()const{return v.size();}
    void doLoop_prepare(CCfg::CConfig c){for(auto&n:v)n.prepare(c);}
    void doLoop_configure(CCfg::CConfig c){for(auto&n:v)n.configure(c);}
};
struct SystemManager{
    Nodes<CoBoNode> c;Nodes<MutantNode> m;unsigned amcSlots=0;
    template<class T>Nodes<T>&getList();
    template<class T>typename Node::List::Iterator<T>iterate(){return typename Node::List::Iterator<T>(getList<T>().v);}
    bool hasMutantMaster(){return m.size()>0;} MutantNode&mutantMaster(){return m.v.front();}
    void selectCoBoBoards(CCfg::CConfig){} void alignDelaysBetweenChassis(){}
    void checkCoherenceBetweenMutantModules(){} void checkCoBoMutantCoherence(){}
    void prepare(CCfg::CConfig);void configure(CCfg::CConfig&,const std::string&);
    void setCoBoInitDone(bool);void setCoBoMutantMode(bool);void setCoBoLevel2Trigger(CCfg::CConfig);
    void setWscaClk2(bool);void sendSyncPulses();
};
template<>Nodes<CoBoNode>&SystemManager::getList<CoBoNode>(){return c;}
template<>Nodes<MutantNode>&SystemManager::getList<MutantNode>(){return m;}
struct AmcSlotManager{
    SystemManager&s;explicit AmcSlotManager(SystemManager&x):s(x){}
    void findOccupiedSlots(unsigned&){if(s.m.size())actions.push_back("alignment");}
    void measureDelays(){}
};
''' + bodies + r'''
}}
int main(int argc,char**argv){
    if(argc!=5)return 2;
    using namespace get::rc;
    SystemManager s;unsigned nm=std::stoul(argv[1]),nc=std::stoul(argv[2]),active=std::stoul(argv[3]);
    CCfg::CConfig cfg(std::stoi(argv[4])!=0);
    for(unsigned i=0;i<nm;++i)s.m.v.emplace_back(i);
    for(unsigned i=0;i<nc;++i)s.c.v.emplace_back(i,(active&(1u<<i))!=0);
    preparing=true;s.prepare(cfg);preparing=false;s.configure(cfg,"");
    for(const auto&a:actions)std::cout<<a<<'\n';
}
'''
    records = [json.loads(row) for row in index.read_text().splitlines()]
    if len(records) != 3:
        raise AssertionError('Expected two three-MuTaNT encodings and one standalone successful TCP audit')
    with tempfile.TemporaryDirectory(prefix='getdaq-system-original-') as tmp:
        cpp = Path(tmp) / 'reference.cpp'
        binary = Path(tmp) / 'reference'
        cpp.write_text(program)
        subprocess.run(['g++','-std=c++11','-pedantic-errors','-Wall','-Wextra',str(cpp),'-o',str(binary)],check=True)
        for record in records:
            expected = subprocess.check_output([str(binary),str(record['mutants']),str(record['cobos']),
                str(record['active']),str(int(record['level2']))],text=True).splitlines()
            rows = [json.loads(line) for line in (Path(record['directory'])/'system.jsonl').read_text().splitlines()]
            assert any(r['phase'] == 'prepare_complete' for r in rows)
            assert any(r['phase'] == 'configure_complete' for r in rows)
            actual = []
            for row in rows:
                phase = row['phase']
                if phase == 'alignment_begin': actual.append('alignment')
                elif phase == 'cobo_prepare_begin': actual.append(row['id']+'.prepare')
                elif phase == 'hardware_begin':
                    stage, ident = row['stage'], row['id']
                    enabled = ident[0] == 'C' and bool(record['active'] & (1 << int(ident[1:])))
                    if stage in ('mutant-prepare','mutant-configure','cobo-configure'):
                        actual.append(ident+('.prepare' if stage == 'mutant-prepare' else '.configure'))
                    elif stage == 'level2': actual.append(ident+'.level2='+str(int(enabled and record['level2'])))
                    elif stage == 'mutant-mode': actual.append(ident+'.mode='+str(int(enabled and record['mutants'] > 0)))
                    elif stage == 'init-done': actual.append(ident+'.init='+str(int(enabled)))
                    elif stage == 'restore-saved-masks': actual.append(ident+'.restore')
                    elif stage.startswith('sync-pulse-'): actual.append(ident+'.sync='+str(int(stage.endswith('high'))))
                    elif stage.startswith('wsca-clock-'): actual.append(ident+'.wsca='+str(int(stage.endswith('high'))))
            assert actual == expected, (record,'Old SystemManager/native TCP stage order differs',expected,actual)
    assert hashlib.sha256(source.read_bytes()).hexdigest() == digest, 'Original source changed'
    print('PASS extracted original SystemManager:',len(names),'actual bodies; 3 successful TCP audits; full host stage/sync order matches; data links/coherence environment/physical effects excluded')


if __name__ == '__main__':
    main()
