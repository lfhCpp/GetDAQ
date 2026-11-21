#include "logtimeline.h"
#include <QRegularExpression>
#include <QVector>
#include <algorithm>

qint64 LogTimeline::Observe(qint64 requested) {
    // Preserve order even if the local wall clock is corrected backwards.
    fLastTime=qMax(fLastTime,requested<0?QDateTime::currentMSecsSinceEpoch():requested);
    return fLastTime;
}
QString LogTimeline::CompactOutput(QString text) {
    static const QRegularExpression waiting("(?:^|(?<=[\\r\\n]))(?:[ \\t]*Waiting for client connection[ .]*[\\r\\n]+)+");
    return text.replace(waiting,QStringLiteral("Waiting for incoming data connection (receiver listening).\n"));
}
void LogTimeline::Append(const QString &source,const QString &text,qint64 observedMs) {
    if(text.isEmpty())return;
    auto content=text;
    if(qint64(content.size())*2>fBudget){content=content.right(int(fBudget/2));++fDropped;}
    fRecords.enqueue({source,content,Observe(observedMs),++fSequence});fBytes+=content.size()*2;
    while((fBytes>fBudget || fRecords.size()>10000) && !fRecords.isEmpty()){fBytes-=fRecords.dequeue().text.size()*2;++fDropped;}
}
void LogTimeline::AppendChunk(const QString &stream,const QString &source,const QString &text,bool reset,qint64 observedMs) {
    const auto time=Observe(observedMs);
    if(reset){
        if(fFragments.contains(stream)){const auto old=fFragments.take(stream);Append(old.source+" [partial line before rotation]",old.text,time);}
        fWaiting.remove(stream);
    }
    auto &pending=fFragments[stream];
    if(pending.text.isEmpty()){pending.source=source;pending.time=time;pending.sequence=++fSequence;}
    pending.text+=text;
    const int end=qMax(pending.text.lastIndexOf('\n'),pending.text.lastIndexOf('\r'));
    if(end>=0){
        const auto complete=CompactOutput(pending.text.left(end+1));
        const bool waiting=complete.trimmed()=="Waiting for incoming data connection (receiver listening).";
        if(!waiting || !fWaiting.value(stream))Append(source,complete,time);
        fWaiting[stream]=waiting;pending.text.remove(0,end+1);pending.time=time;pending.sequence=++fSequence;
    }
    if(pending.text.size()>256*1024){Append(source+" [partial long line]",pending.text,time);pending.text.clear();}
    if(pending.text.isEmpty())fFragments.remove(stream);
}
QString LogTimeline::ExportText(int pendingReads) const {
    // Copy first: the snapshot never seeks a moving end of a live log file.
    QVector<Record> records;for(const auto &entry:fRecords)records.append(entry);
    for(const auto &fragment:fFragments)records.append({fragment.source+" [partial line at snapshot]",fragment.text,fragment.time,fragment.sequence});
    std::stable_sort(records.begin(),records.end(),[](const Record &a,const Record &b){return a.time==b.time?a.sequence<b.sequence:a.time<b.time;});
    QString result="GetDAQ combined log snapshot\nSnapshot UTC: "+QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)+
        "\nOrder: local observation time (UTC), then serialized sequence. Native source text is retained.\n"
        "Backend lines without timestamps use the time received by this GUI; this is not cross-host hardware event time.\n"
        "Scope: this GUI session's collected Event / ECC / Data / Sentry logs. Backend streams start at a fresh end-of-file boundary; output from before attachment is excluded.\n"
        "Each read uses a fixed file-size boundary. Reads already in flight are not part of this snapshot.\n"
        "Clear affects the display only. Original files and collected export history are retained.\n";
    result+="Pending reads: "+QString::number(pendingReads)+"; history budget: "+QString::number(fBudget)+" bytes; discarded older records: "+QString::number(fDropped)+"\n";
    for(const auto &entry:records){result+="\n==================== "+entry.source+" ====================\n";
        result+=QDateTime::fromMSecsSinceEpoch(entry.time,Qt::UTC).toString(Qt::ISODateWithMs)+" [observed; sequence "+QString::number(entry.sequence)+"]\n"+entry.text;
        if(!result.endsWith('\n'))result+='\n';}
    return result;
}
