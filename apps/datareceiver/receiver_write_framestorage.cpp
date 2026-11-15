#include "receiver.h"
#include "framestore.h"
#include "mfmstream.h"
#include <iostream>
#include <utility>

namespace GetDAQ {
void Receiver::WriteFrameStorage()
{
    FrameStore store(fConfig.output, fConfig.rotateBytes, fConfig.durable,
                     fConfig.fileNaming, fConfig.minimumFreeBytes);
    try {
        MfmStream stream(fConfig.maxFrame, [this, &store](const uint8_t *data, const FrameInfo &info) {
            store.Write(data, info);
            fAccepted = store.Bytes();
            fFiles = store.Files();
            ++fFrames;
            // Keep the legacy acquisition-event counter separate from stored frames.
            if (info.type <= 2) ++fAcquisitionEvents;
            fExporter.Publish(data, info.bytes);
        }, fConfig.inputProtocol == "FDT", [this](MfmStream::Control control) {
            fControlBytes += 4;
            if (control == MfmStream::Heartbeat) ++fHeartbeats;
            else ++fGoodbyes;
        });

        while (BlockQueue::Ptr block = fQueue.Pop()) {
            if (block->end) {
                stream.EndConnection();
                store.Seal();
                fSynced = store.Bytes();
                fSealed = true;
            } else {
                fSealed = false;
                stream.Feed(block->data.data(), block->used);
            }
            fQueue.Release(std::move(block));
        }

        // Preserve complete frames after interruption without claiming completeness.
        stream.Finish();
        store.Seal();
        fSynced = store.Bytes();
    } catch (const DiskReserveExceeded &) {
        // Do not flush userspace frame buffers after the configured reserve is reached.
        store.Abandon();
        throw;
    } catch (...) {
        // Preserve confirmed complete frames, but never seal a protocol failure.
        try {
            store.Seal();
            fSynced = store.Bytes();
        } catch (const std::exception &e) {
            std::cerr << "Preserving complete frames failed: " << e.what() << '\n';
        }
        throw;
    }
}
}
