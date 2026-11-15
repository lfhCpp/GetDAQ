#include "hardwareclient.h"
#include "hardware_fixture.h"
#include "eccstatemachine.h"
#include "coboagetconfiguration.h"
#include "aget_fixture.h"
#include "cobo_readout_fixture.h"
#include "cobo_configure_fixture.h"
#include "coboprepareconfiguration.h"
#ifdef GETDAQ_TEST_ECC_JOURNAL
#include "eccjournal.h"
#endif
#include <Ice/Ice.h>
#include <mdaq/hw/Control.h>
#include <get/cobo/CtrlNode.h>
#include <atomic>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <mutex>
#include <stdexcept>
#include <unistd.h>
#include <sys/stat.h>
using namespace mdaq::hw;
namespace {
void Require(bool condition, const char *why) { if (!condition) throw std::runtime_error(why); }
template<class Action> void Rejects(Action action, const std::string &part) {
    try { action(); } catch (const std::exception &e) {
        Require(std::string(e.what()).find(part) != std::string::npos, e.what()); return;
    }
    throw std::runtime_error("Expected rejection: " + part);
}
std::string ReadFile(const std::string &path) {
    std::ifstream input(path); return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}
using GetDAQTest::TestDevice;
using GetDAQTest::BasicTestNode;
using GetDAQTest::TestNode;
// 两个远端设备共同模拟 CoBo 的 AsAd 选择器；所有调用仍经过真实 TCP。
class DelayWarningDevice:public TestDevice {
public:
    DelayWarningDevice():TestDevice(4) {
        fConfig.registers[0].descr.name="readDataDelay";fConfig.registers[0].fields[0].name="AGET0";fConfig.registers[0].fields[0].width=8;fValue=255;
    }
    Ice::Long readField(const std::string &,const std::string &,const Ice::Current &) override {++fReads;return fValue;}
    void writeField(const std::string &reg,const std::string &field,Ice::Long value,const Ice::Current &current) override {
        if(fFailure)TestDevice::writeField(reg,field,value,current);else ++fWrites;
    }
};
struct CalibrationState {
    std::mutex mutex;
    unsigned int selected = 0, mask = 0;
    int failure = 0;
    Ice::Long labels[4] = {0,0,0,0};
    std::vector<std::string> operations;
};
class CalibrationDevice : public TestDevice {
public:
    CalibrationState &fState;
    bool fAsAd;
    CalibrationDevice(CalibrationState &state, bool asad) : TestDevice(asad ? 8 : 4), fState(state), fAsAd(asad) {
        fConfig.descr.name = asad ? "asad" : "ctrl";
        fConfig.registers.clear();
        const auto add = [this](const char *name, Ice::Long offset, const char *field, int width, int bits) {
            RegisterConfig reg; reg.descr.name = name; reg.descr.offset = offset;
            if (*field) { FieldDescription f; f.name = field; f.width = width; f.offset = bits; f.readOnly = false; reg.fields.push_back(f); }
            fConfig.registers.push_back(reg);
        };
        if (asad) {
            add("monitorID", 0x34e, "", 0, 0);
            for (int i = 0; i < 4; ++i) add(("AIM" + std::to_string(i)).c_str(), 0x400 + i, "", 0, 0);
        } else {
            add("ASAD_sc_mask", 0x17c, "select", 4, 0);
            add("asadSelect", 0x148, "d", 2, 0);
            add("asad_sc_start", 0xfc, "", 0, 0);
            add("asadController", 0x120, "reset", 4, 0);
            FieldDescription field; field.name = "manualCal"; field.offset = 8; field.width = 4; field.readOnly = false;
            fConfig.registers.back().fields.push_back(field);
            field.name = "calibration"; field.offset = 4; fConfig.registers.back().fields.push_back(field);
        }
    }
    Ice::Long readRegister(const std::string &reg, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex);
        fState.operations.push_back("read:" + std::to_string(fState.selected) + ":" + reg);
        if (!fAsAd) throw mdaq::utl::CmdException("Unexpected controller read");
        if (reg == "monitorID") return fState.failure == 1 ? 0x40 : 0x41;
        if (reg == "AIM0") return (Ice::Long(1) << 40) | (fState.labels[fState.selected] + (fState.failure == 2 ? 1 : 0));
        if (reg == "AIM2" && fState.failure == 3) throw mdaq::utl::CmdException("Identity read failed");
        if (reg == "AIM1" || reg == "AIM2" || reg == "AIM3") return 0x11;
        throw mdaq::utl::CmdException("Unexpected AsAd read");
    }
    void writeField(const std::string &reg, const std::string &field, Ice::Long value, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex);
        fState.operations.push_back("write:" + reg + "." + field + "=" + std::to_string(value));
        if (reg == "ASAD_sc_mask") fState.mask = static_cast<unsigned int>(value);
        if (reg == "asadSelect") fState.selected = static_cast<unsigned int>(value);
    }
    void writeRegister(const std::string &reg, Ice::Long value, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex);
        fState.operations.push_back("write:" + reg + "=" + std::to_string(value));
        if (fAsAd && reg == "AIM0") fState.labels[fState.selected] = value;
    }
};
struct AgetState {
    std::mutex mutex;
    unsigned int selected = 0, writeMask = 0, maskChip = 0;
    int failure = 0, agetWrites = 0, zeroWrites = 0;
    std::map<std::string, Ice::Long> registers[16], masks[16], control;
    std::vector<std::string> operations;
    unsigned int asadSelected = 0, defaultCalls = 0, triggerCalls = 0, startCalls = 0, stopCalls = 0, period = 0;
    int defaultMv = 0, voltageMv = 0, secondMv = 0;
    bool external = false, random = false, periodicRunning = false;
    std::map<std::string, Ice::Long> asad[4];
    void Reset(int fail) {
        std::lock_guard<std::mutex> lock(mutex); selected = writeMask = maskChip = 0; failure = fail; agetWrites = zeroWrites = 0;
        for (unsigned int i = 0; i < 16; ++i) { registers[i].clear(); masks[i].clear(); }
        control.clear(); operations.clear();
        for (auto &bank : asad) bank.clear();
        asadSelected = defaultCalls = triggerCalls = startCalls = stopCalls = period = 0;
        defaultMv = voltageMv = secondMv = 0; external = random = periodicRunning = false;
    }
};
class AgetDevice : public TestDevice {
public:
    AgetState &fState;
    bool fAget;
    AgetDevice(AgetState &state, bool aget) : TestDevice(aget ? 8 : 4), fState(state), fAget(aget) {
        fConfig.descr.name = aget ? "aget" : "ctrl"; fConfig.registers.clear();
        const auto reg = [this](const std::string &name) {
            RegisterConfig r; r.descr.name = name; r.descr.offset = 4 * fConfig.registers.size(); fConfig.registers.push_back(r);
        };
        const auto field = [this](const std::string &name, int width, int offset) {
            FieldDescription f; f.name = name; f.width = width; f.offset = offset; f.readOnly = false; fConfig.registers.back().fields.push_back(f);
        };
        if (aget) {
            const char *names[] = {"reg1", "reg2", "reg3", "reg4", "reg5", "reg6", "reg7", "reg8", "reg8msb", "reg9", "reg9msb", "reg10", "reg11", "reg12"};
            for (const char *name : names) reg(name);
        } else {
            reg("pipeCtrl"); field("firmwareEnable", 1, 0); field("intEnable", 1, 1); field("isFPNRead", 1, 2); field("modeEnable_2p", 1, 3);
            field("writeHitRegisterMaskEnable", 1, 4); field("testMode", 1, 5); field("MultSubtractAverage", 1, 6);
            field("MultSubtractDevs", 3, 7); field("suppressMultiplicity", 4, 10); field("partialReadOut", 1, 14);
            field("zeroSuppressionEnable", 1, 15); field("zeroSuppressInvert", 1, 16);
            field("multiplicityReadOut", 1, 17); field("bitSlipCalibrate", 1, 18); field("hitRegisterOverride", 1, 19);
            field("SCW", 1, 20); field("SCR", 1, 21); field("ScrScwOverride", 1, 22); field("reset", 1, 23);
            field("backpressure", 1, 24); field("adcDeskew", 1, 25);
            reg("agetController"); field("reset", 1, 0); field("hitRegister", 1, 1); field("calibration", 1, 2);
            reg("aget_sc_configuration"); field("SC_En", 16, 0); field("pulseEnable", 16, 16);
            reg("agetChipSelect"); field("d", 4, 0); reg("ASAD_sc_mask"); field("select", 4, 0);
            reg("asadSelect"); field("d", 2, 0); reg("aget_2p_initNow");
            reg("readOffset"); field("value", 9, 0); reg("readDepth"); reg("maskSelect"); reg("aget_sc_rw");
            for (const char *name : {"readIfHitMask1_4", "readIfHitMask5_36", "readIfHitMask37_68", "readAlwaysMask1_4", "readAlwaysMask5_36", "readAlwaysMask37_68"}) reg(name);
            for (const char *name : {"multThreshold", "multWindSize", "multThreshold_2p", "triggerPeriod_10ns", "triggerDelay", "triggerWindowStart_2p",
                    "triggerDelay_2p", "triggerTime_10ns_2p", "triggerWindowEnd_2p", "ScwMultDelay", "ScwScrDelay", "interruptRate", "aFullLevel", "aEmptyLevel", "memStart", "memEnd", "readPtr"}) reg(name);
            reg("dataSource"); field("value", 8, 0); reg("COBO_index"); field("d", 8, 0);
            reg("hardwareVersion"); reg("firmwareRelease"); reg("firmwareDate");
            reg("mutantConfig"); field("initDone", 1, 0);
            reg("lemoMux"); for (unsigned int i = 0; i < 4; ++i) field("L" + std::to_string(i), 4, int(4 * i));
            reg("readDataDelay"); for (unsigned int i = 0; i < 4; ++i) field("AGET" + std::to_string(i), 8, int(8 * i));
            reg("triggerMask");
            for (unsigned int group = 0; group < 3; ++group) {
                unsigned int id = 0;
                for (const char *name : {"external", "manual", "timing", "multiplicity", "slowControl"})
                    field(std::string(name) + (group == 1 ? "_2p" : group == 2 ? "_out" : ""), 1, int(8 * group + id++));
            }
            for (unsigned int id = 0; id < 4; ++id) for (const char *prefix : {"memStart_asad", "memEnd_asad", "readPtr_asad"}) reg(std::string(prefix) + std::to_string(id));
        }
    }
    Ice::Long readRegister(const std::string &reg, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex);
        const unsigned int id = fAget ? fState.selected : fState.maskChip;
        fState.operations.push_back("read:" + std::string(fAget ? "aget:" : "ctrl:") + std::to_string(id) + ":" + reg);
        if (fAget && reg == "reg5") {
            if (fState.failure == 7) throw mdaq::utl::CmdException("Chip version read failed");
            if (fState.failure == 11 && id == 12) return 0x201;
            return id == 7 ? 0x203 : id == 15 && fState.failure == 1 ? 0x201 : 0x202;
        }
        if (fAget) {
            auto value = fState.registers[id][reg];
            if (id == 0 && ((fState.failure == 2 && reg == "reg1") || ((fState.failure == 3 || fState.failure == 4) && reg == "reg2")))
                value ^= fState.failure == 2 ? 1 : fState.failure == 3 ? 0x200000 : 0x200001;
            return value;
        }
        if (reg.find("Mask") != std::string::npos) return fState.masks[id][reg] ^ (fState.failure == 6 && reg == "readIfHitMask1_4" ? 1 : 0);
        return fState.control[reg];
    }
    Ice::Long readField(const std::string &reg, const std::string &field, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex);
        fState.operations.push_back("read-field:" + reg + "." + field);
        if (fState.failure == 8 && reg == "pipeCtrl" && field == "firmwareEnable") return 1;
        if (fState.failure == 12 && reg == "pipeCtrl" && field == "zeroSuppressInvert") return fState.control[reg + '.' + field] ^ 1;
        return fState.control[reg + '.' + field];
    }
    void writeField(const std::string &reg, const std::string &field, Ice::Long value, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex); fState.control[reg + '.' + field] = value;
        fState.operations.push_back("write-field:" + reg + '.' + field + '=' + std::to_string(value));
        if (reg == "agetChipSelect") fState.selected = static_cast<unsigned int>(value);
        if (reg == "aget_sc_configuration" && field == "SC_En") fState.writeMask = static_cast<unsigned int>(value);
        if (reg == "asadSelect") fState.asadSelected = static_cast<unsigned int>(value);
    }
    void writeRegister(const std::string &reg, Ice::Long value, const Ice::Current &current) override {
        std::lock_guard<std::mutex> lock(fState.mutex);
        if (fAget) {
            unsigned int id = 0;
            while (id < 16 && fState.writeMask != (1U << id)) ++id;
            if (id == 16 || reg == "reg5") throw mdaq::utl::CmdException("Invalid selected chip or version write");
            ++fState.agetWrites; fState.registers[id][reg] = value;
            fState.operations.push_back("write:aget:" + std::to_string(id) + ':' + reg);
            if (fState.failure == 5 && reg == "reg6") current.con->close(true);
        } else {
            fState.operations.push_back("write:ctrl:" + reg);
            if (reg == "maskSelect") fState.maskChip = static_cast<unsigned int>(value);
            if (reg.find("Mask") != std::string::npos) fState.masks[fState.maskChip][reg] = value;
            else fState.control[reg] = value;
        }
    }
};
class AsAdConfigureDevice : public TestDevice {
public:
    AgetState &fState;
    explicit AsAdConfigureDevice(AgetState &state) : TestDevice(8), fState(state) {
        fConfig.descr.name = "asad"; fConfig.registers.clear();
        RegisterConfig r; r.descr.name = "TCM0"; r.descr.offset = 0x200;
        const auto field = [&r](const char *name, int offset, int width) {
            FieldDescription f; f.name = name; f.offset = offset; f.width = width; f.readOnly = false; r.fields.push_back(f);
        };
        field("Calib", 7, 1); field("Rg", 6, 1); field("Trg", 5, 1); field("Delay", 0, 5); fConfig.registers.push_back(r);
        for (const char *name : {"TCM1", "TCM2", "ISPM"}) {
            r.fields.clear(); r.descr.name = name; ++r.descr.offset; fConfig.registers.push_back(r);
        }
        for (const char *name : {"ADC0", "ADC2", "ADC3", "ADC4", "ADC5", "IM0", "IM1", "DAC0", "DAC1", "DAC2", "DAC3", "DAC4", "DAC5", "DAC6", "DAC7"}) {
            r.descr.name = name; ++r.descr.offset; fConfig.registers.push_back(r);
        }
    }
    Ice::Long readRegister(const std::string &reg, const Ice::Current &) override {
        if (reg.find("ADC") == 0) throw mdaq::utl::CmdException("ADC registers are write-only");
        std::lock_guard<std::mutex> lock(fState.mutex); fState.operations.push_back("read-asad:" + std::to_string(fState.asadSelected) + ':' + reg);
        return fState.asad[fState.asadSelected][reg] ^ (fState.failure == 14 && reg == "TCM2" ? 1 : 0);
    }
    Ice::Long readField(const std::string &reg, const std::string &field, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex); fState.operations.push_back("read-asad-field:" + std::to_string(fState.asadSelected) + ':' + reg + '.' + field);
        return fState.asad[fState.asadSelected][reg + '.' + field];
    }
    void writeRegister(const std::string &reg, Ice::Long value, const Ice::Current &current) override {
        std::lock_guard<std::mutex> lock(fState.mutex); fState.operations.push_back("write-asad:" + reg);
        const auto mask = fState.control["ASAD_sc_mask.select"];
        for (unsigned int id = 0; id < 4; ++id) if (mask & (1U << id)) {
            if (reg == "ADC0" && value == 0x400) {
                for (unsigned int i = 0; i < 8; ++i) fState.asad[id]["ADC" + std::to_string(i)] = 0;
            } else fState.asad[id][reg] = value;
        }
        if (fState.failure == 16 && reg == "ADC5") current.con->close(true);
    }
    void writeField(const std::string &reg, const std::string &field, Ice::Long value, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex); fState.operations.push_back("write-asad-field:" + reg + '.' + field);
        const auto mask = fState.control["ASAD_sc_mask.select"];
        for (unsigned int id = 0; id < 4; ++id) if (mask & (1U << id)) fState.asad[id][reg + '.' + field] = value;
    }
};
class PulserNode : public BasicTestNode<get::cobo::CtrlNode> {
public:
    AgetState &fState;
    explicit PulserNode(AgetState &state) : fState(state) {}
    void Dac(Ice::Long voltage) {
        const auto code = GetDAQ::AsAdDacCode(int(voltage)); const auto mask = fState.control["ASAD_sc_mask.select"];
        for (unsigned int id = 0; id < 4; ++id) if (mask & (1U << id)) {
            fState.asad[id]["TCM1"] = code >> 8; fState.asad[id]["TCM2"] = code & 0xff;
        }
    }
    void setDefaultPulserVoltage(Ice::Long voltage, const Ice::Current &current) override {
        std::lock_guard<std::mutex> lock(fState.mutex); ++fState.defaultCalls; fState.defaultMv = int(voltage);
        fState.operations.push_back("pulser-default"); Dac(voltage);
        if (fState.failure == 13) current.con->close(true);
    }
    void triggerPulser(Ice::Long voltage, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex); ++fState.triggerCalls; fState.operations.push_back("pulser-trigger"); Dac(voltage);
    }
    void configureExternalPulser(bool enabled, Ice::Long voltage, const Ice::Current &current) override {
        std::lock_guard<std::mutex> lock(fState.mutex); fState.external = enabled; fState.voltageMv = int(voltage);
        fState.operations.push_back("pulser-external");
        if (fState.failure == 15) throw Ice::OperationNotExistException(__FILE__, __LINE__, current.id, current.facet, current.operation);
    }
    void configurePeriodicPulser(Ice::Long voltage, Ice::Long period, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex); fState.period = unsigned(period); fState.voltageMv = int(voltage); fState.operations.push_back("pulser-periodic");
    }
    void configureDoublePulserMode(Ice::Long voltage, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex); fState.secondMv = int(voltage); fState.operations.push_back("pulser-double");
    }
    void setRandomPulserEnabled(bool enabled, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex); fState.random = enabled; fState.operations.push_back("pulser-random");
    }
    void startPeriodicPulser(const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex); ++fState.startCalls; fState.periodicRunning = true; fState.operations.push_back("pulser-start");
    }
    void stopPeriodicPulser(const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex); ++fState.stopCalls; fState.periodicRunning = false; fState.operations.push_back("pulser-stop");
    }
    void setAsAdAlarmMonitoringEnabled(bool, const Ice::Current &) override { TestDevice::Unsupported(); }
    get::mt::AlarmServicePrx getAlarmService(const Ice::Current &) override { TestDevice::Unsupported(); return nullptr; }
    void blinkLEDs(const Ice::Current &) override { TestDevice::Unsupported(); }
    void setLED(Ice::Int, bool, const Ice::Current &) override { TestDevice::Unsupported(); }
    void setLEDs(bool, const Ice::Current &) override { TestDevice::Unsupported(); }
    void flipLED(Ice::Int, const Ice::Current &) override { TestDevice::Unsupported(); }
    void flipLEDs(const Ice::Current &) override { TestDevice::Unsupported(); }
    void pulseLED(Ice::Int, Ice::Int, const Ice::Current &) override { TestDevice::Unsupported(); }
    void modifyLED(get::cobo::LedType, Ice::Int, get::cobo::LedState, const Ice::Current &) override { TestDevice::Unsupported(); }
};
class ZeroDevice : public TestDevice {
public:
    AgetState &fState;
    explicit ZeroDevice(AgetState &state, unsigned int board) : TestDevice(4), fState(state) {
        fConfig.descr.name = board == 0 ? "zeroSuppress" : "zeroSuppress_AsAd" + std::to_string(board); fConfig.registers.clear();
        for (unsigned int chip = 0; chip < 4; ++chip) for (unsigned int id = 0; id < 68; ++id) {
            RegisterConfig r; r.descr.name = "aget" + std::to_string(chip) + "ch" + std::to_string(id); r.descr.offset = 4 * fConfig.registers.size();
            FieldDescription f; f.name = "threshold"; f.offset = 0; f.width = 12; f.readOnly = false; r.fields.push_back(f); fConfig.registers.push_back(r);
        }
    }
    Ice::Long readField(const std::string &reg, const std::string &field, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex); const auto key = fConfig.descr.name + ':' + reg + '.' + field;
        fState.operations.push_back("read-zero:" + key);
        return fState.control[key] ^ (fState.failure == 9 && fConfig.descr.name == "zeroSuppress" && reg == "aget0ch11" ? 1 : 0);
    }
    void writeField(const std::string &reg, const std::string &field, Ice::Long value, const Ice::Current &current) override {
        std::lock_guard<std::mutex> lock(fState.mutex); const auto key = fConfig.descr.name + ':' + reg + '.' + field;
        fState.control[key] = value; ++fState.zeroWrites; fState.operations.push_back("write-zero:" + key);
        if (fState.failure == 10 && fConfig.descr.name == "zeroSuppress" && reg == "aget0ch10") current.con->close(true);
    }
};
GetDAQ::CoBoAgetSettings AgetBoard()
{
    GetDAQ::CoBoAgetSettings board; board.asadMask = 11;
    for (unsigned int id : {0U, 7U, 15U}) {
        GetDAQ::CoBoAgetChipSettings chip; chip.asadId = id / 4; chip.agetId = id % 4; chip.settings = GetDAQTest::Neutral();
        if (id == 7) { chip.settings.chipVersion = 0x203; chip.settings.globals["Reg1/peackingTime"] = "68"; }
        if (id == 0) chip.settings.globals["Reg1/isFPNRead"] = "true";
        for (auto &c : chip.settings.channels) { c.gain = 3; c.threshold = 15; c.reading = GetDAQ::AgetChannel::Always; }
        chip.settings.channels[0].reading = GetDAQ::AgetChannel::OnlyIfHit;
        board.chips.push_back(chip);
    }
    return board;
}
class DescribeBackend : public GetDAQ::EccBackend {
public:
    GetDAQ::HardwareClient &fClient;
    const GetDAQ::HardwareDescription &fDescription;
    std::string fAudit;
    DescribeBackend(GetDAQ::HardwareClient &client, const GetDAQ::HardwareDescription &description, const std::string &audit)
        : fClient(client), fDescription(description), fAudit(audit) {}
    void Validate(GetDAQ::EccCommand command, GetDAQ::EccState, GetDAQ::EccState) override {
        if (command != GetDAQ::EccCommand::Describe) throw std::runtime_error("Other backend flows unavailable");
        GetDAQ::ValidateHardwareDescription(fDescription);
        if (!fClient.Devices().empty()) throw std::runtime_error("Node not empty");
    }
    void Apply(GetDAQ::EccCommand, GetDAQ::EccState, GetDAQ::EccState) override { fClient.Create(fDescription, fAudit); }
};
}
int main(int argc, char **argv) {
    Ice::CommunicatorPtr communicator;
    try {
        communicator = Ice::initialize();
        auto adapter = communicator->createObjectAdapterWithEndpoints("HardwareTest", "tcp -h 127.0.0.1 -p 0");
        IceUtil::Handle<TestDevice> narrow = new TestDevice(4), wide = new TestDevice(8), invalid = new TestDevice(4);
        invalid->fConfig.registers[0].fields[0].width = 33;
        IceUtil::Handle<TestNode> node = new TestNode;
        node->fDevices["ctrl"] = DevicePrx::uncheckedCast(adapter->add(narrow, communicator->stringToIdentity("ctrl")));
        node->fDevices["wide"] = DevicePrx::uncheckedCast(adapter->add(wide, communicator->stringToIdentity("wide")));
        node->fDevices["invalid"] = DevicePrx::uncheckedCast(adapter->add(invalid, communicator->stringToIdentity("invalid")));
        auto proxy = adapter->add(node, communicator->stringToIdentity("node")); adapter->activate();
        const std::string address = communicator->proxyToString(proxy);
        CalibrationState calibration;
        IceUtil::Handle<TestNode> calibrationNode = new TestNode;
        IceUtil::Handle<CalibrationDevice> calibrationCtrl = new CalibrationDevice(calibration, false),
            calibrationAsAd = new CalibrationDevice(calibration, true);
        calibrationNode->fDevices["ctrl"] = DevicePrx::uncheckedCast(adapter->addWithUUID(calibrationCtrl));
        calibrationNode->fDevices["asad"] = DevicePrx::uncheckedCast(adapter->addWithUUID(calibrationAsAd));
        const auto calibrationProxy = adapter->addWithUUID(calibrationNode);
        const std::string calibrationAddress = communicator->proxyToString(calibrationProxy);
        AgetState agetState; IceUtil::Handle<TestNode> agetNode = new TestNode;
        IceUtil::Handle<AgetDevice> agetCtrl = new AgetDevice(agetState, false), agetDevice = new AgetDevice(agetState, true);
        agetNode->fDevices["ctrl"] = DevicePrx::uncheckedCast(adapter->addWithUUID(agetCtrl));
        agetNode->fDevices["aget"] = DevicePrx::uncheckedCast(adapter->addWithUUID(agetDevice));
        for (unsigned int board : {0U,1U,3U}) {
            IceUtil::Handle<ZeroDevice> zero = new ZeroDevice(agetState, board);
            agetNode->fDevices[zero->fConfig.descr.name] = DevicePrx::uncheckedCast(adapter->addWithUUID(zero));
        }
        const std::string agetAddress = communicator->proxyToString(adapter->addWithUUID(agetNode));
        AgetState configureState; IceUtil::Handle<PulserNode> configureNode = new PulserNode(configureState);
        IceUtil::Handle<AgetDevice> configureCtrl = new AgetDevice(configureState, false), configureAget = new AgetDevice(configureState, true);
        IceUtil::Handle<AsAdConfigureDevice> configureAsAd = new AsAdConfigureDevice(configureState);
        for (const auto &device : std::vector<IceUtil::Handle<TestDevice>>{configureCtrl, configureAget, configureAsAd})
            configureNode->fDevices[device->fConfig.descr.name] = DevicePrx::uncheckedCast(adapter->addWithUUID(device));
        for (unsigned int id : {0U, 1U, 3U}) {
            IceUtil::Handle<ZeroDevice> zero = new ZeroDevice(configureState, id);
            configureNode->fDevices[zero->fConfig.descr.name] = DevicePrx::uncheckedCast(adapter->addWithUUID(zero));
        }
        const std::string configureAddress = communicator->proxyToString(adapter->addWithUUID(configureNode));
        if (argc == 2 && (std::string(argv[1]) == "--serve" || std::string(argv[1]) == "--serve-empty" || std::string(argv[1]) == "--serve-calibration")) {
            std::string serving = address;
            if (std::string(argv[1]) == "--serve-calibration") serving = calibrationAddress;
            if (std::string(argv[1]) == "--serve-empty") {
                IceUtil::Handle<TestNode> empty = new TestNode;
                serving = communicator->proxyToString(adapter->addWithUUID(empty));
            }
            std::cout << serving << std::endl; std::string stop; std::getline(std::cin, stop);
            communicator->destroy(); return 0;
        }
        char path[] = "/tmp/getdaq-hardware-test-XXXXXX";
        Require(::mkdtemp(path) != nullptr, "Cannot create test directory");
        const std::string directory = path;
        std::vector<std::string> audits;
        IceUtil::Handle<TestNode> delayNode=new TestNode;delayNode->fName="DelayCompat";
        IceUtil::Handle<DelayWarningDevice> delayDevice=new DelayWarningDevice;
        delayNode->fDevices["ctrl"]=DevicePrx::uncheckedCast(adapter->addWithUUID(delayDevice));
        const auto delayAddress=communicator->proxyToString(adapter->addWithUUID(delayNode));
        const auto policyPath=directory+"/readback-warning-policy.json";audits.push_back(policyPath);
        for(int encoding=0;encoding<2;++encoding) {
            GetDAQ::HardwareClient compat(delayAddress,encoding,500);GetDAQ::HardwareDescription description;description.nodeId="DelayCompat";description.devices.push_back(compat.Describe("ctrl"));compat.UseDescription(description);
            GetDAQ::HardwareStep expect;expect.kind=GetDAQ::HardwareStep::ExpectValue;expect.device="ctrl";expect.reg="readDataDelay";expect.field="AGET0";expect.value=14;expect.mask=255;
            auto file=[&](const std::string &name){const auto path=directory+"/warning-"+std::to_string(encoding)+'-'+name;audits.push_back(path);return path;};
            ::unsetenv("GETDAQ_READBACK_WARNING_POLICY_FILE");
            Rejects([&]{compat.Execute({expect},file("strict"));},"expectation mismatch");
            {std::ofstream policy(policyPath);policy<<"{\"schema\":1,\"kind\":\"getdaq.read-data-delay-warning\",\"accepted\":true,\"accepted_readback\":\"255\",\"targets\":[{\"node_proxy\":"<<GetDAQ::JsonString(delayAddress)<<",\"node_name\":\"DelayCompat\"}]}\n";}
            Require(::chmod(policyPath.c_str(),0600)==0,"Cannot protect warning policy");::setenv("GETDAQ_READBACK_WARNING_POLICY_FILE",policyPath.c_str(),1);
            const auto accepted=file("accepted");compat.Execute({expect},accepted);Require(ReadFile(accepted).find("accepted_warning_unverified")!=std::string::npos,"Accepted warning was not audited");
            delayDevice->fValue=254;Rejects([&]{compat.Execute({expect},file("other-value"));},"expectation mismatch");delayDevice->fValue=255;
            auto critical=expect;critical.reg="pipeCtrl";critical.field="firmwareEnable";critical.value=0;critical.mask=1;Rejects([&]{compat.Execute({critical},file("critical"));},"expectation mismatch");
            auto write=expect;write.kind=GetDAQ::HardwareStep::WriteValue;delayDevice->fFailure=1;
            Rejects([&]{compat.Execute({write,expect},file("lost-write"));},"write outcome UNKNOWN");delayDevice->fFailure=0;delayDevice->fValue=255;
            {GetDAQ::HardwareClient other(delayAddress,encoding,500);description.nodeId="OtherNode";other.UseDescription(description);Rejects([&]{other.Execute({expect},file("other-node"));},"expectation mismatch");}
            expect.warningOnReadback255=true;Rejects([&]{compat.Execute({expect},file("forged-flag"));},"scoped policy");
            ::unsetenv("GETDAQ_READBACK_WARNING_POLICY_FILE");
        }
        std::cout<<"PASS scoped accepted readDataDelay warning: strict default, exact sentinel only, other checks/writes retained, both Ice encodings\n";
        if(argc==2 && std::string(argv[1])=="--warning-policy-only") {
            for(const auto &file:audits)::unlink(file.c_str());
            ::rmdir(directory.c_str());communicator->destroy();return 0;
        }

        for (int minor = 0; minor < 2; ++minor) {
            GetDAQ::HardwareClient client(address, minor, 500);
            Require(client.Name() == "test CoBo 中文", "Node name mismatch");
            Require(client.Devices().size() == 3, "Device inventory mismatch");
            auto info = client.Describe("ctrl");
            Require(info.registerBytes == 4 && info.registers[0].fields[0].width == 4, "Bytes versus bits mismatch");
            const int readsBefore = narrow->fReads, writesBefore = narrow->fWrites;
            std::string success = directory + "/ok" + std::to_string(minor); audits.push_back(success);
            client.Write("ctrl", "control", "", 0xffffffffULL, success);
            Require(narrow->fReads == readsBefore, "Write must not read a register implicitly");
            Require(client.Read("ctrl", "control") == 0xffffffffULL, "32 bit value mismatch");
            Rejects([&]() { client.Write("ctrl", "control", "", 0x100000000ULL, directory + "/overflow"); }, "width");
            Rejects([&]() { client.Write("ctrl", "control", "mode", 16, directory + "/field-overflow"); }, "width");
            Rejects([&]() { client.Write("ctrl", "mixed", "status", 1, directory + "/readonly"); }, "read-only");
            Rejects([&]() { client.Write("ctrl", "mixed", "", 1, directory + "/mixed"); }, "read-only");
            Rejects([&]() { client.Read("ctrl", "missing"); }, "Unknown hardware register");
            Rejects([&]() { client.Describe("invalid"); }, "field description");
            Rejects([&]() { client.Write("ctrl", "control", "", 1, success); }, "exclusively");
            Require(narrow->fWrites == writesBefore + 1, "Validation failure sent a write");
            Require(ReadFile(success).find("acknowledged") != std::string::npos, "Audit completion missing");
            std::string field = directory + "/field" + std::to_string(minor); audits.push_back(field);
            client.Write("ctrl", "control", "mode", 3, field);
            Require(client.Read("ctrl", "control") == 0xfffffff3ULL, "Field write changed other bits");
            std::string full = directory + "/wide" + std::to_string(minor); audits.push_back(full);
            client.Write("wide", "control", "", UINT64_MAX, full);
            Require(client.Read("wide", "control") == UINT64_MAX, "64 bit high bit lost");
            for (int failure = 1; failure <= 2; ++failure) {
                narrow->fFailure = failure;
                const int before = narrow->fWrites;
                std::string audit = directory + "/unknown" + std::to_string(minor) + std::to_string(failure); audits.push_back(audit);
                Rejects([&]() { client.Write("ctrl", "control", "", 1234, audit); }, "UNKNOWN");
                Require(narrow->fValue == 1234 && narrow->fWrites == before + 1, "Unknown write was retried");
                Require(ReadFile(audit).find("unknown") != std::string::npos, "Unknown audit missing");
                narrow->fFailure = 0;
            }
            GetDAQ::HardwareStep first; first.device = "ctrl"; first.reg = "control"; first.value = 7;
            GetDAQ::HardwareStep bad = first; bad.reg = "mixed"; bad.field = "status"; bad.value = 1;
            int before = narrow->fWrites;
            Rejects([&]() { client.Execute({first, bad}, directory + "/invalid-plan"); }, "read-only");
            Require(narrow->fWrites == before, "Plan preflight allowed a partial write");
            GetDAQ::HardwareStep second = first; second.device = "wide";
            std::string partial = directory + "/partial" + std::to_string(minor); audits.push_back(partial);
            wide->fFailure = 2;
            Rejects([&]() { client.Execute({first, second, first}, partial); }, "UNKNOWN at step 1");
            Require(narrow->fWrites == before + 1, "Plan continued after an unknown write");
            wide->fFailure = 0;
            std::string mismatch = directory + "/mismatch" + std::to_string(minor); audits.push_back(mismatch);
            second = first; second.kind = GetDAQ::HardwareStep::ExpectValue; second.value = 8;
            before = narrow->fWrites;
            Rejects([&]() { client.Execute({first, second, first}, mismatch); }, "expectation mismatch");
            Require(narrow->fWrites == before + 1, "Plan continued after mismatch");
            Require(ReadFile(mismatch).find("mismatch") != std::string::npos, "Mismatch audit missing");
            std::string pipeline = directory + "/pipeline" + std::to_string(minor); audits.push_back(pipeline);
            { std::lock_guard<std::mutex> lock(narrow->fMutex); narrow->fFields.clear(); }
            client.Execute(GetDAQ::CoBoPipelineInit(), pipeline);
            { std::lock_guard<std::mutex> lock(narrow->fMutex);
              Require(narrow->fFields.size() == 15, "Pipeline command count mismatch");
              Require(narrow->fFields[11] == "pipeCtrl.reset=1" && narrow->fFields[12] == "pipeCtrl.reset=0" &&
                  narrow->fFields.back() == "pipeCtrl.intEnable=0", "Pipeline reset/interrupt sequence mismatch"); }
            std::cout << "PASS hardware TCP 1." << minor << ": discovery, metadata, register/field I/O, bounds, audit, no retry\n";
            GetDAQ::HardwareClient calibrator(calibrationAddress, minor, 500);
            for (unsigned int mask : {1U, 5U, 10U, 15U}) {
                { std::lock_guard<std::mutex> lock(calibration.mutex); calibration.operations.clear(); calibration.failure = 0; }
                const std::string audit = directory + "/calibrate" + std::to_string(minor) + "-" + std::to_string(mask); audits.push_back(audit);
                calibrator.Execute(GetDAQ::CoBoAsAdCalibrate(mask), audit);
                std::lock_guard<std::mutex> lock(calibration.mutex);
                Require(calibration.mask == mask, "Active AsAd selection not restored");
                std::vector<std::string> reads;
                for (const auto &operation : calibration.operations) if (operation.find("read:") == 0) reads.push_back(operation);
                std::vector<std::string> expected;
                for (unsigned int board = 0; board < 4; ++board) if (mask & (1U << board)) {
                    Require(calibration.labels[board] == board + 1, "AsAd user label mismatch");
                    expected.push_back("read:" + std::to_string(board) + ":monitorID");
                    for (int i = 0; i < 4; ++i) expected.push_back("read:" + std::to_string(board) + ":AIM" + std::to_string(i));
                }
                Require(reads == expected, "AIM identity read order or inactive-board exclusion mismatch");
                Require(ReadFile(audit).find("observed") != std::string::npos && ReadFile(audit).find("complete") != std::string::npos,
                        "Calibration identity observations missing");
            }
            for (int failure = 1; failure <= 3; ++failure) {
                { std::lock_guard<std::mutex> lock(calibration.mutex); calibration.operations.clear(); calibration.failure = failure; }
                const std::string audit = directory + "/calibrate-failed" + std::to_string(minor) + std::to_string(failure); audits.push_back(audit);
                Rejects([&]() { calibrator.Execute(GetDAQ::CoBoAsAdCalibrate(5), audit); }, failure == 3 ? "read error" : "mismatch");
                std::lock_guard<std::mutex> lock(calibration.mutex);
                Require(calibration.mask == 1, "Calibration restored/began another board after failure");
                for (const auto &operation : calibration.operations)
                    Require(operation.find("read:2:") != 0, "Calibration continued after first board failed");
                Require(ReadFile(audit).find("\"phase\":\"complete\"") == std::string::npos, "Failed calibration logged complete");
            }
            { std::lock_guard<std::mutex> lock(calibration.mutex); calibration.failure = 0; calibration.operations.clear(); }
            Rejects([]() { GetDAQ::CoBoAsAdCalibrate(0); }, "mask");
            Rejects([]() { GetDAQ::CoBoAsAdCalibrate(16); }, "mask");
            auto invalidCalibration = GetDAQ::CoBoAsAdCalibrate(1); invalidCalibration.back().reg = "missing";
            Rejects([&]() { calibrator.Execute(invalidCalibration, directory + "/invalid-calibration"); }, "Unknown hardware register");
            { std::lock_guard<std::mutex> lock(calibration.mutex); Require(calibration.operations.empty(), "Calibration preflight sent a register operation"); }
            std::cout << "PASS AsAd calibration TCP 1." << minor << ": sparse masks, identifiers, ordered reads, failure stop, preflight\n";
            GetDAQ::HardwareClient agetClient(agetAddress, minor, 500);
            const auto board = AgetBoard();
            for (int scenario = 0; scenario < 10; ++scenario) {
                const int failure = scenario == 0 ? 0 : scenario <= 3 ? scenario : scenario == 4 ? 3 : scenario - 1;
                const bool allowReg2 = scenario == 4 || scenario == 5;
                auto options = board; options.allowReg2BankResetDifference = allowReg2; agetState.Reset(failure);
                const std::string audit = directory + "/aget" + std::to_string(minor) + "-" + std::to_string(scenario); audits.push_back(audit);
                if (scenario == 0 || scenario == 4) agetClient.Execute(GetDAQ::CoBoAgetConfigure(options), audit);
                else Rejects([&]() { agetClient.Execute(GetDAQ::CoBoAgetConfigure(options), audit); }, failure == 5 ? "UNKNOWN" : failure == 7 ? "read error" : "mismatch");
                std::lock_guard<std::mutex> lock(agetState.mutex); const auto history = ReadFile(audit);
                if (scenario == 0 || scenario == 4) {
                    Require(agetState.agetWrites == 39, "AGET registers not written exactly once per selected chip");
                    Require(agetState.control["ASAD_sc_mask.select"] == 11 && agetState.writeMask == 0x8081, "AGET/AsAd selection not restored");
                    Require(agetState.control["readDepth"] == 512 && agetState.control["readOffset.value"] == 0 && agetState.control["pipeCtrl.isFPNRead"] == 1, "Final shared CoBo parameters wrong");
                    for (unsigned int id : {0U,7U,15U}) {
                        Require(static_cast<std::uint64_t>(agetState.registers[id]["reg6"]) == UINT64_MAX, "High-bit AGET value lost");
                        Require(agetState.masks[id]["readIfHitMask1_4"] == 8 && agetState.masks[id]["readAlwaysMask1_4"] == 7 &&
                            static_cast<std::uint64_t>(agetState.masks[id]["readAlwaysMask5_36"]) == UINT32_MAX, "AGET readout masks in wrong bank");
                    }
                    const auto firstWrite = std::find(agetState.operations.begin(), agetState.operations.end(), "write:aget:0:reg1");
                    const auto lastVersion = std::find(agetState.operations.begin(), agetState.operations.end(), "read:aget:15:reg5");
                    Require(lastVersion != agetState.operations.end() && lastVersion < firstWrite, "AGET register write preceded all chip-version verification");
                    Require(history.find("\"phase\":\"complete\"") != std::string::npos, "AGET completion missing");
                    Require((history.find("matched_with_warning") != std::string::npos) == allowReg2, "Reg2 warning policy not audited");
                } else {
                    Require(history.find("\"phase\":\"complete\"") == std::string::npos, "Failed AGET flow marked complete");
                    if (failure == 1 || failure == 7 || failure == 8) Require(agetState.agetWrites == 0, "AGET registers changed after version/run precheck failure");
                    if (failure == 5) Require(agetState.agetWrites == 5 && history.find("unknown") != std::string::npos, "AGET unknown write retried or not audited");
                    Require(!agetState.registers[7].count("reg1") && !agetState.registers[15].count("reg1"), "AGET flow continued after failure");
                    if (failure == 8) for (const auto &op : agetState.operations) Require(op.find("write") != 0, "Running hardware guard sent a write");
                }
            }
            agetState.Reset(0); const auto metadata = agetCtrl->fConfig;
            for (auto it = agetCtrl->fConfig.registers.begin(); it != agetCtrl->fConfig.registers.end(); ++it) if (it->descr.name == "readOffset") { agetCtrl->fConfig.registers.erase(it); break; }
            Rejects([&]() { agetClient.Execute(GetDAQ::CoBoAgetConfigure(board), directory + "/aget-bad-metadata"); }, "Unknown hardware register");
            { std::lock_guard<std::mutex> lock(agetState.mutex); Require(agetState.operations.empty(), "AGET preflight allowed earlier hardware operations"); }
            agetCtrl->fConfig = metadata;
            auto invalidDifference = GetDAQ::CoBoAgetConfigure(board); invalidDifference.back().allowedDifference = UINT64_C(1) << 40;
            Rejects([&]() { agetClient.Execute(invalidDifference, directory + "/aget-bad-policy"); }, "Allowed difference");
            invalidDifference = GetDAQ::CoBoAgetConfigure(board);
            for (auto &step : invalidDifference) if (step.kind == GetDAQ::HardwareStep::Delay) step.allowedDifference = 1;
            Rejects([&]() { agetClient.Execute(invalidDifference, directory + "/aget-bad-delay-policy"); }, "Allowed difference");
            { std::lock_guard<std::mutex> lock(agetState.mutex); Require(agetState.operations.empty(), "Invalid difference policy sent a hardware operation"); }
            std::cout << "PASS CoBo AGET TCP 1." << minor << ": mixed versions, high bits, selection/masks, readback, warning policy and failure stop\n";
            auto readout = GetDAQTest::Readout(11); readout.module["dataSource"] = "254"; readout.module["coboId"] = "255";
            readout.module["triggerPeriod_10ns"] = "4294967295"; readout.module["secondaryTriggerMode"] = "onMultiplicity";
            readout.aget.chips[0].settings.channels[0].active = false; readout.chips[0].zeroSuppressionThresholds[0] = 4095;
            for (int scenario = 0; scenario < 6; ++scenario) {
                auto settings = readout; if (scenario == 1) settings.bufferLayout = GetDAQ::CoBoReadoutSettings::SharedBuffer;
                const int failure = scenario < 2 ? 0 : scenario + 7; agetState.Reset(failure);
                const std::string audit = directory + "/readout" + std::to_string(minor) + '-' + std::to_string(scenario); audits.push_back(audit);
                if (scenario < 2) agetClient.Execute(GetDAQ::CoBoReadoutConfigure(settings), audit);
                else Rejects([&]() { agetClient.Execute(GetDAQ::CoBoReadoutConfigure(settings), audit); }, failure == 10 ? "UNKNOWN" : "mismatch");
                std::lock_guard<std::mutex> lock(agetState.mutex); const auto history = ReadFile(audit);
                if (scenario < 2) {
                    Require(agetState.agetWrites == 39 && agetState.zeroWrites == 204, "Readout AGET/zero thresholds missing or replayed");
                    Require(agetState.control["zeroSuppress:aget0ch0.threshold"] == 4095 && agetState.control["zeroSuppress:aget0ch11.threshold"] == 11,
                        "Inactive/FPN thresholds skipped");
                    Require(agetState.control["zeroSuppress_AsAd3:aget0ch67.threshold"] == 271, "Sparse board zero target wrong");
                    Require(static_cast<std::uint64_t>(agetState.control["triggerPeriod_10ns"]) == UINT32_MAX && agetState.control["COBO_index.d"] == 255 &&
                        agetState.control["dataSource.value"] == 254, "Trigger period/header identifiers lost high bits");
                    Require(agetState.control["pipeCtrl.suppressMultiplicity"] == 14 && agetState.control["pipeCtrl.partialReadOut"] == 1 &&
                        agetState.control["triggerMask.external"] == 1 && agetState.control["triggerMask.multiplicity_out"] == 1 &&
                        agetState.control["triggerMask.external_2p"] == 0, "Readout pipe/trigger bits wrong");
                    if (scenario == 0) Require(agetState.control["readPtr_asad3"] == 0xc000000 && agetState.control["memEnd_asad3"] == 0xe000000, "Sparse three-board buffers lost quarter rule");
                    else Require(agetState.control["readPtr"] == 0x8000000 && agetState.control["memEnd"] == 0x10000000 &&
                        !agetState.control.count("readPtr_asad0"), "Shared buffer layout wrote per-AsAd pointers");
                    Require(history.find("\"phase\":\"complete\"") != std::string::npos, "Readout completion missing");
                } else {
                    Require(history.find("\"phase\":\"complete\"") == std::string::npos, "Failed readout marked complete");
                    if (failure == 9 || failure == 10) Require(agetState.zeroWrites == (failure == 9 ? 12 : 11) && agetState.agetWrites == 39,
                        "Zero readback/write failure replayed/continued");
                    if (failure == 10) Require(history.find("unknown") != std::string::npos, "Zero write uncertainty not audited");
                    if (failure == 11) Require(agetState.zeroWrites == 0 && agetState.agetWrites == 0, "Version failure wrote configuration");
                    if (failure == 12) Require(agetState.zeroWrites == 204 && agetState.operations.back() == "read-field:pipeCtrl.zeroSuppressInvert",
                        "Module readback failure continued after mismatch");
                }
            }
            agetState.Reset(0);
            const auto moduleMetadata = agetCtrl->fConfig;
            for (auto it = agetCtrl->fConfig.registers.begin(); it != agetCtrl->fConfig.registers.end(); ++it) if (it->descr.name == "aEmptyLevel") { agetCtrl->fConfig.registers.erase(it); break; }
            Rejects([&]() { agetClient.Execute(GetDAQ::CoBoReadoutConfigure(readout), directory + "/readout-metadata"); }, "Unknown hardware register");
            { std::lock_guard<std::mutex> lock(agetState.mutex); Require(agetState.operations.empty(), "Late Module preflight allowed AGET/control operations"); }
            agetCtrl->fConfig = moduleMetadata;
            auto largest = GetDAQTest::Readout(15);
            for (unsigned int boardId = 0; boardId < 4; ++boardId) for (unsigned int chipId = 1; chipId < 4; ++chipId) {
                auto a = largest.aget.chips[boardId]; a.agetId = chipId; largest.aget.chips.push_back(a);
                auto z = largest.chips[boardId]; z.agetId = chipId; largest.chips.push_back(z);
            }
            Require(GetDAQ::CoBoReadoutConfigure(largest).size() <= 4096, "Full 16-chip readout exceeds plan limit");
            std::cout << "PASS CoBo readout TCP 1." << minor << ": trigger/header, zero thresholds, buffers, strict preflight and unknown/no retry\n";
            GetDAQ::HardwareClient configureClient(configureAddress, minor, 500);
            GetDAQ::HardwareDescription configurePolicy; configurePolicy.nodeId = "configure";
            for (const auto &name : configureClient.Devices()) configurePolicy.devices.push_back(configureClient.Describe(name));
            auto configuration = GetDAQTest::Configure(11);
            configureState.Reset(0);
            Rejects([&] { configureClient.Execute(GetDAQ::CoBoConfigure(configuration), directory + "/configure-unbound"); }, "bound hardware description");
            Require(configureState.operations.empty(), "Unbound manager caused hardware operations");
            configureClient.UseDescription(configurePolicy);
            for (int scenario = 0; scenario < 6; ++scenario) {
                auto settings = configuration;
                settings.asad.generator["externalTriggerMode"] = scenario == 1 ? "true" : "false";
                settings.asad.generator["periodicModeEnabled"] = scenario == 1 ? "true" : "false";
                settings.asad.generator["doublePulseEnabled"] = "true"; settings.asad.generator["poissonModeEnabled"] = "true";
                const int failure = scenario < 2 ? 0 : scenario == 2 ? 13 : scenario == 3 ? 14 : scenario == 4 ? 11 : 15;
                configureState.Reset(failure);
                const auto audit = directory + "/configure" + std::to_string(minor) + '-' + std::to_string(scenario); audits.push_back(audit);
                if (scenario < 2) configureClient.Execute(GetDAQ::CoBoConfigure(settings), audit);
                else Rejects([&] { configureClient.Execute(GetDAQ::CoBoConfigure(settings), audit); }, failure == 13 || failure == 15 ? "UNKNOWN" : "mismatch");
                std::lock_guard<std::mutex> lock(configureState.mutex); const auto history = ReadFile(audit);
                if (scenario < 2) {
                    const auto &ops = configureState.operations;
                    Require(std::count(ops.begin(), ops.end(), "write:ctrl:aget_sc_rw") == 4,
                        "Configure omitted slow-control switch before an active AsAd");
                    unsigned int slowCount = 0, tcmCount = 0;
                    for (const auto &op : ops) {
                        if (op == "write:ctrl:aget_sc_rw") ++slowCount;
                        if (op == "write-asad-field:TCM0.Calib") {
                            ++tcmCount;
                            Require(slowCount == tcmCount + 1, "TCM configured before its own slow-control switch");
                        }
                    }
                    Require(tcmCount == 3, "Active AsAd TCM sequence missing");
                    Require(configureState.defaultCalls == 3 && configureState.triggerCalls == (scenario == 1 ? 3U : 0U), "AsAd pulser count/selection");
                    Require(configureState.asad[2].empty() && configureState.asad[3]["TCM0.Rg"] == 1 && configureState.asad[1]["TCM0.Calib"] == 1,
                        "Inactive AsAd touched or TCM settings wrong");
                    const unsigned int code = scenario == 1 ? 15430 : 8089;
                    for (unsigned int id : {0U, 1U, 3U}) Require(configureState.asad[id]["ISPM"] == 5 &&
                        configureState.asad[id]["TCM1"] == (code >> 8) && configureState.asad[id]["TCM2"] == (code & 0xff), "DAC/inspection bank selection");
                    Require(configureState.control["ASAD_sc_mask.select"] == 11 && configureState.control["aget_2p_initNow"] == 0,
                        "Mask restore/2p falling edge missing");
                    Require(configureState.startCalls == 0 && configureState.stopCalls == 1 && !configureState.periodicRunning, "Configure started pulser timer");
                    Require(configureState.period == (scenario == 1 ? 1000U : 0U) && configureState.secondMv == (scenario == 1 ? -300 : 0) &&
                        configureState.random == (scenario == 1), "Periodic/double/Poisson configuration missing");
                    Require(history.find("\"operation\":\"delay\",\"milliseconds\":20") != std::string::npos &&
                        history.find("\"phase\":\"complete\"") != std::string::npos, "2p delay/completion audit missing");
                } else {
                    Require(history.find("\"phase\":\"complete\"") == std::string::npos && !configureState.control.count("aget_2p_initNow"), "Failed Configure continued into 2p");
                    if (failure == 13 || failure == 14) Require(configureState.defaultCalls == 1 && configureState.asad[3].empty(), "Unknown/mismatching DAC replayed or continued");
                    if (failure == 13 || failure == 15) Require(history.find("\"phase\":\"unknown\"") != std::string::npos, "Manager failure not audited");
                    if (failure == 11) Require(configureState.agetWrites == 0 && configureState.zeroWrites == 0 && configureState.defaultCalls == 0, "Version error reached AsAd");
                }
            }
            configureState.Reset(0); const auto asadMetadata = configureAsAd->fConfig;
            auto &asadRegisters = configureAsAd->fConfig.registers;
            asadRegisters.erase(std::find_if(asadRegisters.begin(), asadRegisters.end(), [](const RegisterConfig &r) { return r.descr.name == "ISPM"; }));
            Rejects([&] { configureClient.Execute(GetDAQ::CoBoConfigure(configuration), directory + "/configure-missing-ispm"); }, "differs from bound description");
            Require(configureState.operations.empty(), "Late AsAd metadata error sent hardware command"); configureAsAd->fConfig = asadMetadata;
            auto readOnlyPolicy = configurePolicy;
            for (auto &device : readOnlyPolicy.devices) if (device.name == "asad") for (auto &reg : device.registers) if (reg.name == "TCM1") reg.readOnly = true;
            configureClient.UseDescription(readOnlyPolicy);
            Rejects([&] { configureClient.Execute(GetDAQ::CoBoConfigure(configuration), directory + "/configure-manager-readonly"); }, "read-only");
            Require(configureState.operations.empty(), "Manager bypassed local register permissions"); configureClient.UseDescription(configurePolicy);
            GetDAQ::HardwareClient plainManager(agetAddress, minor, 500); GetDAQ::HardwareDescription plainPolicy; plainPolicy.nodeId = "plain";
            for (const auto &name : plainManager.Devices()) plainPolicy.devices.push_back(plainManager.Describe(name));
            plainManager.UseDescription(plainPolicy);
            Rejects([&] { plainManager.Execute(GetDAQ::CoBoConfigure(configuration), directory + "/configure-no-manager"); }, "does not implement AsAdPulserMgr");
            auto allConfigure = GetDAQTest::Configure(15); allConfigure.readout = largest;
            Require(GetDAQ::CoBoConfigure(allConfigure).size() <= 4096, "Full Configure plan exceeds 4096 steps");
            GetDAQ::HardwareStep timer; timer.kind = GetDAQ::HardwareStep::PulserCommand; timer.pulserOperation = GetDAQ::HardwareStep::StartPeriodic;
            auto timerStop = timer; timerStop.pulserOperation = GetDAQ::HardwareStep::StopPeriodic;
            const auto timerAudit = directory + "/pulser-start-stop" + std::to_string(minor); audits.push_back(timerAudit);
            configureClient.Execute({timer, timerStop}, timerAudit);
            Require(configureState.startCalls == 1 && configureState.stopCalls == 1 && !configureState.periodicRunning, "Explicit timer operations failed");
            auto invalidPulser = timer; invalidPulser.voltageMv = 1001;
            Rejects([&] { configureClient.Execute({timer, invalidPulser}, directory + "/invalid-pulser"); }, "Invalid pulser");
            Require(configureState.startCalls == 1, "Late invalid pulser argument passed preflight");
            std::cout << "PASS CoBo Configure TCP 1." << minor << ": AsAd/DAC/inspection, manager actions, 2p pulse, permissions and no replay\n";
            // Prepare 的确定性子流程；这里不把它们连成 Prepared 状态。
            GetDAQ::CoBoPrepareFoundationSettings foundation;
            foundation.lemo = {{"trigger_pulse_output", "busy_output", "external_trigger_input", "external_timestamp_reset_input"}};
            const auto prepareAudit = directory + "/prepare-foundation" + std::to_string(minor); audits.push_back(prepareAudit);
            configureState.Reset(0); configureState.control["hardwareVersion"] = 2;
            configureState.control["firmwareRelease"] = 26; configureState.control["firmwareDate"] = 0x20190529;
            configureClient.Execute(GetDAQ::CoBoPrepareFoundation(foundation), prepareAudit);
            Require(configureState.control["mutantConfig.initDone"] == 0 && configureState.control["lemoMux.L3"] == 15 &&
                configureState.control["readDepth"] == 512 && configureState.control["triggerPeriod_10ns"] == 500000000 &&
                configureState.control["memEnd_asad3"] == 0x10000000 && configureState.control["readPtr_asad2"] == 0x0c000000 &&
                configureState.control["pipeCtrl.reset"] == 0 && configureState.control["pipeCtrl.intEnable"] == 0,
                "Prepare foundation defaults/half-open buffers or stop state wrong");
            Require(ReadFile(prepareAudit).find("\"phase\":\"complete\"") != std::string::npos, "Prepare foundation not audited");
            configureState.Reset(0); configureState.control["hardwareVersion"] = 1;
            const auto versionAudit = directory + "/prepare-version" + std::to_string(minor); audits.push_back(versionAudit);
            Rejects([&] { configureClient.Execute(GetDAQ::CoBoPrepareFoundation(foundation), versionAudit); }, "mismatch");
            Require(!configureState.control.count("pipeCtrl.reset") && !configureState.control.count("lemoMux.L0"), "Wrong hardware version reached initialization");
            auto legacyFoundation = foundation; legacyFoundation.hardwareVersion = 1; legacyFoundation.verifyHardwareVersion = false;
            legacyFoundation.firmwareInfoRegisters = legacyFoundation.mutantStatusRegister = legacyFoundation.interruptRateRegister = false;
            legacyFoundation.bufferLayout = GetDAQ::CoBoReadoutSettings::SharedBuffer;
            const auto controlMetadata = configureCtrl->fConfig;
            auto &controlRegisters = configureCtrl->fConfig.registers;
            controlRegisters.erase(std::remove_if(controlRegisters.begin(), controlRegisters.end(), [](const RegisterConfig &r) {
                return r.descr.name == "hardwareVersion" || r.descr.name == "firmwareRelease" || r.descr.name == "firmwareDate" ||
                    r.descr.name == "mutantConfig" || r.descr.name == "lemoMux" || r.descr.name == "interruptRate" || r.descr.name.find("_asad") != std::string::npos;
            }), controlRegisters.end());
            GetDAQ::HardwareClient legacyClient(configureAddress, minor, 500);
            GetDAQ::HardwareDescription legacyPolicy; legacyPolicy.nodeId = "legacy";
            for (const auto &name : legacyClient.Devices()) legacyPolicy.devices.push_back(legacyClient.Describe(name));
            legacyClient.UseDescription(legacyPolicy); configureState.Reset(0);
            const auto legacyAudit = directory + "/prepare-legacy" + std::to_string(minor); audits.push_back(legacyAudit);
            legacyClient.Execute(GetDAQ::CoBoPrepareFoundation(legacyFoundation), legacyAudit);
            Require(configureState.control["memEnd"] == 0x10000000 && configureState.control["readPtr"] == 0x08000000 &&
                !configureState.control.count("lemoMux.L0") && !configureState.control.count("memStart_asad0"), "Explicit legacy layout used unavailable registers");
            configureCtrl->fConfig = controlMetadata; configureClient.UseDescription(configurePolicy);
            configureState.Reset(0);
            const auto missingControl = std::find_if(controlRegisters.begin(), controlRegisters.end(), [](const RegisterConfig &r) { return r.descr.name == "aEmptyLevel"; });
            controlRegisters.erase(missingControl);
            Rejects([&] { configureClient.Execute(GetDAQ::CoBoPrepareFoundation(foundation), directory + "/prepare-metadata"); }, "differs from bound description");
            Require(configureState.operations.empty(), "Late Prepare metadata failure performed earlier reset");
            configureCtrl->fConfig = controlMetadata;
            GetDAQ::AsAdInputSettings inputs; inputs.asadMask = 11;
            for (unsigned int id : {0U, 1U, 3U}) {
                GetDAQ::AsAdInputBoardSettings b; b.asadId = id;
                for (unsigned int pos = 0; pos < 4; ++pos) b.positions[pos].active = id == 3 || (id == 0 && (pos == 0 || pos == 2)) || (id == 1 && pos == 3);
                if (id == 0) { b.positions[0].enabled = false; b.positions[0].connectGroundToZAP = true; b.positions[2].antiSparklingEnabled = false; }
                inputs.boards.push_back(b);
            }
            configureState.Reset(0);
            const auto protectAudit = directory + "/prepare-protect" + std::to_string(minor); audits.push_back(protectAudit);
            configureClient.Execute(GetDAQ::CoBoAsAdInputProtections(11), protectAudit);
            for (unsigned int id : {0U, 1U, 3U}) Require(configureState.asad[id]["IM0"] == 15 && configureState.asad[id]["IM1"] == 0, "Initial input protections missing");
            Require(configureState.asad[2].empty(), "Protection wrote inactive AsAd");
            const auto inputsAudit = directory + "/prepare-inputs" + std::to_string(minor); audits.push_back(inputsAudit);
            configureClient.Execute(GetDAQ::CoBoAsAdInputsInit(inputs), inputsAudit);
            Require(configureState.asad[0]["ADC0"] == 0x16 && configureState.asad[0]["IM0"] == 0x1b && configureState.asad[0]["IM1"] == 0x44 &&
                configureState.asad[1]["ADC0"] == 0x0e && configureState.asad[1]["IM1"] == 0x88 && configureState.asad[3]["ADC0"] == 0 &&
                configureState.asad[3]["IM1"] == 255 && configureState.asad[2].empty() && configureState.control["ASAD_sc_mask.select"] == 11,
                "Input slot polarity/bank selection or restore wrong");
            for (bool ddr : {true, false}) {
                configureState.Reset(0);
                const auto adcAudit = directory + "/prepare-adc" + std::to_string(minor) + (ddr ? "-ddr" : "-sdr"); audits.push_back(adcAudit);
                configureClient.Execute(GetDAQ::CoBoAsAdAdcInit(11, ddr), adcAudit);
                for (unsigned int id : {0U, 1U, 3U}) Require(configureState.asad[id]["ADC0"] == 0 && configureState.asad[id]["ADC2"] == 0x200 &&
                    configureState.asad[id]["ADC3"] == 0x0c3 && configureState.asad[id]["ADC5"] == (ddr ? 0x401 : 0x403), "ADC reset/defaults wrong");
                Require(configureState.asad[2].empty() && configureState.control["pipeCtrl.adcDeskew"] == 0 &&
                    configureState.control["pipeCtrl.bitSlipCalibrate"] == 0, "ADC active mask or calibration pulse wrong");
                const auto &ops = configureState.operations;
                Require(std::count(ops.begin(), ops.end(), "write-asad:ADC2") == 3 &&
                    std::find(ops.begin(), ops.end(), "write-field:pipeCtrl.adcDeskew=1") <
                    std::find(ops.begin(), ops.end(), "write-field:pipeCtrl.bitSlipCalibrate=1"), "ADC deskew/calibration order wrong");
                const auto history = ReadFile(adcAudit);
                const auto firstDelay = history.find("\"operation\":\"delay\",\"milliseconds\":1000");
                Require(firstDelay != std::string::npos && history.find("\"operation\":\"delay\",\"milliseconds\":1000", firstDelay + 1) != std::string::npos,
                    "ADC waits missing from audit");
            }
            const auto dacAudit = directory + "/prepare-dac" + std::to_string(minor); audits.push_back(dacAudit);
            configureClient.Execute(GetDAQ::CoBoAsAdDacInit(11), dacAudit);
            for (unsigned int id : {0U, 1U, 3U}) Require(configureState.asad[id]["DAC0"] == 0x81 && configureState.asad[id]["DAC6"] == 0 &&
                !configureState.asad[id].count("DAC2"), "DAC defaults or readonly VERSION wrong");
            const auto delayAudit = directory + "/prepare-manual-delay" + std::to_string(minor); audits.push_back(delayAudit);
            configureClient.Execute(GetDAQ::CoBoManualReadDataDelay(255), delayAudit);
            Require(configureState.control["readDataDelay.AGET3"] == 255, "Manual delay upper limit");
            Rejects([] { GetDAQ::CoBoManualReadDataDelay(256); }, "0..255");
            Rejects([] { GetDAQ::CoBoAsAdAdcInit(0); }, "1..15");
            Rejects([] { GetDAQ::CoBoAsAdDacInit(16); }, "1..15");
            Rejects([] { GetDAQ::CoBoSwitchToSlowControl(0); }, "1..65535");
            Rejects([] { GetDAQ::CoBoSwitchToSlowControl(65536); }, "1..65535");
            configureState.Reset(16);
            auto adcThenDac = GetDAQ::CoBoAsAdAdcInit(11); const auto dac = GetDAQ::CoBoAsAdDacInit(11);
            adcThenDac.insert(adcThenDac.end(), dac.begin(), dac.end());
            const auto dropAdcAudit = directory + "/prepare-adc-drop" + std::to_string(minor); audits.push_back(dropAdcAudit);
            Rejects([&] { configureClient.Execute(adcThenDac, dropAdcAudit); }, "UNKNOWN");
            Require(std::count(configureState.operations.begin(), configureState.operations.end(), "write-asad:ADC5") == 1 &&
                !configureState.asad[0].count("DAC0") && !configureState.control.count("pipeCtrl.adcDeskew") &&
                ReadFile(dropAdcAudit).find("\"phase\":\"unknown\"") != std::string::npos,
                "Applied ADC write replayed or Prepare continued after dropped response");
            configureState.Reset(0); configureAsAd->fConfig.registers.pop_back(); // 最后一个 DAC7。
            Rejects([&] { configureClient.Execute(adcThenDac, directory + "/prepare-late-dac"); }, "differs from bound description");
            Require(configureState.operations.empty(), "Late missing DAC metadata allowed ADC reset"); configureAsAd->fConfig = asadMetadata;
            auto adcReadOnlyPolicy = configurePolicy;
            for (auto &device : adcReadOnlyPolicy.devices) if (device.name == "asad") for (auto &reg : device.registers) if (reg.name == "ADC0") reg.readOnly = true;
            configureClient.UseDescription(adcReadOnlyPolicy);
            Rejects([&] { configureClient.Execute(GetDAQ::CoBoAsAdAdcInit(11), directory + "/prepare-adc-readonly"); }, "read-only");
            Require(configureState.operations.empty(), "ADC reset bypassed local read-only policy"); configureClient.UseDescription(configurePolicy);
            configureState.Reset(8);
            const auto runningAudit = directory + "/prepare-running" + std::to_string(minor); audits.push_back(runningAudit);
            Rejects([&] { configureClient.Execute(GetDAQ::CoBoAsAdAdcInit(11), runningAudit); }, "mismatch");
            Require(!configureState.asad[0].count("ADC0"), "ADC reset during firmware acquisition");
            std::cout << "PASS Prepare subflows TCP 1." << minor << ": defaults/variants, inputs, write-only ADC, DAC, permissions and no replay\n";
            GetDAQ::HardwareDescription aliased; aliased.nodeId = "test";
            auto namedWide = client.Describe("wide"); namedWide.name = "wide"; aliased.devices.push_back(namedWide);
            client.UseDescription(aliased);
            Rejects([&]() { client.Read("wide", "control"); }, "name differs");
            GetDAQ::HardwareDescription description; description.nodeId = "created CoBo";
            GetDAQ::DeviceInfo described; described.name = "ctrl"; described.access = "test-memory"; described.registerBytes = 8;
            described.baseAddress = UINT64_C(0x8000000000000000);
            GetDAQ::RegisterInfo r; r.name = "readonly"; r.readOnly = true; r.offset = 16; described.registers.push_back(r);
            r.name = "flags"; r.readOnly = false; r.offset = 32;
            GetDAQ::FieldInfo f; f.name = "C/S"; f.width = 1; f.offset = 63; f.readOnly = true; r.fields.push_back(f);
            described.registers.push_back(r); description.devices.push_back(described);
            for (int failure = 0; failure < 4; ++failure) {
                IceUtil::Handle<TestNode> empty = new TestNode; empty->fCreateFailure = failure;
                const auto entry = adapter->addWithUUID(empty);
                GetDAQ::HardwareClient deploy(communicator->proxyToString(entry), minor, 500);
                const std::string audit = directory + "/create" + std::to_string(minor) + std::to_string(failure); audits.push_back(audit);
                auto bad = description; bad.devices[0].registers[1].fields[0].width = 2;
                Rejects([&]() { deploy.Create(bad, directory + "/bad-create"); }, "width");
                Require(empty->fCreates == 0, "Invalid description sent CREATE");
                DescribeBackend backend(deploy, description, audit); GetDAQ::EccStateMachine stateMachine(backend);
                if (failure == 1 || failure == 2) Rejects([&]() { stateMachine.Execute(GetDAQ::EccCommand::Describe); }, "UNKNOWN");
                else if (failure == 3) Rejects([&]() { stateMachine.Execute(GetDAQ::EccCommand::Describe); }, "verification failed");
                else {
                    stateMachine.Execute(GetDAQ::EccCommand::Describe);
                    Require(stateMachine.Status().state == GetDAQ::EccState::Described, "CREATE success not committed");
                    Rejects([&]() { stateMachine.Execute(GetDAQ::EccCommand::Prepare); }, "unavailable");
                    Require(!stateMachine.Status().outcomeUnknown && stateMachine.Status().state == GetDAQ::EccState::Described,
                            "Missing recipe advanced state or reported unknown writes");
                    Require(deploy.Describe("ctrl").baseAddress == described.baseAddress, "High address bit lost during CREATE");
                    Rejects([&]() { deploy.Write("ctrl", "readonly", "", 1, directory + "/policy-ro"); }, "read-only");
                    Rejects([&]() { deploy.Write("ctrl", "flags", "C/S", 1, directory + "/policy-field"); }, "read-only");
                    GetDAQ::HardwareClient rebound(communicator->proxyToString(entry), minor, 500);
                    rebound.UseDescription(description);
                    Rejects([&]() { rebound.Write("ctrl", "readonly", "", 1, directory + "/policy-rebound"); }, "read-only");
                    bad = description; bad.devices[0].registers[0].offset += 1; rebound.UseDescription(bad);
                    Rejects([&]() { rebound.Read("ctrl", "readonly"); }, "differs");
                }
                if (failure) {
                    Require(stateMachine.Status().outcomeUnknown && stateMachine.Status().state == GetDAQ::EccState::Idle,
                            "Partial CREATE reported as described");
                    Rejects([&]() { stateMachine.Execute(GetDAQ::EccCommand::Describe); }, "UNKNOWN");
                }
                Require(empty->fCreates == 1 && deploy.Devices().size() == 1, "CREATE retried or partial node disappeared");
                Rejects([&]() { deploy.Create(description, directory + "/second-create"); }, "not empty");
                Require(empty->fCreates == 1, "Nonempty node was recreated");
            }
            std::cout << "PASS CREATE TCP 1." << minor << ": preflight, partial failure, dropped reply, metadata verification, bound policy\n";
#ifdef GETDAQ_TEST_ECC_JOURNAL
            for (int failure : {0, 2}) {
                IceUtil::Handle<TestNode> persistentNode = new TestNode; persistentNode->fCreateFailure = failure;
                const auto persistentProxy = adapter->addWithUUID(persistentNode);
                GetDAQ::HardwareClient persistentClient(communicator->proxyToString(persistentProxy), minor, 500);
                const auto stem = directory + "/persistent" + std::to_string(minor) + std::to_string(failure);
                const auto audit = stem + "-hardware.jsonl", journalPath = stem + "-state.jsonl";
                audits.push_back(audit); audits.push_back(journalPath);
                const auto scope = communicator->proxyToString(persistentProxy) + "/" + description.nodeId;
                DescribeBackend backend(persistentClient, description, audit);
                {
                    GetDAQ::EccJournal journal(journalPath, scope); GetDAQ::EccStateMachine controller(backend, journal);
                    if (failure) Rejects([&]() { controller.Execute(GetDAQ::EccCommand::Describe, "describe-1", "metadata-v1"); }, "UNKNOWN");
                    else controller.Execute(GetDAQ::EccCommand::Describe, "describe-1", "metadata-v1");
                    Require(controller.Status().outcomeUnknown == (failure != 0), "Persistent controller lost hardware outcome");
                    Require(journal.Operation("describe-1").phase == (failure ? "unknown" : "complete"), "Native hardware outcome not persisted");
                }
                {
                    GetDAQ::EccJournal journal(journalPath, scope); GetDAQ::EccStateMachine restarted(backend, journal);
                    Require(restarted.Status().needsReconciliation && restarted.Status().outcomeUnknown == (failure != 0), "Restart forgot native board operation");
                    Rejects([&]() { restarted.Execute(GetDAQ::EccCommand::Describe, "describe-1", "metadata-v1"); }, failure ? "UNKNOWN" : "reconciliation");
                    Rejects([&]() { restarted.Reconcile("recover-1", GetDAQ::EccState::Described, "must verify real hardware first"); }, "not implemented");
                    Require(persistentNode->fCreates == 1, "Restart replayed native CREATE");
                }
            }
            std::cout << "PASS persistent controller TCP 1." << minor << ": native CREATE outcome, restart fence, no replay\n";
#endif
        }
        Require(GetDAQ::ParseUnsigned("18446744073709551615") == UINT64_MAX, "decimal precision lost");
        Require(GetDAQ::ParseUnsigned("0xFFFFFFFFFFFFFFFF") == UINT64_MAX, "hex precision lost");
        for (const auto &bad : {"", "-1", "18446744073709551616", "0x10000000000000000", " 1", "1x"})
            Rejects([&]() { GetDAQ::ParseUnsigned(bad); }, "");
        for (const auto &audit : audits) ::unlink(audit.c_str());
        ::rmdir(directory.c_str());
        communicator->destroy(); return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        if (communicator) { try { communicator->destroy(); } catch (...) {} }
        return 1;
    }
}
