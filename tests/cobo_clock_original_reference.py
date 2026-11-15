#!/usr/bin/env python3
"""Compile the received GET LCM algorithms as an independent standard-C++ oracle.

No GET/CCfg/Boost installation, networking, hardware or original-source edits.
The extracted CEA code keeps its source license and runs only in a temporary test.
"""
import hashlib
import json
import xml.etree.ElementTree as ET
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    root = Path(__file__).resolve().parents[1]
    original = root / 'source/get/GetBench-software-release-20210208/src/get/asad/utl.cpp'
    before = original.read_bytes()
    pll_original = root / 'source/get/GetBench-software-release-20210208/src/get/rc/CoBoPLLManager.cpp'
    pll_before = pll_original.read_bytes()
    parser_original = root / 'source/get/GetBench-software-release-20210208/src/get/rc/CoBoConfigParser.cpp'
    parser_before = parser_original.read_bytes()
    config_original = root / 'source/get/GetBench-software-release-20210208/data/config/configure-pedestals.xcfg'
    config_before = config_original.read_bytes()
    content = before.decode('utf-8')
    pll_content = pll_before.decode('utf-8')
    sampling = content[content.index('struct LCM1_uint64:'):content.index('void parseAsAdLcm1DebugConfig(')]
    reading = content[content.index('struct LCM2_uint64:'):content.index('void parseAsAdLcm2DebugConfig(')]
    license_header = content[:content.index('#include')]
    table_start = pll_content.index('const uint32_t CoBoPLLManager::PLL_DEFAULT_CONFIG')
    pll_table = pll_content[table_start:pll_content.index('};', table_start) + 2]

    def extract_function(content, signature):
        start = content.index(signature)
        brace = content.index('{', start)
        depth = 1
        end = brace + 1
        while depth:
            if content[end] == '{':
                depth += 1
            elif content[end] == '}':
                depth -= 1
            end += 1
        return content[start:end]

    pll_functions = '\n'.join(extract_function(pll_content, 'void CoBoPLLManager::' + name + '(') for name in ['setCKWFrequency', 'setRegisters', 'loadPLLConfig'])
    # 仅替换 Boost 集合构造语法，不改变原始分频、覆盖或第二包同步算法。
    pll_functions = pll_functions.replace('boost::assign::list_of(3.125)(6.25)(12.5)(25)(50)(100)',
                                          '{3.125f,6.25f,12.5f,25.0f,50.0f,100.0f}')
    pll_code = (pll_table + '\n' + pll_functions).replace('CoBoPLLManager::', 'OriginalPLL::')
    parser_code = extract_function(parser_before.decode('utf-8'), 'uint32_t CoBoConfigParser::getPllRegValue(').replace('CoBoConfigParser::', 'OriginalParser::')
    pll_node = ET.fromstring(config_before).find("./Node[@id='CoBo']/Instance[@id='*']/Module/PLL")
    if pll_node is None:
        raise RuntimeError('Received default PLL configuration missing')
    defaults = ','.join('{' + json.dumps('Module/PLL/' + node.tag) + ',' + json.dumps(node.text.strip()) + '}' for node in pll_node)
    baseline = 'const std::map<std::string,std::string> pllDefaults = {' + defaults + '};\n'
    support = r'''
#include "coboclockconfiguration.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <iterator>
#include <sstream>
#include <set>
#include <stdexcept>
struct QuietLog { template<class T> QuietLog &operator<<(const T &) { return *this; } } quietLog;
#define LOG_INFO() quietLog
#define LOG_DEBUG() quietLog
#define LOG_WARN() quietLog
namespace boost {
template<class T> std::string lexical_cast(float value) { std::ostringstream s; s << value; return s.str(); }
struct format { explicit format(const char *) {} template<class T> format &operator%(const T &) { return *this; } };
}
namespace mdaq { namespace utl { struct CmdException : std::runtime_error { using std::runtime_error::runtime_error; }; } }
struct RegField {
    template<unsigned int P, unsigned int W> static uint64_t getField(uint64_t word) { return (word >> P) & ((UINT64_C(1) << W) - 1); }
    template<unsigned int P, unsigned int W> static void setField(uint64_t &word, uint64_t v) {
        const uint64_t mask = (UINT64_C(1) << W) - 1; word = (word & ~(mask << P)) | ((v & mask) << P);
    }
};
struct BitField_uint64 {
    typedef uint64_t WordBasicType; typedef uint64_t WordType; uint64_t regValue = 0;
    uint64_t value() const { return regValue; } void setValue(uint64_t word) { regValue = word; }
};
namespace utl { template<class T> struct BitFieldHelper {
    static bool getBit(T word, unsigned int bit) { return (word >> bit) & 1; }
    static void setBit(T &word, unsigned int bit, bool value) { if (value) word |= T(1) << bit; else word &= ~(T(1) << bit); }
    static void setField(T &word, unsigned int position, unsigned int width, unsigned int value) {
        const T mask = (T(1) << width) - 1; word = (word & ~(mask << position)) | ((T(value) & mask) << position);
    }
}; }
namespace CCfg {
class CConfig {
    const std::map<std::string,std::string> *values_; std::string path_;
public:
    struct NotFound : std::runtime_error { using std::runtime_error::runtime_error; };
    explicit CConfig(const std::map<std::string,std::string> &values, const std::string &path = "") : values_(&values), path_(path) {}
    CConfig operator()(const std::string &tag) const { return CConfig(*values_,path_.empty() ? tag : path_ + '/' + tag); }
    const std::string &Value() const {
        const auto found = values_->find(path_); if (found == values_->end()) throw NotFound("Missing reference parameter: " + path_);
        return found->second;
    }
    int getIntValue() const { return std::stoi(Value()); }
    bool getBoolValue() const { if (Value() == "true") return true; if (Value() == "false") return false; throw std::runtime_error("Invalid reference boolean"); }
};
}
class OriginalParser { public: static uint32_t getPllRegValue(CCfg::CConfig, const size_t &); };
#define PLL_INFO() quietLog
#define PLL_DEBUG() quietLog
#define PLL_WARN() quietLog
class OriginalPLL {
public:
    typedef std::map<int16_t,int32_t> RegisterMap;
    static const uint32_t PLL_RESET_MASK = 0x20000;
    static const size_t LMK04800_REG_NUM = 26;
    static const uint32_t PLL_DEFAULT_CONFIG[26];
    uint32_t pllConfig_[26]; std::vector<uint32_t> packets;
    void setCKWFrequency(const float &,const RegisterMap &);
    void setRegisters(const RegisterMap &);
    void loadPLLConfig(float,const RegisterMap &);
    void resetWriteFIFO() { packets.clear(); } void resetReadFIFO() {}
    void writeToFIFO(uint32_t value) { packets.push_back(value); } bool isWriteFIFOFull() { return false; }
};
const uint32_t OriginalPLL::PLL_RESET_MASK;
const size_t OriginalPLL::LMK04800_REG_NUM;
'''
    driver = r'''
int main() {
    unsigned int samplingCases = 0, readingCases = 0, pllCases = 0, namedCases = 0;
    for (unsigned int i = 4; i <= 800; ++i) {
        const float clock = i / 4.0f;
        for (float divider : {0.5f,1.0f,2.0f,3.0f,4.0f,7.0f,100.0f}) {
            if (clock / divider < 1 || clock / divider > 200) continue;
            const auto expected = buildLcm1RegisterValue(clock, divider);
            const auto actual = GetDAQ::CompileAsAdSamplingClock(clock, divider);
            if (actual.value != expected) { std::cerr << "LCM1 mismatch " << clock << '/' << divider << '\n'; return 1; }
            ++samplingCases;
        }
    }
    // 3.125MHz 不在上述 0.25MHz 网格上，单独核对全部旧标准与两种 1MHz 特例。
    for (float clock : {3.125f,6.25f,12.5f,25.0f,50.0f,100.0f}) {
        if (GetDAQ::CompileAsAdSamplingClock(clock).value != buildLcm1RegisterValue(clock,1)) return 1;
        ++samplingCases;
    }
    for (float clock : {50.0f,100.0f}) {
        if (GetDAQ::CompileAsAdSamplingClock(clock,clock).value != buildLcm1RegisterValue(clock,clock)) return 1;
        ++samplingCases;
    }
    for (float clock : {25.0f,12.5f,6.25f}) for (int phase : {0,90,180,270}) for (int delay = -5335; delay <= 5335; ++delay) {
        if (GetDAQ::CompileAsAdReadingClock(clock,phase,delay).value != buildLcm2RegisterValue(clock,phase,delay)) {
            std::cerr << "LCM2 mismatch " << clock << '/' << phase << '/' << delay << '\n'; return 1;
        }
        ++readingCases;
    }
    const auto comparePLL = [&pllCases](float clock,const std::map<unsigned int,uint32_t> &overrides) {
        OriginalPLL old; OriginalPLL::RegisterMap wire;
        for (const auto &entry : overrides) { int32_t value; std::memcpy(&value,&entry.second,sizeof value); wire[int16_t(entry.first)] = value; }
        old.loadPLLConfig(clock,wire);
        if (GetDAQ::CompileCoBoPll(clock,overrides).packets != old.packets) throw std::runtime_error("PLL packet mismatch");
        ++pllCases;
    };
    for (unsigned int i = 4; i <= 800; ++i) comparePLL(i / 4.0f,{});
    for (float clock : {3.125f,6.25f,12.5f,25.0f,50.0f,100.0f}) comparePLL(clock,{});
    for (unsigned int id = 0; id <= 31; ++id) if (id <= 16 || id >= 24) comparePLL(100,{{id,0x80000000U | id}});
    comparePLL(100,{{11,0x0401000b},{13,0x1b12906d}});
    const auto compareNamed = [&namedCases](const std::map<std::string,std::string> &values, const std::map<std::string,std::string> &newValues) {
        CCfg::CConfig cfg(values); OriginalPLL old; OriginalPLL::RegisterMap registers;
        for (unsigned int reg = 10; reg <= 13; ++reg) {
            const uint32_t bits = OriginalParser::getPllRegValue(cfg,reg); int32_t wire;
            std::memcpy(&wire,&bits,sizeof wire); registers[int16_t(reg)] = wire;
        }
        old.loadPLLConfig(100,registers);
        GetDAQ::CoBoClockSettings settings; settings.asadMask = 1; settings.pll = newValues;
        GetDAQ::AsAdBoardClockSettings board; settings.boards.push_back(board);
        if (GetDAQ::CompileCoBoClock(settings).pll.packets != old.packets) throw std::runtime_error("Named PLL original-parser packet mismatch");
        ++namedCases;
    };
    compareNamed(pllDefaults,{}); // Defaults come from received XML, not the new descriptor table.
    for (const auto &p : GetDAQ::CoBoPllParameters()) for (unsigned int value = 0; value < (1U << p.width); ++value) {
        const std::string text = p.boolean ? (value ? "true" : "false") : std::to_string(value);
        auto values = pllDefaults; values["Module/PLL/" + p.name] = text;
        compareNamed(values,{{p.name,text}});
    }
    std::cout << "PASS received GET clock oracle: " << samplingCases << " LCM1, " << readingCases << " LCM2 and " << pllCases << " PLL and " << namedCases << " named original-parser cases\n";
}
'''
    with tempfile.TemporaryDirectory(prefix='getdaq-clock-reference-') as directory:
        source = Path(directory) / 'original_clock_reference.cpp'
        binary = Path(directory) / 'original_clock_reference'
        source.write_text(license_header + support + sampling + reading + pll_code + parser_code + baseline + driver, encoding='utf-8')
        compiler = sys.argv[1] if len(sys.argv) == 2 else 'g++'
        subprocess.run([compiler, '-std=c++11', '-pedantic-errors', '-O1', '-Wno-deprecated-declarations',
                        '-I' + str(root / 'common/experiment'), str(source),
                        str(root / 'common/experiment/coboclockconfiguration.cpp'),
                        str(root / 'common/experiment/coboreadoutconfiguration.cpp'),
                        str(root / 'common/experiment/coboagetconfiguration.cpp'),
                        str(root / 'common/experiment/agetconfiguration.cpp'), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
    if hashlib.sha256(original.read_bytes()).digest() != hashlib.sha256(before).digest():
        raise RuntimeError('Original GET source changed during reference test')
    if hashlib.sha256(pll_original.read_bytes()).digest() != hashlib.sha256(pll_before).digest():
        raise RuntimeError('Original GET PLL source changed during reference test')
    for path, before_bytes in [(parser_original, parser_before), (config_original, config_before)]:
        if hashlib.sha256(path.read_bytes()).digest() != hashlib.sha256(before_bytes).digest():
            raise RuntimeError('Original parser/configuration changed during reference test')


if __name__ == '__main__':
    main()
