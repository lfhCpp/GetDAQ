#include "waveformcanvas.h"

#include "waveformexport.h"

#include <QColor>
#include <QFontMetricsF>
#include <cmath>
#include <limits>
#include <algorithm>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QWheelEvent>


namespace GetDAQ {

WaveformCanvas::WaveformCanvas() : fAget(0), fHasData(false), fSurface(false), fYaw(0.55), fElevation(0.72), fDragging(false)
{
        setMinimumSize(600, 360);
        setAutoFillBackground(true);
    }

void WaveformCanvas::SetTraces(const QVector<GetDAQ::WaveformPlotTrace> &traces, int aget)
{
        fTraces = traces;
        for(auto &trace:fTraces)std::stable_sort(trace.samples.begin(),trace.samples.end(),[](const QPointF &a,const QPointF &b){return a.x()<b.x();});
        fAget = aget;
        fHasData = !fTraces.isEmpty();
        UpdateSurfaceSize();
        update();
    }

void WaveformCanvas::Clear()
{ fTraces.clear(); fHasData = false; UpdateSurfaceSize();update(); }

void WaveformCanvas::SetSurface(bool enabled)
{ fSurface = enabled; UpdateSurfaceSize();update(); }

void WaveformCanvas::SetBaselineAlignment(bool enabled)
{ fAlignBaselines = enabled; update(); }

const QVector<GetDAQ::WaveformPlotTrace> &WaveformCanvas::Traces() const
{ return fTraces; }

bool WaveformCanvas::SavePng(const QString &path, QString *error)
{
        if (!fHasData || path.isEmpty()) {
            if (error) *error = tr("No waveform is available to export");
            return false;
        }
        return GetDAQ::WriteWidgetPng(path, this, error);
    }

void WaveformCanvas::paintEvent(QPaintEvent *)
{
        QPainter painter(this);
        painter.fillRect(rect(), QColor(250, 250, 250));
        if (fSurface) {
            DrawSurface(painter);
            return;
        }
        const int legendColumns = qMax(1, qMin(18, int((width() - 80) / 38)));
        const int legendRows = (fTraces.size() + legendColumns - 1) / legendColumns;
        const qreal legendHeight = legendRows * 15;
        if (!fHasData) {
            painter.drawText(rect(), Qt::AlignCenter, (property("emptyMessage").toString().isEmpty()?tr("Open a .graw or .gmat waveform file"):property("emptyMessage").toString()));return;
        }
        double minimum=std::numeric_limits<double>::infinity(),maximum=-minimum;
        for(const auto &trace:fTraces)for(const auto &point:trace.samples)if(std::isfinite(point.y())){minimum=qMin(minimum,point.y());maximum=qMax(maximum,point.y());}
        if(!std::isfinite(minimum) || !std::isfinite(maximum) || !std::isfinite(maximum-minimum)) {
            painter.drawText(rect(),Qt::AlignCenter,tr("No representable waveform range"));return;
        }
        if(maximum<=minimum){minimum-=1.0;maximum+=1.0;}
        const double margin=(maximum-minimum)*0.08;minimum-=margin;maximum+=margin;
        if(!std::isfinite(minimum) || !std::isfinite(maximum) || maximum<=minimum){painter.drawText(rect(),Qt::AlignCenter,tr("Waveform range is too large to plot"));return;}
        QStringList yLabels;const QFontMetricsF font(painter.font());qreal labelWidth=0;
        for(int i=0;i<=5;++i){const auto label=QString::number(maximum-(maximum-minimum)*i/5.0,'g',6);yLabels.append(label);labelWidth=qMax(labelWidth,font.horizontalAdvance(label));}
        const QRectF plot=QRectF(rect()).adjusted(36+labelWidth+10,22,-28,-(68+legendHeight));
        if(plot.width()<20 || plot.height()<20)return;
        setAccessibleDescription(tr("X: sample bucket 0 to 511. Y: ADC value %1 to %2.").arg(QString::number(minimum,'g',8),QString::number(maximum,'g',8)));
        painter.setPen(QPen(QColor(220,225,230),1));
        for(int i=0;i<=5;++i){const qreal y=plot.top()+plot.height()*i/5.0;painter.drawLine(QPointF(plot.left(),y),QPointF(plot.right(),y));}
        const QVector<int> buckets={0,64,128,192,256,320,384,448,511};
        for(int bucket:buckets){const qreal x=plot.left()+plot.width()*bucket/511.0;painter.drawLine(QPointF(x,plot.top()),QPointF(x,plot.bottom()));}
        painter.save();painter.setClipRect(plot.adjusted(-1,-1,1,1));
        for (const GetDAQ::WaveformPlotTrace &trace : fTraces) {
            if (trace.samples.isEmpty()) continue;
            painter.setPen(QPen(ChannelColor(trace.channel), 1.2));
            QPainterPath path;
            for (int i = 0; i < trace.samples.size(); ++i) {
                const QPointF &point = trace.samples.at(i);
                const qreal x = plot.left() + plot.width() * point.x() / 511.0;
                const qreal y = plot.bottom() - plot.height() * (point.y() - minimum) / (maximum - minimum);
                if (!i) path.moveTo(x, y); else path.lineTo(x, y);
            }
            painter.drawPath(path);
        }
        painter.restore();
        painter.setPen(Qt::black);
        painter.drawRect(plot);
        for(int i=0;i<=5;++i){const qreal y=plot.top()+plot.height()*i/5.0;painter.drawLine(QPointF(plot.left()-4,y),QPointF(plot.left(),y));painter.drawText(QRectF(30,y-font.height()/2,labelWidth+10,font.height()),Qt::AlignRight|Qt::AlignVCenter,yLabels[i]);}
        for(int bucket:buckets){const qreal x=plot.left()+plot.width()*bucket/511.0;painter.drawLine(QPointF(x,plot.bottom()),QPointF(x,plot.bottom()+4));painter.drawText(QRectF(x-25,plot.bottom()+5,50,font.height()),Qt::AlignHCenter|Qt::AlignTop,QString::number(bucket));}
        painter.drawText(QRectF(plot.left(), 0, plot.width(), 18), Qt::AlignCenter,
                         tr("AGET %1; %2 channel traces").arg(fAget).arg(fTraces.size()));
        const qreal legendY = plot.bottom() + 27;
        const qreal cellWidth = plot.width() / legendColumns;
        for (int i = 0; i < fTraces.size(); ++i) {
            const qreal x = plot.left() + (i % legendColumns) * cellWidth;
            const qreal y = legendY + (i / legendColumns) * 15;
            painter.setPen(QPen(ChannelColor(fTraces.at(i).channel), 2));
            painter.drawLine(QPointF(x, y + 6), QPointF(x + 10, y + 6));
            painter.setPen(Qt::black);
            painter.drawText(QRectF(x + 13, y, cellWidth - 13, 13), Qt::AlignLeft | Qt::AlignVCenter,
                             QString::number(fTraces.at(i).channel));
        }
        painter.drawText(QRectF(plot.left(), plot.bottom() + legendHeight + 44, plot.width(), 24), Qt::AlignCenter,
                         tr("Bucket (sample index)"));
        painter.save();
        painter.translate(16, plot.center().y());
        painter.rotate(-90);
        painter.drawText(QRectF(-plot.height() / 2, -8, plot.height(), 18), Qt::AlignCenter,
                         tr("ADC value"));
        painter.restore();

    }

void WaveformCanvas::mousePressEvent(QMouseEvent *event)
{
        if (fSurface && event->button() == Qt::LeftButton) {
            fDragging = true;
            fLastMousePosition = event->pos();
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

void WaveformCanvas::mouseMoveEvent(QMouseEvent *event)
{
        if (fSurface && fDragging) {
            const QPoint delta = event->pos() - fLastMousePosition;
            fLastMousePosition = event->pos();
            fYaw = qBound(-1.2, fYaw + delta.x() * 0.008, 1.2);
            fElevation = qBound(0.25, fElevation + delta.y() * 0.005, 1.2);
            update();
            event->accept();
            return;
        }
        QWidget::mouseMoveEvent(event);
    }

void WaveformCanvas::mouseReleaseEvent(QMouseEvent *event)
{
        if (fSurface && event->button() == Qt::LeftButton && fDragging) {
            fDragging = false;
            event->accept();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }

void WaveformCanvas::UpdateSurfaceSize()
{
    int first=67,last=0;
    for(const auto &trace:fTraces)if(trace.channel>=0 && trace.channel<68 && !trace.samples.isEmpty()){first=qMin(first,trace.channel);last=qMax(last,trace.channel);}
    fDepthMinimum=first<=last?first:0;fDepthMaximum=first<=last?last:1;
    setMinimumSize(600,fSurface?qMax(480,280+qMax(1,last-first)*fChannelGap):360);
}
void WaveformCanvas::wheelEvent(QWheelEvent *event)
{
    if(!fSurface){QWidget::wheelEvent(event);return;}
    // Ctrl+wheel changes physical channel separation; normal wheel scrolls the plot.
    if(!(event->modifiers()&Qt::ControlModifier)){event->ignore();return;}
    fChannelGap=qBound(8,fChannelGap+(event->angleDelta().y()>0?4:-4),48);
    UpdateSurfaceSize();update();event->accept();
}
void WaveformCanvas::mouseDoubleClickEvent(QMouseEvent *event)
{
    if(fSurface && event->button()==Qt::LeftButton){fYaw=0.55;fElevation=0.72;fChannelGap=16;UpdateSurfaceSize();update();event->accept();return;}
    QWidget::mouseDoubleClickEvent(event);
}

QColor WaveformCanvas::ChannelColor(int channel) const
{
    return QColor::fromHsv((channel * 137) % 360, 210, 185);
}
} // namespace GetDAQ
