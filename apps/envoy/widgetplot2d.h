#ifndef WIDGETPLOT2D_H
#define WIDGETPLOT2D_H
#include "runconfig.h"
#include <QWidget>
#include <QMap>
#include <QPointF>
#include <QElapsedTimer>

// 控制台仅绘制低频速率统计；有界历史，无逐事件绘图。
class WidgetPlot2D : public QWidget
{
    Q_OBJECT
public:
    explicit WidgetPlot2D(QWidget *parent = nullptr);
    void SetNodes(const QVector<NodeConfig> &nodes);
    void AddStatus(const QVector<NodeStatus> &status);
    void Clear();
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QMap<int, QVector<QPointF> > fPoints;
    QMap<int, QString> fNames;
    QElapsedTimer fClock;
    qint64 fLast = -1000;
};
#endif
