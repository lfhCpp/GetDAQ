#include "iceingress.h"
#include "../datareceiver/bytestore.h"
#include "../datareceiver/framestore.h"
#include <iostream>

namespace GetDAQ {

void IceIngressRecorder::WriteByteStorage() {
    ByteStore store(fConfig.output,fConfig.rotateBytes,true,fConfig.fileNaming,fConfig.minimumFreeBytes);
    try {
        while (auto block=fQueue.Pop()) {
            store.Write(block->data.data(),block->used);
            fAccepted=store.Bytes();fFiles=store.Files();fQueue.Release(std::move(block));
        }
        store.Seal();fSynced=store.Bytes();
    } catch (const DiskReserveExceeded &) {store.Abandon();throw;}
    catch (...) {
        try {store.Seal();fSynced=store.Bytes();}
        catch (const std::exception &e) {std::cerr<<"Preserving ICE bytes failed: "<<e.what()<<'\n';}
        throw;
    }
}

}
