#include "receiver.h"
#include "bytestore.h"
#include "framestore.h"
#include <iostream>
#include <utility>

namespace GetDAQ {
void Receiver::WriteByteStorage()
{
    ByteStore store(fConfig.output, fConfig.rotateBytes, fConfig.durable,
                    fConfig.fileNaming, fConfig.minimumFreeBytes);
    try {
        while (BlockQueue::Ptr block = fQueue.Pop()) {
            if (block->end) {
                fSealed = true;
            } else {
                fSealed = false;
                store.Write(block->data.data(), block->used);
                fAccepted = store.Bytes();
                fFiles = store.Files();
            }
            fQueue.Release(std::move(block));
        }
        store.Seal();
        fSynced = store.Bytes();
    } catch (const DiskReserveExceeded &) {
        store.Abandon();
        throw;
    } catch (...) {
        try {
            store.Seal();
            fSynced = store.Bytes();
        } catch (const std::exception &e) {
            std::cerr << "Preserving received bytes failed: " << e.what() << '\n';
        }
        throw;
    }
}
}
