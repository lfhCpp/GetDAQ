#ifndef GETDAQ_TEST_MUTANT_FIXTURE_H
#define GETDAQ_TEST_MUTANT_FIXTURE_H
#include "hardwareclient.h"
#include "mutantconfiguration.h"
#include "hardware_fixture.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <unistd.h>
namespace GetDAQMutantFixture {
using namespace GetDAQ;
using namespace GetDAQTest;
using namespace mdaq::hw;
inline void Require(bool condition,const char *message) { if (!condition) throw std::runtime_error(message); }
template<class F> void Rejects(F call,const std::string &message) {
    try { call(); } catch (const std::exception &e) { Require(std::string(e.what()).find(message) != std::string::npos,e.what()); return; }
    throw std::runtime_error("Expected failure: "+message);
}
inline std::uint64_t Bits(Ice::Long value) { std::uint64_t result; std::memcpy(&result,&value,8); return result; }
inline Ice::Long Wire(std::uint64_t value) { Ice::Long result; std::memcpy(&result,&value,8); return result; }
inline std::uint64_t Mask(unsigned int width) { return width == 64 ? UINT64_MAX : (1ULL<<width)-1; }
inline std::string ReadFile(const std::string &path) { std::ifstream f(path); return std::string(std::istreambuf_iterator<char>(f),{}); }
struct State {
    std::mutex mutex;
    std::map<std::string,std::uint64_t> values;
    std::vector<std::string> writes;
    int metadata = 0, commandReads = 0, failure = 0, failedWrites = 0;
    std::function<void(const std::string &)> onAction;
    void Reset() {
        std::lock_guard<std::mutex> lock(mutex); values.clear(); writes.clear(); metadata = commandReads = failure = failedWrites = 0;
        values["GPIO_SW_LEDS/BOARD_ID_PWR_STAT"] = 0x7f123456;
        values["GPIO_SW_LEDS/TDC_STAT_CARD_B_FLGS"] = 0x359a;
        values["CARD_B_EMC/PWR_STAT_B"] = 0xff; values["CARD_B_EMC/MUTANT_STATUS"] = 1<<8;
        values["GPIO_CLK_TREE/CLOCK_TREE_CONFIG1"] = 0x90000000 | (7<<12) | 11;
        values["GPIO_CLK_TREE/CLOCK_TREE_CONFIG2"] = 0x1b2 | (1<<9);
        values["wide/value"] = 0xfedcba9876543210ULL;
    }
};
class MutantDevice : public TestDevice {
    State &fState;
    const RegisterConfig &Reg(const std::string &name) const {
        for (const auto &reg : fConfig.registers) if (reg.descr.name == name) return reg;
        throw mdaq::utl::CmdException("Missing fixture register");
    }
    FieldDescription Field(const std::string &reg,const std::string &name) const {
        for (const auto &field : Reg(reg).fields) if (field.name == name) return field;
        throw mdaq::utl::CmdException("Missing fixture field");
    }
public:
    MutantDevice(const DeviceInfo &device,State &state) : TestDevice(device.registerBytes),fState(state) {
        fConfig.descr.name = device.name; fConfig.descr.registerAccess = device.access;
        fConfig.descr.baseAddress = Wire(device.baseAddress); fConfig.registers.clear();
        for (const auto &r : device.registers) {
            RegisterConfig reg; reg.descr.name = r.name; reg.descr.offset = Wire(r.offset);
            for (const auto &f : r.fields) {
                FieldDescription field; field.name = f.name; field.offset = f.offset; field.width = f.width;
                field.readOnly = false; reg.fields.push_back(field); // 旧板端不保留权限，主机策略必须保留。
            }
            fConfig.registers.push_back(reg);
        }
    }
    void getDeviceConfig(DeviceConfig &out,const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex); ++fState.metadata; out = fConfig;
    }
    Ice::Long readRegister(const std::string &reg,const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex); Reg(reg);
        if (reg == "TRIGGER_CMD") { ++fState.commandReads; Unsupported(); }
        return Wire(fState.values[fConfig.descr.name+'/'+reg]);
    }
    Ice::Long readField(const std::string &reg,const std::string &name,const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex); const auto field = Field(reg,name);
        if (reg == "TRIGGER_CMD" || (reg == "SCALER_CTRL" && name.substr(0,2) == "cs")) { ++fState.commandReads; Unsupported(); }
        if (reg == "CLOCK_TREE_CONFIG2" && name == "start_pulse") { ++fState.commandReads; Unsupported(); }
        if (fState.failure == 2 && fState.failedWrites) throw mdaq::utl::CmdException("Applied masked update then readback failure");
        return Wire((fState.values[fConfig.descr.name+'/'+reg]>>field.offset)&Mask(field.width));
    }
    void writeRegister(const std::string &reg,Ice::Long value,const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex); Reg(reg);
        fState.values[fConfig.descr.name+'/'+reg] = Bits(value)&Mask(8*fConfig.descr.registerWidth);
        fState.writes.push_back(fConfig.descr.name+'/'+reg+"/="+std::to_string(Bits(value)));
        if (fState.onAction) fState.onAction(fState.writes.back());
    }
    void writeField(const std::string &reg,const std::string &name,Ice::Long value,const Ice::Current &current) override {
        bool close = false;
        {
            std::lock_guard<std::mutex> lock(fState.mutex); const auto field = Field(reg,name);
            const auto mask = Mask(field.width)<<field.offset;
            auto &old = fState.values[fConfig.descr.name+'/'+reg]; old = (old&~mask)|((Bits(value)<<field.offset)&mask);
            fState.writes.push_back(fConfig.descr.name+'/'+reg+'/'+name+'='+std::to_string(Bits(value)));
            if ((reg == "TRIGGER_CMD" && name != "euc" && name != "eec") || (reg == "SCALER_CTRL" && name.substr(0,2) == "cs")) old &= ~mask;
            if (reg == "CLOCK_TREE_CONFIG2" && name == "start_pulse") old &= ~mask; // 命令可自清零，不能强求回显。
            if (reg == "CLOCK_TREE_CONFIG2" && name == "clk_path_sel" && (fState.failure == 1 || fState.failure == 2)) {
                ++fState.failedWrites; close = fState.failure == 1;
            }
            if (reg == "CLOCK_TREE_CONFIG2" && name == "start_pulse" && value == 1 && fState.failure == 3) {
                ++fState.failedWrites; close = true;
            }
            if (fState.onAction) fState.onAction(fState.writes.back());
        }
        if (close) current.con->close(true);
    }
};
inline void Rename(HardwareDescription &description,bool legacy,bool noTwoProtonCommand) {
    for (auto &device : description.devices) if (device.name == "CARD_B_EMC") for (auto &reg : device.registers) {
        if (legacy) {
            if (reg.name == "SHELF_MASK") reg.name = "MASTER_MASK";
            else if (reg.name == "AMC_TAG_ONLY") reg.name = "USER_REG1";
            else if (reg.name == "USER_REG") reg.name = "USER_REG2";
        }
        if (noTwoProtonCommand && reg.name == "TRIGGER_CMD")
            reg.fields.erase(std::remove_if(reg.fields.begin(),reg.fields.end(),[](const FieldInfo &f) { return f.name == "l2ptc"; }),reg.fields.end());
    }
}
}
#endif
