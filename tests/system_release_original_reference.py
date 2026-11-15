#!/usr/bin/env python3
"""Compare release audits against nine extracted received GET host functions.

The recording peripherals do not use new planners to generate expectations.
Only successful command order is compared. New metadata checks/readback,
failure propagation, owned subscription IDs, and the global power-off-before-
unsubscribe boundary intentionally differ from the received implementation.
Legacy disconnects for nodes without an owned sender are explicitly excluded.
"""
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path
from alignment_original_reference import extract


def rows(path):
    return [json.loads(line) for line in path.read_text().splitlines()]


def main():
    root = Path(__file__).resolve().parents[1]
    indices = [Path(p) for p in sys.argv[1:]] if len(sys.argv)>1 else [root / ('build-ice-compat/system-release-normal-'+str(e)+'-audits.jsonl') for e in (0,1)]
    folder = root / 'source/get/GetBench-software-release-20210208/src/get/rc'
    names = {
        'SystemManager': ('breakup', 'unprepare', 'setCoBoInitDone', 'setCoBoMutantMode'),
        'CoBoNode': ('impl_unprepare', 'selectAsAdForRead', 'setInitDone', 'setMutantMode'),
        'Node': ('daqDisconnect',),
    }
    before = {}
    bodies = []
    for cls, functions in names.items():
        source = folder / (cls + '.cpp')
        data = source.read_bytes()
        before[source] = hashlib.sha256(data).hexdigest()
        bodies.extend(extract(data.decode(), cls + '::' + name) for name in functions)
    program = r'''
#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using std::size_t;
#define LOG_WARN() std::ostringstream()
#define NODE_WARN() std::ostringstream()
#define LOG_INFO() std::ostringstream()
namespace Ice {
struct LocalException { std::string ice_name() const { return "stub"; } };
struct UserException { std::string ice_name() const { return "stub"; } };
}
namespace mdaq { namespace utl { struct CmdException:std::runtime_error {
    std::string reason; explicit CmdException(const std::string&s):std::runtime_error(s),reason(s){}
}; } }
namespace utl { template<class T>struct BitFieldHelper {
    static bool getBit(T v,size_t n) { return (v&(T(1)<<n))!=0; }
}; }
namespace CCfg {
struct Object { explicit Object(const char*){} };
struct CConfig { explicit CConfig(Object*){} };
}
namespace get { namespace asad { namespace utl {
std::string stringifyMonitoringAlertStatus(uint16_t) { return "raw"; }
} } }
namespace get { namespace rc {
using CmdException=mdaq::utl::CmdException;
std::vector<std::string> actions;
struct State { unsigned mask=0,selected=0,status=0; };
struct Control {
    std::string id,device;State&state;
    Control(const std::string&i,const char*d,State&s):id(i),device(d),state(s){}
    void writeField(const std::string&reg,const std::string&field,uint64_t value) {
        actions.push_back(id+'.'+device+'.'+reg+'.'+field+'='+std::to_string(value));
        if(reg=="asadSelect")state.selected=unsigned(value);
    }
    uint32_t readRegister(const std::string&reg) {
        unsigned value=reg=="asadEnable"?state.mask:state.status;
        actions.push_back(id+'.'+device+'.'+reg+'?'+std::to_string(value));return value;
    }
    void writeRegister(const std::string&reg,uint32_t value) {
        actions.push_back(id+'.'+device+'.'+reg+'='+std::to_string(value));state.mask=value;
    }
};
struct HwNode { std::string id; explicit HwNode(const std::string&i):id(i){}
    void disconnectRunProcessor() { actions.push_back(id+".disconnect"); }
};
namespace mt {
struct AlarmService {std::string id;explicit AlarmService(const std::string&i):id(i){}
    void unsubscribe(const std::string&) { actions.push_back(id+".unsubscribe"); }
};
using AlarmServicePrx=AlarmService*;
}
struct CtrlProxy {mt::AlarmService service;explicit CtrlProxy(const std::string&i):service(i){}
    mt::AlarmServicePrx getAlarmService() { return &service; }
};
struct Node {
    HwNode hardware;explicit Node(const std::string&i):hardware(i){}
    HwNode&hwNode(){return hardware;}void daqDisconnect();
    struct List {template<class T>struct Iterator {
        std::vector<T>&v;size_t i=0;explicit Iterator(std::vector<T>&x):v(x){}
        bool hasNext()const{return i<v.size();}void operator++(){++i;}T*operator->(){return &v.at(i);}
    };};
};
struct CoBoNode:Node {
    bool enabled;State state;Control control,asadControl;CtrlProxy proxy;
    CoBoNode(unsigned i,bool e,unsigned mask,unsigned status):Node("C"+std::to_string(i)),enabled(e),
        control(hardware.id,"ctrl",state),asadControl(hardware.id,"asad",state),proxy(hardware.id) {
        state.mask=mask;state.status=status;
    }
    bool isCoBoActive(){return enabled;}Control*ctrl(){return &control;}Control*asad(){return &asadControl;}
    CtrlProxy*ctrlPrx(){return &proxy;}void setInitDone(bool);void setMutantMode(bool);
    void impl_unprepare();void selectAsAdForRead(size_t);
};
struct MutantNode:Node {explicit MutantNode(unsigned i):Node("M"+std::to_string(i)){};};
template<class T>struct Nodes {std::vector<T>v;
    void doLoop_unprepare(CCfg::CConfig){for(auto&n:v)n.impl_unprepare();}
};
struct SystemManager {
    Nodes<CoBoNode>c;Nodes<MutantNode>m;template<class T>Nodes<T>&getList();
    template<class T>typename Node::List::Iterator<T>iterate(){return typename Node::List::Iterator<T>(getList<T>().v);}
    void breakup();void unprepare();void setCoBoInitDone(bool);void setCoBoMutantMode(bool);
};
template<>Nodes<CoBoNode>&SystemManager::getList<CoBoNode>(){return c;}
template<>Nodes<MutantNode>&SystemManager::getList<MutantNode>(){return m;}
''' + '\n'.join(bodies) + r'''
}}
int main(int argc,char**argv) {
    if(argc!=5)return 2;
    using namespace get::rc;SystemManager s;
    // Reserving avoids moving self-referencing recording peripheral stubs.
    s.c.v.reserve(3);s.m.v.reserve(3);
    for(unsigned i=0;i<3;++i){s.c.v.emplace_back(i,i<2,std::stoul(argv[i+2]),i==0?8:0);s.m.v.emplace_back(i);}
    if(std::string(argv[1])=="breakup")s.breakup();else s.unprepare();
    for(const auto&a:actions)std::cout<<a<<'\n';
}
'''
    indexed_records = [(record,index) for index in indices for record in rows(index)]
    records = [record for record,_ in indexed_records]
    assert len(records) == 16, 'Expected 12 Breakup and 4 Unprepare successes in two encodings'
    assert {(r['encoding_minor'], r['from']) for r in records if r['phase'] == 'breakup'} == {
        (e, p) for e in (0, 1) for p in ('ready', 'running', 'paused', 'stopped', 'closed', 'alarm-running')}
    assert {(r['encoding_minor'], r['from']) for r in records if r['phase'] == 'unprepare'} == {
        (e, p) for e in (0, 1) for p in ('', 'after-breakup')}
    comparisons = 0
    with tempfile.TemporaryDirectory(prefix='getdaq-release-original-') as tmp:
        source, binary = Path(tmp) / 'original.cpp', Path(tmp) / 'original'
        source.write_text(program)
        subprocess.run(['g++', '-std=c++11', '-pedantic-errors', '-Wall', '-Wextra', str(source), '-o', str(binary)], check=True)
        for r,index in indexed_records:
            directory = Path(r['directory'])
            if not directory.is_absolute():
                directory = index.parent / directory
            if r['phase'] == 'breakup':
                expected = subprocess.check_output([str(binary), 'breakup', '0', '0', '0'], text=True).splitlines()
                # The modern run object owns exactly the connected sender set.
                expected = [s for s in expected if s not in ('C2.disconnect', 'M2.disconnect')]
                if r['from'] == 'closed':
                    expected = [s for s in expected if not s.endswith('.disconnect')]
                run = rows(directory / 'system-run.jsonl')
                assert run[-1]['phase'] == 'complete' and run[-1]['stage'] == 'broken-prepare-snapshot-retained'
                actual = []
                link_row = next(row for row in run if row['phase'] == 'all_sender_disconnect_confirmed')
                link_path = Path(r.get('data_links_audit_directory', link_row['data_links_audit_directory']))
                if not link_path.is_absolute():
                    link_path = index.parent / link_path
                link_rows = rows(link_path / 'data-links.jsonl')
                assert link_rows[-1]['phase'] == 'disconnected'
                for row in link_rows if r['from'] != 'closed' else []:
                    if row.get('phase') == 'sender_begin' and row['stage'] == 'disconnect':
                        child = rows(link_path / row['audit_file'])
                        assert child[-1]['phase'] == 'complete'
                        actual.append(row['sender_id'] + '.disconnect')
                release = directory / 'configuration'
                root_rows = rows(release / 'system-release.jsonl')
                assert root_rows[-1]['phase'] == 'complete'
                for row in root_rows:
                    if row.get('phase') != 'step_begin':
                        continue
                    child = rows(release / row['audit_file'])
                    assert child[-1]['phase'] == 'complete'
                    for step in child[0]['steps']:
                        if step['operation'] == 'write':
                            actual.append(row['id'] + '.' + step['device'] + '.' + step['register'] + '.' + step['field'] + '=' + step['value'])
                assert actual == expected, (r, actual, expected)
                comparisons += 1
            else:
                masks = (9, 9, 0) if r['from'] == 'after-breakup' else (11, 0, 2)
                expected = subprocess.check_output([str(binary), 'unprepare'] + list(map(str, masks)), text=True).splitlines()
                # Raw status does not have to be zero. The chain fixture uses 0;
                # the standalone fixture deliberately observes auto-protection 8.
                if r['from'] == 'after-breakup':
                    expected = [s.replace('interruptStatus1?8', 'interruptStatus1?0') for s in expected]
                expected = [s for s in expected if not s.endswith('.unsubscribe')]
                root_rows = rows(directory / 'system-release.jsonl')
                assert root_rows[-1]['phase'] == 'complete' and root_rows[-1]['receiver_sealed'] is False
                actual = []
                off_boundary = next(i for i, row in enumerate(root_rows) if row['phase'] == 'all_power_off_confirmed')
                for pos, row in enumerate(root_rows):
                    if row.get('phase') != 'step_begin':
                        continue
                    child = rows(directory / row['audit_file'])
                    if row['stage'] == 'unsubscribe-owned-alarm':
                        assert pos > off_boundary and child[-1]['phase'] == 'acknowledged'
                        continue
                    assert child[-1]['phase'] == 'complete'
                    for step_no, step in enumerate(child[0]['steps']):
                        evidence = [q for q in child[1:] if q.get('step') == step_no]
                        if any(q['phase'] == 'skipped' for q in evidence):
                            continue
                        target = row['id'] + '.' + step['device'] + '.' + step['register']
                        if step['operation'] == 'write':
                            assert any(q['phase'] == 'acknowledged' for q in evidence)
                            actual.append(target + ('.' + step['field'] if step['field'] else '') + '=' + step['value'])
                        elif step['operation'] == 'read':
                            observations = [q['actual'] for q in evidence if q['phase'] == 'observed']
                            assert len(observations) == 1
                            actual.append(target + '?' + observations[0])
                assert actual == expected, (r, actual, expected)
                comparisons += 1
    for p, digest in before.items():
        assert hashlib.sha256(p.read_bytes()).hexdigest() == digest
    print('PASS 9 extracted received SystemManager/CoBoNode/Node bodies vs ' + str(comparisons) +
          ' complete TCP release phases: owned disconnect before all status clears; actual powered mask monitoring before power-off; global off before owned unsubscribe')


if __name__ == '__main__':
    main()
