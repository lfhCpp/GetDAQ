#include "widgetplot2d.h"
#include <QPainter>
#include <QPainterPath>
#include <algorithm>

WidgetPlot2D::WidgetPlot2D(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(132);
    fClock.start();
}
void WidgetPlot2D::SetNodes(const QVector<NodeConfig> &nodes)
{
    fNames.clear();
    for (const NodeConfig &n : nodes) if (!n.mutant) fNames[n.id] = n.name;
    Clear();
}
void WidgetPlot2D::Clear() { fPoints.clear(); fClock.restart(); fLast = -1000; update(); }
void WidgetPlot2D::AddStatus(const QVector<NodeStatus> &status)
{
    const qint64 now = fClock.elapsed();
    if (now - fLast < 500) return;
    fLast = now;
    for (const NodeStatus &s : status) if (fNames.contains(s.id)) {
        QVector<QPointF> &points = fPoints[s.id];
        points.append(QPointF(now / 1000.0, s.sentryOnline ? s.rateMB : 0));
        while (points.size() > 600) points.removeFirst();
    }
    update();
}
void WidgetPlot2D::paintEvent(QPaintEvent *)
{
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.fillRect(rect(),Qt::white);
    const QColor text("#244354"),grid("#e0e9ef");
    const QVector<QColor> colors={QColor("#0072b2"),QColor("#df8c00"),QColor("#c04ca1"),
        QColor("#00966d"),QColor("#009dc6"),QColor("#d45032"),QColor("#7953c3"),
        QColor("#337f94"),QColor("#3d64d0"),QColor("#aa7b00"),QColor("#bb4374")};
    QFont body=font();body.setPixelSize(12);p.setFont(body);
    QFont heading=body;heading.setPixelSize(15);heading.setBold(true);p.setFont(heading);p.setPen(QColor("#153249"));
    p.drawText(QRectF(0,0,270,25),Qt::AlignVCenter,tr("Data throughput · Last 300 s"));p.setFont(body);
    const int legendStart=60,entryWidth=90;
    const int legendColumns=qMax(1,(width()-legendStart)/entryWidth);
    const int legendRows=qMax(1,(fNames.size()+legendColumns-1)/legendColumns);
    int i=0;
    for(auto it=fNames.constBegin();it!=fNames.constEnd();++it,++i) {
        const int x=legendStart+(i%legendColumns)*entryWidth,y=36+(i/legendColumns)*19;
        p.setPen(QPen(colors[i%colors.size()],2));p.drawLine(x,y,x+17,y);p.setPen(text);
        const QRect label(x+22,y-9,entryWidth-23,18);
        p.drawText(label,Qt::AlignVCenter,p.fontMetrics().elidedText(it.value(),Qt::ElideRight,label.width()));
    }
    const int top=legendRows*19+34;
    const QRectF area(60,top,qMax(1,width()-74),qMax(1,height()-top-40));
    double maxY=1,maxX=10;
    for(const auto &points:fPoints)for(const auto &point:points) {maxY=qMax(maxY,point.y());maxX=qMax(maxX,point.x());}
    maxY*=1.15;const double minX=qMax(0.0,maxX-300);
    const int intervals=qBound(1,int(area.height()/28),3);
    for(int tick=0;tick<=intervals;++tick) {
        const double y=area.bottom()-area.height()*tick/intervals;
        p.setPen(grid);p.drawLine(QPointF(area.left(),y),QPointF(area.right(),y));p.setPen(text);
        p.drawText(QRectF(0,y-9,53,18),Qt::AlignRight|Qt::AlignVCenter,QString::number(maxY*tick/intervals,'f',1));
    }
    for(int tick=0;tick<=6;++tick) {
        const double x=area.left()+area.width()*tick/6;
        p.setPen(grid);p.drawLine(QPointF(x,area.top()),QPointF(x,area.bottom()));p.setPen(text);
        p.drawText(QRectF(x-24,area.bottom()+4,48,16),Qt::AlignCenter,QString::number(minX+(maxX-minX)*tick/6,'f',0));
    }
    p.setPen(QColor("#9fb5c3"));p.drawLine(area.bottomLeft(),area.bottomRight());p.drawLine(area.bottomLeft(),area.topLeft());
    p.setPen(text);p.drawText(QRectF(area.left(),height()-18,area.width(),18),Qt::AlignCenter,tr("Time (s)"));
    p.save();p.translate(10,area.center().y());p.rotate(-90);p.drawText(QRectF(-35,-9,70,18),Qt::AlignCenter,"MB/s");p.restore();
    i=0;
    for(auto it=fNames.constBegin();it!=fNames.constEnd();++it,++i) {
        QPainterPath path;bool first=true;
        for(const auto &point:fPoints.value(it.key())) {
            if(point.x()<minX)continue;
            const QPointF pixel(area.left()+(point.x()-minX)/(maxX-minX)*area.width(),area.bottom()-point.y()/maxY*area.height());
            if(first) {path.moveTo(pixel);first=false;}else path.lineTo(pixel);
        }
        p.save();p.setClipRect(area.adjusted(-1,-1,1,1));p.setPen(QPen(colors[i%colors.size()],1.7));p.drawPath(path);p.restore();
    }
}
