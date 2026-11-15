#!/usr/bin/env python3
"""Run actual received monitoring functions using standard C++ test surroundings.

Only valid finite 8-bit limits are compared; old out-of-range conversions are not
used as a reference. No CCfg/Boost installation, network or source edits.
"""
import hashlib
from pathlib import Path
import subprocess
import tempfile


def extract(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    level, end = 1, brace + 1
    while level:
        if text[end] == '{':
            level += 1
        elif text[end] == '}':
            level -= 1
        end += 1
    return text[start:end]


def main():
    root = Path(__file__).resolve().parents[1]
    folder = root / 'source/get/GetBench-software-release-20210208/src/get/rc'
    node, parser = folder / 'CoBoNode.cpp', folder / 'CoBoConfigParser.cpp'
    originals = {p: p.read_bytes() for p in (node, parser)}
    content = originals[node].decode('utf-8')
    signatures = ['void CoBoNode::asadMonitoringSetOperationalMode(bool',
                  'void CoBoNode::asadMonitoringSetOperationalMode(CCfg::CConfig &',
                  'void CoBoNode::asadSetAutoProtectionLevel(',
                  'void CoBoNode::setAsAdVddLimits(', 'void CoBoNode::setAsAdIddLimits(', 'void CoBoNode::setAsAdExtTempLimits(',
                  'double CoBoNode::readAsAdVdd(', 'double CoBoNode::readAsAdVdd1(', 'double CoBoNode::readAsAdIdd(',
                  'int8_t CoBoNode::readAsAdIntTemp(', 'int8_t CoBoNode::readAsAdExtTemp(']
    functions = '\n'.join(extract(content, signature) for signature in signatures).replace('CoBoNode::', 'OriginalNode::')
    functions += '\n' + extract(originals[parser].decode('utf-8'), 'uint8_t CoBoConfigParser::getAsAdMonitoringSingleChannel(').replace('CoBoConfigParser::', 'OriginalNode::')
    license_header = content[:content.index('#include')]
    support = r'''
#include "cobomonitoringconfiguration.h"
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
struct QuietLog { template<class T> QuietLog &operator<<(const T &) { return *this; } } quietLog;
#define NODE_DEBUG() quietLog
#define NODE_INFO() quietLog
#define NODE_WARN() quietLog
namespace utl { template<class T> struct BitFieldHelper {
    static void setBit(T &word, unsigned int position, bool value) { const T bit = T(1) << position; word = value ? word | bit : word & ~bit; }
    static void setField(T &word, unsigned int position, unsigned int width, unsigned int value) {
        const T mask = (T(1) << width)-1; word = (word & ~(mask << position)) | ((T(value) & mask) << position);
    }
}; }
namespace CCfg {
class CConfig {
    const std::map<std::string,std::string> *values_; std::string path_;
public:
    struct NotFound : std::runtime_error { using std::runtime_error::runtime_error; };
    explicit CConfig(const std::map<std::string,std::string> &values, const std::string &path = "") : values_(&values),path_(path) {}
    CConfig operator()(const std::string &tag) const { return CConfig(*values_,path_.empty() ? tag : path_ + '/' + tag); }
    std::string getStringValue() const { const auto found = values_->find(path_); if (found == values_->end()) throw NotFound(path_); return found->second; }
    bool getBoolValue() const { const auto s = getStringValue(); if (s == "true") return true; if (s == "false") return false; throw std::runtime_error("Reference boolean"); }
    bool getBoolValue(bool fallback) const { try { return getBoolValue(); } catch (const NotFound &) { return fallback; } }
    double getRealValue() const { return std::stod(getStringValue()); }
    int getIntValue() const { return std::stoi(getStringValue()); }
};
}
struct Device {
    std::map<std::string,std::uint64_t> values;
    void writeRegister(const std::string &name, std::uint64_t value) { values[name] = value; }
    void writeField(const std::string &, const std::string &, std::uint64_t) {}
    std::uint64_t readRegister(const std::string &name) { return values[name]; }
};
class OriginalNode {
public:
    Device asadDevice,ctrlDevice;
    Device *asad() { return &asadDevice; } Device *ctrl() { return &ctrlDevice; }
    CCfg::CConfig asadConfig(CCfg::CConfig cfg, std::size_t) { return cfg; }
    uint8_t getAsAdMonitoringSingleChannel(CCfg::CConfig &, const size_t &);
    void asadMonitoringSetOperationalMode(bool,bool,bool,uint8_t,bool);
    void asadMonitoringSetOperationalMode(CCfg::CConfig &,size_t);
    void asadSetAutoProtectionLevel(CCfg::CConfig &,size_t);
    void setAsAdVddLimits(CCfg::CConfig,size_t); void setAsAdIddLimits(CCfg::CConfig,size_t); void setAsAdExtTempLimits(CCfg::CConfig,size_t);
    double readAsAdVdd(size_t); double readAsAdVdd1(size_t); double readAsAdIdd(size_t);
    int8_t readAsAdIntTemp(size_t); int8_t readAsAdExtTemp(size_t);
};
'''
    driver = r'''
std::string Decimal(double value) { std::ostringstream s; s << std::setprecision(17) << value; return s.str(); }
void Require(bool condition,const char *message) { if (!condition) throw std::runtime_error(message); }
int main() {
    try {
        unsigned int modes = 0, limits = 0, samples = 0, masks = 0;
        for (const char *item : {"VDD1","Tint","Text","VDD","IDD"}) for (unsigned int bits = 0; bits < 16; ++bits) {
            std::map<std::string,std::string> v{{"Monitoring/isAlarmSet",bits & 1 ? "true" : "false"},
                {"Monitoring/fastConversionEnabled",bits & 2 ? "true" : "false"},{"Monitoring/singleChannelModeEnabled",bits & 4 ? "true" : "false"},
                {"Monitoring/averagingDisabled",bits & 8 ? "true" : "false"},{"Monitoring/singleChannelItem",item}};
            CCfg::CConfig cfg(v); OriginalNode old;
            GetDAQ::CoBoMonitoringSettings s; s.asadMask = 1; GetDAQ::AsAdMonitoringBoardSettings b;
            b.alarmsEnabled = bits & 1; b.fastConversionEnabled = bits & 2; b.singleChannelModeEnabled = bits & 4; b.averagingDisabled = bits & 8; b.singleChannelItem = item;
            s.boards.push_back(b); s.modeMapping = GetDAQ::CoBoMonitoringSettings::LegacyCallArguments;
            old.asadMonitoringSetOperationalMode(cfg,0); const auto legacy = GetDAQ::CompileCoBoMonitoring(s).boards[0];
            Require(old.asadDevice.values["configuration2"] == legacy.configuration2 && old.asadDevice.values["configuration3"] == legacy.configuration3 && old.asadDevice.values["configuration1"] == legacy.protectionConfiguration1,"Actual old caller mapping mismatch"); ++modes;
            s.modeMapping = GetDAQ::CoBoMonitoringSettings::DeclaredParameters; const auto declared = GetDAQ::CompileCoBoMonitoring(s).boards[0];
            old.asadMonitoringSetOperationalMode(b.alarmsEnabled,b.averagingDisabled,b.singleChannelModeEnabled,old.getAsAdMonitoringSingleChannel(cfg,0),b.fastConversionEnabled);
            Require(old.asadDevice.values["configuration2"] == declared.configuration2 && old.asadDevice.values["configuration3"] == declared.configuration3,"Declared parameter meaning mismatch"); ++modes;
        }
        for (unsigned int mask = 0; mask < 32; ++mask) for (bool alarms : {false,true}) {
            const char *names[] = {"LimitsText/disableAlertsTooHigh","LimitsText/disableAlertsTooLow","LimitsText/disableAlertsOutOfOrder","LimitsVDD/disableAlerts","LimitsIDD/disableAlerts"};
            std::map<std::string,std::string> v; v["Monitoring/isAlarmSet"] = alarms ? "true" : "false";
            GetDAQ::CoBoMonitoringSettings s; s.asadMask = 1; GetDAQ::AsAdMonitoringBoardSettings b; b.alarmsEnabled = alarms;
            for (unsigned int i = 0; i < 5; ++i) { b.disableAlerts[i] = mask & (1U << i); v[std::string("Monitoring/")+names[i]] = b.disableAlerts[i] ? "true" : "false"; }
            s.boards.push_back(b); CCfg::CConfig cfg(v); OriginalNode old; old.asadSetAutoProtectionLevel(cfg,0);
            Require(old.asadDevice.values["interruptMask1"] == GetDAQ::CompileCoBoMonitoring(s).boards[0].protectionMask,"Actual protection mask mismatch"); ++masks;
        }
        for (unsigned int i = 0; i <= 1000; ++i) {
            const double volts = i*3.74/1000,amps = i*3.0/1000;
            std::map<std::string,std::string> v{{"Monitoring/LimitsVDD/lowerValue",Decimal(volts)},{"Monitoring/LimitsVDD/upperValue",Decimal(volts)},
                {"Monitoring/LimitsIDD/lowerValue",Decimal(amps)},{"Monitoring/LimitsIDD/upperValue",Decimal(amps)}};
            CCfg::CConfig cfg(v); OriginalNode old; old.setAsAdVddLimits(cfg,0); old.setAsAdIddLimits(cfg,0);
            Require(old.asadDevice.values["VDDLowLimit"] == GetDAQ::CompileAsAdVddLimit(volts) && old.asadDevice.values["VDDHighLimit"] == GetDAQ::CompileAsAdVddLimit(volts),"Actual VDD threshold mismatch");
            Require(old.asadDevice.values["IDDLowLimit"] == GetDAQ::CompileAsAdIddLimit(amps) && old.asadDevice.values["IDDHighLimit"] == GetDAQ::CompileAsAdIddLimit(amps),"Actual IDD threshold mismatch"); limits += 2;
        }
        for (int t = -128; t <= 127; ++t) {
            std::map<std::string,std::string> v{{"Monitoring/LimitsText/lowerValue",std::to_string(t)},{"Monitoring/LimitsText/upperValue",std::to_string(t)}};
            CCfg::CConfig cfg(v); OriginalNode old; old.setAsAdExtTempLimits(cfg,0);
            GetDAQ::CoBoMonitoringSettings s; s.asadMask = 1; GetDAQ::AsAdMonitoringBoardSettings b; b.externalTemperatureLower = b.externalTemperatureUpper = t; s.boards.push_back(b);
            const auto current = GetDAQ::CompileCoBoMonitoring(s).boards[0]; Require(old.asadDevice.values["extTempLowLimit"] == current.limits[0] && old.asadDevice.values["extTempHighLimit"] == current.limits[1],"Actual signed threshold mismatch"); ++limits;
        }
        for (unsigned int raw = 0; raw <= 255; ++raw) {
            OriginalNode old; for (const char *name : {"VDD","VDD1","IDD","internalTemperature","externalTemperature"}) old.asadDevice.values[name] = raw;
            const auto current = GetDAQ::DecodeAsAdMeasurements({{raw,raw,raw,raw,raw}});
            Require(current.vddVolts == old.readAsAdVdd(0) && current.vdd1Volts == old.readAsAdVdd1(0) && current.iddAmps == old.readAsAdIdd(0),"Actual voltage/current measurement mismatch");
            Require(current.internalTemperatureC == old.readAsAdIntTemp(0) && current.externalTemperatureC == old.readAsAdExtTemp(0),"Local old signed return mismatch"); ++samples;
        }
        std::cout << "PASS received GET monitoring oracle: " << modes << " mode, " << masks << " protection, " << limits << " threshold and " << samples << " raw measurement cases\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
'''
    with tempfile.TemporaryDirectory(prefix='getdaq-monitoring-oracle-') as temporary:
        cpp, executable = Path(temporary) / 'reference.cpp', Path(temporary) / 'reference'
        cpp.write_text(license_header + support + functions + driver)
        subprocess.run(['c++', '-std=c++11', '-pedantic-errors', '-O2', '-I' + str(root / 'common/experiment'),
                        str(cpp), str(root / 'common/experiment/cobomonitoringconfiguration.cpp'), '-o', str(executable)], check=True)
        subprocess.run([str(executable)], check=True, timeout=30)
    assert all(hashlib.sha256(p.read_bytes()).digest() == hashlib.sha256(data).digest() for p, data in originals.items())


if __name__ == '__main__':
    main()
