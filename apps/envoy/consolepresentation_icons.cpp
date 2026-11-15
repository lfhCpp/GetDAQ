#include "consolepresentation.h"
#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>

namespace ConsolePresentation {
void LampDelegate::paint(QPainter *p,const QStyleOptionViewItem &option,const QModelIndex &index) const {
    QStyleOptionViewItem cell(option);initStyleOption(&cell,index);cell.text.clear();
    const auto *style=option.widget ? option.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem,&cell,p,option.widget);
    p->save();p->setRenderHint(QPainter::Antialiasing);
    p->setPen(Qt::NoPen);p->setBrush(HealthColor(static_cast<Health>(index.data(LampRole).toInt())));
    p->drawEllipse(QPointF(option.rect.center()),5.0,5.0);p->restore();
}
QIcon Icon(Symbol symbol,const QColor &color) {
    QIcon icon;
    for(auto mode:{QIcon::Normal,QIcon::Disabled}) {
        QPixmap pixmap(40,40);pixmap.setDevicePixelRatio(2);pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);p.setRenderHint(QPainter::Antialiasing);
        const QColor ink=mode==QIcon::Disabled ? QColor("#8a9aa7") : color;
        p.setPen(QPen(ink,1.6,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p.setBrush(Qt::NoBrush);
        switch(symbol) {
        case Folder: {QPainterPath path;path.moveTo(2,6);path.lineTo(2,16);path.lineTo(17,16);path.lineTo(18,7);path.lineTo(9,7);path.lineTo(7,4);path.lineTo(2,4);path.closeSubpath();p.drawPath(path);break;}
        case Save:p.drawRoundedRect(QRectF(3,2,14,16),1,1);p.drawRect(QRectF(6,2,8,5));p.drawRect(QRectF(6,11,8,7));break;
        case Play: {QPainterPath path;path.moveTo(5,3);path.lineTo(17,10);path.lineTo(5,17);path.closeSubpath();p.fillPath(path,ink);break;}
        case Stop:p.fillRect(QRectF(4,4,12,12),ink);break;
        case Back:case Forward: {QPainterPath path;const bool back=symbol==Back;
            for(int i=0;i<2;++i) {const qreal x=back ? 10+i*7 : 3+i*7;path.moveTo(x,4);path.lineTo(x+(back ? -7 : 7),10);path.lineTo(x,16);path.closeSubpath();}
            p.fillPath(path,ink);break;}
        case Link:case Unlink:
            p.save();p.translate(10,10);p.rotate(-45);p.drawRoundedRect(QRectF(-3,-9,6,10),3,3);p.drawRoundedRect(QRectF(-3,-1,6,10),3,3);p.drawLine(QPointF(0,-3),QPointF(0,3));p.restore();
            if(symbol==Unlink)p.drawLine(QPointF(2,2),QPointF(18,18));
            break;
        case Retry:p.drawArc(QRectF(3,3,14,14),35*16,295*16);p.drawLine(QPointF(16,3),QPointF(16,8));p.drawLine(QPointF(16,8),QPointF(11,7));break;
        case Document:p.drawRoundedRect(QRectF(4,2,12,16),1,1);p.drawLine(7,7,13,7);p.drawLine(7,11,13,11);p.drawLine(7,14,11,14);break;
        case Plus:p.drawLine(3,10,17,10);p.drawLine(10,3,10,17);break;
        case Trash:p.drawLine(4,5,16,5);p.drawRect(QRectF(6,5,8,12));p.drawLine(8,2,12,2);p.drawLine(9,8,9,14);p.drawLine(12,8,12,14);break;
        }
        p.end();icon.addPixmap(pixmap,mode);
    }
    return icon;
}
}
