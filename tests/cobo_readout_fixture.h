#ifndef GETDAQ_TEST_COBO_READOUT_FIXTURE_H
#define GETDAQ_TEST_COBO_READOUT_FIXTURE_H
#include "coboreadoutconfiguration.h"
#include "aget_fixture.h"
namespace GetDAQTest {
inline GetDAQ::CoBoReadoutSettings Readout(unsigned int mask = 1)
{
    GetDAQ::CoBoReadoutSettings s; s.aget.asadMask = mask;
    s.module = {{"enableWriteHittedregister", "true"}, {"isAcqTestMode", "false"}, {"enableMem2pMode", "false"},
        {"multiplicityThreshold", "50"}, {"multWindowSize", "10"}, {"multiplicityThreshold_2p", "60"},
        {"triggerPeriod", "1000"}, {"triggerDelay", "800"}, {"triggerDeadTime_2p", "800"},
        {"triggerDelay_2p", "800"}, {"triggerTime_2p", "800"}, {"triggerTimeOut_2p", "800"},
        {"triggerMode", "external"}, {"secondaryTriggerMode", "noTrigger"}};
    s.circularBuffer = {{"startOfMemory", "0x8000000"}, {"endOfMemory", "0x10000000"},
        {"levelAlmostFullMemory", "0x280000"}, {"levelAlmostEmptyMemory", "0x280000"}};
    for (unsigned int id = 0; id < 4; ++id) if (mask & (1U << id)) {
        GetDAQ::CoBoAgetChipSettings a; a.asadId = id; a.settings = Neutral(); s.aget.chips.push_back(a);
        GetDAQ::CoBoChipReadout r; r.asadId = id;
        for (unsigned int channel = 0; channel < 68; ++channel) r.zeroSuppressionThresholds[channel] = id * 68 + channel;
        s.chips.push_back(r);
    }
    return s;
}
}
#endif
