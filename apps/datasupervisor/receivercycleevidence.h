#ifndef GETDAQ_RECEIVERCYCLEEVIDENCE_H
#define GETDAQ_RECEIVERCYCLEEVIDENCE_H
#include <memory>
#include <string>
#include <vector>
namespace GetDAQ {
// Metadata binding produced only after the live supervisor verified a normal
// completed cycle. Never opens raw payload files, hashes frames or adopts PIDs.
std::string CaptureReceiverCycleEvidence(const std::string &deploymentRoot,
    const std::vector<std::string> &metadataFiles,const std::vector<std::string> &receiverDirectories);
class ReceiverCycleEvidence {
public:
    explicit ReceiverCycleEvidence(const std::string &json);
    ~ReceiverCycleEvidence();
    ReceiverCycleEvidence(const ReceiverCycleEvidence &)=delete;
    ReceiverCycleEvidence &operator=(const ReceiverCycleEvidence &)=delete;
    void Check() const; // Identity/metadata/size drift latches; restoration does not recover.
    // After activation, retain the retired deployment lease and audit metadata.
    // Historical receiver output may be archived without holding new acquisition.
    void CheckReservation() const;
    void ReserveRetiredDeployment(); // Exclusive original deployment lease; no process signals.
    bool Faulted() const;
private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
}
#endif
