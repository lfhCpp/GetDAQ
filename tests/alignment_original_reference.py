#!/usr/bin/env python3
"""Actual old host function bodies versus new coordinator's loopback TCP actions.

Stubs replace the old environment only; they do not call new planning functions.
No CCfg/Boost/Ice dependency in the independently compiled reference executable.
"""
import hashlib
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path


def extract(text, qualified, index=0):
    matches = list(re.finditer(r'^.*\b' + re.escape(qualified) + r'\([^\n]*\)', text, re.MULTILINE))
    match = matches[index]
    opening = text.index('{', match.end())
    end, depth = opening + 1, 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[match.start():end]


def main():
    root = Path(__file__).resolve().parents[1]
    fixture = Path(sys.argv[1]).resolve()
    directory = root / 'source/get/GetBench-software-release-20210208/src/get/rc'
    sources = [directory / 'AmcSlotManager.cpp',directory / 'SystemManager.cpp']
    digests = {p:hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}
    amc, system = (p.read_text() for p in sources)
    bodies = [extract(amc,'AmcSlotManager::'+name) for name in
              ('buildSlotUID','buildSlotUIDString','setCoBoMutantLink','findSlotOfCoBo','findOccupiedSlots')]
    bodies += [extract(amc,'AmcSlotManager::measureDelays',i) for i in range(2)]
    bodies += [extract(system,'SystemManager::setWscaClk2')]
    bodies += [extract(system,'SystemManager::alignDelaysBetweenChassis',i) for i in range(2)]
    program = r'''
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
using std::size_t;
#define LOG_DEBUG() std::ostringstream()
#define LOG_INFO() std::ostringstream()
namespace mdaq { namespace utl { struct CmdException : std::runtime_error { explicit CmdException(const std::string &s):std::runtime_error(s){} }; } }
using mdaq::utl::CmdException;
namespace get {
namespace cobo { enum MutantLinkMode { Normal,Disconnected,GMC,WSCA }; }
namespace mutant {
enum Clock { GMC,WSCA };
struct SlotAlignmentResult { bool isAmcPresent = false; int amcDeltaDelay = 100, amcProgDelay = 7; };
typedef std::map<short,SlotAlignmentResult> AlignmentResults;
struct MultiShelveAlignmentResults { int measuredDelay=0,programmedDelay=0; std::string comment; };
struct AlignmentAlarm : std::runtime_error { std::string reason; explicit AlignmentAlarm(const std::string &s):std::runtime_error(s),reason(s){} };
}
namespace rc {
struct NodeId {
    std::string instanceId;
    explicit NodeId(const std::string &s=""):instanceId(s){}
    std::string toString() const {return instanceId;}
    bool operator<(const NodeId &o) const {return instanceId<o.instanceId;}
    bool operator==(const NodeId &o) const {return instanceId==o.instanceId;}
    bool operator!=(const NodeId &o) const {return !(*this==o);}
};
std::vector<std::string> actions;
unsigned modes[4]={0,0,0,0}, masks[3][2]={{0,0},{0,0},{0,0}};
struct Link {
    unsigned index; explicit Link(unsigned i):index(i){}
    void setupMutantLink(cobo::MutantLinkMode mode) {modes[index]=unsigned(mode);actions.push_back("C"+std::to_string(index)+".link="+std::to_string(unsigned(mode)));}
};
struct Alignment {
    unsigned index; explicit Alignment(unsigned i):index(i){}
    void alignDelayTDC(mutant::Clock clock,bool,mutant::AlignmentResults &results) {
        actions.push_back("M"+std::to_string(index)+".tdc="+std::to_string(unsigned(clock)));
        const unsigned module[]={0,1,2,1}, slot[]={0,11,5,10}; masks[index][unsigned(clock)]=0;
        for (unsigned s=0;s<12;++s) {
            mutant::SlotAlignmentResult r;
            for (unsigned c=0;c<4;++c) if(module[c]==index && slot[c]==s && modes[c]==unsigned(clock)+2) r.isAmcPresent=true;
            if(r.isAmcPresent)masks[index][unsigned(clock)]|=1u<<s;
            results[short(s)]=r;
        }
    }
    void step(unsigned s,mutant::Clock c,mutant::MultiShelveAlignmentResults &) {actions.push_back("M"+std::to_string(index)+".multi="+std::to_string(unsigned(c))+":"+std::to_string(s));masks[index][0]=masks[index][1]=4095;}
    void multiShelveAlignDelayStep1(mutant::Clock c,bool,mutant::MultiShelveAlignmentResults &r){step(1,c,r);}
    void multiShelveAlignDelayStep2(mutant::Clock c,bool,mutant::MultiShelveAlignmentResults &r){step(2,c,r);}
    void multiShelveAlignDelayStep3(mutant::Clock c,bool,mutant::MultiShelveAlignmentResults &r){step(3,c,r);}
};
struct CoBoNode {
    NodeId identity; bool enabled; Link link;
    CoBoNode(unsigned i,bool active):identity("C"+std::to_string(i)),enabled(active),link(i){}
    NodeId id() const{return identity;}
    bool isEnabled() const{return enabled;}
    Link *mutantLinkManager(){return &link;}
};
struct MutantNode {
    NodeId identity; unsigned moduleId_; Alignment alignment; unsigned saved[2]={0,0};
    explicit MutantNode(unsigned i):identity("M"+std::to_string(i)),moduleId_(i),alignment(i){}
    NodeId id() const{return identity;}
    Alignment *alignmentServer(){return &alignment;}
    void storeClockShelfMasks(){saved[0]=masks[moduleId_][0];saved[1]=masks[moduleId_][1];}
    void setWscaClk2(bool value){actions.push_back(identity.instanceId+".wsca="+std::to_string(value));}
};
struct Node {
    struct List {
        template<class T> struct Iterator {
            std::vector<T*> nodes; size_t at=0;
            explicit Iterator(const std::vector<T*> &v):nodes(v){}
            bool hasNext()const{return at<nodes.size();}
            Iterator &operator++(){++at;return *this;}
            T *operator->(){return nodes.at(at);}
            NodeId id()const{return nodes.at(at)->id();}
        };
    };
    struct ListList {
        std::vector<CoBoNode*> cobos; std::vector<MutantNode*> mutants;
        template<class T> std::vector<T*> getList();
        template<class T> typename List::Iterator<T> iterate(){return typename List::Iterator<T>(getList<T>());}
        template<class T> T &getNode(const std::string &id){for(auto p:getList<T>())if(p->id().instanceId==id)return *p;throw CmdException("missing node");}
    };
};
template<> std::vector<CoBoNode*> Node::ListList::getList<CoBoNode>(){return cobos;}
template<> std::vector<MutantNode*> Node::ListList::getList<MutantNode>(){return mutants;}
class AmcSlotManager {
public:
    typedef std::pair<NodeId,size_t> SlotUID;
    typedef std::map<SlotUID,mutant::SlotAlignmentResult> AlignmentResults;
    Node::ListList &nodeLists;
    explicit AmcSlotManager(Node::ListList &l):nodeLists(l){}
    static SlotUID buildSlotUID(const NodeId &,const size_t &);
    static std::string buildSlotUIDString(const NodeId &,const size_t &);
    void setCoBoMutantLink(const cobo::MutantLinkMode &,bool);
    SlotUID findSlotOfCoBo(const std::string &,const mutant::Clock &);
    void findOccupiedSlots(std::map<std::string,SlotUID> &);
    void measureDelays(); void measureDelays(const mutant::Clock &,AlignmentResults &);
};
class SystemManager : public Node::ListList {
public:
    typedef std::vector<MutantNode*> MutantSeq;
    MutantSeq mutantSlaves(){return MutantSeq(mutants.begin()+1,mutants.end());}
    MutantNode &mutantMaster(){return *mutants.front();}
    void setWscaClk2(bool);
    void alignDelaysBetweenChassis();
    void alignDelaysBetweenChassis(MutantNode &,get::mutant::Clock,size_t);
};
'''+ '\n'.join(bodies)+r'''
}}
int main(int argc,char **argv){
    if(argc!=3)return 2;
    using namespace get::rc; const unsigned count=unsigned(std::stoul(argv[1])),active=unsigned(std::stoul(argv[2]));
    SystemManager system;std::vector<std::unique_ptr<CoBoNode>> cobos;std::vector<std::unique_ptr<MutantNode>> mutants;
    for(unsigned i=0;i<count;++i){mutants.emplace_back(new MutantNode(i));system.mutants.push_back(mutants.back().get());}
    for(unsigned i=0;i<4;++i){cobos.emplace_back(new CoBoNode(i,(active>>i)&1));system.cobos.push_back(cobos.back().get());}
    AmcSlotManager manager(system);std::map<std::string,AmcSlotManager::SlotUID> slots;
    manager.findOccupiedSlots(slots);manager.measureDelays();system.alignDelaysBetweenChassis();system.setWscaClk2(true);
    std::cout<<"{\"actions\":[";for(size_t i=0;i<actions.size();++i){if(i)std::cout<<',';std::cout<<'"'<<actions[i]<<'"';}
    std::cout<<"],\"located\":[";bool first=true;for(const auto &s:slots){if(!first)std::cout<<',';first=false;std::cout<<"[\""<<s.first<<"\",\""<<s.second.first.instanceId<<"\","<<s.second.second<<']';}
    std::cout<<"],\"masks\":[";for(unsigned i=0;i<count;++i){if(i)std::cout<<',';std::cout<<'['<<mutants[i]->saved[0]<<','<<mutants[i]->saved[1]<<']';}std::cout<<"]}\n";return 0;
}
'''
    with tempfile.TemporaryDirectory(prefix='getdaq-alignment-original-') as temporary:
        folder = Path(temporary); source = folder/'reference.cpp'; source.write_text(program)
        reference = folder/'reference'
        subprocess.run(['g++','-std=c++11','-pedantic-errors','-Wall','-Wextra',str(source),'-o',str(reference)],check=True)
        cases = 0
        for modules in range(1,4):
            for active in range(1<<modules):
                original = json.loads(subprocess.check_output([str(reference),str(modules),str(active)],text=True))
                output = folder/('new-'+str(cases)+'.json')
                subprocess.run([str(fixture),str(root/'tests/data/mutant-hardware.json'),str(root/'tests/data/cobo-hardware.json'),
                                '--record',str(output),str(modules),str(active)],check=True,timeout=45)
                current = json.loads(output.read_text())
                assert current == original, (modules,active,'Independent old host actions/mappings/masks differ')
                cases += 1
        for p,digest in digests.items(): assert hashlib.sha256(p.read_bytes()).hexdigest()==digest
    print('PASS extracted original alignment:',len(bodies),'actual host functions,',cases,'single/two/three-crate and active-board combinations; TCP actions/AMC mappings/saved masks match; no physical phase claim')


if __name__ == '__main__':
    main()
