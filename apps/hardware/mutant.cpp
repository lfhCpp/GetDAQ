// Adapted from CEA GET MutantNode (received 2021 release).
// Original CeCILL license retained in third_party/licenses/getbench/COPYING.
#include "hardwareclient.h"
#include "mutantconfiguration.h"
#include <stdexcept>
namespace GetDAQ {
namespace {
void Add(std::vector<HardwareStep> &steps,HardwareStep::Kind kind,const char *device,const char *reg,const std::string &field,std::uint64_t value = 0)
{
    HardwareStep step; step.kind = kind; step.device = device; step.reg = reg; step.field = field; step.value = value; steps.push_back(step);
}
void Write(std::vector<HardwareStep> &steps,const char *device,const char *reg,const std::string &field,std::uint64_t value)
{ Add(steps,HardwareStep::WriteValue,device,reg,field,value); }
void Verified(std::vector<HardwareStep> &steps,const char *device,const char *reg,const std::string &field,std::uint64_t value)
{ Write(steps,device,reg,field,value); Add(steps,HardwareStep::ExpectValue,device,reg,field,value); }
void Card(std::vector<HardwareStep> &steps,const char *reg,const std::string &field,std::uint64_t value,bool command = false)
{ if (command) Write(steps,"CARD_B_EMC",reg,field,value); else Verified(steps,"CARD_B_EMC",reg,field,value); }
void Masked(std::vector<HardwareStep> &steps,const char *device,const char *reg,const char *field,unsigned int value,unsigned int mask)
{ Add(steps,HardwareStep::MaskedWriteValue,device,reg,field,value); steps.back().mask = mask; }
std::vector<HardwareStep> Begin(const char *operation)
{
    std::vector<HardwareStep> steps; Add(steps,HardwareStep::ExpectValue,"CARD_B_EMC","TRIGGER_CTRL","run",0);
    steps.back().requireBoundDescription = true;
    steps.back().annotation = std::string(operation)+"; run=0 is not sender stop/alignment/ownership/complete Prepared; command bits have no implicit readback";
    return steps;
}
void Sources(std::vector<HardwareStep> &steps,const MutantImage &image)
{
    Card(steps,"GMC_WSCA_SEL","GMC",0); Card(steps,"GMC_WSCA_SEL","WSCA",image.wscaSource);
    Card(steps,"GMC_WSCA_SEL","PromptWSCA",image.promptWsca); Card(steps,"GLOBAL_CONF","",image.globalConfiguration);
}
void ClearCounters(std::vector<HardwareStep> &steps)
{
    const char *fields[] = {"cl0c","cl1ac","cl1bc","cl2c"};
    for (const auto *field : fields) Card(steps,"TRIGGER_CMD",field,1,true);
    for (const auto *field : fields) Card(steps,"TRIGGER_CMD",field,0,true);
}
void ClearScalers(std::vector<HardwareStep> &steps)
{
    for (unsigned int i = 1; i <= 5; ++i) { const auto field = "cs"+std::to_string(i); Card(steps,"SCALER_CTRL",field,1,true); Card(steps,"SCALER_CTRL",field,0,true); }
}
void Counters(std::vector<HardwareStep> &steps,const MutantSettings &s,const MutantImage &image)
{
    Card(steps,"TRIGGER_CMD","euc",0,true); Card(steps,"TRIGGER_CMD","eec",0,true);
    if (s.clearCounters) ClearCounters(steps);
    if (s.preloadTimestamp) {
        const char *registers[] = {"SET_TIME_LOW","SET_TIME_MID","SET_TIME_HIGH"};
        for (unsigned int i = 0; i < 3; ++i) Card(steps,registers[i],"",image.timestampWords[i]);
        Card(steps,"TRIGGER_CMD","luc",1,true); Card(steps,"TRIGGER_CMD","luc",0,true);
        if (s.twoProtonCounterCommand) { Card(steps,"TRIGGER_CMD","l2ptc",1,true); Card(steps,"TRIGGER_CMD","l2ptc",0,true); }
    }
    if (s.preloadEventNumber) {
        Card(steps,"SET_EVT_NUM_LOW","",image.eventWords[0]); Card(steps,"SET_EVT_NUM_HIGH","",image.eventWords[1]);
        Card(steps,"TRIGGER_CMD","lec",1,true); Card(steps,"TRIGGER_CMD","lec",0,true);
    }
    Card(steps,"TRIGGER_CMD","euc",1,true); Card(steps,"TRIGGER_CMD","eec",1,true);
}
}
std::vector<HardwareStep> MutantPrepare(const MutantSettings &s)
{
    const auto image = CompileMutant(s); auto steps = Begin("MutantPrepare");
    Verified(steps,"GPIO_SW_LEDS","DIR1","",0xfcfc); Verified(steps,"GPIO_SW_LEDS","DIR2","",0xffffffffULL);
    Add(steps,HardwareStep::ExpectValue,"GPIO_SW_LEDS","BOARD_ID_PWR_STAT","",0x7f000000); steps.back().mask = 0x7f000000;
    Add(steps,HardwareStep::ExpectValue,"CARD_B_EMC","PWR_STAT_B","",0xff); steps.back().mask = 0xff;
    steps.back().allowedDifference = s.allowPrototypePowerDf ? 0x20 : 0;
    steps.back().annotation = s.allowPrototypePowerDf ? "explicit prototype power 0xdf accepted with warning; not electrical proof" : "strict power status 0xff required";
    Add(steps,s.verifyBoardId ? HardwareStep::ExpectValue : HardwareStep::ReadValue,"GPIO_SW_LEDS","BOARD_ID_PWR_STAT","mutant_id",s.verifyBoardId ? s.expectedBoardId : 0);
    Card(steps,"TRIGGER_CTRL","run",0); Verified(steps,"CLK200_SPI","VCXO_CONTROL","",s.vcxoControl);
    Sources(steps,image);
    // 用明确gmc位域保存其他GPIO标志；等价于旧读改写的目标位，不覆写只读状态。
    Verified(steps,"GPIO_SW_LEDS","TDC_STAT_CARD_B_FLGS","gmc",image.externalClockSource);
    return steps;
}
std::vector<HardwareStep> MutantConfigure(const MutantSettings &s)
{
    const auto image = CompileMutant(s); auto steps = Begin("MutantConfigure");
    Add(steps,HardwareStep::ExpectValue,"GPIO_CLK_TREE","CLOCK_TREE_CONFIG1","amc_clk1_out_en",s.gmcShelfMask);
    Add(steps,HardwareStep::ExpectValue,"GPIO_CLK_TREE","CLOCK_TREE_CONFIG1","amc_clk2_out_en",s.wscaShelfMask);
    Masked(steps,"GPIO_CLK_TREE","CLOCK_TREE_CONFIG2","clk_path_sel",image.clockPathValue,5);
    Sources(steps,image); Card(steps,"TRIGGER_CTRL","test",s.testMode); Card(steps,"TRIGGER_CTRL","read_mode",!s.readoutEnabled);
    Counters(steps,s,image);
    for (unsigned int i = 1; i <= 5; ++i) Card(steps,"SCALER_CTRL","ens"+std::to_string(i),0);
    if (s.clearScalers) ClearScalers(steps);
    for (unsigned int i = 0; i < 5; ++i) if (s.scalerEnabled[i]) Card(steps,"SCALER_CTRL","ens"+std::to_string(i+1),1);
    Card(steps,"SOURCE_SYSTEM","source",s.sourceId); Card(steps,"FIFO_FRAME_TYPE","ft",s.frameType);
    Card(steps,image.shelfRegister,"",image.shelfMask); Card(steps,image.tagRegister,"",s.amcTagOnly); Card(steps,image.userRegister,"",s.userRegister);
    Card(steps,"L0_GATE_DELAY","",image.level0GateDelay); Card(steps,"L0_GATE_DURATION","",image.level0GateDuration);
    const char *thresholds[] = {"L1_A_LOW_MULT_THRESHOLD","L1_A_HIGH_MULT_THRESHOLD","L1_B_LOW_MULT_THRESHOLD","L1_B_HIGH_MULT_THRESHOLD"};
    const char *buckets[] = {"L1_A_LOW_TH_N_BUCKETS","L1_A_HIGH_TH_N_BUCKETS","L1_B_LOW_TH_N_BUCKETS","L1_B_HIGH_TH_N_BUCKETS"};
    for (unsigned int i = 0; i < 4; ++i) Card(steps,thresholds[i],"",image.multiplicityThresholds[i]);
    for (unsigned int i = 0; i < 4; ++i) Card(steps,buckets[i],"",image.bucketCounts[i]);
    Card(steps,"L1_B_TIMEOUT","",image.level1bTimeout); Card(steps,"L1_POST_TRIG_DELAY","",image.level1PostTriggerDelay);
    Card(steps,"FP_LOG_IREG","",image.frontInspection); Card(steps,"RP_LOG_IREG","",image.rearInspection);
    Add(steps,HardwareStep::ReadValue,"CARD_B_EMC","MUTANT_STATUS","");
    Add(steps,HardwareStep::ExpectValue,"CARD_B_EMC","MUTANT_STATUS","mrdy",1);
    for (unsigned int slot = 1; slot <= 12; ++slot) {
        const unsigned int end = ((slot-1)/4+1)*4; const auto reg = "AMC"+std::to_string(end)+"_"+std::to_string(end-3)+"_STATUS";
        Add(steps,HardwareStep::ReadValue,"CARD_B_EMC",reg.c_str(),"amc"+std::to_string(slot)+"_status_code");
    }
    if (s.amcTagOnly) {
        Masked(steps,"GPIO_CLK_TREE","CLOCK_TREE_CONFIG1","amc_clk1_out_en",s.amcTagOnly,s.amcTagOnly);
        Masked(steps,"GPIO_CLK_TREE","CLOCK_TREE_CONFIG1","amc_clk2_out_en",s.amcTagOnly,s.amcTagOnly);
    }
    return steps;
}
std::vector<HardwareStep> MutantRunInit(const MutantSettings &s)
{
    CompileMutant(s); auto steps = Begin("MutantRunInit; counters only, not DAQ Start");
    if (s.clearScalers) ClearScalers(steps);
    if (s.clearCounters) ClearCounters(steps);
    return steps;
}
std::vector<HardwareStep> MutantWscaClock(bool enabled)
{ auto steps = Begin("MutantWscaClock"); Masked(steps,"GPIO_CLK_TREE","CLOCK_TREE_CONFIG2","clk_path_sel",enabled ? 8 : 0,8); return steps; }
std::vector<HardwareStep> MutantSyncPulse(bool enabled)
{ auto steps = Begin("MutantSyncPulse; requires stopped CoBo, mutantMode=1/initDone=0 and prior alignment from system owner"); Write(steps,"GPIO_CLK_TREE","CLOCK_TREE_CONFIG2","start_pulse",enabled); return steps; }
std::vector<HardwareStep> MutantClockShelfMasks(unsigned int gmc,unsigned int wsca)
{
    if (gmc > 4095 || wsca > 4095) throw std::invalid_argument("MuTaNT clock shelf masks must fit 12 slots");
    auto steps = Begin("MutantClockShelfMasks"); Verified(steps,"GPIO_CLK_TREE","CLOCK_TREE_CONFIG1","amc_clk1_out_en",gmc);
    Verified(steps,"GPIO_CLK_TREE","CLOCK_TREE_CONFIG1","amc_clk2_out_en",wsca); return steps;
}
}
