#ifndef GETDAQ_TEST_COBO_CONFIGURE_FIXTURE_H
#define GETDAQ_TEST_COBO_CONFIGURE_FIXTURE_H
#include "coboasadconfiguration.h"
#include "cobo_readout_fixture.h"
namespace GetDAQTest {
inline GetDAQ::CoBoConfigureSettings Configure(unsigned int mask = 1)
{
    GetDAQ::CoBoConfigureSettings s; s.readout = Readout(mask); s.asad.asadMask = mask;
    s.asad.generator = {{"amplitudeStart", "-1000"}, {"defaultVoltage", "0"}, {"secondPulseAmplitude", "-300"}};
    for (unsigned int id = 0; id < 4; ++id) if (mask & (1U << id)) {
        GetDAQ::CoBoAsAdBoardSettings b; b.asadId = id; b.triggerDelay = id;
        b.calibrationMode = (id & 1) != 0; b.capacitor = id == 3 ? "11pF" : "1pF"; s.asad.boards.push_back(b);
    }
    return s;
}
}
#endif
