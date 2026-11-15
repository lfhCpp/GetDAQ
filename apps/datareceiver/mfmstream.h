#ifndef GETDAQ_MFMSTREAM_H
#define GETDAQ_MFMSTREAM_H
#include "mfmheader.h"
#include <cstdint>
#include <cstddef>
#include <functional>
#include <vector>

namespace GetDAQ {
// 只检查帧边界和存储路由所需的头字段，不把波形展开成逐通道对象。
// 所有成员由写盘线程独占。缓冲区复用，恶意长度在分配前拒绝。
class MfmStream {
public:
    typedef std::function<void(const uint8_t *, const FrameInfo &)> Sink;
    enum Control { Heartbeat, Goodbye };
    typedef std::function<void(Control)> ControlSink;
    // Sink同步消费；指针只在回调期间有效，不得保留、修改或重入Feed。
    MfmStream(std::size_t maximumFrame, Sink sink, bool fdt = false, ControlSink controls = ControlSink());
    void Feed(const uint8_t *data, std::size_t bytes);
    void Finish() const;
    // 一个自然EOF后才能开始下一连接；GOODBYE本身不等于EOF。
    void EndConnection();
    std::size_t Pending() const { return fFrame.size(); }
    static FrameInfo Inspect(const uint8_t *data, std::size_t bytes);
private:
    std::size_t fMaximum, fExpected = 8;
    Sink fSink;
    std::vector<uint8_t> fFrame;
    bool fFdt, fGoodbye = false;
    ControlSink fControls;
    bool ConsumeControl(const uint8_t *data);
};
}
#endif
