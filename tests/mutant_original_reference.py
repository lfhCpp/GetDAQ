#!/usr/bin/env python3
"""Extract actual GET MuTaNT functions into recording stubs for an independent oracle.

No Qt/Ice/Boost/CCfg installation, real hardware or edits to historical sources.
Additional binding, stopped-state, readback and failure checks are tested over TCP.
"""
import hashlib
from pathlib import Path
import re
import subprocess
import tempfile


def extract(text, name):
    start = text.index('void MutantNode::' + name + '(')
    opening = text.index('{', start)
    end, depth = opening + 1, 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end].replace('MutantNode::', 'Original::')


def main():
    root = Path(__file__).resolve().parents[1]
    directory = root / 'source/get/GetBench-software-release-20210208/src/get/rc'
    sources = [directory / name for name in ('MutantNode.cpp', 'MutantConfigParser.cpp')]
    digests = {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}
    text = sources[0].read_text()
    parser = sources[1].read_text()
    names = ['selectClockSource', 'setGlobalConfigurationRegister', 'selectReadoutMode', 'selectTestMode',
             'setTimestampAndEventCounters', 'enableScaler', 'enableScalers', 'clearEventCounters', 'clearScalers',
             'configureScalers', 'setSourceId', 'setFrameType', 'setClockShelfMasks', 'setShelfMask', 'setAmcTagOnly',
             'setUserRegister', 'configureLevel0Trigger', 'configureLevel1Trigger', 'configureLogicalInspectionSignals',
             'configureRegistersMutantA', 'configureRegistersMutantB', 'registerBeastBoards', 'setWscaClk2', 'setSyncPulse', 'runInit']
    bodies = [extract(text, name) for name in names]
    declarations = '\n'.join(body[:body.index('{')].replace('Original::', '').strip() + ';' for body in bodies)
    # Extract actual quantization statements, retaining float and constants from the parser.
    quantizers = []
    for variable, member, name in [('gateDelay_us', 'level0GateDelay_10ns', 'Delay'),
                                   ('gateDuration_us', 'level0GateDuration_10ns', 'Duration'),
                                   ('postTrigDelay_us', 'level1PostTrigDelay_10ns', 'PostDelay')]:
        start = parser.index('float ' + variable + ' =')
        end = parser.index(';', parser.index(member + ' =', start)) + 1
        block = parser[start:end]
        block = re.sub(r'cfg\([^;]+?\.getRealValue\(\)', 'requested', block)
        quantizers.append('unsigned Original'+name+'(double requested) { unsigned '+member+' = 0; '+block+' return '+member+'; }')
    timeout_expression = re.search(r'level1bTimeout_2560ns = ([^;]+);', parser).group(1)
    quantizers.append('unsigned OriginalTimeout(double requested) { float timeout_us = requested; return '+timeout_expression+'; }')
    program = r'''
#include "hardwareclient.h"
#include "mutantconfiguration.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace CCfg { struct CConfig { struct NotFound : std::runtime_error { using std::runtime_error::runtime_error; }; CConfig(int = 0) {} }; }
namespace mdaq { namespace utl { struct CmdException : std::runtime_error { std::string reason; explicit CmdException(const std::string &s) : std::runtime_error(s),reason(s) {} }; } }
using mdaq::utl::CmdException;
namespace utl { template<class T> struct BitFieldHelper {
    static T getField(T value,unsigned offset,unsigned width) { return (value>>offset)&((std::uint64_t(1)<<width)-1); }
    static bool getBit(T value,unsigned bit) { return (value>>bit)&1; }
    static void setField(T &value,unsigned offset,unsigned width,std::uint64_t field) { const T mask = ((std::uint64_t(1)<<width)-1)<<offset; value = (value&~mask)|((field<<offset)&mask); }
    static void setBit(T &value,unsigned bit,bool enabled) { setField(value,bit,1,enabled); }
}; }
template<class T,class V> T lexical_cast(V v) { std::ostringstream out; out << v; return out.str(); }
#define NODE_DEBUG() std::ostringstream()
#define NODE_WARN() std::ostringstream()
enum CsMode { CsDisabled, CENTRUM, Scalers };
std::vector<std::string> recorded;
std::map<std::string,std::uint64_t> values;
std::string Key(const std::string &d,const std::string &r,const std::string &f = "") { return d+"."+r+(f.empty() ? "" : "."+f); }
void Write(const std::string &d,const std::string &r,const std::string &f,std::uint64_t v) { auto k = Key(d,r,f); values[k] = v; recorded.push_back(k+"="+std::to_string(v)); }
struct Device {
    std::string name; explicit Device(const char *n) : name(n) {}
    void writeField(const std::string &r,const std::string &f,std::uint64_t v) { Write(name,r,f,v); }
    void writeRegister(const std::string &r,std::uint64_t v) { Write(name,r,"",v); }
    std::uint64_t readField(const std::string &r,const std::string &f) { return values[Key(name,r,f)]; }
};
struct RemoteRegisterProcessor { Device &d; explicit RemoteRegisterProcessor(Device &device) : d(device) {}
    RemoteRegisterProcessor &write(const std::string &r,std::uint64_t v) { d.writeRegister(r,v); return *this; }
};
struct Hw { RemoteRegisterProcessor proc; explicit Hw(Device &d) : proc(d) {} RemoteRegisterProcessor &regProc(const char *) { return proc; } };
struct Config {
    bool isSlave=false,isSlavePortA=true,vhdciPortAEnabled=true,vhdciPortBEnabled=true; unsigned slaveId=0;
    bool level0Enabled=true,level1Enabled=true,level2Enabled=true,hitPatternModified=true,l0l1=false,tpdModeEnabled=false,halfEventReadoutEnabled=false;
    unsigned level1Selection=0,level0GateDelay_10ns=0,level0GateDuration_10ns=1,level1PostTrigDelay_10ns=0,level1bTimeout_2560ns=1;
    unsigned level1aLowThreshold=1,level1aHighThreshold=65535,level1bLowThreshold=0,level1bHighThreshold=65535;
    unsigned level1aLowNumBuckets=511,level1aHighNumBuckets=511,level1bLowNumBuckets=511,level1bHighNumBuckets=511;
    CsMode csMode=CsDisabled; bool testModeEnabled=false,readoutModeEnabled=true,clearScalersEnabled=true,clearCountersEnabled=true,preLoadTimestamp=true,preLoadEventNumber=true;
    std::map<size_t,bool> enableScalers; std::uint64_t timestamp_10ns=0,eventNumber=0;
    unsigned sourceId=0,frameType=0,amcTagOnly=0,userReg=0,frontPanelInsp1=0,frontPanelInsp2=0,rearPanelInsp1=0,rearPanelInsp2=0;
    bool externalGmcClockEnabled=false;
};
struct Original {
    Config config; size_t mutantModuleCount_=1; std::uint16_t fMaskGmc_=11,fMaskWsca_=7;
    Device card{"CARD_B_EMC"},gpio{"GPIO_CLK_TREE"}; Hw hardware{card};
    Device *device(const char *n) { return std::string(n) == "CARD_B_EMC" ? &card : &gpio; }
    Hw &hwNode() { return hardware; } unsigned moduleId_ = 0;
    void checkStatus() {} void checkAmcStatus() {}
'''+declarations+'\n};\n'+'\n'.join(bodies)+'\n'+'\n'.join(quantizers)+r'''
void Bind(Original &o,const GetDAQ::MutantSettings &s) {
    auto &c=o.config; o.mutantModuleCount_=s.moduleCount; o.fMaskGmc_=s.gmcShelfMask; o.fMaskWsca_=s.wscaShelfMask;
    c.isSlave=s.isSlave; c.isSlavePortA=s.slavePortA; c.slaveId=s.isSlave ? s.slaveId : 0;
    c.vhdciPortAEnabled=s.isSlave ? s.slavePortA : s.masterPortAEnabled; c.vhdciPortBEnabled=s.isSlave ? !s.slavePortA : s.masterPortBEnabled;
    c.level0Enabled=s.level0Enabled; c.level1Enabled=s.level1Enabled; c.level2Enabled=s.level2Enabled; c.hitPatternModified=s.hitPatternModified;
    c.l0l1=s.level1BeforeLevel0; c.tpdModeEnabled=s.twoProtonMode; c.halfEventReadoutEnabled=s.twoProtonMode && s.halfEventReadout;
    const char *selections[]={"LOW","HIGH","OR","AND"}; for (unsigned i=0;i<4;++i) if (s.level1Selection==selections[i]) c.level1Selection=i;
    c.csMode=static_cast<CsMode>(s.centrumScalerMode); c.readoutModeEnabled=s.readoutEnabled; c.testModeEnabled=s.testMode;
    c.clearCountersEnabled=s.clearCounters; c.clearScalersEnabled=s.clearScalers; c.preLoadTimestamp=s.preloadTimestamp; c.preLoadEventNumber=s.preloadEventNumber;
    c.timestamp_10ns=s.timestamp10ns; c.eventNumber=s.eventNumber;
    for (unsigned i=0;i<5;++i) c.enableScalers[i+1]=s.scalerEnabled[i];
    c.level0GateDelay_10ns=OriginalDelay(s.level0GateDelayUs); c.level0GateDuration_10ns=OriginalDuration(s.level0GateDurationUs);
    c.level1PostTrigDelay_10ns=OriginalPostDelay(s.level1PostTriggerDelayUs); c.level1bTimeout_2560ns=s.twoProtonMode ? OriginalTimeout(s.level1bTimeoutUs) : 1;
    c.level1aLowThreshold=s.multiplicityThresholds[0]; c.level1aHighThreshold=s.multiplicityThresholds[1];
    c.level1aLowNumBuckets=s.bucketCounts[0]; c.level1aHighNumBuckets=s.bucketCounts[1];
    if (s.twoProtonMode) { c.level1bLowThreshold=s.multiplicityThresholds[2]; c.level1bHighThreshold=s.multiplicityThresholds[3]; c.level1bLowNumBuckets=s.bucketCounts[2]; c.level1bHighNumBuckets=s.bucketCounts[3]; }
    c.sourceId=s.sourceId; c.frameType=s.frameType; c.amcTagOnly=s.amcTagOnly; c.userReg=s.userRegister;
    c.frontPanelInsp1=s.inspection[0]; c.frontPanelInsp2=s.inspection[1]; c.rearPanelInsp1=s.inspection[2]; c.rearPanelInsp2=s.inspection[3];
    c.externalGmcClockEnabled=s.isSlave || s.externalClockEnabled;
}
void Reset(const GetDAQ::MutantSettings &s) { recorded.clear(); values.clear(); values[Key("GPIO_CLK_TREE","CLOCK_TREE_CONFIG2","clk_path_sel")]=0x1b2;
    values[Key("GPIO_CLK_TREE","CLOCK_TREE_CONFIG1","amc_clk1_out_en")]=s.gmcShelfMask;
    values[Key("GPIO_CLK_TREE","CLOCK_TREE_CONFIG1","amc_clk2_out_en")]=s.wscaShelfMask;
}
std::vector<std::string> Projection(const std::vector<GetDAQ::HardwareStep> &steps,const GetDAQ::MutantSettings &s) {
    Reset(s);
    for (const auto &step : steps) {
        if (step.kind == GetDAQ::HardwareStep::WriteValue) Write(step.device,step.reg,step.field,step.value);
        else if (step.kind == GetDAQ::HardwareStep::MaskedWriteValue) Write(step.device,step.reg,step.field,(values[Key(step.device,step.reg,step.field)]&~step.mask)|step.value);
    }
    return recorded;
}
void Compare(const std::vector<std::string> &old,const std::vector<std::string> &current) {
    if (old == current) return;
    for (size_t i=0;i<std::min(old.size(),current.size());++i) if (old[i]!=current[i]) { std::cerr << i << ": old " << old[i] << "; new " << current[i] << '\n'; break; }
    throw std::runtime_error("Extracted original MuTaNT write order/value mismatch");
}
int main() {
    try {
        unsigned globalCases=0;
        for (unsigned flags=0;flags<512;++flags) for (unsigned selection=0;selection<4;++selection) for (unsigned cs=0;cs<3;++cs) {
            GetDAQ::MutantSettings s; s.level0Enabled=flags&1; s.level1Enabled=flags&2; s.level2Enabled=flags&4;
            s.hitPatternModified=flags&8; s.level1BeforeLevel0=flags&16; s.twoProtonMode=flags&32; s.halfEventReadout=flags&64;
            s.masterPortAEnabled=flags&128; s.masterPortBEnabled=flags&256;
            const char *selections[]={"LOW","HIGH","OR","AND"}; s.level1Selection=selections[selection]; s.centrumScalerMode=static_cast<GetDAQ::MutantSettings::CentrumScalerMode>(cs);
            Original old; Bind(old,s); Reset(s); old.setGlobalConfigurationRegister();
            if (values[Key("CARD_B_EMC","GLOBAL_CONF")]!=GetDAQ::CompileMutant(s).globalConfiguration) throw std::runtime_error("Original GLOBAL_CONF differs");
            ++globalCases;
        }
        for (unsigned raw=0;raw<65536;++raw) {
            double us=raw/100.0;
            if (OriginalDelay(us)!=GetDAQ::MutantTenNsCode(us) || OriginalPostDelay(us)!=GetDAQ::MutantTenNsCode(us) || (raw && OriginalDuration(us)!=GetDAQ::MutantTenNsCode(us,true)) || OriginalTimeout(raw*2.56)!=GetDAQ::MutantL1bTimeoutCode(raw*2.56)) throw std::runtime_error("Original timing expression differs");
        }
        for (unsigned flags=0;flags<64;++flags) {
            GetDAQ::MutantSettings s; s.moduleCount=2; s.gmcShelfMask=11; s.wscaShelfMask=7; s.amcTagOnly=128;
            s.clearScalers=flags&1; s.clearCounters=flags&2; s.preloadTimestamp=flags&4; s.preloadEventNumber=flags&8; s.twoProtonMode=flags&16; s.halfEventReadout=s.twoProtonMode; s.readoutEnabled=flags&32;
            s.externalClockEnabled=true; s.timestamp10ns=0x123456789abcULL; s.eventNumber=0x76543210; s.inspection={{0x12,0x34,0x56,0x78}}; s.scalerEnabled={{true,false,true,false,true}};
            s.multiplicityThresholds={{2,34,56,78}}; s.bucketCounts={{12,34,56,78}}; s.level1bTimeoutUs=5.12;
            Original old; Bind(old,s); Reset(s); old.configureRegistersMutantA(0); old.configureRegistersMutantB(0); old.registerBeastBoards(0);
            const auto configuration=recorded; Compare(configuration,Projection(GetDAQ::MutantConfigure(s),s));
            Reset(s); old.runInit(); const auto init=recorded; Compare(init,Projection(GetDAQ::MutantRunInit(s),s));
            for (bool enabled : {false,true}) {
                Reset(s); old.setWscaClk2(enabled); const auto wsca=recorded; Compare(wsca,Projection(GetDAQ::MutantWscaClock(enabled),s));
                Reset(s); old.setSyncPulse(enabled); const auto sync=recorded; Compare(sync,Projection(GetDAQ::MutantSyncPulse(enabled),s));
            }
        }
        std::cout << "PASS extracted original MuTaNT: " << globalCases << " global combinations, 65536 timing codes, 64 configure/run/WSCA/sync write-order cases\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
'''
    with tempfile.TemporaryDirectory(prefix='getdaq-mutant-original-') as temporary:
        folder = Path(temporary)
        program_path = folder / 'reference.cpp'; program_path.write_text(program)
        binary = folder / 'reference'
        subprocess.run(['g++', '-std=c++11', '-pedantic-errors', '-Wall', '-Wextra', '-I'+str(root/'apps/hardware'),
                        '-I'+str(root/'common/experiment'), str(program_path), str(root/'apps/hardware/mutant.cpp'),
                        str(root/'common/experiment/mutantconfiguration.cpp'), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
    assert all(hashlib.sha256(p.read_bytes()).hexdigest() == digest for p, digest in digests.items())


if __name__ == '__main__':
    main()
