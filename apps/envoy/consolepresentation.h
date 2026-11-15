#ifndef GETDAQ_CONSOLE_PRESENTATION_H
#define GETDAQ_CONSOLE_PRESENTATION_H
#include "runconfig.h"
#include <QColor>
#include <QIcon>
#include <QStyledItemDelegate>

namespace ConsolePresentation {
enum { RowHeight=24, ParameterRowHeight=20, LampWidth=28, LampRole=Qt::UserRole+32 };
enum Health { Unknown, Healthy, Transitioning, Fault };
enum Symbol { Folder, Save, Play, Stop, Link, Unlink, Back, Forward, Retry, Document, Plus, Trash };
QColor HealthColor(Health health);
QIcon Icon(Symbol symbol,const QColor &color=QColor("#244354"));
QString ElapsedText(qint64 milliseconds);
QString StyleSheet();
QString MenuStyleSheet();
QString OfflineToolStyleSheet();
struct Overview {
    int ready=0,total=0,errors=0;
    bool errorsKnown=false,rateKnown=false,diskKnown=false;
    double rateMB=0,maxDiskPercent=0;
};
Health NodeHealth(const NodeStatus *status,bool mutant,const RunConfig &config);
Overview Summarize(const RunConfig &config,const QVector<NodeStatus> &status);
class LampDelegate : public QStyledItemDelegate {
public:
    explicit LampDelegate(QObject *parent):QStyledItemDelegate(parent) {}
    void paint(QPainter *,const QStyleOptionViewItem &,const QModelIndex &) const override;
};
}
#endif
