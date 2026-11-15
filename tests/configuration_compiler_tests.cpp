#include "configuration_compiler_tests_p.h"

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QTemporaryDir>
#include <QStringList>

#include "agetcompiler.h"
#include "calibrationcompiler.h"
#include "clockcompiler.h"
#include "hardwarecompiler.h"
#include "monitoringcompiler.h"
#include "mutantcompiler.h"
#include "xcfgdocument.h"

namespace GetDAQTestInternal {

void RunMutantRepository()
{
    const QString file = QFINDTESTDATA("../source/get/GetBench-software-release-20210208/data/config/configure-ExternalTrigger-2CoBo_1Mutant.xcfg");
    XcfgDocument document; QString error; QJsonObject output; GetDAQ::MutantSettings options, settings;
    options.moduleCount = 2; options.gmcShelfMask = 3; options.wscaShelfMask = 7;
    QVERIFY(document.Load(file,&error));
    QVERIFY2(MutantCompiler::Compile(document,"master",options,&output,&error,&settings),qPrintable(error));
    QCOMPARE(output["kind"].toString(),QString("getdaq.mutant-image")); QCOMPARE(output["shelf_mask"].toInt(),3);
    QVERIFY(!output["is_slave"].toBool()); QVERIFY(!output["hardware_deployment_supported"].toBool()); QVERIFY(!output["alignment_verified"].toBool());
    QVERIFY(settings.level0Enabled); QVERIFY(!settings.level1Enabled); QVERIFY(!settings.level2Enabled);
    QCOMPARE(GetDAQ::CompileMutant(settings).globalConfiguration,0x411U);
    QCOMPARE(output["timings"].toObject()["level0_duration"].toObject()["code"].toInt(),65534);
    QVERIFY(output["sources"].toArray().size() == 1); QVERIFY(output["parameters"].toArray().size() > 35);
    QTemporaryDir directory; const auto path = directory.filePath("mutant.json");
    QVERIFY(MutantCompiler::Save(document,"master",options,path,&error));
    QFile saved(path); QVERIFY(saved.open(QIODevice::ReadOnly)); const auto bytes = saved.readAll(); saved.close();
    QVERIFY(!MutantCompiler::Save(document,"master",options,path,&error)); QVERIFY(saved.open(QIODevice::ReadOnly)); QCOMPARE(saved.readAll(),bytes);
}

void RunMutantRejectedParameters()
{
    const QString file = QFINDTESTDATA("../source/get/GetBench-software-release-20210208/data/config/configure-ExternalTrigger-2CoBo_1Mutant.xcfg");
    XcfgDocument document; QString error; QVERIFY(document.Load(file,&error));
    GetDAQ::MutantSettings options, settings; settings.sourceId = 73;
    QJsonObject keep; keep["keep"] = true;
    for (const auto &id : {QString("*"),QString("missing"),QString("a/b")}) {
        QVERIFY(!MutantCompiler::Compile(document,id,options,&keep,&error,&settings));
        QVERIFY(keep["keep"].toBool()); QCOMPARE(settings.sourceId,73U);
    }
    options.gmcShelfMask = 4096;
    QVERIFY(!MutantCompiler::Compile(document,"master",options,&keep,&error,&settings)); QVERIFY(keep["keep"].toBool()); QCOMPARE(settings.sourceId,73U);
    QTemporaryDir directory; const auto output = directory.filePath("bad.json");
    QVERIFY(!MutantCompiler::Save(document,"master",options,output,&error)); QVERIFY(!QFile::exists(output));
    QFile original(file); QVERIFY(original.open(QIODevice::ReadOnly)); auto bytes = original.readAll();
    QVERIFY(bytes.contains("<gateDelay unit=\"us\"")); bytes.replace("<gateDelay unit=\"us\"","<gateDelay unit=\"ms\"");
    QFile bad(directory.filePath("bad.xcfg")); QVERIFY(bad.open(QIODevice::WriteOnly)); bad.write(bytes); bad.close();
    QVERIFY(document.Load(bad.fileName(),&error)); options.gmcShelfMask = 0;
    QVERIFY(!MutantCompiler::Compile(document,"master",options,&keep,&error,&settings)); QVERIFY(error.contains("Unsupported unit"));
    QVERIFY(keep["keep"].toBool()); QCOMPARE(settings.sourceId,73U);
}

void RunCoboCalibrationRepository()
{
    const QString directory = QFINDTESTDATA("../source/get/GetBench-software-release-20210208/data/config");
    XcfgDocument document; QString error; QJsonObject output; GetDAQ::CoBoCalibrationSettings settings;
    const QMap<int,quint16> versions = {{0,0x202},{1,0x202},{2,0x202},{3,0x203}};
    QVERIFY(document.Load(QDir(directory).filePath("configure-pedestals.xcfg"),&error));
    QVERIFY2(CalibrationCompiler::Compile(document,"0",versions,GetDAQ::CoBoCalibrationSettings::GlobalDelayField,&output,&error,&settings),qPrintable(error));
    QCOMPARE(output["kind"].toString(),QString("getdaq.cobo-calibration-image")); QCOMPARE(settings.asadMask,1U);
    QCOMPARE(output["aget_mask"].toInt(),15); QCOMPARE(output["calibration_asad_mask"].toInt(),1);
    QVERIFY(settings.computeAutomaticReadDelay); QVERIFY(!output["complete_prepare"].toBool()); QVERIFY(!output["physical_delay_verified"].toBool());
    QCOMPARE(output["chips"].toArray().size(),4); QVERIFY(!output["parameters"].toArray().isEmpty());
    QTemporaryDir dir; const auto path = dir.filePath("calibration.json");
    QVERIFY(CalibrationCompiler::Save(document,"0",versions,GetDAQ::CoBoCalibrationSettings::GlobalDelayField,path,&error));
    QFile existing(path); QVERIFY(existing.open(QIODevice::ReadOnly)); const auto bytes = existing.readAll(); existing.close();
    QVERIFY(!CalibrationCompiler::Save(document,"0",versions,GetDAQ::CoBoCalibrationSettings::GlobalDelayField,path,&error));
    QVERIFY(existing.open(QIODevice::ReadOnly)); QCOMPARE(existing.readAll(),bytes);
    QVERIFY(document.Load(QDir(directory).filePath("configure-ExternalTrigger-2CoBo_1Mutant.xcfg"),&error));
    QVERIFY2(CalibrationCompiler::Compile(document,"Crate00_Slot00",versions,GetDAQ::CoBoCalibrationSettings::GlobalDelayField,&output,&error),qPrintable(error));
}

void RunCoboCalibrationPoliciesAndCoverage()
{
    QTemporaryDir dir; QFile file(dir.filePath("calibration.xcfg")); QVERIFY(file.open(QIODevice::WriteOnly));
    QByteArray text = "<Setup><Node id=\"CoBo\"><Instance id=\"0\"><isActive>true</isActive><Module><calibrateAutoReadDataDelay>false</calibrateAutoReadDataDelay><enableAutoReadDataDelay>true</enableAutoReadDataDelay></Module>";
    for (int id = 0; id < 4; ++id) {
        text += QByteArray("<AsAd id=\"") + QByteArray::number(id) + "\"><Control><isActive>" + (id < 2 ? "true" : "false") + "</isActive>";
        if (id == 0) text += "<enableAutoReadDataDelay>false</enableAutoReadDataDelay>";
        text += "</Control>";
        for (int chip = 0; chip < 4; ++chip) text += QByteArray("<Aget id=\"") + QByteArray::number(chip) + "\"><Control><isActive>" + (chip == 2 && id == 0 ? "true" : "false") + "</isActive></Control></Aget>";
        text += "</AsAd>";
    }
    text += "</Instance></Node></Setup>"; file.write(text); file.close();
    XcfgDocument document; QString error; QJsonObject output; GetDAQ::CoBoCalibrationSettings settings;
    QVERIFY(document.Load(file.fileName(),&error));
    const QMap<int,quint16> versions = {{2,0x201}};
    QVERIFY(!CalibrationCompiler::Compile(document,"0",versions,GetDAQ::CoBoCalibrationSettings::GlobalDelayField,&output,&error));
    QVERIFY(error.contains("different active AsAd policies"));
    QVERIFY2(CalibrationCompiler::Compile(document,"0",versions,GetDAQ::CoBoCalibrationSettings::PerAsAdDelayFields,&output,&error,&settings),qPrintable(error));
    QCOMPARE(output["automatic_read_delay_mask"].toInt(),2); QCOMPARE(output["calibration_asad_mask"].toInt(),1);
    QCOMPARE(output["first_calibration_global_chip"].toInt(),2); QVERIFY(!settings.computeAutomaticReadDelay);
    const auto boards = output["boards"].toArray(); QCOMPARE(boards[0].toObject()["calibration_aget_id"].toInt(),2); QVERIFY(boards[1].toObject()["calibration_aget_id"].isNull());
    QVERIFY(boards[0].toObject()["delay_policy_path"].toString().contains("AsAd[0]/Control"));
    QVERIFY(boards[1].toObject()["delay_policy_path"].toString().contains("Module/enable"));
    QVERIFY(!CalibrationCompiler::Compile(document,"0",versions,GetDAQ::CoBoCalibrationSettings::PerAsAdDelayFields,&output,&error,nullptr,false));
    QVERIFY(error.contains("Nonzero calibration chip"));
}

void RunCoboCalibrationFailurePreservesOutput()
{
    const QString original = QFINDTESTDATA("../source/get/GetBench-software-release-20210208/data/config/configure-pedestals.xcfg");
    XcfgDocument document; QString error; QJsonObject keep; keep["keep"] = true; GetDAQ::CoBoCalibrationSettings settings; settings.asadMask = 9;
    QVERIFY(document.Load(original,&error));
    const QMap<int,quint16> missing = {{0,0x202}};
    QVERIFY(!CalibrationCompiler::Compile(document,"0",missing,GetDAQ::CoBoCalibrationSettings::GlobalDelayField,&keep,&error,&settings));
    QVERIFY(error.contains("Missing version")); QVERIFY(keep["keep"].toBool()); QCOMPARE(settings.asadMask,9U);
    QTemporaryDir dir; QVERIFY(!CalibrationCompiler::Save(document,"0",missing,GetDAQ::CoBoCalibrationSettings::GlobalDelayField,dir.filePath("bad.json"),&error));
    QVERIFY(!QFile::exists(dir.filePath("bad.json")));
    const QMap<int,quint16> bad = {{0,0x202},{1,0x202},{2,0x202},{3,0x204}};
    QVERIFY(!CalibrationCompiler::Compile(document,"0",bad,GetDAQ::CoBoCalibrationSettings::GlobalDelayField,&keep,&error)); QVERIFY(keep["keep"].toBool());
}

void RunCoboMonitoringRepository()
{
    const QString directory = QFINDTESTDATA("../source/get/GetBench-software-release-20210208/data/config");
    XcfgDocument document; QString error; QJsonObject output; GetDAQ::CoBoMonitoringSettings settings;
    QVERIFY(document.Load(QDir(directory).filePath("configure-pedestals.xcfg"),&error));
    QVERIFY2(MonitoringCompiler::Compile(document,"0",GetDAQ::CoBoMonitoringSettings::DeclaredParameters,&output,&error,&settings),qPrintable(error));
    QCOMPARE(output["kind"].toString(),QString("getdaq.cobo-monitoring-image")); QCOMPARE(settings.asadMask,1U);
    const auto board = output["boards"].toArray()[0].toObject();
    QCOMPARE(board["limits"].toObject()["VDD"].toObject()["lower_code"].toString(),QString("204"));
    QCOMPARE(board["vdd_settle_minimum_raw"].toString(),QString("205")); QCOMPARE(board["protection_mask1"].toString(),QString("35"));
    QVERIFY(!output["complete_prepare"].toBool()); QVERIFY(!output["hardware_deployment_supported"].toBool());
    QVERIFY(!output["host_alarm_subscription_supported"].toBool()); QVERIFY(!output["led_manager_supported"].toBool());
    QVERIFY(!output["parameters"].toArray().isEmpty()); QCOMPARE(output["sources"].toArray().size(),1);
    QTemporaryDir dir; const auto path = dir.filePath("monitoring.json");
    QVERIFY(MonitoringCompiler::Save(document,"0",GetDAQ::CoBoMonitoringSettings::DeclaredParameters,path,&error));
    QFile existing(path); QVERIFY(existing.open(QIODevice::ReadOnly)); const auto bytes = existing.readAll(); existing.close();
    QVERIFY(!MonitoringCompiler::Save(document,"0",GetDAQ::CoBoMonitoringSettings::LegacyCallArguments,path,&error));
    QVERIFY(existing.open(QIODevice::ReadOnly)); QCOMPARE(existing.readAll(),bytes);
    QVERIFY(document.Load(QDir(directory).filePath("configure-ExternalTrigger-2CoBo_1Mutant.xcfg"),&error));
    QVERIFY2(MonitoringCompiler::Compile(document,"Crate00_Slot00",GetDAQ::CoBoMonitoringSettings::LegacyCallArguments,&output,&error),qPrintable(error));
}

void RunCoboMonitoringModeMapping()
{
    const QString original = QFINDTESTDATA("../source/get/GetBench-software-release-20210208/data/config/configure-pedestals.xcfg");
    QFile source(original); QVERIFY(source.open(QIODevice::ReadOnly)); QByteArray bytes = source.readAll();
    QVERIFY(bytes.contains("{IDD, Text, Tint, VDD, VDD1}\">VDD1"));
    bytes.replace("{IDD, Text, Tint, VDD, VDD1}\">VDD1","{IDD, Text, Tint, VDD, VDD1}\">IDD");
    bytes.replace("<fastConversionEnabled>false</fastConversionEnabled>","<fastConversionEnabled>true</fastConversionEnabled>");
    bytes.replace("<singleChannelModeEnabled>false</singleChannelModeEnabled>","<singleChannelModeEnabled>true</singleChannelModeEnabled>");
    QTemporaryDir dir; QFile file(dir.filePath("mapping.xcfg")); QVERIFY(file.open(QIODevice::WriteOnly)); file.write(bytes); file.close();
    XcfgDocument document; QString error; QJsonObject declared,legacy;
    QVERIFY(document.Load(file.fileName(),&error));
    QVERIFY2(MonitoringCompiler::Compile(document,"0",GetDAQ::CoBoMonitoringSettings::DeclaredParameters,&declared,&error),qPrintable(error));
    QVERIFY2(MonitoringCompiler::Compile(document,"0",GetDAQ::CoBoMonitoringSettings::LegacyCallArguments,&legacy,&error),qPrintable(error));
    const auto a = declared["boards"].toArray()[0].toObject(), b = legacy["boards"].toArray()[0].toObject();
    QCOMPARE(a["configuration2"].toString(),QString("21")); QCOMPARE(a["configuration3"].toString(),QString("1"));
    QCOMPARE(b["configuration2"].toString(),QString("49")); QCOMPARE(b["configuration3"].toString(),QString("0"));
}

void RunCoboMonitoringAlarmPolicy()
{
    const QString original = QFINDTESTDATA("../source/get/GetBench-software-release-20210208/data/config/configure-pedestals.xcfg");
    QFile source(original); QVERIFY(source.open(QIODevice::ReadOnly)); const QByteArray originalBytes = source.readAll();
    QTemporaryDir dir; const QString path = dir.filePath("alarms.xcfg");
    const QList<QByteArray> policies = {"<checkAsAd type=\"bool\">false</checkAsAd><subscribe>false</subscribe>",
                                       "<checkAsAd>1</checkAsAd>","<subscribe unit=\"V\">true</subscribe>"};
    for (int i = 0; i < policies.size(); ++i) {
        QByteArray bytes = originalBytes;
        bytes.replace("<Instance id=\"0\" />",QByteArray("<Instance id=\"0\"><Alarms>")+policies[i]+"</Alarms></Instance>");
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(bytes),qint64(bytes.size())); file.close();
        XcfgDocument document; QString error; QJsonObject output; output["keep"] = true;
        GetDAQ::CoBoMonitoringSettings settings; settings.asadMask = 9;
        QVERIFY(document.Load(path,&error));
        const bool compiled = MonitoringCompiler::Compile(document,"0",GetDAQ::CoBoMonitoringSettings::DeclaredParameters,&output,&error,&settings);
        if (i == 0) {
            QVERIFY2(compiled,qPrintable(error)); QVERIFY(!settings.checkAsAdAlarms); QVERIFY(!settings.subscribeAlarms);
            QVERIFY(!output["alarm_policy"].toObject()["check_asad"].toBool());
            QVERIFY(output["native_alarm_subscription_available"].toBool());
            QVERIFY(!output["hardware_deployment_supported"].toBool());
        } else { QVERIFY(!compiled); QVERIFY(output["keep"].toBool()); QCOMPARE(settings.asadMask,9U); }
    }
}

void RunCoboMonitoringValidationPreservesOutputs()
{
    const QString original = QFINDTESTDATA("../source/get/GetBench-software-release-20210208/data/config/configure-pedestals.xcfg");
    QFile source(original); QVERIFY(source.open(QIODevice::ReadOnly)); QByteArray bytes = source.readAll();
    bytes.replace("<Instance id=\"0\" />","<Instance id=\"0\"><AsAd id=\"0\"><Monitoring><LimitsIDD><upperValue unit=\"mA\">2</upperValue></LimitsIDD></Monitoring></AsAd></Instance>");
    QTemporaryDir dir; QFile file(dir.filePath("invalid.xcfg")); QVERIFY(file.open(QIODevice::WriteOnly)); file.write(bytes); file.close();
    XcfgDocument document; QString error; QJsonObject keep; keep["keep"] = true; GetDAQ::CoBoMonitoringSettings settings; settings.asadMask = 9;
    QVERIFY(document.Load(file.fileName(),&error));
    QVERIFY(!MonitoringCompiler::Compile(document,"0",GetDAQ::CoBoMonitoringSettings::DeclaredParameters,&keep,&error,&settings));
    QVERIFY(error.contains("Unsupported unit")); QVERIFY(keep["keep"].toBool()); QCOMPARE(settings.asadMask,9U);
    QVERIFY(!MonitoringCompiler::Save(document,"0",GetDAQ::CoBoMonitoringSettings::DeclaredParameters,dir.filePath("failed.json"),&error));
    QVERIFY(!QFile::exists(dir.filePath("failed.json")));
}

void RunCoboClockConfigurationRepository()
{
    const QString directory = QFINDTESTDATA("../source/get/GetBench-software-release-20210208/data/config");
    XcfgDocument document; QString error; QJsonObject output; GetDAQ::CoBoClockSettings settings;
    QVERIFY(document.Load(QDir(directory).filePath("configure-pedestals.xcfg"),&error));
    QVERIFY2(ClockCompiler::Compile(document,"0",2,&output,&error,&settings),qPrintable(error));
    QCOMPARE(output["kind"].toString(),QString("getdaq.cobo-clock-image")); QCOMPARE(output["asad_mask"].toInt(),1);
    QCOMPARE(output["pll_packets"].toArray().size(),26); QVERIFY(output["nominal_targets_matched"].toBool());
    QCOMPARE(output["boards"].toArray()[0].toObject()["lcm1"].toObject()["hex"].toString(),QString("0x48900001020000cf"));
    QVERIFY(!output["hardware_deployment_supported"].toBool()); QVERIFY(!output["complete_prepare"].toBool());
    QCOMPARE(settings.boards.size(),std::size_t(1)); QCOMPARE(settings.writingMHz,100.0f); QVERIFY(!settings.allowNominalDifference);
    QTemporaryDir dir; const auto path = dir.filePath("clock.json"); QVERIFY(ClockCompiler::Save(document,"0",2,path,&error));
    QFile existing(path); QVERIFY(existing.open(QIODevice::ReadOnly)); const auto bytes = existing.readAll(); existing.close();
    QVERIFY(!ClockCompiler::Save(document,"0",2,path,&error)); QVERIFY(existing.open(QIODevice::ReadOnly)); QCOMPARE(existing.readAll(),bytes);
    QVERIFY2(ClockCompiler::Compile(document,"0",1,&output,&error),qPrintable(error)); QVERIFY(output["program_reading_divider"].toBool());
    QVERIFY(document.Load(QDir(directory).filePath("configure-ExternalTrigger-2CoBo_1Mutant.xcfg"),&error));
    QVERIFY2(ClockCompiler::Compile(document,"Crate00_Slot00",2,&output,&error),qPrintable(error));
}

void RunCoboClockValidationPreservesOutputs()
{
    const QString directory = QFINDTESTDATA("../source/get/GetBench-software-release-20210208/data/config");
    QFile original(QDir(directory).filePath("configure-pedestals.xcfg")); QVERIFY(original.open(QIODevice::ReadOnly));
    QByteArray bytes = original.readAll(); QTemporaryDir dir; QFile file(dir.filePath("invalid-clock.xcfg"));
    bytes.replace("<Instance id=\"0\" />","<Instance id=\"0\"><AsAd id=\"0\"><Clocking><phaseShiftAdcClock>45</phaseShiftAdcClock></Clocking></AsAd></Instance>");
    QVERIFY(file.open(QIODevice::WriteOnly)); file.write(bytes); file.close();
    XcfgDocument document; QString error; QJsonObject keep; keep["keep"] = true; GetDAQ::CoBoClockSettings settings; settings.asadMask = 9;
    QVERIFY(document.Load(file.fileName(),&error));
    QVERIFY(!ClockCompiler::Compile(document,"0",2,&keep,&error,&settings)); QVERIFY(keep["keep"].toBool()); QCOMPARE(settings.asadMask,9U);
    QVERIFY(!ClockCompiler::Save(document,"0",2,dir.filePath("failed.json"),&error)); QVERIFY(!QFile::exists(dir.filePath("failed.json")));
}

void RunCoboConfigureParametersRepository()
{
    const QString directory = QFINDTESTDATA("../source/get/GetBench-software-release-20210208/data/config");
    XcfgDocument document; QString error; QJsonObject output;
    const QMap<int, quint16> versions = {{0,0x202},{1,0x202},{2,0x202},{3,0x202}};
    QVERIFY(document.Load(QDir(directory).filePath("configure-pedestals.xcfg"), &error));
    QVERIFY2(AgetCompiler::CompileConfigure(document, "0", versions, &output, &error), qPrintable(error));
    QCOMPARE(output["kind"].toString(), QString("getdaq.cobo-configure-image"));
    QCOMPARE(output["asad_mask"].toInt(), 1); QCOMPARE(output["asad_boards"].toArray().size(), 1);
    QCOMPARE(output["asad_boards"].toArray()[0].toObject()["inspection_value"].toString(), QString("5"));
    QCOMPARE(output["pulser"].toObject()["dac"].toString(), QString("15430"));
    QVERIFY(!output["hardware_deployment_supported"].toBool()); QCOMPARE(output["initialize_2p_delay_ms"].toInt(), 20);
    QTemporaryDir dir; const auto path = dir.filePath("configure.json");
    QVERIFY(AgetCompiler::SaveConfigure(document, "0", versions, path, &error));
    QVERIFY(!AgetCompiler::SaveConfigure(document, "0", versions, path, &error));
}

void RunCoboConfigureParametersRejectBeforeOutput()
{
    const QString directory = QFINDTESTDATA("../source/get/GetBench-software-release-20210208/data/config");
    QFile original(QDir(directory).filePath("configure-pedestals.xcfg")); QVERIFY(original.open(QIODevice::ReadOnly));
    QByteArray bytes = original.readAll(); QTemporaryDir dir; QFile file(dir.filePath("bad-generator.xcfg"));
    bytes.replace("<Instance id=\"0\" />", "<Instance id=\"0\"><AsAd id=\"0\"><Generator><triggerDelay>32</triggerDelay></Generator></AsAd></Instance>");
    QVERIFY(file.open(QIODevice::WriteOnly)); file.write(bytes); file.close();
    XcfgDocument document; QString error; QJsonObject keep; keep["keep"] = true;
    const QMap<int, quint16> versions = {{0,0x202},{1,0x202},{2,0x202},{3,0x202}};
    QVERIFY(document.Load(file.fileName(), &error));
    QVERIFY(!AgetCompiler::CompileConfigure(document, "0", versions, &keep, &error));
    QVERIFY(error.contains("triggerDelay")); QVERIFY(keep["keep"].toBool());
    QVERIFY(!AgetCompiler::SaveConfigure(document, "0", versions, dir.filePath("failed.json"), &error));
    QVERIFY(!QFile::exists(dir.filePath("failed.json")));
}

void RunCoboReadoutConfigurationRepository()
{
    const QString directory = QFINDTESTDATA("../source/get/GetBench-software-release-20210208/data/config");
    XcfgDocument document; QString error; QJsonObject output;
    const QMap<int, quint16> versions = {{0,0x202},{1,0x202},{2,0x202},{3,0x202}};
    QVERIFY(document.Load(QDir(directory).filePath("configure-pedestals.xcfg"), &error));
    QVERIFY2(AgetCompiler::CompileReadout(document, "0", versions, &output, &error), qPrintable(error));
    QCOMPARE(output["kind"].toString(), QString("getdaq.cobo-readout-image")); QVERIFY(output["full_readout"].toBool());
    QCOMPARE(output["trigger_mask"].toString(), QString("4")); QVERIFY(!output["hardware_deployment_supported"].toBool());
    int thresholds = 0; bool foundPeriod = false, foundPointer = false;
    for (const auto &v : output["writes"].toArray()) {
        const auto w = v.toObject();
        if (w["field"].toString() == "threshold") { ++thresholds; QCOMPARE(w["value"].toString(), QString("600")); }
        if (w["register"].toString() == "triggerPeriod_10ns") { foundPeriod = true; QCOMPARE(w["value"].toString(), QString("100000000")); }
        if (w["register"].toString() == "readPtr_asad3") { foundPointer = true; QCOMPARE(w["value"].toString(), QString::number(0xe000000)); }
    }
    QCOMPARE(thresholds, 272); QVERIFY(foundPeriod && foundPointer);
    QVERIFY(!output["parameters"].toArray().isEmpty()); QCOMPARE(output["sources"].toArray().size(), 1);
    QTemporaryDir dir; const auto path = dir.filePath("new.json");
    QVERIFY2(AgetCompiler::SaveReadout(document, "0", versions, path, &error), qPrintable(error));
    QVERIFY(!AgetCompiler::SaveReadout(document, "0", versions, path, &error));
}

void RunCoboReadoutConfigurationRejectsGlobalConflicts()
{
    const QString directory = QFINDTESTDATA("../source/get/GetBench-software-release-20210208/data/config");
    QFile original(QDir(directory).filePath("configure-pedestals.xcfg")); QVERIFY(original.open(QIODevice::ReadOnly));
    QByteArray bytes = original.readAll(); QTemporaryDir dir; QFile file(dir.filePath("conflict.xcfg"));
    bytes.replace("<Instance id=\"0\" />", "<Instance id=\"0\"><AsAd id=\"0\"><Aget id=\"0\"><Global><Reg1><isAllChannelRead>false</isAllChannelRead></Reg1></Global></Aget></AsAd></Instance>");
    QVERIFY(file.open(QIODevice::WriteOnly)); file.write(bytes); file.close();
    XcfgDocument document; QString error; QJsonObject keep; keep["keep"] = true;
    const QMap<int, quint16> versions = {{0,0x202},{1,0x202},{2,0x202},{3,0x202}};
    QVERIFY(document.Load(file.fileName(), &error));
    QVERIFY(!AgetCompiler::CompileReadout(document, "0", versions, &keep, &error)); QVERIFY(error.contains("isAllChannelRead")); QVERIFY(keep["keep"].toBool());
    QVERIFY(!AgetCompiler::SaveReadout(document, "0", versions, dir.filePath("failed.json"), &error)); QVERIFY(!QFile::exists(dir.filePath("failed.json")));
}

void RunCoboAgetConfigurationRepository()
{
    const QString directory = QFINDTESTDATA("../source/get/GetBench-software-release-20210208/data/config");
    XcfgDocument document; QString error; QJsonObject output; const QMap<int, quint16> versions = {{0,0x202},{1,0x202},{2,0x202},{3,0x202}};
    QVERIFY(document.Load(QDir(directory).filePath("configure-pedestals.xcfg"), &error));
    QVERIFY2(AgetCompiler::CompileCoBo(document, "0", versions, &output, &error), qPrintable(error));
    QCOMPARE(output["kind"].toString(), QString("getdaq.cobo-aget-register-image"));
    QCOMPARE(output["asad_mask"].toInt(), 1); QCOMPARE(output["aget_mask"].toInt(), 15);
    QCOMPARE(output["chips"].toArray().size(), 4); QVERIFY(output["fpn_read"].toBool());
    QCOMPARE(output["readout_offset"].toInt(), 0); QCOMPARE(output["readout_depth"].toInt(), 512);
    for (int i = 0; i < 4; ++i) {
        const auto chip = output["chips"].toArray()[i].toObject(); QCOMPARE(chip["aget_id"].toInt(), i);
        QVERIFY(!chip.contains("sources")); QCOMPARE(chip["registers"].toArray()[4].toObject()["hex"].toString(), QString("0xaaaaaaaaaaaaaaaa"));
    }
    QJsonObject keep; keep["keep"] = true; auto missing = versions; missing.remove(3);
    QVERIFY(!AgetCompiler::CompileCoBo(document, "0", missing, &keep, &error)); QVERIFY(error.contains("Missing version")); QVERIFY(keep["keep"].toBool());
    auto extra = versions; extra[15] = 0x202;
    QVERIFY(!AgetCompiler::CompileCoBo(document, "0", extra, &keep, &error)); QVERIFY(error.contains("inactive")); QVERIFY(keep["keep"].toBool());
}

void RunCoboAgetConfigurationMixedVersionAndOffsetConflict()
{
    const QString directory = QFINDTESTDATA("../source/get/GetBench-software-release-20210208/data/config");
    QFile original(QDir(directory).filePath("configure-pedestals.xcfg")); QVERIFY(original.open(QIODevice::ReadOnly));
    const QByteArray bytes = original.readAll(); QTemporaryDir dir; QFile file(dir.filePath("mixed.xcfg")); QVERIFY(file.open(QIODevice::WriteOnly));
    QByteArray mixed = bytes; mixed.replace("<Instance id=\"0\" />", "<Instance id=\"0\"><AsAd id=\"0\"><Aget id=\"3\"><Global><Reg1><peackingTime>68</peackingTime></Reg1></Global></Aget></AsAd></Instance>");
    file.write(mixed); file.close(); XcfgDocument document; QString error; QJsonObject output;
    QMap<int, quint16> versions = {{0,0x202},{1,0x202},{2,0x202},{3,0x203}};
    QVERIFY(document.Load(file.fileName(), &error));
    QVERIFY2(AgetCompiler::CompileCoBo(document, "0", versions, &output, &error), qPrintable(error));
    QCOMPARE(output["chips"].toArray()[3].toObject()["chip_version"].toString(), QString("0x203"));
    QByteArray conflict = bytes; conflict.replace("<Instance id=\"0\" />", "<Instance id=\"0\"><Module><readoutDepth>500</readoutDepth></Module><AsAd id=\"0\"><Aget id=\"3\"><Global><ReadoutPointerOffset>12</ReadoutPointerOffset></Global></Aget></AsAd></Instance>");
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate)); file.write(conflict); file.close();
    QVERIFY(document.Load(file.fileName(), &error)); versions[3] = 0x202;
    QVERIFY(!AgetCompiler::SaveCoBo(document, "0", versions, dir.filePath("invalid.json"), &error)); QVERIFY(error.contains("must share"));
    QVERIFY(!QFile::exists(dir.filePath("invalid.json")));
}

void RunAgetConfigurationRepository()
{
    const QString directory = QFINDTESTDATA("../source/get/GetBench-software-release-20210208/data/config");
    XcfgDocument document; QString error; QJsonObject output;
    QVERIFY2(document.Load(QDir(directory).filePath("configure-pedestals.xcfg"), &error), qPrintable(error));
    QVERIFY2(AgetCompiler::Compile(document, "0", 0, 0, 0x202, &output, &error), qPrintable(error));
    QCOMPARE(output["kind"].toString(), QString("getdaq.aget-register-image"));
    QVERIFY(!output["hardware_deployment_supported"].toBool());
    const auto registers = output["registers"].toArray(); QCOMPARE(registers.size(), 13);
    QCOMPARE(registers[0].toObject()["hex"].toString(), QString("0x283ac079"));
    QCOMPARE(registers[1].toObject()["hex"].toString(), QString("0x28a187"));
    QCOMPARE(registers[4].toObject()["value"].toString(), QString("12297829382473034410"));
    QCOMPARE(registers[7].toObject()["value"].toString(), QString("18446744073709551615"));
    const auto masks = output["readout_masks"].toArray(); QCOMPARE(masks.size(), 3);
    QCOMPARE(masks[0].toObject()["read_if_hit"].toString(), QString("15"));
    QCOMPARE(masks[1].toObject()["read_if_hit"].toString(), QString("4294967295"));
    bool inherited = false;
    for (const auto &p : output["parameters"].toArray()) {
        const auto entry = p.toObject();
        if (entry["query_path"].toString().endsWith("/Global/Reg1/peackingTime")) {
            QVERIFY(entry["source_path"].toString().contains("Instance[*]/AsAd[*]/Aget[*]")); inherited = true;
        }
    }
    QVERIFY(inherited); QCOMPARE(output["sources"].toArray().size(), 1);
    QVERIFY2(document.Load(QDir(directory).filePath("configure-ExternalTrigger-2CoBo_1Mutant.xcfg"), &error), qPrintable(error));
    QVERIFY2(AgetCompiler::Compile(document, "Crate00_Slot00", 0, 0, 0x202, &output, &error), qPrintable(error));
    QCOMPARE(output["cobo_instance"].toString(), QString("Crate00_Slot00"));
}

void RunAgetConfigurationRejectsInvalidSelection()
{
    const QString directory = QFINDTESTDATA("../source/get/GetBench-software-release-20210208/data/config");
    XcfgDocument document; QString error; QJsonObject output; output["keep"] = true;
    QVERIFY(document.Load(QDir(directory).filePath("configure-pedestals.xcfg"), &error));
    QVERIFY(!AgetCompiler::Compile(document, "missing", 0, 0, 0x202, &output, &error)); QVERIFY(output["keep"].toBool());
    QVERIFY(!AgetCompiler::Compile(document, "0", 4, 0, 0x202, &output, &error));
    QVERIFY(!AgetCompiler::Compile(document, "0", 0, 0, 0x203, &output, &error)); QVERIFY(error.contains("peackingTime"));
    QVERIFY(!AgetCompiler::Compile(document, "*", 0, 0, 0x202, &output, &error));
    QVERIFY(output["keep"].toBool());
}

void RunAgetConfigurationIncludeAndNewOutput()
{
    const QString directory = QFINDTESTDATA("../source/get/GetBench-software-release-20210208/data/config");
    QFile original(QDir(directory).filePath("configure-pedestals.xcfg")); QVERIFY(original.open(QIODevice::ReadOnly));
    const QByteArray contents = original.readAll(); const int begin = contents.indexOf("<Aget id=\"*\">");
    const int end = contents.indexOf("</Aget>", begin); QVERIFY(begin >= 0 && end > begin);
    QTemporaryDir dir; QFile included(dir.filePath("aget-defaults.xcfg")); QVERIFY(included.open(QIODevice::WriteOnly));
    included.write(contents.mid(begin, end + 7 - begin)); included.close();
    QFile file(dir.filePath("experiment.xcfg")); QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("<Setup><Node id=\"CoBo\"><Instance id=\"0\"><isActive>true</isActive>"
        "<Module><enableMem2pMode>false</enableMem2pMode><readoutDepth>512</readoutDepth></Module>"
        "<AsAd id=\"0\"><Control><isActive>true</isActive></Control><Aget id=\"0\" include=\"aget-defaults.xcfg\" />"
        "</AsAd></Instance></Node></Setup>"); file.close();
    XcfgDocument document; QString error; QJsonObject output;
    QVERIFY2(document.Load(file.fileName(), &error), qPrintable(error));
    QVERIFY2(AgetCompiler::Compile(document, "0", 0, 0, 0x202, &output, &error), qPrintable(error));
    QCOMPARE(output["sources"].toArray().size(), 2);
    QCOMPARE(output["registers"].toArray()[0].toObject()["hex"].toString(), QString("0x283ac079"));
    QCOMPARE(output["readout_masks"].toArray()[0].toObject()["read_if_hit"].toString(), QString("15"));
    const QString result = dir.filePath("compiled.json");
    QVERIFY2(AgetCompiler::Save(document, "0", 0, 0, 0x202, result, &error), qPrintable(error));
    QFile read(result); QVERIFY(read.open(QIODevice::ReadOnly)); const auto bytes = read.readAll(); read.close();
    QVERIFY(!AgetCompiler::Save(document, "0", 0, 0, 0x202, result, &error));
    QVERIFY(read.open(QIODevice::ReadOnly)); QCOMPARE(read.readAll(), bytes);
}

void RunHardwareDescriptionDefaultsAndNames()
{
    QTemporaryDir dir; QFile file(dir.filePath("hardware.xcfg")); QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("<Setup id=\"board\"><Device id=\"*\"><registerAccess>MemBus</registerAccess><registerWidth>8</registerWidth>"
               "<baseAddress>0xFFFFFFFFFFFFF000</baseAddress><Register id=\"*\"><offset>0x10</offset>"
               "<BitField id=\"*\"><offset>0</offset><width>1</width></BitField></Register></Device>"
               "<Device id=\"ctrl\"><Register id=\"flags\"><BitField id=\"C/S\"><width>4</width></BitField></Register>"
               "<Register id=\"status\"><readOnly>true</readOnly><offset>0x20</offset></Register></Device></Setup>"); file.close();
    XcfgDocument document; QString error; QVERIFY2(document.Load(file.fileName(), &error), qPrintable(error));
    const int scalar = document.Lookup("Setup/Device[ctrl]/Register[flags]/BitField[C/S]/width", &error);
    QVERIFY2(scalar >= 0, qPrintable(error)); QCOMPARE(document.Nodes()[scalar].value, QString("4"));
    QVERIFY(document.Lookup("Setup/Device[ctrl", &error) < 0);
    QJsonObject compiled; QVERIFY2(HardwareCompiler::Compile(document, &compiled, &error), qPrintable(error));
    const auto devices = compiled["devices"].toArray(); QCOMPARE(devices.size(), 1);
    const auto device = devices[0].toObject(); QCOMPARE(device["base_address"].toString(), QString("18446744073709547520"));
    QCOMPARE(device["register_bytes"].toInt(), 8);
    const auto registers = device["registers"].toArray(); QCOMPARE(registers.size(), 2);
    const auto field = registers[0].toObject()["fields"].toArray()[0].toObject();
    QCOMPARE(field["name"].toString(), QString("C/S")); QCOMPARE(field["width_bits"].toInt(), 4);
    QVERIFY(registers[1].toObject()["read_only"].toBool()); QVERIFY(!compiled["warnings"].toArray().isEmpty());
}

void RunHardwareDescriptionInvalidIsTransactional()
{
    QTemporaryDir dir; QFile file(dir.filePath("invalid.xcfg")); QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("<Setup id=\"board\"><Device id=\"ctrl\"><registerAccess>MemBus</registerAccess><registerWidth>4</registerWidth>"
               "<baseAddress>0x0</baseAddress><Register id=\"flags\"><offset>0x0</offset><BitField id=\"tooWide\">"
               "<offset>31</offset><width>2</width></BitField></Register></Device></Setup>"); file.close();
    XcfgDocument document; QString error; QVERIFY(document.Load(file.fileName(), &error));
    QJsonObject output; output["keep"] = true;
    QVERIFY(!HardwareCompiler::Compile(document, &output, &error)); QVERIFY(output["keep"].toBool());
    QVERIFY(error.contains("exceeds register width"));
}

void RunHardwareDescriptionRepository()
{
    const QString directory = QFINDTESTDATA("../source/get/GetBench-software-release-20210208/data/config");
    QVERIFY(!directory.isEmpty());
    const QStringList names = {"fullCoBoStandAlone", "Mutant", "Zedboard"};
    const QList<int> counts = {11, 5, 4};
    for (int i = 0; i < names.size(); ++i) {
        XcfgDocument document; QString error; QJsonObject compiled;
        QVERIFY2(document.Load(QDir(directory).filePath("hardwareDescription_" + names[i] + ".xcfg"), &error), qPrintable(error));
        QVERIFY2(HardwareCompiler::Compile(document, &compiled, &error), qPrintable(error));
        const auto devices = compiled["devices"].toArray(); QCOMPARE(devices.size(), counts[i]);
        if (i == 0) {
            bool thresholdFound = false;
            for (const auto &value : devices) if (value.toObject()["name"] == "zeroSuppress") {
                for (const auto &reg : value.toObject()["registers"].toArray()) if (reg.toObject()["name"] == "aget0ch1") {
                    QCOMPARE(reg.toObject()["offset"].toString(), QString("4"));
                    const auto field = reg.toObject()["fields"].toArray()[0].toObject();
                    QCOMPARE(field["name"].toString(), QString("threshold")); QCOMPARE(field["width_bits"].toInt(), 12);
                    thresholdFound = true;
                }
            }
            QVERIFY(thresholdFound);
        }
    }
}

} // namespace GetDAQTestInternal
