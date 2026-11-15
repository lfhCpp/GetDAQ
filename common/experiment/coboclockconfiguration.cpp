// Adapted from CEA GET asad/utl.cpp and CoBoPLLManager.cpp.
// Original CeCILL license retained in third_party/licenses/getbench/COPYING.
#include "coboclockconfiguration.h"
#include "coboclockconfiguration_p.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <locale>
#include <sstream>
#include "coboreadoutconfiguration.h"
#include <stdexcept>
namespace GetDAQ {
namespace {
struct Field { const char *name; unsigned int offset, width; };
const Field samplingFields[] = {{"FINDIV",57,7}, {"FBDIV",50,7}, {"OADIV",45,5}, {"OAMUX",32,3},
    {"FBSEL",24,2}, {"FBDLY",19,5}, {"xDlySel",18,1}, {"DlyGLA",13,5}, {"VCOSel",3,2}};
const Field readingFields[] = {{"FINDIV",57,7}, {"FBDIV",50,7}, {"OADIV",45,5}, {"OBDIV",40,5},
    {"OAMUX",33,2}, {"OBMUX",30,2}, {"FBSEL",24,2}, {"FBDLY",19,5}, {"xDlySel",18,1}, {"DlyGLA",13,5}, {"DlyGLB",8,5}};
unsigned int Reverse(unsigned int value, unsigned int width)
{
    unsigned int result = 0;
    for (unsigned int i = 0; i < width; ++i) result = (result << 1) | ((value >> i) & 1);
    return result;
}
void Set(std::uint64_t &word, const Field &f, unsigned int value)
{
    const std::uint64_t mask = (UINT64_C(1) << f.width) - 1;
    if (value > mask) throw std::invalid_argument(std::string("AsAd clock field out of range: ") + f.name);
    word = (word & ~(mask << f.offset)) | (std::uint64_t(Reverse(value, f.width)) << f.offset);
}
unsigned int Get(std::uint64_t word, const Field &f)
{
    return Reverse(unsigned((word >> f.offset) & ((UINT64_C(1) << f.width) - 1)), f.width);
}
template<std::size_t N> void Patch(std::uint64_t &word, const Field (&fields)[N], const AsAdClockExpertFields &expert)
{
    if (expert.empty()) return;
    if (expert.size() != N) throw std::invalid_argument("AsAd clock expert mode requires all defined fields");
    for (const auto &f : fields) {
        const auto entry = expert.find(f.name);
        if (entry == expert.end()) throw std::invalid_argument(std::string("Missing AsAd clock expert field: ") + f.name);
        Set(word, f, entry->second);
    }
}

int DelayPs(unsigned int code) { return code == 0 ? 225 : 760 + int(code - 1) * 160; }
double Output(float input, unsigned int n, unsigned int m, unsigned int divider)
{
    return double(input) * (m + 1) / (double(n + 1) * (divider + 1));
}
}
const std::vector<AsAdClockParameter> &AsAdSamplingClockParameters()
{
    static const std::vector<AsAdClockParameter> parameters = [] {
        std::vector<AsAdClockParameter> result;
        for (const auto &f : samplingFields) result.push_back({f.name, f.width, std::string(f.name) == "xDlySel"});
        return result;
    }();
    return parameters;
}
const std::vector<AsAdClockParameter> &AsAdReadingClockParameters()
{
    static const std::vector<AsAdClockParameter> parameters = [] {
        std::vector<AsAdClockParameter> result;
        for (const auto &f : readingFields) result.push_back({f.name, f.width, std::string(f.name) == "xDlySel"});
        return result;
    }();
    return parameters;
}
unsigned int CompileReducedCoBoWritingDivider(float frequency)
{
    static const float clocks[] = {100, 50, 25, 12.5f, 6.25f, 3.125f};
    for (unsigned int i = 0; i < 6; ++i) if (frequency == clocks[i]) return i;
    throw std::invalid_argument("R-CoBo CKW must be one of 100,50,25,12.5,6.25,3.125MHz");
}
AsAdClockImage CompileAsAdSamplingClock(float ckw, float divider, const AsAdClockExpertFields &expert)
{
    CoBoClockInternal::Frequency(ckw, "CKW");
    if (!std::isfinite(divider) || divider <= 0) throw std::invalid_argument("AsAd sampling divider must be positive and finite");
    const float wck = ckw / divider; CoBoClockInternal::Frequency(wck, "AsAd WCK");
    unsigned int n = 18, m = 18, u = 0, vco = 2;
    bool standard = false;
    if (std::fabs(divider - 1.0f) < std::numeric_limits<float>::epsilon()) {
        static const float frequencies[] = {100, 50, 25, 12.5f, 6.25f, 3.125f};
        static const unsigned int ns[] = {18, 9, 4, 2, 1, 0}, ms[] = {18, 9, 4, 5, 7, 7};
        static const unsigned int us[] = {0, 0, 0, 1, 3, 7}, bands[] = {2, 1, 0, 0, 0, 0};
        for (unsigned int i = 0; i < 6; ++i) if (ckw == frequencies[i]) {
            n = ns[i]; m = ms[i]; u = us[i]; vco = bands[i]; standard = true; break;
        }
    } else if ((ckw == 100 || ckw == 50) && wck == 1) {
        n = ckw == 100 ? 19 : 9; m = 4; u = 24; vco = 0; standard = true;
    }
    if (!standard) {
        u = unsigned(std::ceil(24.0 / wck) - 1);
        // 对 >=24MHz，旧代码 ceil(24/f)-1 为 0，避免任何负数到无符号转换。
        m = unsigned(std::ceil((u + 1.0) * wck / 5.5) - 1);
        const float requestedN = float(m + 1) * divider / float(u + 1);
        n = std::min(std::max(unsigned(requestedN), 1U), 128U) - 1;
        const float vcoMHz = (u + 1) * wck;
        static const float limits[] = {40, 80, 150, 200};
        vco = 0; while (vco < 4 && limits[vco] < vcoMHz) ++vco;
        if (vco > 3) throw std::invalid_argument("AsAd sampling VCO exceeds representable band");
    }
    AsAdClockImage image; image.value = UINT64_C(0x48900001020000cf);
    Set(image.value, samplingFields[0], n); Set(image.value, samplingFields[1], m); Set(image.value, samplingFields[2], u);
    Set(image.value, samplingFields[3], 4); Set(image.value, samplingFields[4], 1);
    Set(image.value, samplingFields[7], 0); Set(image.value, samplingFields[8], vco);
    Patch(image.value, samplingFields, expert);
    const auto mux = Get(image.value, samplingFields[3]);
    image.nominalOutputKnown = mux == 0 || mux >= 4;
    image.nominalOutputMHz = !image.nominalOutputKnown ? 0 : mux == 0 ? ckw : Output(ckw, Get(image.value, samplingFields[0]), Get(image.value, samplingFields[1]), Get(image.value, samplingFields[2]));
    image.nominalMatchesRequested = image.nominalOutputKnown && std::fabs(image.nominalOutputMHz - wck) < std::numeric_limits<float>::epsilon();
    image.nominalWithinAgetRange = image.nominalOutputKnown && image.nominalOutputMHz >= 1 && image.nominalOutputMHz <= 200;
    return image;
}
AsAdClockImage CompileAsAdReadingClock(float ckr, int phase, int delay, const AsAdClockExpertFields &expert)
{
    if (!std::isfinite(ckr) || (ckr != 25 && ckr != 12.5f && ckr != 6.25f))
        throw std::invalid_argument("AsAd CKR must be 25,12.5 or 6.25MHz");
    if (phase != 0 && phase != 90 && phase != 180 && phase != 270)
        throw std::invalid_argument("ADC phase must be 0,90,180 or 270 degrees");
    if (delay < -5335 || delay > 5335) throw std::invalid_argument("ADC clock delay must be within -5335..5335ps");
    const unsigned int n = ckr == 25 ? 4 : ckr == 12.5f ? 2 : 1;
    const unsigned int m = ckr == 25 ? 4 : ckr == 12.5f ? 5 : 7;
    const unsigned int obmux = phase == 0 ? 2 : phase == 90 ? 1 : phase == 180 ? 0 : 3;
    unsigned int delayA = 0, delayB = 0;
    if (delay < -375) delayA = unsigned(1 + (-delay - 535) / 160);
    else if (delay >= 375 && delay < 5335) delayB = unsigned(1 + (delay - 535) / 160);
    else if (delay == 5335) delayB = 31;
    AsAdClockImage image; image.value = UINT64_C(0x20400001620000c7);
    Set(image.value, readingFields[0], n); Set(image.value, readingFields[1], m);
    Set(image.value, readingFields[2], 0); Set(image.value, readingFields[3], 0);
    Set(image.value, readingFields[4], 0); Set(image.value, readingFields[5], obmux);
    Set(image.value, readingFields[9], delayA); Set(image.value, readingFields[10], delayB);
    Patch(image.value, readingFields, expert);
    const unsigned int finalN = Get(image.value, readingFields[0]), finalM = Get(image.value, readingFields[1]);
    image.nominalOutputMHz = Output(ckr, finalN, finalM, Get(image.value, readingFields[2]));
    image.nominalAdcMHz = Output(ckr, finalN, finalM, Get(image.value, readingFields[3]));
    image.nominalMatchesRequested = std::fabs(image.nominalOutputMHz - 25) < std::numeric_limits<float>::epsilon() &&
        std::fabs(image.nominalAdcMHz - 25) < std::numeric_limits<float>::epsilon();
    image.nominalWithinAgetRange = image.nominalOutputMHz >= 1 && image.nominalOutputMHz <= 200 && image.nominalAdcMHz >= 1 && image.nominalAdcMHz <= 200;
    image.quantizedAdcDelayPs = DelayPs(Get(image.value, readingFields[10])) - DelayPs(Get(image.value, readingFields[9]));
    return image;
}
float ParseClockReal(const std::string &text, const std::string &name, double minimum, double maximum)
{
    if (text.empty() || text.size() > 64 || !std::isfinite(minimum) || !std::isfinite(maximum) || minimum > maximum)
        throw std::invalid_argument("Invalid clock number: " + name);
    for (char c : text) if ((c < '0' || c > '9') && c != '+' && c != '-' && c != '.' && c != 'e' && c != 'E')
        throw std::invalid_argument("Invalid decimal clock number: " + name);
    std::istringstream stream(text); stream.imbue(std::locale::classic()); stream >> std::noskipws;
    double value = 0; stream >> value;
    if (!stream || !stream.eof() || !std::isfinite(value) || value < minimum || value > maximum)
        throw std::invalid_argument("Clock number outside allowed range: " + name);
    const float result = float(value);
    if (!std::isfinite(result) || result < minimum || result > maximum)
        throw std::invalid_argument("Clock number cannot be represented: " + name);
    return result;
}
CoBoClockImage CompileCoBoClock(const CoBoClockSettings &s)
{
    if (!s.hardwareVersion || s.hardwareVersion == UINT32_MAX || (!s.verifyHardwareVersion && s.hardwareVersion != 1))
        throw std::invalid_argument("Invalid or unverifiable CoBo clock hardware version");
    if (!s.asadMask || s.asadMask > 15) throw std::invalid_argument("Active AsAd mask must be 1..15");
    CoBoClockInternal::Frequency(s.writingMHz,"CKW");
    if (s.readingMHz != 25 && s.readingMHz != 12.5f && (s.hardwareVersion == 1 || s.readingMHz != 6.25f))
        throw std::invalid_argument("Unsupported CoBo reading clock request");
    CoBoClockImage image; image.asadMask = s.asadMask;
    image.programPll = s.hardwareVersion >= 2 && !s.skipPllConfig;
    image.programWritingDivider = !image.programPll; image.programReadingDivider = s.hardwareVersion == 1;
    if (image.programPll) {
        std::map<unsigned int,std::uint32_t> registers = {{10,0x9140410a},{11,0x0400000b},{12,0x0b0c006c},{13,0x1b12902d}};
        for (const auto &entry : s.pll) {
            bool known = false; for (const auto &p : CoBoPllParameters()) if (entry.first == p.name) known = true;
            if (!known) throw std::invalid_argument("Unknown CoBo PLL parameter: " + entry.first);
        }
        for (const auto &p : CoBoPllParameters()) {
            const auto entry = s.pll.find(p.name); const auto value = entry == s.pll.end() ? p.defaultValue : entry->second;
            unsigned int number = 0;
            if (p.boolean) {
                if (value != "true" && value != "false") throw std::invalid_argument("PLL boolean must be true or false: " + p.name);
                number = value == "true";
            } else number = ParseCoBoUnsigned(value,p.name,(1U << p.width)-1);
            const auto mask = ((UINT32_C(1) << p.width)-1) << p.offset;
            registers[p.reg] = (registers[p.reg] & ~mask) | (number << p.offset);
        }
        image.pll = CompileCoBoPll(s.writingMHz,registers);
        image.nominalCoBoWritingMHz = image.pll.nominalDefaultWritingMHz;
        // 此为旧 2000MHz VCO 假设下的模型；专家修改设备模式/VCO 分频后不能推断上游频率。
        image.nominalUpstreamKnown = ((registers[10] >> 8) & 7) == 1 && ((registers[11] >> 27) & 31) == 0;
    } else {
        image.writingDivider = CompileReducedCoBoWritingDivider(s.writingMHz);
        image.nominalCoBoWritingMHz = s.writingMHz;
        if (s.hardwareVersion >= 2) image.nominalUpstreamKnown = false; // skipPLL 的旧 µTCA 分频字段不保证生效。
    }
    image.nominalTargetsMatched = image.nominalUpstreamKnown && std::fabs(image.nominalCoBoWritingMHz-s.writingMHz) < std::numeric_limits<float>::epsilon()
        && std::fabs(image.nominalCoBoReadingMHz-s.readingMHz) < std::numeric_limits<float>::epsilon();
    image.nominalKnownDifference = image.nominalUpstreamKnown && !image.nominalTargetsMatched;
    unsigned int seen = 0;
    for (const auto &b : s.boards) {
        if (b.asadId > 3 || !(s.asadMask & (1U << b.asadId)) || (seen & (1U << b.asadId)))
            throw std::invalid_argument("Invalid, inactive or duplicate AsAd clock board");
        seen |= 1U << b.asadId;
        AsAdBoardClockImage board; board.asadId = b.asadId;
        board.sampling = CompileAsAdSamplingClock(s.writingMHz,b.samplingDivider,b.samplingExpert);
        board.reading = CompileAsAdReadingClock(s.readingMHz,b.phaseDegrees,b.delayPs,b.readingExpert);
        board.effectiveNominalSamplingMHz = board.sampling.nominalOutputMHz * image.nominalCoBoWritingMHz/s.writingMHz;
        board.effectiveNominalReadingMHz = board.reading.nominalOutputMHz * image.nominalCoBoReadingMHz/s.readingMHz;
        board.effectiveNominalAdcMHz = board.reading.nominalAdcMHz * image.nominalCoBoReadingMHz/s.readingMHz;
        if (image.nominalUpstreamKnown) image.nominalKnownDifference = image.nominalKnownDifference ||
            (board.sampling.nominalOutputKnown && std::fabs(board.effectiveNominalSamplingMHz-double(s.writingMHz)/b.samplingDivider) >= std::numeric_limits<float>::epsilon()) ||
            std::fabs(board.effectiveNominalReadingMHz-25) >= std::numeric_limits<float>::epsilon() ||
            std::fabs(board.effectiveNominalAdcMHz-25) >= std::numeric_limits<float>::epsilon();
        image.nominalTargetsMatched = image.nominalTargetsMatched && board.sampling.nominalOutputKnown &&
            std::fabs(board.effectiveNominalSamplingMHz-double(s.writingMHz)/b.samplingDivider) < std::numeric_limits<float>::epsilon() &&
            std::fabs(board.effectiveNominalReadingMHz-25) < std::numeric_limits<float>::epsilon() && std::fabs(board.effectiveNominalAdcMHz-25) < std::numeric_limits<float>::epsilon();
        image.nominalOutputsWithinRange = image.nominalOutputsWithinRange && board.sampling.nominalOutputKnown &&
            board.effectiveNominalSamplingMHz >= 1 && board.effectiveNominalSamplingMHz <= 200 &&
            board.effectiveNominalReadingMHz >= 1 && board.effectiveNominalReadingMHz <= 200 &&
            board.effectiveNominalAdcMHz >= 1 && board.effectiveNominalAdcMHz <= 200;
        image.boards.push_back(board);
    }
    if (seen != s.asadMask) throw std::invalid_argument("Missing active AsAd clock board");
    std::sort(image.boards.begin(),image.boards.end(),[](const AsAdBoardClockImage &a,const AsAdBoardClockImage &b) { return a.asadId < b.asadId; });
    return image;
}
}
