#!/usr/bin/env python3
"""Compile original CoBoNode calibration bodies with recording stubs; compare raw native plan order.

Does not modify source/ or require CCfg, Boost, Qt or Ice. Version/metadata checks
added by the rewrite are covered separately by the actual TCP fixture.
"""
from pathlib import Path
import subprocess
import tempfile


def function(text, signature):
    start = text.index(signature)
    opening = text.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        if text[end] == '{':
            depth += 1
        elif text[end] == '}':
            depth -= 1
        end += 1
    return text[start:end].replace('CoBoNode::', 'Original::')


def main():
    root = Path(__file__).resolve().parents[1]
    text = (root / 'source/get/GetBench-software-release-20210208/src/get/rc/CoBoNode.cpp').read_text()
    serial = function(text, 'void CoBoNode::agetCal(CCfg::CConfig nodeCfg)')
    automatic = function(text, 'void CoBoNode::calibrateAutoReadDataDelay()')
    program = r'''
#include "hardwareclient.h"
#include "cobocalibrationconfiguration.h"
#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace CCfg { typedef int CConfig; }
struct CmdException : std::runtime_error { using std::runtime_error::runtime_error; };
std::vector<std::string> recorded;
void sleep_ms(int ms) { recorded.push_back("D:"+std::to_string(ms)); }
struct Device {
    std::string name;
    explicit Device(const char *n) : name(n) {}
    void writeField(const char *reg, const char *field, std::uint64_t v) {
        recorded.push_back("W:"+name+"."+reg+"."+field+"="+std::to_string(v));
    }
    void writeRegister(const char *reg, std::uint64_t v) {
        recorded.push_back("W:"+name+"."+reg+"="+std::to_string(v));
    }
    std::uint64_t readRegister(const char *reg) { recorded.push_back("R:"+name+"."+reg); return 0; }
};
#define NODE_DEBUG() std::ostringstream()
struct Original {
    const std::uint16_t agetMask_ = 0x8024;
    enum AdcMode { DDR, SDR };
    Device ctrlDevice{"ctrl"}, asadDevice{"asad"}, agetDevice{"aget"};
    Device *ctrl() { return &ctrlDevice; } Device *asad() { return &asadDevice; } Device *aget() { return &agetDevice; }
    // Pure chip selection and version/slow-control checks have independent tests.
    void coboSetCalibrationAgetChips(std::uint16_t,CCfg::CConfig) {}
    void printAgetChipVersionNumbers() {}
    void adcReset() { asad()->writeRegister("ADC0",0x400); }
    void adcTest(AdcMode mode) { asad()->writeRegister("ADC2",0x2a0); asad()->writeRegister("ADC3",1); asad()->writeRegister("ADC4",0); asad()->writeRegister("ADC5",mode == DDR ? 0x401 : 0x403); }
    void pipeInterruptEnable(bool enabled) { ctrl()->writeField("pipeCtrl","intEnable",enabled); }
    void agetCal(CCfg::CConfig); void calibrateAutoReadDataDelay();
};
'''+serial+'\n'+automatic+r'''
std::vector<std::string> Projection(const std::vector<GetDAQ::HardwareStep> &steps, bool serial) {
    using namespace GetDAQ; std::vector<std::string> out; bool start = !serial;
    for (const auto &s : steps) {
        if (s.kind == HardwareStep::ExpectValue) continue;
        if (!start && s.kind == HardwareStep::WriteValue && s.device == "asad" && s.reg == "ADC0") start = true;
        if (!start) continue;
        if (serial && s.kind == HardwareStep::WriteValue && s.device == "ctrl" && s.reg == "pipeCtrl") break; // start of final switchToSlowControl
        if (s.kind == HardwareStep::Delay) out.push_back("D:"+std::to_string(s.delayMs));
        else if (s.kind == HardwareStep::ReadValue) out.push_back("R:"+s.device+"."+s.reg);
        else if (s.kind == HardwareStep::WriteValue)
            out.push_back("W:"+s.device+"."+s.reg+(s.field.empty() ? "" : "."+s.field)+"="+std::to_string(s.value));
        else throw std::runtime_error("Unexpected projected step");
    }
    return out;
}
int main() {
    try {
        GetDAQ::CoBoCalibrationSettings settings; settings.asadMask = 11;
        settings.chipVersions = {{2,0x201},{5,0x202},{15,0x203}};
        for (unsigned int id : {0,1,3}) { GetDAQ::AsAdCalibrationSettings b; b.asadId = id; settings.boards.push_back(b); }
        Original original; original.agetCal(0); const auto serial = recorded; recorded.clear();
        original.calibrateAutoReadDataDelay(); const auto automatic = recorded;
        if (serial != Projection(GetDAQ::CoBoAgetCalibrate(settings),true)) throw std::runtime_error("Original serial command/delay order differs");
        if (automatic != Projection(GetDAQ::CoBoAutomaticReadDelayCalibrate(settings),false)) throw std::runtime_error("Original automatic delay command/delay order differs");
        std::cout << "PASS original extracted calibration bodies: " << serial.size() << " serial + " << automatic.size() << " automatic commands/read observations/delays; no source mutation\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
'''
    with tempfile.TemporaryDirectory(prefix='getdaq-calibration-original-') as temporary:
        folder = Path(temporary)
        source = folder / 'reference.cpp'
        source.write_text(program)
        binary = folder / 'reference'
        hardware = root / 'apps/hardware'
        experiment = root / 'common/experiment'
        subprocess.run(['g++', '-std=c++11', '-pedantic-errors', '-Wall', '-Wextra', '-I'+str(hardware), '-I'+str(experiment),
                        str(source), *[str(hardware / name) for name in ('cobocalibration.cpp', 'cobomaintenance.cpp', 'cobomonitoring.cpp', 'coboprepare.cpp')],
                        str(root / 'build-experiment/libgetdaq-experiment-core.a'), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    main()
