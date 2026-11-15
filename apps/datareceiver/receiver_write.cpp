#include "receiver.h"
#include <iostream>
#include <stdexcept>

namespace GetDAQ {
void Receiver::Write()
{
    try {
        if (fConfig.storageMode == "byte-storage") WriteByteStorage();
        else WriteFrameStorage();
    } catch (const std::exception &e) { Fail(e.what()); }
    fWriterDone = true;
}
}
