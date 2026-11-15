#ifndef GETDAQ_TEST_SOURCE_OBSERVATION_BARRIER_H
#define GETDAQ_TEST_SOURCE_OBSERVATION_BARRIER_H
#include "ice_ecc_source_fixture.h"
#include <future>
namespace GetDAQReceiverFixture {
// Test-only one-shot barrier. Production control calls after this read continue
// on another server dispatch thread; no sleep-based inference of entry.
class ObservationBarrierSource : public EccIceSource {
    std::atomic<bool> fArmed{false},fEntered{false},fReleased{false};
    std::mutex fMutex;
    std::condition_variable fCondition;
public:
    ObservationBarrierSource(IceProducer &producer,const GetDAQ::SourceControlOptions &options)
        : EccIceSource(producer,options) {}
    void ArmObservation() {fEntered=false;fReleased=false;fArmed=true;}
    void WaitForObservation() {
        std::unique_lock<std::mutex> lock(fMutex);
        Require(fCondition.wait_for(lock,std::chrono::seconds(2),[this] {return fEntered.load();}),"Source observation never reached barrier");
    }
    void ReleaseObservation() {fReleased=true;fCondition.notify_all();}
    GetDAQ::SourceInfo describeSource(const Ice::Current &current) override {
        if(fArmed.exchange(false)) {
            fEntered=true;fCondition.notify_all();
            std::unique_lock<std::mutex> lock(fMutex);
            Require(fCondition.wait_for(lock,std::chrono::seconds(2),[this] {return fReleased.load();}),"Test source observation barrier not released");
        }
        return EccIceSource::describeSource(current);
    }
};
}
#endif
