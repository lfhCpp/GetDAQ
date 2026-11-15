#!/usr/bin/env python3
"""Extract actual old impl_prepare; compare orchestration with compiled native stages.

Only the surrounding services/configuration are recording stubs. This verifies
host order, not the old backend, physical timing, or new fault recovery.
"""
from pathlib import Path
import hashlib
import subprocess
import tempfile


def main():
    root = Path(__file__).resolve().parents[1]
    old = root / 'source/get/GetBench-software-release-20210208/src/get/rc/CoBoNode.cpp'
    digest = hashlib.sha256(old.read_bytes()).hexdigest()
    text = old.read_text()
    start = text.index('void CoBoNode::impl_prepare(CCfg::CConfig nodeCfg)')
    opening = text.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    actual = text[start:end].replace('CoBoNode::', 'Original::', 1)
    program = r'''
#include "coboprepareclient.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>
namespace CCfg { typedef int CConfig; struct Exception : std::runtime_error { using std::runtime_error::runtime_error; }; }
namespace mdaq { namespace utl { struct CmdException : std::runtime_error { using std::runtime_error::runtime_error; }; } }
#define NODE_INFO() std::ostringstream()
struct Id { std::string toString() const { return "CoBo0"; } };
std::vector<std::string> recorded;
struct Original {
    enum Mode { DDR };
    bool active = true;
    Id id() const { return Id(); }
    bool isCoBoActive() const { return active; }
    void setAgetMask(CCfg::CConfig) {} // pure logical configuration, no hardware command
    void setInitDone(bool b) { if (b) throw std::runtime_error("Unexpected initDone true"); recorded.push_back("setInitDone"); }
    void checkReleaseNumbers() { recorded.push_back("checkReleaseNumbers"); }
    void pipeInit() { recorded.push_back("pipeInit"); }
    void setLemoModes(CCfg::CConfig) { recorded.push_back("setLemoModes"); }
    void triggerInit() { recorded.push_back("triggerInit"); }
    void circularBufferInit() { recorded.push_back("circularBufferInit"); }
    void asadInit(CCfg::CConfig,int ms) { if (ms != 10) throw std::runtime_error("Old power delay changed"); recorded.push_back("asadInit"); }
    void clockInit(CCfg::CConfig) { recorded.push_back("clockInit"); }
    void setReadDataDelay(CCfg::CConfig) { recorded.push_back("setReadDataDelay"); }
    void asadAgetInputManagerInit(CCfg::CConfig) { recorded.push_back("asadAgetInputManagerInit"); }
    void agetInit(CCfg::CConfig) { recorded.push_back("agetInit"); }
    void asadAdcInit(Mode) { recorded.push_back("asadAdcInit"); }
    void asadDacInit() { recorded.push_back("asadDacInit"); }
    void asadAlertsInit(CCfg::CConfig) { recorded.push_back("asadAlertsInit"); }
    void subscribeToCoBoAlarms(CCfg::CConfig) { recorded.push_back("subscribeToCoBoAlarms"); }
    void asadMonitoringRead() { recorded.push_back("asadMonitoringRead"); }
    void setGlobalRegThresholds(unsigned int v) { if (v != 2) throw std::runtime_error("Old threshold changed"); recorded.push_back("setGlobalRegThresholds"); }
    void calibrateAutoReadDataDelay(CCfg::CConfig) { recorded.push_back("calibrateAutoReadDataDelay"); }
    void selectReadDataDelay(CCfg::CConfig) { recorded.push_back("selectReadDataDelay"); }
    void impl_prepare(CCfg::CConfig);
};
''' + actual + r'''
std::vector<std::string> Stages(const std::vector<GetDAQ::HardwareStep> &plan) {
    std::vector<std::string> result;
    const std::string prefix = "prepare_stage=";
    for (const auto &step : plan) if (step.annotation.find(prefix) == 0) result.push_back(step.annotation.substr(prefix.size(),step.annotation.find(';')-prefix.size()));
    return result;
}
int main() {
    try {
        Original old; old.impl_prepare(0);
        const std::vector<std::string> foundation{"setInitDone","checkReleaseNumbers","pipeInit","setLemoModes","triggerInit","circularBufferInit"};
        if (!std::equal(foundation.begin(),foundation.end(),recorded.begin())) throw std::runtime_error("Actual old foundation order differs");
        auto reference = std::vector<std::string>{"foundation"}; reference.insert(reference.end(),recorded.begin()+6,recorded.end());
        unsigned int cases = 0;
        for (unsigned int mask : {1U,9U,15U}) for (bool compute : {false,true}) for (bool subscribe : {false,true}) {
            GetDAQ::CoBoPrepareSettings s; s.inputs.asadMask = s.clock.asadMask = s.monitoring.asadMask = s.calibration.asadMask = mask;
            s.calibration.computeAutomaticReadDelay = compute; s.monitoring.subscribeAlarms = subscribe;
            for (unsigned int id = 0; id < 4; ++id) if (mask&(1U<<id)) {
                GetDAQ::AsAdInputBoardSettings input; input.asadId = id; input.positions[2].active = true; s.inputs.boards.push_back(input);
                GetDAQ::AsAdBoardClockSettings clock; clock.asadId = id; s.clock.boards.push_back(clock);
                GetDAQ::AsAdMonitoringBoardSettings monitor; monitor.asadId = id; s.monitoring.boards.push_back(monitor);
                GetDAQ::AsAdCalibrationSettings calibration; calibration.asadId = id; s.calibration.boards.push_back(calibration);
                s.calibration.chipVersions[4*id+2] = 0x202;
            }
            const auto plan = GetDAQ::BuildCoBoPreparePlan(s); auto before = Stages(plan.beforeSubscription), after = Stages(plan.afterSubscription);
            const auto adc = std::find(before.begin(),before.end(),"adcChannelsFinal");
            if (adc == before.end() || adc == before.begin() || *(adc-1) != "asadAdcInit" || adc+1 == before.end() || *(adc+1) != "asadDacInit")
                throw std::runtime_error("Final ADC channel repair must follow last reset before DAC");
            before.erase(adc); // explicit, documented behavior change; tested separately with actual write-only ADC fixture
            if (after.empty() || after.front() != "boardAlarmMonitor") throw std::runtime_error("Board watcher not at post-subscription boundary");
            before.push_back("subscribeToCoBoAlarms"); after.erase(after.begin()); // owned Subscribe is executed by client at this boundary
            before.insert(before.end(),after.begin(),after.end());
            if (before != reference) throw std::runtime_error("Compiled native orchestration differs from extracted old impl_prepare");
            ++cases;
        }
        recorded.clear(); old.active = false; old.impl_prepare(0); if (!recorded.empty()) throw std::runtime_error("Old inactive board mutated");
        std::cout << "PASS actual old impl_prepare: " << cases << " active-set/compute/subscription combinations; explicit ADC repair and owned subscription differences; inactive old board skipped\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
'''
    with tempfile.TemporaryDirectory(prefix='getdaq-prepare-original-') as temporary:
        folder = Path(temporary)
        source, binary = folder / 'reference.cpp', folder / 'reference'
        source.write_text(program)
        hardware = root / 'apps/hardware'
        core = root / 'common/experiment'
        subprocess.run(['g++', '-std=c++11', '-pedantic-errors', '-Wall', '-Wextra', '-I'+str(hardware), '-I'+str(core),
                        str(source), *[str(hardware / name) for name in ('coboprepareplan.cpp', 'coboprepare.cpp', 'cobocalibration.cpp', 'cobomonitoring.cpp', 'coboclock.cpp', 'cobomaintenance.cpp')],
                        str(root / 'build-experiment/libgetdaq-experiment-core.a'), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
    if hashlib.sha256(old.read_bytes()).hexdigest() != digest:
        raise RuntimeError('Original source changed during comparison')


if __name__ == '__main__':
    main()
