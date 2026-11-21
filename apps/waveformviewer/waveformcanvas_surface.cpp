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
        QVector<double> values;
        for(const auto &p:trace.samples)if(std::isfinite(p.x()) && std::isfinite(p.y()) && p.x()>=0 && p.x()<=511)values<<p.y();
        if(values.isEmpty())continue;
        double baseline=0;
        if(fAlignBaselines){
            const int middle=values.size()/2;std::nth_element(values.begin(),values.begin()+middle,values.end());baseline=values[middle];
            if(values.size()%2==0)baseline=baseline/2.0+*std::max_element(values.begin(),values.begin()+middle)/2.0;
        }
        // Change a display copy only. Raw traces, 2D view and CSV retain ADC values.
        auto displayed=trace;
        for(auto &p:displayed.samples)if(std::isfinite(p.x()) && std::isfinite(p.y()) && p.x()>=0 && p.x()<=511){p.setY(p.y()-baseline);minimum=qMin(minimum,p.y());maximum=qMax(maximum,p.y());}
        traces<<displayed;
    }
    if(traces.isEmpty() || !std::isfinite(maximum-minimum)){
        painter.setPen(QColor(40,55,70));painter.drawText(rect(),Qt::AlignCenter,
            property("emptyMessage").toString().isEmpty()?tr("No waveform samples to display"):property("emptyMessage").toString());return;
    }
    if(maximum==minimum){minimum-=1;maximum+=1;}
    const double padding=(maximum-minimum)*0.06;minimum-=padding;maximum+=padding;
    if(!std::isfinite(maximum-minimum)){painter.drawText(rect(),Qt::AlignCenter,tr("Waveform range is too large to plot"));return;}
    const double span=maximum-minimum;
    const double belowBaseline=fAlignBaselines?qMax(0.0,-minimum/span*150.0):0.0;
    const QRectF plot=QRectF(rect()).adjusted(105,45,-95,-90-belowBaseline);
    if(plot.width()<100 || plot.height()<150)return;
    auto project=[&](double bucket,double channel,double value){return ProjectSurfacePoint(bucket,channel,value,plot,nullptr);};
    auto amplitude=[&](double value){return (fAlignBaselines?value:value-minimum)/span;};
    const auto origin=project(0,fDepthMinimum,0),endTime=project(511,fDepthMinimum,0),
        endDepth=project(511,fDepthMaximum,0),endAmplitude=project(0,fDepthMinimum,amplitude(maximum)),
        startAmplitude=project(0,fDepthMinimum,amplitude(minimum));
    QPolygonF floor;floor<<origin<<endTime<<endDepth<<project(0,fDepthMaximum,0);
    painter.setBrush(QColor(246,249,252));painter.setPen(Qt::NoPen);painter.drawPolygon(floor);
    // Only the shared reference floor is filled. Open waveform paths must not
    // inherit its brush, which implicitly closes and fills every channel.
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(218,225,233),1,Qt::DashLine));
    for(int bucket:{0,128,256,384,511})painter.drawLine(project(bucket,fDepthMinimum,0),project(bucket,fDepthMaximum,0));
    std::sort(traces.begin(),traces.end(),[](const WaveformPlotTrace &a,const WaveformPlotTrace &b){return a.channel>b.channel;});
    for(const auto &trace:traces){
        painter.setPen(QPen(QColor(226,232,239),1));painter.drawLine(project(0,trace.channel,0),project(511,trace.channel,0));
        QPainterPath path;bool connected=false;double previous=0;
        for(const auto &sample:trace.samples){
            if(!std::isfinite(sample.x()) || !std::isfinite(sample.y()) || sample.x()<0 || sample.x()>511){connected=false;continue;}
            const auto point=project(sample.x(),trace.channel,amplitude(sample.y()));
            if(!connected || sample.x()-previous>1.5)path.moveTo(point);else path.lineTo(point);
            previous=sample.x();connected=true;
        }
        painter.setPen(QPen(ChannelColor(trace.channel),1.7));painter.drawPath(path);
    }
    const QColor ink(37,54,72);painter.setPen(QPen(ink,1.4));
    painter.drawLine(origin,endTime);painter.drawLine(endTime,endDepth);painter.drawLine(startAmplitude,endAmplitude);
    for(int bucket:{0,128,256,384,511}){
        const auto p=project(bucket,fDepthMinimum,0);painter.drawLine(p,p+QPointF(0,5));painter.drawText(QRectF(p.x()-23,p.y()+7,46,20),Qt::AlignCenter,QString::number(bucket));
    }
    QVector<double> ticks;for(int tick=0;tick<=4;++tick)ticks<<minimum+tick/4.0*span;
    if(fAlignBaselines){int nearest=0;for(int i=1;i<ticks.size();++i)if(std::abs(ticks[i])<std::abs(ticks[nearest]))nearest=i;ticks[nearest]=0;}
    for(const auto value:ticks){
        const auto p=project(0,fDepthMinimum,amplitude(value));painter.drawLine(p-QPointF(5,0),p);
        painter.drawText(QRectF(4,p.y()-10,plot.left()-12,20),Qt::AlignRight|Qt::AlignVCenter,QString::number(value,'g',6));
    }
    QSet<int> labeled;
    for(const auto &trace:traces)if(!labeled.contains(trace.channel)){
        labeled.insert(trace.channel);const auto p=project(511,trace.channel,0);painter.setPen(QPen(ChannelColor(trace.channel),1.4));painter.drawLine(p,p+QPointF(5,0));
        painter.setPen(ink);painter.drawText(QRectF(p.x()+8,p.y()-9,65,18),Qt::AlignLeft|Qt::AlignVCenter,QString::number(trace.channel));
    }
    painter.drawText(QRectF(0,5,width(),25),Qt::AlignCenter,tr("AGET %1 — 3D channel waveforms (%2)").arg(fAget).arg(fAlignBaselines?tr("baseline aligned"):tr("absolute ADC")));
    painter.drawText(QRectF(origin.x(),endTime.y()+31,endTime.x()-origin.x(),22),Qt::AlignCenter,tr("Time (sample bucket)"));
    painter.drawText(QRectF(endDepth.x()-90,endDepth.y()-58,180,22),Qt::AlignCenter,tr("Channel number"));
    painter.save();painter.translate(20,endAmplitude.y()+75);painter.rotate(-90);painter.drawText(QRectF(-80,-10,160,22),Qt::AlignCenter,fAlignBaselines?tr("Amplitude (ΔADC)"):tr("Amplitude (ADC)"));painter.restore();
    painter.drawText(QRectF(0,height()-29,width(),24),Qt::AlignCenter,tr("Drag: depth angle   |   Ctrl+wheel: channel spacing (%1 px)   |   Double-click: reset").arg(fChannelGap));
    setAccessibleDescription(tr("3D waveform. Time: sample bucket 0 to 511. Amplitude: %1 %2 to %3. Depth: channel number %4 to %5. %6 selected traces; no interpolated channels. Channel spacing %7 pixels. %8").arg(fAlignBaselines?QString("delta ADC"):QString("ADC")).arg(minimum).arg(maximum).arg(fDepthMinimum).arg(fDepthMaximum).arg(traces.size()).arg(fChannelGap).arg(fAlignBaselines?tr("Per-channel median baseline aligned to zero; raw ADC unchanged."):tr("Absolute ADC; channel pedestal differences retained.")));
}
}
