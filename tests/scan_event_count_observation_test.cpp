#include "scaneventcountobservation.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using namespace GetDAQ;

void Require(bool value,const char *message) {
    if (!value) throw std::runtime_error(message);
}

template<class F> void Reject(F operation) {
    bool rejected=false;
    try { operation(); } catch (const std::exception &) { rejected=true; }
    Require(rejected,"Invalid scan event observation was accepted");
}

struct Fixture {
    ScanRuntimeIdentityExpectation expected;
    EccControlSystemStatus status;
    EccControlReceiverObservation receivers;
    std::string response;

    Fixture() {
        expected.instanceId=std::string(64,'a');
        expected.snapshotSha256=std::string(64,'b');
        expected.dataBindingSha256=std::string(64,'c');
        expected.dataNodes.push_back({"A0",std::string(64,'d'),std::string(64,'e')});

        status.instanceId=expected.instanceId;
        status.snapshotSha256=expected.snapshotSha256;
        status.controller.state=5;

        receivers.instanceId=expected.instanceId;
        receivers.snapshotSha256=expected.snapshotSha256;
        receivers.dataBindingSha256=expected.dataBindingSha256;
        EccControlReceiverNodeObservation node;
        node.agentId="A0";
        node.instanceId=expected.dataNodes[0].instanceId;
        node.planSha256=expected.dataNodes[0].planSha256;
        node.available=true;
        node.lifecycle.state="ready";
        node.lifecycle.sessionId=std::string(64,'f');
        EccControlReceiverProcess process;
        process.id="C0";process.state="running";process.pid=4242;
        node.receivers.push_back(process);
        receivers.nodes.push_back(node);

        response="{\"schema\":1,\"kind\":\"getdaq.ecc-receiver-events\",\"instance_id\":\""+
            expected.instanceId+"\",\"snapshot_sha256\":\""+expected.snapshotSha256+
            "\",\"data_binding_sha256\":\""+expected.dataBindingSha256+
            "\",\"events\":{\"agent_id\":\"A0\",\"plan_sha256\":\""+
            expected.dataNodes[0].planSha256+"\",\"instance_id\":\""+
            expected.dataNodes[0].instanceId+"\",\"source_id\":\"C0\",\"session_id\":\""+
            node.lifecycle.sessionId+"\",\"pid\":4242,\"available\":true,\"acquisition_events\":\"1234\",\"observed_at_unix_ms\":\"1700000000000\"}}";
    }

    ScanEventCountBaseline Bind(bool statusFresh=true,bool receiversFresh=true) const {
        return BindScanEventCountObservation("C0",response,statusFresh,status,receiversFresh,
                                             receivers,expected,"C0");
    }
};
}

int main() {
    try {
        Fixture fixture;
        const ScanEventCountBaseline baseline=fixture.Bind();
        Require(baseline.instanceId==fixture.expected.instanceId &&
                baseline.snapshotSha256==fixture.expected.snapshotSha256 &&
                baseline.dataBindingSha256==fixture.expected.dataBindingSha256 &&
                baseline.agentId=="A0" && baseline.sourceId=="C0" &&
                baseline.pid==4242 && baseline.acquisitionEvents==1234 &&
                baseline.observedAtUnixMs==1700000000000ull,
                "Validated scan event sample lost its bound identity/counters");

        Reject([&] { fixture.Bind(false,true); });
        Reject([&] { fixture.Bind(true,false); });
        Reject([&] { BindScanEventCountObservation("C1",fixture.response,true,fixture.status,true,
                                                    fixture.receivers,fixture.expected,"C0"); });
        {
            auto status=fixture.status;status.controller.state=4;
            Reject([&] { BindScanEventCountObservation("C0",fixture.response,true,status,true,
                                                        fixture.receivers,fixture.expected,"C0"); });
        }
        {
            auto status=fixture.status;status.snapshotSha256=std::string(64,'9');
            Reject([&] { BindScanEventCountObservation("C0",fixture.response,true,status,true,
                                                        fixture.receivers,fixture.expected,"C0"); });
        }
        {
            auto receivers=fixture.receivers;receivers.dataBindingSha256=std::string(64,'9');
            Reject([&] { BindScanEventCountObservation("C0",fixture.response,true,fixture.status,true,
                                                        receivers,fixture.expected,"C0"); });
        }
        {
            auto receivers=fixture.receivers;receivers.nodes[0].receivers[0].pid=4243;
            Reject([&] { BindScanEventCountObservation("C0",fixture.response,true,fixture.status,true,
                                                        receivers,fixture.expected,"C0"); });
        }
        std::cout<<"scan event count observation binding passed"<<std::endl;
        return 0;
    } catch (const std::exception &error) {
        std::cerr<<error.what()<<std::endl;
        return 1;
    }
}
