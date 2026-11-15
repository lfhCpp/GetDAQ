#include "iceingress.h"
#include "iceingress_preflight_p.h"
#include "../../common/text/utf8.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace GetDAQ {
IceIngressRecorder::IceIngressRecorder(const IceIngressConfig &c) : fConfig(c),fLease(c.output),fQueue(c.blocks,c.blockBytes),fExporter(c.exportBind,c.exportPort,c.exportBudget) {
    ValidateIceIngressOutput(c);
    fWriter=std::thread(&IceIngressRecorder::Write,this);
}
IceIngressRecorder::~IceIngressRecorder() {
    if (fWriter.joinable()) {Fail("ICE receiver closed without confirmed stream end");fQueue.Close();fWriter.join();}
}
void IceIngressRecorder::Fail(const std::string &message) {
    std::lock_guard<std::mutex> lock(fErrorMutex);
    if (fError.empty()) {fError=EccErrorText(message);std::cerr<<"ERROR: "<<fError<<'\n';}
    fStop=true;
}
void IceIngressRecorder::Feed(const std::uint8_t *data,std::size_t bytes) {
    if (fEnd || fStop) throw std::runtime_error("ICE stream is ended/failed; payload refused");
    if (bytes>UINT64_MAX-fReceived.load()) throw std::runtime_error("ICE received byte counter overflow");
    while (bytes) {
        auto block=fQueue.Acquire(fStop);if (!block) throw std::runtime_error("ICE writer failed/interrupted; no further payload accepted");
        const auto count=std::min(bytes,block->data.size());std::memcpy(block->data.data(),data,count);block->used=count;
        fReceived+=count;fQueue.Push(std::move(block));data+=count;bytes-=count;
    }
}
void IceIngressRecorder::EndStream(std::uint64_t expectedBytes) {
    if (fEnd || fStop) throw std::runtime_error("ICE stream end already requested or writer failed");
    if (expectedBytes!=fReceived) throw std::runtime_error("ICE source end byte count differs; no end accepted");
    fEnd=true;fQueue.Close();
}
void IceIngressRecorder::Write() {
    try { if (fConfig.storageMode=="byte-storage") WriteByteStorage(); else WriteFrameStorage(); }
    catch (const std::exception &e) {Fail(e.what());}
    fWriterDone=true;
}
int IceIngressRecorder::Finish() {
    if (!fEnd && !fStop) Fail("ICE input has no source finishStream marker; completeness is unconfirmed");
    fQueue.Close();if(fWriter.joinable()) fWriter.join();
    if (!fStop) {try {SaveReceipt();} catch(const std::exception &e) {Fail(e.what());}}
    return fStop ? 1 : 0;
}
}
