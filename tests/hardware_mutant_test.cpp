#include "mutant_fixture.h"
using namespace GetDAQMutantFixture;
int main(int argc,char **argv)
{
    Ice::CommunicatorPtr communicator;
    try {
        Require(argc == 2,"Pass the original-description-derived MuTaNT JSON fixture");
        const auto original = LoadHardwareDescription(argv[1]);
        Require(original.nodeId == "MuTanT" && original.devices.size() == 5,"Wrong original MuTaNT description fixture");
        communicator = Ice::initialize();
        auto adapter = communicator->createObjectAdapterWithEndpoints("MutantTest","tcp -h 127.0.0.1 -p 0");
        IceUtil::Handle<TestNode> node = new TestNode; State state;
        const auto install = [&](const HardwareDescription &description) {
            std::lock_guard<std::mutex> lock(node->fMutex); node->fDevices.clear();
            for (const auto &device : description.devices)
                node->fDevices[device.name] = DevicePrx::uncheckedCast(adapter->addWithUUID(new MutantDevice(device,state)));
        };
        install(original); const auto address = communicator->proxyToString(adapter->addWithUUID(node)); adapter->activate();
        char directory[] = "/tmp/getdaq-mutant-tcp-XXXXXX"; Require(mkdtemp(directory) != nullptr,"Cannot create MuTaNT test directory");
        unsigned int count = 0; const auto audit = [&] { return std::string(directory)+"/audit-"+std::to_string(count++)+".jsonl"; };
        for (int minor : {0,1}) {
            install(original); HardwareClient client(address,minor,1000); client.UseDescription(original);
            MutantSettings s; s.moduleCount = 2; s.gmcShelfMask = 11; s.wscaShelfMask = 7; s.amcTagOnly = 128;
            s.timestamp10ns = 0x123456789abcULL; s.eventNumber = 0x76543210; s.externalClockEnabled = true; s.scalerEnabled = {{true,false,true,false,true}};
            state.Reset(); const auto prepare = audit(); client.Execute(MutantPrepare(s),prepare);
            Require(state.values["GPIO_SW_LEDS/DIR1"] == 0xfcfc && state.values["GPIO_SW_LEDS/DIR2"] == UINT32_MAX,"Power/GPIO directions differ");
            Require(state.values["GPIO_SW_LEDS/TDC_STAT_CARD_B_FLGS"] == ((0x359a&~0x300)|0x200),"GMC overwrote unrelated GPIO bits");
            Require(ReadFile(prepare).find("\"phase\":\"complete\"") != std::string::npos,"Prepare scope audit absent");
            state.Reset(); const auto configured = audit(); client.Execute(MutantConfigure(s),configured);
            Require(state.values["GPIO_CLK_TREE/CLOCK_TREE_CONFIG2"] == (0x1b7|(1<<9)),"Masked clock update damaged unrelated/sync bits");
            Require(state.values["GPIO_CLK_TREE/CLOCK_TREE_CONFIG1"] == (0x90000000|(135<<12)|139),"BEAST outputs or upper bits differ");
            Require(state.values["CARD_B_EMC/SHELF_MASK"] == 3 && state.values["CARD_B_EMC/AMC_TAG_ONLY"] == 128,"Shelf mask wrongly includes ATO");
            Require(state.values["CARD_B_EMC/SET_TIME_LOW"] == 0x9abc && state.values["CARD_B_EMC/SET_TIME_HIGH"] == 0x1234 && state.values["CARD_B_EMC/SET_EVT_NUM_HIGH"] == 0x7654,"Counter word order lost");
            Require((state.values["CARD_B_EMC/TRIGGER_CTRL"]&2) == 0 && (state.values["CARD_B_EMC/SCALER_CTRL"]&31) == 21,"Readout/scaler polarity differs");
            Require(state.commandReads == 0 && state.values["CARD_B_EMC/TRIGGER_CMD"] == 10,"Command pulses were echoed or counter enables lost");
            const std::vector<std::string> expectedCommands = {
                "euc=0","eec=0","cl0c=1","cl1ac=1","cl1bc=1","cl2c=1","cl0c=0","cl1ac=0","cl1bc=0","cl2c=0",
                "luc=1","luc=0","l2ptc=1","l2ptc=0","lec=1","lec=0","euc=1","eec=1"};
            std::vector<std::string> commands;
            for (const auto &write : state.writes) if (write.find("CARD_B_EMC/TRIGGER_CMD/") == 0) commands.push_back(write.substr(23));
            Require(commands == expectedCommands,"Timestamp/event clear/load command order differs");
            state.Reset(); auto twoProton = s; twoProton.twoProtonMode = twoProton.halfEventReadout = true;
            twoProton.readoutEnabled = false; twoProton.level1bTimeoutUs = 5.12;
            client.Execute(MutantConfigure(twoProton),audit());
            Require((state.values["CARD_B_EMC/GLOBAL_CONF"]&(1<<12)) && state.values["CARD_B_EMC/L1_B_TIMEOUT"] == 2 && (state.values["CARD_B_EMC/TRIGGER_CTRL"]&2),"2p/readout-disabled bits differ");
            state.Reset(); client.Execute(MutantWscaClock(true),audit()); Require(state.values["GPIO_CLK_TREE/CLOCK_TREE_CONFIG2"] == (0x1ba|(1<<9)),"WSCA changed unrelated bits");
            client.Execute(MutantWscaClock(false),audit()); Require(state.values["GPIO_CLK_TREE/CLOCK_TREE_CONFIG2"] == (0x1b2|(1<<9)),"WSCA disable changed unrelated bits");
            client.Execute(MutantSyncPulse(false),audit()); Require(state.values["GPIO_CLK_TREE/CLOCK_TREE_CONFIG2"] == 0x1b2,"Sync changed clock path");
            client.Execute(MutantSyncPulse(true),audit()); Require(state.commandReads == 0,"Sync pulse used an implicit echo");
            client.Execute(MutantClockShelfMasks(5,9),audit()); Require(state.values["GPIO_CLK_TREE/CLOCK_TREE_CONFIG1"] == (0x90000000|(9<<12)|5),"Shelf fields changed other bits");
            state.Reset(); client.Execute(MutantRunInit(s),audit()); Require(state.writes.size() == 18 && state.commandReads == 0,"RunInit started DAQ or repeated preload");
            state.Reset(); state.values["CARD_B_EMC/TRIGGER_CTRL"] = 1<<15;
            Rejects([&] { client.Execute(MutantConfigure(s),audit()); },"mismatch"); Require(state.writes.empty(),"Running MuTaNT allowed configuration");
            state.Reset(); state.values["GPIO_CLK_TREE/CLOCK_TREE_CONFIG1"] ^= 1;
            Rejects([&] { client.Execute(MutantConfigure(s),audit()); },"mismatch"); Require(state.writes.empty(),"Stale clock snapshot wrote parameters");
            state.Reset(); state.values["CARD_B_EMC/PWR_STAT_B"] = 0xdf;
            Rejects([&] { client.Execute(MutantPrepare(s),audit()); },"mismatch"); Require(state.writes.size() == 2,"Strict power failure continued VCXO");
            state.Reset(); state.values["CARD_B_EMC/PWR_STAT_B"] = 0xdf; auto prototype = s; prototype.allowPrototypePowerDf = true;
            const auto prototypeAudit = audit(); client.Execute(MutantPrepare(prototype),prototypeAudit);
            Require(ReadFile(prototypeAudit).find("matched_with_warning") != std::string::npos,"Prototype power difference hidden");
            state.Reset(); prototype.verifyBoardId = true; prototype.expectedBoardId = 0x123457;
            Rejects([&] { client.Execute(MutantPrepare(prototype),audit()); },"mismatch"); Require(state.writes.size() == 2,"Board ID mismatch continued configuration");
            state.Reset(); state.values["CARD_B_EMC/MUTANT_STATUS"] = 0;
            Rejects([&] { client.Execute(MutantConfigure(s),audit()); },"mismatch");
            Require(state.values["GPIO_CLK_TREE/CLOCK_TREE_CONFIG1"] == (0x90000000|(7<<12)|11),"Not-ready MuTaNT enabled BEAST late");
            HardwareClient unbound(address,minor,1000); state.Reset();
            Rejects([&] { unbound.Execute(MutantConfigure(s),audit()); },"bound hardware"); Require(!state.metadata && state.writes.empty(),"Missing binding touched metadata");
            auto denied = original;
            for (auto &d : denied.devices) for (auto &r : d.registers) if (r.name == "RP_LOG_IREG") r.readOnly = true;
            HardwareClient readOnly(address,minor,1000); readOnly.UseDescription(denied); state.Reset();
            Rejects([&] { readOnly.Execute(MutantConfigure(s),audit()); },"read-only"); Require(state.writes.empty(),"Late permission error wrote early parameters");
            auto missing = original;
            for (auto &d : missing.devices) if (d.name == "CARD_B_EMC") d.registers.pop_back();
            HardwareClient missingClient(address,minor,1000); missingClient.UseDescription(missing); state.Reset();
            Rejects([&] { missingClient.Execute(MutantConfigure(s),audit()); },"description"); Require(state.writes.empty(),"Late descriptor mismatch wrote hardware");
            state.Reset(); auto invalid = MutantWscaClock(true); invalid.back().value = 16;
            Rejects([&] { client.Execute(invalid,audit()); },"masked update"); Require(state.writes.empty(),"Invalid masked value wrote hardware");
            state.Reset(); state.failure = 1; const auto lost = audit();
            Rejects([&] { client.Execute(MutantConfigure(s),lost); },"UNKNOWN");
            Require(state.failedWrites == 1 && state.writes.size() == 1 && state.values["GPIO_CLK_TREE/CLOCK_TREE_CONFIG2"] == (0x1b7|(1<<9)),"Applied masked update retried or continued");
            Require(ReadFile(lost).find("\"phase\":\"unknown\"") != std::string::npos,"Unknown masked write not durable");
            state.Reset(); state.failure = 2;
            Rejects([&] { client.Execute(MutantConfigure(s),audit()); },"readback failed"); Require(state.failedWrites == 1 && state.writes.size() == 1,"Failed readback repeated acknowledged write");
            auto legacy = original; Rename(legacy,true,true); install(legacy);
            HardwareClient old(address,minor,1000); old.UseDescription(legacy); auto legacySettings = s;
            legacySettings.layout = MutantSettings::LegacyNamesV15; legacySettings.twoProtonCounterCommand = false;
            state.Reset(); old.Execute(MutantConfigure(legacySettings),audit()); Require(state.values["CARD_B_EMC/MASTER_MASK"] == 3 && state.values["CARD_B_EMC/USER_REG1"] == 128,"Explicit old names missing");
            state.Reset(); auto missingCommand = legacySettings; missingCommand.twoProtonCounterCommand = true;
            Rejects([&] { old.Execute(MutantConfigure(missingCommand),audit()); },"Unknown hardware field: l2ptc"); Require(state.writes.empty(),"Optional command absence fell back after write");
            state.Reset(); auto wrongNames = legacySettings; wrongNames.layout = MutantSettings::ModernNamesV15;
            Rejects([&] { old.Execute(MutantConfigure(wrongNames),audit()); },"Unknown hardware register: SHELF_MASK"); Require(state.writes.empty(),"Modern layout fell back after write");
            auto widePolicy = original; DeviceInfo wide; wide.name = "wide"; wide.access = "test-memory"; wide.registerBytes = 8;
            RegisterInfo wideRegister; wideRegister.name = "value"; wide.registers.push_back(wideRegister); widePolicy.devices.push_back(wide); install(widePolicy);
            HardwareClient wideClient(address,minor,1000); wideClient.UseDescription(widePolicy); state.Reset();
            HardwareStep masked; masked.kind = HardwareStep::MaskedWriteValue; masked.device = "wide"; masked.reg = "value";
            masked.requireBoundDescription = true; masked.mask = 0x80000000000000ffULL; masked.value = 0x7f;
            wideClient.Execute({masked},audit()); Require(state.values["wide/value"] == 0x7edcba987654327fULL,"64-bit masked update lost sign/other bits");
            masked.value = 0x8000000000000055ULL; wideClient.Execute({masked},audit());
            Require(state.values["wide/value"] == 0xfedcba9876543255ULL,"64-bit masked update lost high value bit");
            std::cout << "PASS MuTaNT encoding 1." << minor << " actual description, commands/masks/counters, full preflight and lost-reply stop\n";
        }
        communicator->destroy(); communicator = nullptr; return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; if (communicator) communicator->destroy(); return 1; }
}
