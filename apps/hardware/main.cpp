#include "hardwareclient.h"
#include "coboalarmclient.h"
#include <mdaq/utl/CmdException.h>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
using namespace GetDAQ;
namespace {
void Usage() {
    std::cerr << "Usage: getdaq-hardware [--encoding 1.0|1.1] [--description FILE] COMMAND PROXY [ARGS]\n"
        "  validate-description FILE  (offline)\n  create-description PROXY FILE NEW_AUDIT_FILE  (empty node only)\n"
        "  devices PROXY\n  describe PROXY DEVICE\n  read-register PROXY DEVICE REGISTER\n"
        "  read-field PROXY DEVICE REGISTER FIELD\n"
        "  write-register PROXY DEVICE REGISTER VALUE NEW_AUDIT_FILE\n"
        "  write-field PROXY DEVICE REGISTER FIELD VALUE NEW_AUDIT_FILE\n"
        "  pipeline-init PROXY NEW_AUDIT_FILE  (resets CoBo pipeline; not a full Prepare or realignment)\n"
        "  asad-calibrate PROXY MASK NEW_AUDIT_FILE  (requires --description; powered/initialized AsAd, acquisition stopped)\n"
        "  watch-asad-alarms PROXY COBO_ID LISTEN_ENDPOINTS SECONDS NEW_EVENT_FILE NEW_SUBSCRIBE_FILE NEW_UNSUBSCRIBE_FILE\n"
        "    Optional --callback-published ENDPOINTS; observes existing board watcher, never sends hardware Stop.\n"
        "    SECONDS: 1..86400; returns 3 for a latched alarm/fault, 1 for an operation failure.\n"
        "Default encoding: 1.0. Values: unsigned decimal or 0x hexadecimal. No automatic readback or retry.\n";
}
std::string Number(std::uint64_t n) { return JsonString(std::to_string(n)); }
void PrintAlarmState(const char *phase, const CoBoAlarmSnapshot &s)
{
    const char *states[] = {"fresh","subscribing","active","unsubscribing","closed","unknown"};
    std::cout << "{\"phase\":" << JsonString(phase) << ",\"subscription\":" << JsonString(states[s.subscription])
        << ",\"received\":" << Number(s.received) << ",\"durable\":" << Number(s.durable) << ",\"lost\":" << Number(s.lost)
        << ",\"invalid\":" << Number(s.invalid) << ",\"pending\":" << s.pending << ",\"alarm_mask\":" << s.alarmMask
        << ",\"stop_requested\":" << (s.stopRequested ? "true" : "false")
        << ",\"queue_overflow\":" << (s.queueOverflow ? "true" : "false") << ",\"audit_failed\":" << (s.auditFailed ? "true" : "false")
        << ",\"budget_exceeded\":" << (s.budgetExceeded ? "true" : "false")
        << ",\"hardware_stop_performed\":false,\"callback_health_verified\":false}" << std::endl;
}
int WatchAlarms(const CoBoAlarmOptions &options, unsigned int seconds, const std::string &subscribeAudit, const std::string &unsubscribeAudit)
{
    CoBoAlarmClient client(options);
    const auto drain = [&] {
        for (const auto &event : client.Drain(options.maxPendingEvents))
            std::cout << "{\"phase\":\"alarm\",\"sequence\":" << Number(event.sequence)
                << ",\"received_unix_ms\":" << Number(event.receivedUnixMs) << ",\"alarm_bits\":" << event.alarmBits
                << ",\"stop_requested\":true,\"hardware_stop_performed\":false}" << std::endl;
    };
    try {
        client.Subscribe(subscribeAudit); PrintAlarmState("subscription_active",client.Snapshot());
        const auto deadline = std::chrono::steady_clock::now()+std::chrono::seconds(seconds);
        while (!client.Snapshot().stopRequested && std::chrono::steady_clock::now() < deadline) {
            drain(); std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        drain(); client.Unsubscribe(unsubscribeAudit); drain(); const auto final = client.Snapshot();
        PrintAlarmState("finished",final); return final.stopRequested ? 3 : 0;
    } catch (...) { PrintAlarmState("failed",client.Snapshot()); throw; }
}
void Print(const DeviceInfo &device) {
    std::cout << "{\"name\":" << JsonString(device.name) << ",\"access\":" << JsonString(device.access)
        << ",\"base_address\":" << Number(device.baseAddress) << ",\"register_bytes\":" << device.registerBytes << ",\"registers\":[";
    bool firstRegister = true;
    for (const auto &reg : device.registers) {
        if (!firstRegister) std::cout << ',';
        firstRegister = false;
        std::cout << "{\"name\":" << JsonString(reg.name) << ",\"offset\":" << Number(reg.offset)
            << ",\"read_only\":" << (reg.readOnly ? "true" : "false") << ",\"fields\":[";
        bool firstField = true;
        for (const auto &field : reg.fields) {
            if (!firstField) std::cout << ',';
            firstField = false;
            std::cout << "{\"name\":" << JsonString(field.name) << ",\"offset_bits\":" << field.offset
                << ",\"width_bits\":" << field.width << ",\"read_only\":" << (field.readOnly ? "true" : "false") << '}';
        }
        std::cout << "]}";
    }
    std::cout << "]}\n";
}
}
int main(int argc, char **argv) {
    int begin = 1, minor = 0;
    std::string descriptionPath, publishedEndpoints;
    if (argc == 2 && std::string(argv[1]) == "--help") { Usage(); return 0; }
    bool encodingSeen = false, publishedSeen = false;
    while (argc > begin && std::string(argv[begin]).find("--") == 0) {
        if (argc <= begin + 1) { Usage(); return 2; }
        const std::string option = argv[begin], value = argv[begin + 1];
        if (option == "--encoding" && !encodingSeen && (value == "1.0" || value == "1.1")) {
            minor = value == "1.1" ? 1 : 0; encodingSeen = true;
        } else if (option == "--description" && descriptionPath.empty() && !value.empty()) descriptionPath = value;
        else if (option == "--callback-published" && !publishedSeen && !value.empty()) { publishedEndpoints = value; publishedSeen = true; }
        else { Usage(); return 2; }
        begin += 2;
    }
    if (argc < begin + 2) { Usage(); return 2; }
    const std::string command = argv[begin];
    int count = 0;
    if (command == "devices") count = 2;
    else if (command == "describe") count = 3;
    else if (command == "read-register") count = 4;
    else if (command == "read-field") count = 5;
    else if (command == "write-register") count = 6;
    else if (command == "write-field") count = 7;
    else if (command == "pipeline-init") count = 3;
    else if (command == "asad-calibrate") count = 4;
    else if (command == "create-description") count = 4;
    else if (command == "validate-description") count = 2;
    else if (command == "watch-asad-alarms") count = 8;
    if (!count || argc != begin + count) { Usage(); return 2; }
    if ((publishedSeen && command != "watch-asad-alarms") || (command == "watch-asad-alarms" && !descriptionPath.empty())) { Usage(); return 2; }
    const bool writing = command == "write-register" || command == "write-field";
    std::uint64_t value = 0;
    unsigned int asadMask = 0;
    unsigned int watchSeconds = 0;
    if (command == "watch-asad-alarms") {
        try {
            const auto seconds = ParseUnsigned(argv[begin+4]);
            if (seconds < 1 || seconds > 86400) throw std::invalid_argument("Alarm watch duration must be 1..86400 seconds");
            watchSeconds = static_cast<unsigned int>(seconds);
            for (int i = 5; i < 8; ++i) for (int j = i+1; j < 8; ++j)
                if (std::string(argv[begin+i]) == argv[begin+j]) throw std::invalid_argument("Alarm event and command audit paths must be distinct");
        } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 2; }
    }
    if (command == "asad-calibrate") {
        try {
            const auto mask = ParseUnsigned(argv[begin + 2]);
            if (mask == 0 || mask > 15 || descriptionPath.empty())
                throw std::invalid_argument("asad-calibrate requires --description FILE and mask 1..15");
            asadMask = static_cast<unsigned int>(mask);
        } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 2; }
    }
    if (writing) {
        try { value = ParseUnsigned(argv[argc - 2]); }
        catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 2; }
    }
    try {
        if (command == "watch-asad-alarms") {
            CoBoAlarmOptions options; options.nodeProxy = argv[begin+1]; options.expectedCoBoId = argv[begin+2];
            options.callbackEndpoints = argv[begin+3]; options.publishedEndpoints = publishedEndpoints;
            options.eventAuditPath = argv[begin+5]; options.encodingMinor = minor;
            return WatchAlarms(options,watchSeconds,argv[begin+6],argv[begin+7]);
        }
        if (command == "validate-description") {
            if (!descriptionPath.empty()) throw std::invalid_argument("Use the positional file for validate-description");
            const auto description = LoadHardwareDescription(argv[begin + 1]);
            std::cout << "{\"valid\":true,\"node_id\":" << JsonString(description.nodeId)
                << ",\"devices\":" << description.devices.size() << "}\n";
            return 0;
        }
        HardwareDescription description;
        if (command == "create-description") {
            if (!descriptionPath.empty()) throw std::invalid_argument("Use the positional file for create-description");
            description = LoadHardwareDescription(argv[begin + 2]);
        } else if (!descriptionPath.empty()) description = LoadHardwareDescription(descriptionPath);
        HardwareClient client(argv[begin + 1], minor);
        if (!descriptionPath.empty()) client.UseDescription(description);
        if (command == "create-description") {
            client.Create(description, argv[begin + 3]);
            std::cout << "{\"phase\":\"complete\",\"metadata_verified\":true,\"register_values_read\":false,\"board_permissions_verified\":false}\n";
        } else if (command == "pipeline-init") {
            client.Execute(CoBoPipelineInit(), argv[begin + 2]);
            std::cout << "{\"phase\":\"complete\",\"operation\":\"pipeline-init\",\"full_prepare\":false}\n";
        } else if (command == "asad-calibrate") {
            client.Execute(CoBoAsAdCalibrate(asadMask), argv[begin + 3]);
            std::cout << "{\"phase\":\"complete\",\"operation\":\"asad-calibrate\",\"asad_mask\":" << asadMask
                << ",\"full_prepare\":false}\n";
        } else if (command == "devices") {
            const auto names = client.Devices();
            std::cout << "{\"node\":" << JsonString(client.Name()) << ",\"devices\":[";
            for (std::size_t i = 0; i < names.size(); ++i) { if (i) std::cout << ','; std::cout << JsonString(names[i]); }
            std::cout << "]}\n";
        } else if (command == "describe") Print(client.Describe(argv[begin + 2]));
        else {
            const std::string device = argv[begin + 2], reg = argv[begin + 3];
            const std::string field = command == "write-field" || command == "read-field" ? argv[begin + 4] : "";
            if (writing) {
                client.Write(device, reg, field, value, argv[argc - 1]);
                std::cout << "{\"phase\":\"acknowledged\",\"readback_verified\":false}\n";
            } else std::cout << "{\"value\":" << Number(client.Read(device, reg, field)) << "}\n";
        }
        return 0;
    } catch (const mdaq::utl::CmdException &e) { std::cerr << "Hardware error: " << e.reason << '\n'; }
      catch (const std::exception &e) { std::cerr << e.what() << '\n'; }
    return 1;
}
