#ifndef NATIVESTATUSOBSERVATIONS_H
#define NATIVESTATUSOBSERVATIONS_H

#include "../../common/control/eccprotocol.h"
#include <QByteArray>
#include <QElapsedTimer>
#include <QSet>
#include <QString>

struct NativeObservationUpdate {
    QString error;
    bool errorChanged=false;
    bool bodyChanged=false;
};

// Owns decoded Native status evidence and its freshness lifecycle. It has no
// control, logging, journal, or UI side effects; those remain with the caller.
class NativeStatusObservations {
public:
    void Reset();
    void ResetAlarms();
    void ResetSources();
    void ResetReceiverFailure();

    NativeObservationUpdate ObserveAlarms(const QByteArray &body,const QString &error);
    NativeObservationUpdate ObserveReceivers(const QByteArray &body,const QString &error,
                                             const QSet<QString> &expectedAgents);
    NativeObservationUpdate ObserveSources(const QByteArray &body,const QString &error);
    NativeObservationUpdate ObserveReceiverFailure(const QByteArray &body,const QString &error);

    bool AlarmsFresh(int maximumAgeMs) const;
    bool ReceiversFresh(int maximumAgeMs) const;
    bool SourcesFresh(int maximumAgeMs) const;
    bool ReceiverFailureFresh(int maximumAgeMs) const;

    const GetDAQ::EccControlAlarmStatus &Alarms() const { return fAlarms; }
    const GetDAQ::EccControlReceiverObservation &Receivers() const { return fReceivers; }
    const GetDAQ::EccControlSourceObservation &Sources() const { return fSources; }
    const GetDAQ::EccControlReceiverFailureStatus &ReceiverFailure() const { return fReceiverFailure; }
    const QString &ReceiverError() const { return fReceiverError; }
    const QString &SourceError() const { return fSourceError; }
    const QString &ReceiverFailureError() const { return fReceiverFailureError; }
    const QByteArray &AlarmBody() const { return fAlarmBody; }
    const QByteArray &ReceiverBody() const { return fReceiverBody; }
    const QByteArray &ReceiverFailureBody() const { return fReceiverFailureBody; }

private:
    GetDAQ::EccControlAlarmStatus fAlarms;
    QElapsedTimer fAlarmClock;
    QByteArray fAlarmBody;
    GetDAQ::EccControlReceiverObservation fReceivers;
    QElapsedTimer fReceiverClock;
    QString fReceiverError;
    QByteArray fReceiverBody;
    GetDAQ::EccControlSourceObservation fSources;
    QElapsedTimer fSourceClock;
    QString fSourceError;
    GetDAQ::EccControlReceiverFailureStatus fReceiverFailure;
    QByteArray fReceiverFailureBody;
    QElapsedTimer fReceiverFailureClock;
    QString fReceiverFailureError;
};

#endif
