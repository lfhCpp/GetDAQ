#ifndef GETDAQ_HARDWAREDESCRIPTIONWINDOW_H
#define GETDAQ_HARDWAREDESCRIPTIONWINDOW_H
#include "hardwaredescription.h"
#include <QDialog>
class QLabel;
class QTreeWidget;
class QTreeWidgetItem;

// 离线描述查看；不创建Ice对象，不读取寄存器值，不改变配置。
class HardwareDescriptionWindow : public QDialog {
    Q_OBJECT
public:
    explicit HardwareDescriptionWindow(QWidget *parent=nullptr);
    bool Open(const QString &path,QString *error);
private:
    GetDAQ::HardwareDescription fDescription;
    QLabel *fSummary;
    QTreeWidget *fTree;
    void Expand(QTreeWidgetItem *item);
};
#endif
