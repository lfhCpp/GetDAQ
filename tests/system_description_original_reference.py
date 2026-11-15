#!/usr/bin/env python3
"""Extract received Describe/status methods and compare real successful TCP audits.

Only successful CoBo status writes are compared. Explicit replacement modes,
preflights, readback, durable audit and fail-on-error are intentional new behavior.
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
    index=Path(sys.argv[1]) if len(sys.argv)>1 else root/'build-ice-compat/system-description-audits.jsonl'
    folder=root/'source/get/GetBench-software-release-20210208/src/get/rc'
    sources=[folder/'SystemManager.cpp',folder/'CoBoNode.cpp']
    before={p:hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}
    system,cobo=(p.read_text() for p in sources)
    bodies='\n'.join(extract(system,'SystemManager::'+n) for n in ('describe','setCoBoInitDone','setCoBoMutantMode'))
    bodies+='\n'+'\n'.join(extract(cobo,'CoBoNode::'+n) for n in ('setInitDone','setMutantMode'))
    program=r'''
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#define LOG_INFO() std::ostringstream()
#define NODE_WARN() std::ostringstream()
namespace Ice { struct LocalException {std::string ice_name()const{return "stub";}}; }
namespace mdaq { namespace utl { struct CmdException:std::runtime_error {
    std::string reason; explicit CmdException(const std::string&s):std::runtime_error(s),reason(s){}
}; } }
namespace get { namespace rc {
std::vector<std::string> actions;
struct Control {
    std::string id; explicit Control(const std::string&s):id(s){}
    void writeField(const std::string&reg,const std::string&field,bool value) {
        actions.push_back(id+'.'+reg+'.'+field+'='+std::to_string(value));
    }
};
struct CoBoNode {
    bool enabled; Control control;
    CoBoNode(unsigned i,bool e):enabled(e),control("C"+std::to_string(i)){}
    bool isCoBoActive(){return enabled;} Control*ctrl(){return &control;}
    void setInitDone(bool); void setMutantMode(bool);
};
struct MutantNode{};
struct Node {struct List {template<class T>struct Iterator {
    std::vector<T>&v;std::size_t i=0;explicit Iterator(std::vector<T>&x):v(x){}
    bool hasNext()const{return i<v.size();}void operator++(){++i;}T*operator->(){return &v.at(i);}
};};};
template<class T>struct Nodes{std::vector<T>v;std::size_t size()const{return v.size();}};
struct SystemManager {
    Nodes<CoBoNode>c;Nodes<MutantNode>m;
    template<class T>Nodes<T>&getList();
    template<class T>typename Node::List::Iterator<T>iterate(){return typename Node::List::Iterator<T>(getList<T>().v);}
    void describe();void setCoBoInitDone(bool);void setCoBoMutantMode(bool);
};
template<>Nodes<CoBoNode>&SystemManager::getList<CoBoNode>(){return c;}
template<>Nodes<MutantNode>&SystemManager::getList<MutantNode>(){return m;}
'''+bodies+r'''
}}
int main() {
    using namespace get::rc;SystemManager s;
    s.c.v.emplace_back(0,true);s.c.v.emplace_back(1,false);s.m.v.emplace_back();s.describe();
    for(const auto&a:actions)std::cout<<a<<'\n';
}
'''
    records=[json.loads(row) for row in index.read_text().splitlines()]
    assert len(records)==6, 'Expected 2 encodings x 3 complete description policies'
    assert {(r['encoding_minor'],r['policy']) for r in records}=={(e,p) for e in (0,1) for p in (0,1,2)}
    with tempfile.TemporaryDirectory(prefix='getdaq-description-original-') as tmp:
        source=Path(tmp)/'original.cpp'; binary=Path(tmp)/'original'
        source.write_text(program)
        subprocess.run(['g++','-std=c++11','-pedantic-errors','-Wall','-Wextra',str(source),'-o',str(binary)],check=True)
        expected=subprocess.check_output([str(binary)],text=True).splitlines()
        for r in records:
            directory=Path(r['directory'])
            if not directory.is_absolute(): directory=index.parent/directory
            rows=[json.loads(row) for row in (directory/'system-description.jsonl').read_text().splitlines()]
            assert rows[-1]['phase']=='described'
            actual=[]
            for row in rows:
                if row.get('phase')!='node_begin' or row['stage'] not in ('initDone','mutantMode'): continue
                child=[json.loads(line) for line in (directory/row['audit_file']).read_text().splitlines()]
                assert child[-1]['phase']=='complete'
                for step in child[0]['steps']:
                    if step['operation']=='write':
                        assert step['device']=='ctrl' and step['register']=='mutantConfig'
                        actual.append(row['id']+'.'+step['register']+'.'+step['field']+'='+step['value'])
            assert actual==expected,(r,actual,expected)
    for p,digest in before.items(): assert hashlib.sha256(p.read_bytes()).hexdigest()==digest
    print('PASS 5 extracted received SystemManager/CoBoNode bodies vs 6 complete TCP Describe phases: all-initDone then all-mutantMode, inactive status preserved')

if __name__=='__main__':
    main()
