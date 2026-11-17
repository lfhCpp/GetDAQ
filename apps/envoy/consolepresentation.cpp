#include "consolepresentation.h"
#include <cmath>
#include <QSet>

namespace ConsolePresentation {
QString ElapsedText(qint64 milliseconds) {
    const qint64 seconds=qMax(qint64(0),milliseconds/1000);
    return QString("Elapsed: %1 h %2 min %3 s").arg(seconds/3600).arg((seconds/60)%60,2,10,QChar('0')).arg(seconds%60,2,10,QChar('0'));
}

QColor HealthColor(Health health) {
    switch(health) {
    case Healthy:return QColor("#087f5b");
    case Transitioning:return QColor("#b57b16");
    case Fault:return QColor("#c74349");
    default:return QColor("#96a5af");
    }
}
namespace {
bool HasError(const NodeStatus &s){return !s.error.isEmpty() || !s.sentryError.isEmpty() || (s.eccOnline && (s.state<0 || s.state>6 || s.transition<0 || s.transition>9));}
bool Observed(const NodeStatus &s,bool mutant,const RunConfig &){return s.eccOnline && (mutant || s.sentryOnline);}

}
Health NodeHealth(const NodeStatus *s,bool mutant,const RunConfig &config) {
    if(!s)return Unknown;
    if(HasError(*s))return Fault;
    if(!s->eccOnline)return Unknown;
    if(s->transition!=0)return Transitioning;
    if(!Observed(*s,mutant,config))return Unknown;
    return Healthy;
}
Overview Summarize(const RunConfig &config,const QVector<NodeStatus> &status) {
    Overview out;QMap<int,NodeStatus> byId;QSet<QString> monitors;
    for(const auto &s:status)byId.insert(s.id,s);
    out.errorsKnown=!config.nodes.isEmpty();out.rateKnown=out.diskKnown=true;
    for(const auto &n:config.nodes) {
        const auto found=byId.constFind(n.id);const bool present=found!=byId.constEnd();
        if(present && HasError(found.value()))++out.errors;
        if(!present || !Observed(found.value(),n.mutant,config))out.errorsKnown=false;
        if(n.mutant)continue;
        ++out.total;
        if(!present) {out.rateKnown=out.diskKnown=false;continue;}
        const auto &s=found.value();
        if(s.eccOnline && s.state==4 && s.transition==0)++out.ready;
        if(!s.sentryOnline || !std::isfinite(s.rateMB) || s.rateMB<0)out.rateKnown=false;
        else {const auto key=(n.sentryHost.isEmpty()?n.host:n.sentryHost).toLower()+':'+QString::number(n.sentryPort);if(s.projectRateKnown){if(!monitors.contains(key))out.rateMB+=s.projectRateMB;}else if(s.sentrySourceSpecific || !monitors.contains(key))out.rateMB+=s.rateMB;monitors.insert(key);}
        if(!s.sentryOnline || !std::isfinite(s.totalGB) || !std::isfinite(s.availableGB) ||
           s.totalGB<=0 || s.availableGB<0 || s.availableGB>s.totalGB)out.diskKnown=false;
        else out.maxDiskPercent=qMax(out.maxDiskPercent,100*(1-s.availableGB/s.totalGB));
    }
    if(!out.total)out.rateKnown=out.diskKnown=false;
    return out;
}

}
