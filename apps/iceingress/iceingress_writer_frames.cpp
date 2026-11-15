#include "iceingress.h"
#include "../datareceiver/framestore.h"
#include "../datareceiver/mfmstream.h"
#include <iostream>

namespace GetDAQ {

void IceIngressRecorder::WriteFrameStorage() {
    FrameStore store(fConfig.output,fConfig.rotateBytes,true,fConfig.fileNaming,fConfig.minimumFreeBytes);
    try {
        MfmStream stream(fConfig.maxFrame,[this,&store](const std::uint8_t *data,const FrameInfo &info) {
            store.Write(data,info);fAccepted=store.Bytes();fFiles=store.Files();++fFrames;
            fExporter.Publish(data,info.bytes);
        });
        while (auto block=fQueue.Pop()) {
            stream.Feed(block->data.data(),block->used);
            fQueue.Release(std::move(block));
        }
        stream.Finish();store.Seal();fSynced=store.Bytes();
    } catch (const DiskReserveExceeded &) {store.Abandon();throw;}
    catch (...) {
        try {store.Seal();fSynced=store.Bytes();}
        catch (const std::exception &e) {std::cerr<<"Preserving ICE frames failed: "<<e.what()<<'\n';}
        throw;
    }
}

}
