#ifndef GETDAQ_TEST_AGET_FIXTURE_H
#define GETDAQ_TEST_AGET_FIXTURE_H
#include "agetconfiguration.h"
namespace GetDAQTest {
inline GetDAQ::AgetSettings Neutral()
{
    GetDAQ::AgetSettings s; s.chipVersion = 0x202;
    s.globals = {
        {"ReadoutPointerOffset", "0"}, {"Reg1/isIcsax2", "false"}, {"Reg1/TestModeRange", "120fF"}, {"Reg1/peackingTime", "70"},
        {"Reg1/TestModeSelection", "nothing"}, {"Reg1/IntegrationMode", "variable"}, {"Reg1/SCA_Pointer", "column_0"},
        {"Reg1/SCA_Splitting", "false"}, {"Reg1/is32channels", "false"}, {"Reg1/isAllChannelRead", "false"},
        {"Reg1/isFPNRead", "false"}, {"Reg1/isPositivePolarity", "false"}, {"Reg1/Vicm", "1.25V"},
        {"Reg1/GlobalThresholdValue", "0"}, {"Reg1/isThresholdSignedPositive", "false"}, {"Reg1/TriggerVetoOption", "none"},
        {"Reg1/isckwriteSynchro", "false"}, {"Reg1/isTOTActive", "false"}, {"Reg1/isTriggerWidth200ns", "false"},
        {"Reg1/TriggerWidthRange", "width0"}, {"Reg1/ExternalLink", "none"}, {"Reg2/DebugSelection", "standby"},
        {"Reg2/isReadFromColumn0", "false"}, {"Reg2/isDigitalOutputTest", "false"}, {"Reg2/isResetLevelUndefined", "false"},
        {"Reg2/isMarkerEnable", "false"}, {"Reg2/isResetLevelVdd", "false"}, {"Reg2/isBoostPower", "false"},
        {"Reg2/isOutputscCkSynchro", "false"}, {"Reg2/isSynchroEdgeFalling", "false"}, {"Reg2/isSCoutputBufferInTriState", "false"},
        {"Reg2/currentBias", "current0"}, {"Reg2/bufferCurrent", "1.503mA"}, {"Reg2/isPWDonWrite", "false"},
        {"Reg2/isPWDonRead", "false"}, {"Reg2/isAlternatePower", "false"}, {"Reg2/is1bitShortResetReadSeq", "false"},
        {"Reg2/isTriggerOutputDisable", "false"}, {"Reg2/isRisingedgeWriteAutoresetBank", "false"}, {"Reg2/islvdsTriggerOutput", "false"}
    };
    for (unsigned int id = 0; id < 68; ++id) { GetDAQ::AgetChannel c; c.id = id; s.channels.push_back(c); }
    return s;
}
}
#endif
