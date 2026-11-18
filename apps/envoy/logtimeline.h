#ifndef GETDAQ_LOGTIMELINE_H
#define GETDAQ_LOGTIMELINE_H
#include <QString>
#include <QQueue>
#include <QMap>
#include <QDateTime>

// All writes and export snapshots belong to the GUI thread. Workers only return
// immutable read results; their completion order receives a serialized sequence.
class LogTimeline {
public:
    explicit LogTimeline(qint64 budget = 8*1024*1024): fBudget(budget) {}
    void Append(const QString &source,const QString &text,qint64 observedMs=-1);
    void AppendChunk(const QString &stream,const QString &source,const QString &text,
                     bool reset=false,qint64 observedMs=-1);
    QString ExportText(int pendingReads=0) const;
    static QString CompactOutput(QString text);
private:
    struct Record { QString source,text; qint64 time; quint64 sequence; };
    struct Fragment { QString source,text; qint64 time; quint64 sequence; };
    QQueue<Record> fRecords;
    QMap<QString,Fragment> fFragments;
    QMap<QString,bool> fWaiting;
    qint64 fBudget,fBytes=0,fLastTime=0;
    quint64 fSequence=0,fDropped=0;
    qint64 Observe(qint64 requested);
};
#endif
