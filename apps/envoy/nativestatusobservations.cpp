#include "nativestatusobservations.h"
#include <stdexcept>

namespace {
NativeObservationUpdate Result(const QString &previous,const QString &error,bool bodyChanged=false) {
    NativeObservationUpdate update;
    update.error=error;
    update.errorChanged=previous!=error;
    update.bodyChanged=bodyChanged;
    return update;
}
}

void NativeStatusObservations::Reset() {
    ResetAlarms();
    fReceivers=GetDAQ::EccControlReceiverObservation();
    fReceiverClock.invalidate();fReceiverError.clear();fReceiverBody.clear();
    ResetSources();ResetReceiverFailure();
}

void NativeStatusObservations::ResetAlarms() {
    fAlarms=GetDAQ::EccControlAlarmStatus();
    fAlarmClock.invalidate();fAlarmBody.clear();
}

void NativeStatusObservations::ResetSources() {
    fSources=GetDAQ::EccControlSourceObservation();
    fSourceClock.invalidate();fSourceError.clear();
}

void NativeStatusObservations::ResetReceiverFailure() {
    fReceiverFailure=GetDAQ::EccControlReceiverFailureStatus();
    fReceiverFailureClock.invalidate();fReceiverFailureError.clear();fReceiverFailureBody.clear();
}

NativeObservationUpdate NativeStatusObservations::ObserveAlarms(
    const QByteArray &body,const QString &error) {
    NativeObservationUpdate update;
    update.error=error;
    if(!error.isEmpty()) {
        fAlarmClock.invalidate();
        return update;
    }
    try {
        const auto observation=GetDAQ::DecodeEccControlAlarmStatus(body.toStdString());
        update.bodyChanged=body!=fAlarmBody;
        fAlarms=observation;
        if(update.bodyChanged)fAlarmBody=body;
        fAlarmClock.restart();
    } catch(const std::exception &e) {
        update.error=QString::fromUtf8(e.what()).left(512);
    }
    return update;
}

NativeObservationUpdate NativeStatusObservations::ObserveReceivers(
    const QByteArray &body,const QString &error,const QSet<QString> &expectedAgents) {
    const QString previous=fReceiverError;
    QString failure=error;
    if(failure.isEmpty())try {
        const auto observation=GetDAQ::DecodeEccControlReceiverObservation(body.toStdString());
        QSet<QString> actual;
        for(const auto &node:observation.nodes)actual.insert(QString::fromStdString(node.agentId));
        if(expectedAgents!=actual)
            throw std::runtime_error("Receiver observation agents differ from configured whole system");
        fReceivers=observation;fReceiverBody=body;fReceiverClock.restart();
    } catch(const std::exception &e) {failure=QString::fromUtf8(e.what()).left(512);}
    if(!failure.isEmpty()) {fReceiverClock.invalidate();fReceiverBody.clear();}
    fReceiverError=failure;
    return Result(previous,failure);
}

NativeObservationUpdate NativeStatusObservations::ObserveSources(const QByteArray &body,const QString &error) {
    const QString previous=fSourceError;
    QString failure=error;
    if(failure.isEmpty())try {
        fSources=GetDAQ::DecodeEccControlSourceObservation(body.toStdString());fSourceClock.restart();
    } catch(const std::exception &e) {failure=QString::fromUtf8(e.what()).left(512);}
    if(!failure.isEmpty())fSourceClock.invalidate();
    fSourceError=failure;
    return Result(previous,failure);
}

NativeObservationUpdate NativeStatusObservations::ObserveReceiverFailure(const QByteArray &body,const QString &error) {
    const QString previous=fReceiverFailureError;
    QString failure=error;
    bool bodyChanged=false;
    if(failure.isEmpty())try {
        const auto policy=GetDAQ::DecodeEccControlReceiverFailureStatus(body.toStdString());
        fReceiverFailure=policy;fReceiverFailureClock.restart();
        bodyChanged=body!=fReceiverFailureBody;
        if(bodyChanged)fReceiverFailureBody=body;
    } catch(const std::exception &e) {failure=QString::fromUtf8(e.what()).left(512);}
    if(!failure.isEmpty())fReceiverFailureClock.invalidate();
    fReceiverFailureError=failure;
    return Result(previous,failure,bodyChanged);
}

bool NativeStatusObservations::ReceiversFresh(int maximumAgeMs) const {
    return fReceiverClock.isValid() && fReceiverClock.elapsed()<=maximumAgeMs;
}
bool NativeStatusObservations::AlarmsFresh(int maximumAgeMs) const {
    return fAlarmClock.isValid() && fAlarmClock.elapsed()<=maximumAgeMs;
}
bool NativeStatusObservations::SourcesFresh(int maximumAgeMs) const {
    return fSourceClock.isValid() && fSourceClock.elapsed()<=maximumAgeMs;
}
bool NativeStatusObservations::ReceiverFailureFresh(int maximumAgeMs) const {
    return fReceiverFailureClock.isValid() && fReceiverFailureClock.elapsed()<=maximumAgeMs;
}
