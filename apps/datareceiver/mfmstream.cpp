#include "mfmstream.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace GetDAQ {
MfmStream::MfmStream(std::size_t maximumFrame, Sink sink, bool fdt, ControlSink controls)
    : fMaximum(maximumFrame), fExpected(fdt ? 4 : 8), fSink(std::move(sink)),
      fFdt(fdt), fControls(std::move(controls))
{
    if (fMaximum < 16 || fMaximum > 1024u * 1024u * 1024u) throw std::invalid_argument("max frame must be 16 B..1 GiB");
    fFrame.reserve(fMaximum);
}
bool MfmStream::ConsumeControl(const uint8_t *data)
{
    const uint64_t word = MfmHeaderInteger(data, 4, false);
    if (word != 0 && word != 0xffffffffu) return false;
    const Control control = word == 0 ? Heartbeat : Goodbye;
    if (control == Goodbye) fGoodbye = true;
    if (fControls) fControls(control);
    return true;
}
FrameInfo MfmStream::Inspect(const uint8_t *p, std::size_t bytes)
{
    return InspectMfmHeader(p,bytes,bytes);
}
void MfmStream::Feed(const uint8_t *data, std::size_t bytes)
{
    while (bytes) {
        if (fGoodbye) throw std::runtime_error("FDT bytes received after GOODBYE before TCP EOF");
        // 控制字只在帧边界识别；载荷中的全0/全FF必须原样保存。
        if (fFdt && fFrame.empty() && bytes >= 4 && ConsumeControl(data)) {
            data += 4; bytes -= 4;
            continue;
        }
        // 当前块已经含完整帧时同步借用它；跨块帧仍进入有界重组缓存。
        if (fFrame.empty() && bytes >= 8) {
            const std::size_t expected = MfmFrameBytes(data,bytes);
            if (expected < 8 || expected > fMaximum) throw std::runtime_error("MFM frame exceeds configured size bounds");
            if (bytes >= expected) {
                const FrameInfo info = Inspect(data, expected);
                fSink(data, info);
                data += expected; bytes -= expected;
                continue;
            }
        }
        const std::size_t amount = std::min(bytes, fExpected - fFrame.size());
        fFrame.insert(fFrame.end(), data, data + amount); data += amount; bytes -= amount;
        if (fFrame.size() != fExpected) continue;
        if (fExpected == 4) {
            if (ConsumeControl(fFrame.data())) {
                fFrame.clear();
                continue;
            }
            fExpected = 8;
            continue;
        }
        if (fExpected == 8) {
            fExpected = MfmFrameBytes(fFrame.data(),fFrame.size());
            if (fExpected < 8 || fExpected > fMaximum) throw std::runtime_error("MFM frame exceeds configured size bounds");
            if (fFrame.size() < fExpected) continue;
        }
        const FrameInfo info = Inspect(fFrame.data(), fFrame.size());
        fSink(fFrame.data(), info);
        fFrame.clear(); fExpected = fFdt ? 4 : 8;
    }
}
void MfmStream::EndConnection()
{
    Finish();
    fGoodbye = false;
    fExpected = fFdt ? 4 : 8;
}
void MfmStream::Finish() const
{
    if (!fFrame.empty()) throw std::runtime_error("Connection ended in a partial MFM frame: " + std::to_string(fFrame.size()) + " bytes buffered");
}
}
