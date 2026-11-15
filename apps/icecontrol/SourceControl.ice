// Source-side control extension. Raw MFM is absent from this interface.
#include <mdaq/DaqControl.ice>
module GetDAQ
{
struct SourceInfo
{
    int version;
    string sourceId;
    string instanceId;
    string receiverAddress;
    string receiverSession;
    string state;
    string error;
    long issuedBytes;
    long locallySentBytes;
    bool startIssued;
    bool stopAcknowledged;
    bool drainAcknowledged;
    bool finishIssued;
    bool sourceEndAcknowledged;
    bool failureStopIssued;
    bool failureStopAcknowledged;
};
interface DataSourceControl extends mdaq::DaqCtrlNode
{
    idempotent SourceInfo describeSource();
    void connectIceStream(string receiverAddress,string receiverSession,string sourceId,string sourceInstanceId) throws mdaq::utl::CmdException;
    void configureSourcePolicy(int mask,bool alwaysFlush) throws mdaq::utl::CmdException;
    void closeIceStream(string receiverSession) throws mdaq::utl::CmdException;
    void stopIceSourceAfterFailure(string receiverSession) throws mdaq::utl::CmdException;
};
// Optional recovery extension: original DataSourceControl/SourceInfo wire layout
// stays unchanged for existing source applications and control clients.
struct FailedSourceRetirement
{
    int version;
    SourceInfo source;
    bool issued;
    bool producerIdleVerified;
    bool dataConnectionClosed;
    bool retired;
};
interface DataSourceRecovery extends DataSourceControl
{
    idempotent FailedSourceRetirement describeFailedSourceRetirement(string receiverSession);
    FailedSourceRetirement retireFailedIceStream(string receiverSession) throws mdaq::utl::CmdException;
};
};
