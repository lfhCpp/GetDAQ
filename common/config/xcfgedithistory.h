#ifndef GETDAQ_XCFGEDITHISTORY_H
#define GETDAQ_XCFGEDITHISTORY_H
#include "xcfgstructureeditor.h"
#include <functional>

// 当前窗口会话的单文件历史；重放前审阅、核对全部来源，不执行硬件操作。
class XcfgEditHistory {
public:
    struct Preview {
        bool redo = false;
        quint64 revision = 0;
        QString label, destinationFile;
        QByteArray originalBytes, updatedBytes;
        QJsonObject beforeSnapshot, afterSnapshot, report;
    };
    explicit XcfgEditHistory(int maximumEntries = 32, qint64 byteBudget = 64 * 1024 * 1024);
    void Clear();
    int UndoSteps() const { return fCursor; }
    int RedoSteps() const { return fEntries.size() - fCursor; }
    qint64 RetainedBytes() const { return fBytes; } // 保存字节/来源标识预算，不是整个RSS。
    bool SetValue(XcfgDocument *document, int node, const QString &value, QString *error);
    bool ApplyOverride(XcfgDocument *document, const XcfgDocument::OverridePreview &preview, QString *error);
    bool ApplyStructure(XcfgDocument *document, const XcfgStructureEditor::Preview &preview, QString *error);
    bool CreatePreview(const XcfgDocument &document, bool redo, Preview *preview, QString *error) const;
    bool Apply(XcfgDocument *document, const Preview &preview, QString *error);
private:
    struct Entry {
        QString root, label, destination, beforeDigest, afterDigest;
        QByteArray beforeBytes, afterBytes;
        QMap<QString, QString> guards;
        qint64 cost = 0;
    };
    QVector<Entry> fEntries;
    int fCursor = 0, fMaximum;
    qint64 fBudget, fBytes = 0;
    quint64 fRevision = 0;
    bool Execute(XcfgDocument *document, const QString &label, const std::function<bool()> &operation, QString *error,
                 const QMap<QString,QByteArray> &dependencies = QMap<QString,QByteArray>());
    static bool Candidate(const XcfgDocument &document, const Entry &entry, const QByteArray &bytes,
                          XcfgDocument *candidate, QString *error);
};
#endif
