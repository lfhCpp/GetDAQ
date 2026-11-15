#include "configwindow.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFutureWatcher>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressDialog>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

namespace {
struct ValueEditResult {
    XcfgDocument document;
    XcfgEditHistory history;
    bool saved = false;
    QString error;
};
}

bool ConfigWindow::SaveValueAsync(int id, const QString &value)
{
    if (fReadOnly || fValueEditPending || id < 0 || id >= fDocument.Nodes().size()) return false;
    const auto node = fDocument.Nodes()[id];
    if (!node.scalar || !node.valueEditable) return false;
    if (value == node.value) return true;
    if (node.path.contains("[*]") && QMessageBox::question(this, tr("Edit default declaration?"),
        tr("This is a `[*]` template declaration. Changing it affects every concrete instance that inherits this value; instances with their own value declaration keep that value. The save also validates the effective configuration before and after the edit. Continue?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return false;

    fValueEditPending = true;
    fStatus->setText(tr("Validating effective values and saving the local declaration…"));
    auto progress = new QProgressDialog(tr("Validating the effective configuration and saving…"), QString(), 0, 0, this);
    progress->setWindowTitle(tr("Saving configuration"));
    progress->setWindowModality(Qt::WindowModal);
    progress->setCancelButton(nullptr);
    progress->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
    progress->setMinimumDuration(0);
    progress->setAutoClose(false);
    progress->setAutoReset(false);
    progress->show();

    const bool wildcardDeclaration = node.path.contains("[*]");
    auto watcher = new QFutureWatcher<ValueEditResult>(this);
    connect(watcher, &QFutureWatcher<ValueEditResult>::finished, this,
            [this, watcher, progress, wildcardDeclaration] {
        const auto result = watcher->result();
        watcher->deleteLater();
        progress->close();
        progress->deleteLater();
        fValueEditPending = false;
        fDocument = result.document;
        fHistory = result.history;
        UpdateHistoryButtons();
        if (result.saved) {
            Rebuild();
            fStatus->setText(wildcardDeclaration
                ? tr("Saved the default declaration. Instances inheriting this value may also change; explicit values remain unchanged. No hardware command was sent.")
                : tr("Saved the declaration. No hardware command was sent."));
        } else {
            if (fDocument.Nodes().isEmpty()) Rebuild();
            fStatus->setText(result.error);
        }
    });
    const XcfgDocument document = fDocument;
    const XcfgEditHistory history = fHistory;
    watcher->setFuture(QtConcurrent::run([document, history, id, value]() mutable {
        ValueEditResult result;
        result.document = document;
        result.history = history;
        result.saved = result.history.SetValue(&result.document, id, value, &result.error);
        return result;
    }));
    return true;
}

void ConfigWindow::ClearOverride()
{
    fHaveOverride = false;
    fOverride = XcfgDocument::OverridePreview();
    fOverrideApply->setEnabled(false);
    fOverrideText->clear();
    fOverrideText->hide();
}

void ConfigWindow::OpenValueEditor()
{
    if (fReadOnly) return;
    const int id = fTree->currentItem() ? fTree->currentItem()->data(0, Qt::UserRole).toInt() : -1;
    if (id < 0 || id >= fDocument.Nodes().size()) return;
    const auto node = fDocument.Nodes()[id];
    if (!node.scalar || !node.valueEditable) return;
    const int revision = fRevision;
    const bool multiline = fSelectedValue.contains('\n') || fSelectedValue.contains('\r');
    const bool pendingInline = !multiline && fValue->text() != fSelectedValue;
    const QString initial = multiline ? fSelectedValue : fValue->text();

    QDialog dialog(this);
    dialog.setObjectName("ConfigValueEditorDialog");
    dialog.setWindowTitle(tr("Edit configuration value"));
    dialog.resize(780, 480);
    auto layout = new QVBoxLayout(&dialog);
    auto source = new QLabel(tr("%1\n%2:%3\nEdit the complete scalar value. Line endings entered here are saved as LF.")
        .arg(node.path, node.file).arg(node.line), &dialog);
    source->setWordWrap(true);
    layout->addWidget(source);
    auto editor = new QPlainTextEdit(&dialog);
    editor->setObjectName("ConfigMultilineValueEditor");
    editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    QString normalized = fSelectedValue;
    normalized.replace("\r\n", "\n");
    normalized.replace('\r', '\n');
    editor->setPlainText(multiline ? normalized : initial);
    layout->addWidget(editor, 1);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) return;
    const QString value = editor->toPlainText();
    if (revision != fRevision || id >= fDocument.Nodes().size()) {
        fStatus->setText(tr("The configuration changed while the value editor was open. Reopen the editor before saving."));
        return;
    }
    if (!pendingInline && value == normalized) return;
    if (fSelectedValue.contains('\r') && value != fSelectedValue &&
        QMessageBox::warning(this, tr("Normalize line endings?"),
            tr("This value contains carriage-return line endings. The text editor saves line endings as LF. Continue?"),
            QMessageBox::Save | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Save) return;
    SaveValueAsync(id, value);
}
