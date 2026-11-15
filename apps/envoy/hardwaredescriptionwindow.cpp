#include "hardwaredescriptionwindow.h"
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <limits>
#include <exception>

namespace {
QString Hex(std::uint64_t value) {return "0x"+QString::number(qulonglong(value),16);}
QString Text(const std::string &text) {return QString::fromUtf8(text.data(),int(text.size()));}
}
HardwareDescriptionWindow::HardwareDescriptionWindow(QWidget *parent) : QDialog(parent)
{
    setObjectName("HardwareDescriptionWindow");setWindowTitle(tr("Local compiled hardware description"));resize(1150,650);
    auto layout=new QVBoxLayout(this);
    auto note=new QLabel(tr("This view shows the local description and host access policy. Board layout/loading and live file health remain unverified. Register values are not read."),this);
    note->setTextFormat(Qt::PlainText);note->setWordWrap(true);layout->addWidget(note);
    fSummary=new QLabel(this);fSummary->setObjectName("HardwareDescriptionSummary");fSummary->setTextFormat(Qt::PlainText);fSummary->setWordWrap(true);
    fSummary->setTextInteractionFlags(Qt::TextSelectableByMouse);layout->addWidget(fSummary);
    fTree=new QTreeWidget(this);fTree->setObjectName("HardwareDescriptionTree");
    fTree->setHeaderLabels({tr("Device / register / field"),tr("Address / bit offset"),tr("Register width / field bits"),tr("Host policy / access"),tr("Register offset")});
    fTree->setEditTriggers(QAbstractItemView::NoEditTriggers);fTree->setColumnWidth(0,330);fTree->setColumnWidth(1,240);fTree->header()->setStretchLastSection(true);layout->addWidget(fTree,1);
    connect(fTree,&QTreeWidget::itemExpanded,this,&HardwareDescriptionWindow::Expand);
    auto close=new QPushButton(tr("Close"),this);connect(close,&QPushButton::clicked,this,&QDialog::close);layout->addWidget(close);
}
bool HardwareDescriptionWindow::Open(const QString &path,QString *error)
{
    fTree->clear();fDescription=GetDAQ::HardwareDescription();fSummary->clear();
    try {
        QFile source(path);
        if (!source.open(QIODevice::ReadOnly)) { *error=source.errorString();return false; }
        if (source.size()>8*1024*1024) { *error="Hardware description exceeds 8 MiB";return false; }
        const auto bytes=source.read(8*1024*1024+1);
        if (source.error()!=QFile::NoError || bytes.size()>8*1024*1024) { *error="Cannot read bounded hardware description";return false; }
        auto description=GetDAQ::DecodeHardwareDescription(bytes.toStdString());
        std::size_t registers=0,fields=0;
        for (const auto &device:description.devices) {
            registers+=device.registers.size();for (const auto &reg:device.registers) fields+=reg.fields.size();
        }
        fDescription=std::move(description);
        const auto digest=QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
        const auto policy=QByteArray::fromStdString(GetDAQ::HardwarePolicyJson(fDescription));
        const auto policyDigest=QString::fromLatin1(QCryptographicHash::hash(policy,QCryptographicHash::Sha256).toHex());
        fSummary->setText(tr("File: %1\nNode: %2; devices: %3; registers: %4; fields: %5\nFile SHA-256: %6\nHost policy SHA-256: %7\nExpand a device/register to inspect details. These hashes identify local bytes/policy, not actual board loading.")
            .arg(QFileInfo(path).absoluteFilePath(),Text(fDescription.nodeId)).arg(qulonglong(fDescription.devices.size())).arg(qulonglong(registers)).arg(qulonglong(fields)).arg(digest,policyDigest));
        for (std::size_t i=0;i<fDescription.devices.size();++i) {
            const auto &device=fDescription.devices[i];
            auto item=new QTreeWidgetItem(fTree,{Text(device.name),Hex(device.baseAddress),tr("%1 bytes (%2 bits)").arg(device.registerBytes).arg(device.registerBytes*8),Text(device.access)});
            item->setData(0,Qt::UserRole,int(i));item->setData(0,Qt::UserRole+1,-1);item->setData(0,Qt::UserRole+2,false);
            if (!device.registers.empty()) new QTreeWidgetItem(item,{tr("Expand to load registers")});
        }
        error->clear();return true;
    } catch (const std::exception &e) {*error=QString::fromUtf8(e.what());fSummary->setText(*error);return false;}
}
void HardwareDescriptionWindow::Expand(QTreeWidgetItem *item)
{
    if (item->data(0,Qt::UserRole+2).toBool()) return;
    const int deviceIndex=item->data(0,Qt::UserRole).toInt(),registerIndex=item->data(0,Qt::UserRole+1).toInt();
    if (deviceIndex<0 || std::size_t(deviceIndex)>=fDescription.devices.size()) return;
    const auto &device=fDescription.devices[std::size_t(deviceIndex)];
    item->setData(0,Qt::UserRole+2,true);
    while (item->childCount()) delete item->takeChild(0);
    if (registerIndex<0) {
        for (std::size_t i=0;i<device.registers.size();++i) {
            const auto &reg=device.registers[i];
            auto child=new QTreeWidgetItem(item,{Text(reg.name),Hex(device.baseAddress+reg.offset),QString::number(device.registerBytes*8),reg.readOnly ? tr("Host read-only") : tr("Host writable"),Hex(reg.offset)});
            child->setData(0,Qt::UserRole,deviceIndex);child->setData(0,Qt::UserRole+1,int(i));child->setData(0,Qt::UserRole+2,false);
            if (!reg.fields.empty()) new QTreeWidgetItem(child,{tr("Expand to load fields")});
        }
    } else if (std::size_t(registerIndex)<device.registers.size()) {
        const auto &reg=device.registers[std::size_t(registerIndex)];
        for (const auto &field:reg.fields) {
            auto child=new QTreeWidgetItem(item,{Text(field.name),QString::number(field.offset),QString::number(field.width),
                (reg.readOnly || field.readOnly) ? tr("Host read-only") : tr("Host writable")});
            child->setData(0,Qt::UserRole+2,true);
        }
    }
}
