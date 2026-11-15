#include "configwindow.h"

#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTreeWidget>

void ConfigWindow::Rebuild()
{
    UpdateHistoryButtons();
    ++fRevision;
    ClearOverride();
    fSnapshotReview->clear();
    fSnapshotReview->hide();
    fTree->clear();
    for (auto it = fDocument.Sources().constBegin(); it != fDocument.Sources().constEnd(); ++it) {
        auto file = new QTreeWidgetItem(fTree, {it.key()});
        file->setData(0, Qt::UserRole, -1);
        AddNode(file, it.value().root);
        file->setExpanded(true);
    }
    SelectionChanged();
}

void ConfigWindow::AddNode(QTreeWidgetItem *parent, int index)
{
    const auto &node = fDocument.Nodes()[index];
    const QString label = node.name + (node.attributes.contains("id")
        ? "[" + node.attributes.value("id") + "]" : "");
    auto item = new QTreeWidgetItem(parent, {label, node.value, node.attributes.value("unit"),
        node.attributes.value("range"), QString::number(node.line) +
        (node.includedRoot >= 0 ? " -> " + node.attributes.value("include") : "")});
    item->setData(0, Qt::UserRole, index);
    if(node.scalar && node.valueEditable && !fReadOnly)item->setFlags(item->flags() | Qt::ItemIsEditable);
    item->setToolTip(0, node.file + ":" + QString::number(node.line) + "\n" + node.path);
    for (int child : node.children) AddNode(item, child);
}

void ConfigWindow::SelectionChanged()
{
    const int id = fTree->currentItem()
        ? fTree->currentItem()->data(0, Qt::UserRole).toInt() : -1;
    const bool valid = id >= 0 && id < fDocument.Nodes().size();
    fSelectedValue=valid ? fDocument.Nodes()[id].value : QString();
    const bool multilineValue=valid && (fSelectedValue.contains('\n') || fSelectedValue.contains('\r'));
    fValue->setText(multilineValue ? tr("[multi-line value — use Edit value...]") : fSelectedValue);
    const bool legacySplit = valid && fDocument.Nodes()[id].scalar && !fDocument.Nodes()[id].valueEditable;
    const bool editable = valid && fDocument.Nodes()[id].scalar && !legacySplit && !fReadOnly;
    fValue->setReadOnly(multilineValue || legacySplit);
    fValue->setEnabled(editable && !multilineValue);
    fApply->setEnabled(editable && !multilineValue);
    fEditValue->setEnabled(editable);
    fStructureButton->setEnabled(valid && !fReadOnly);
    const QString readOnlyReason = legacySplit
        ? tr("This legacy typed value is split across XML markup. Its original value is preserved; normalize the XML text explicitly before editing.")
        : QString();
    fValue->setToolTip(readOnlyReason);
    fApply->setToolTip(readOnlyReason);
    if (valid && fDocument.Nodes()[id].scalar) {
        const auto &node=fDocument.Nodes()[id];
        const QString inheritance=node.path.contains("[*]")
            ? tr(" This is a `[*]` default; concrete instances without their own value inherit it.") : QString();
        fSelectionInfo->setText(tr("Scalar parameter: %1\nSource: %2:%3. Double-click its value cell, or focus that cell and press F2, to edit in place. Enumerations and booleans use a dropdown; bounded integers use a spin box. The field below remains available for longer values.%4")
            .arg(node.path,node.file).arg(node.line).arg(inheritance));
    } else if(valid) {
        const auto &node=fDocument.Nodes()[id];
        fSelectionInfo->setText(tr("Selected object: %1\nObjects have no scalar value. Select a parameter beneath this object to edit it, or double-click the object / use Edit selected declaration structure to add or remove parameters.").arg(node.path));
    } else {
        fSelectionInfo->setText(tr("Select a scalar parameter; double-click its value cell, or focus that cell and press F2, to edit directly in the table."));
    }
    if (legacySplit) fStatus->setText(readOnlyReason);
    else if(valid && !fDocument.Nodes()[id].scalar)fStatus->setText(tr("Selected item is an object; select one of its scalar parameters to edit a value."));
    else if(multilineValue)fStatus->setText(tr("This scalar contains line breaks. Edit value opens the complete value in a larger text editor."));
}
