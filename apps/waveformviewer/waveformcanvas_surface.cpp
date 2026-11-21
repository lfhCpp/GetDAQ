#include "waveformcanvas.h"
#include <QPainter>
#include <QPainterPath>
#include <QSet>
#include <QVariant>
#include <algorithm>
#include <cmath>
#include <limits>

namespace GetDAQ {
QPointF WaveformCanvas::ProjectSurfacePoint(double bucket,double channel,double value,
                                           const QRectF &plot,double *depth) const
{
    const double fraction=(channel-fDepthMinimum)/qMax(1.0,fDepthMaximum-fDepthMinimum);
    const double depthHeight=qMax(40.0,plot.height()-180.0)*(0.6+0.4*std::sin(fElevation));
    const QPointF origin(plot.left(),plot.bottom());
    if(depth)*depth=fraction;
    return origin+QPointF(bucket/511.0*plot.width()*0.72 + fraction*plot.width()*0.22*std::cos(fYaw),
                          -fraction*depthHeight-value*150.0);
}

void WaveformCanvas::DrawSurface(QPainter &painter)
{
    painter.fillRect(rect(),Qt::white);
    painter.setRenderHint(QPainter::Antialiasing);
    double minimum=std::numeric_limits<double>::infinity(),maximum=-minimum;
    QVector<WaveformPlotTrace> traces;
    for(const auto &trace:fTraces){
        if(trace.channel<0 || trace.channel>=68)continue;
        bool valid=false;
        for(const auto &p:trace.samples)if(std::isfinite(p.x()) && std::isfinite(p.y()) && p.x()>=0 && p.x()<=511){minimum=qMin(minimum,p.y());maximum=qMax(maximum,p.y());valid=true;}
        if(valid)traces<<trace;
    }
    if(traces.isEmpty() || !std::isfinite(maximum-minimum)){
        painter.setPen(QColor(40,55,70));painter.drawText(rect(),Qt::AlignCenter,
            property("emptyMessage").toString().isEmpty()?tr("No waveform samples to display"):property("emptyMessage").toString());return;
    }
    if(maximum==minimum){minimum-=1;maximum+=1;}
    const double padding=(maximum-minimum)*0.06;minimum-=padding;maximum+=padding;
    if(!std::isfinite(maximum-minimum)){painter.drawText(rect(),Qt::AlignCenter,tr("Waveform range is too large to plot"));return;}
    const QRectF plot=QRectF(rect()).adjusted(105,45,-95,-90);
    if(plot.width()<100 || plot.height()<150)return;
    auto project=[&](double bucket,double channel,double value){return ProjectSurfacePoint(bucket,channel,value,plot,nullptr);};
    const auto origin=project(0,fDepthMinimum,0),endTime=project(511,fDepthMinimum,0),
        endDepth=project(511,fDepthMaximum,0),endAmplitude=project(0,fDepthMinimum,1);
    QPolygonF floor;floor<<origin<<endTime<<endDepth<<project(0,fDepthMaximum,0);
    painter.setBrush(QColor(246,249,252));painter.setPen(Qt::NoPen);painter.drawPolygon(floor);
    painter.setPen(QPen(QColor(218,225,233),1,Qt::DashLine));
    for(int bucket:{0,128,256,384,511})painter.drawLine(project(bucket,fDepthMinimum,0),project(bucket,fDepthMaximum,0));
    std::sort(traces.begin(),traces.end(),[](const WaveformPlotTrace &a,const WaveformPlotTrace &b){return a.channel>b.channel;});
    for(const auto &trace:traces){
        painter.setPen(QPen(QColor(226,232,239),1));painter.drawLine(project(0,trace.channel,0),project(511,trace.channel,0));
        QPainterPath path;bool connected=false;double previous=0;
        for(const auto &sample:trace.samples){
            if(!std::isfinite(sample.x()) || !std::isfinite(sample.y()) || sample.x()<0 || sample.x()>511){connected=false;continue;}
            const auto point=project(sample.x(),trace.channel,(sample.y()-minimum)/(maximum-minimum));
            if(!connected || sample.x()-previous>1.5)path.moveTo(point);else path.lineTo(point);
            previous=sample.x();connected=true;
        }
        // A thin white halo distinguishes crossings without inventing a surface
        // or zero samples for channels that were not selected/read.
        painter.setPen(QPen(Qt::white,4.2));painter.drawPath(path);
        painter.setPen(QPen(ChannelColor(trace.channel),1.7));painter.drawPath(path);
    }
    const QColor ink(37,54,72);painter.setPen(QPen(ink,1.4));
    painter.drawLine(origin,endTime);painter.drawLine(endTime,endDepth);painter.drawLine(origin,endAmplitude);
    for(int bucket:{0,128,256,384,511}){
        const auto p=project(bucket,fDepthMinimum,0);painter.drawLine(p,p+QPointF(0,5));painter.drawText(QRectF(p.x()-23,p.y()+7,46,20),Qt::AlignCenter,QString::number(bucket));
    }
    for(int tick=0;tick<=4;++tick){
        const double fraction=tick/4.0;const auto p=project(0,fDepthMinimum,fraction);painter.drawLine(p-QPointF(5,0),p);
        painter.drawText(QRectF(4,p.y()-10,plot.left()-12,20),Qt::AlignRight|Qt::AlignVCenter,QString::number(minimum+fraction*(maximum-minimum),'g',6));
    }
    QSet<int> labeled;
    for(const auto &trace:traces)if(!labeled.contains(trace.channel)){
        labeled.insert(trace.channel);const auto p=project(511,trace.channel,0);painter.setPen(QPen(ChannelColor(trace.channel),1.4));painter.drawLine(p,p+QPointF(5,0));
        painter.setPen(ink);painter.drawText(QRectF(p.x()+8,p.y()-9,65,18),Qt::AlignLeft|Qt::AlignVCenter,QString::number(trace.channel));
    }
    painter.drawText(QRectF(0,5,width(),25),Qt::AlignCenter,tr("AGET %1 — 3D channel waveforms").arg(fAget));
    painter.drawText(QRectF(origin.x(),endTime.y()+31,endTime.x()-origin.x(),22),Qt::AlignCenter,tr("Time (sample bucket)"));
    painter.drawText(QRectF(endDepth.x()-90,endDepth.y()-32,180,22),Qt::AlignCenter,tr("Channel number"));
    painter.save();painter.translate(20,endAmplitude.y()+75);painter.rotate(-90);painter.drawText(QRectF(-80,-10,160,22),Qt::AlignCenter,tr("Amplitude (ADC)"));painter.restore();
    painter.drawText(QRectF(0,height()-29,width(),24),Qt::AlignCenter,tr("Drag: depth angle   |   Ctrl+wheel: channel spacing (%1 px)   |   Double-click: reset").arg(fChannelGap));
    setAccessibleDescription(tr("3D waveform. Time: sample bucket 0 to 511. Amplitude: ADC %1 to %2. Depth: channel number %3 to %4. %5 selected traces; no interpolated channels. Channel spacing %6 pixels.").arg(minimum).arg(maximum).arg(fDepthMinimum).arg(fDepthMaximum).arg(traces.size()).arg(fChannelGap));
}
}
