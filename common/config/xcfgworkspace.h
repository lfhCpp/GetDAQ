#ifndef GETDAQ_XCFGWORKSPACE_H
#define GETDAQ_XCFGWORKSPACE_H
#include "xcfgdocument.h"
#include "xcfgstructureeditor.h"
#include <QJsonObject>
#include <functional>

// 多根文档共用一份物理来源；编辑先留在内存，Save All逐文件原子替换。
class XcfgWorkspace {
public:
    struct Preview {
        quint64 revision = 0;
        QMap<QString,QByteArray> writes;
        QMap<QString,QJsonObject> reports;
    };
    struct SaveResult {
        bool complete = false;
        QStringList savedFiles, remainingFiles;
        QString recoveryDirectory;
    };
    struct StructurePreview {
        quint64 revision=0;
        QString root;
        XcfgStructureEditor::Preview edit;
        Preview workspace;
    };
    struct MoveRequest {QString sourceRoot,targetRoot;int sourceNode=-1,targetParent=-1;};
    struct MovePreview {
        quint64 revision=0;
        MoveRequest request;
        QString sourceFile,targetFile,sourcePath,targetPath;
        QMap<QString,QByteArray> before,after;
        QJsonObject report;
        Preview workspace;
    };
    bool Open(const QString &root, QString *error);
    bool StageSource(const QString &file, const QByteArray &bytes, QString *error);
    bool StageSources(const QMap<QString,QByteArray> &changes, QString *error);
    bool Document(const QString &root,XcfgDocument *document,QString *error) const;
    bool PreviewStructure(const QString &root,const XcfgStructureEditor::Request &request,StructurePreview *preview,QString *error) const;
    bool StageStructure(const StructurePreview &preview,QString *error);
    bool PreviewMove(const MoveRequest &request,MovePreview *preview,QString *error) const;
    bool StageMove(const MovePreview &preview,QString *error);
    bool Undo(QString *error);
    bool Redo(QString *error);
    bool CanUndo() const { return !fRequiresReopen && fHistoryPosition>0; }
    bool CanRedo() const { return !fRequiresReopen && fHistoryPosition<fHistory.size(); }
    int HistoryEntries() const { return fHistory.size(); }
    qint64 HistoryBytes() const { return fHistoryBytes; }
    bool CreatePreview(Preview *preview, QString *error) const;
    bool SaveAll(const Preview &preview, SaveResult *result, QString *error,
                 const std::function<void(const QString &,const QString &)> &progress={});
    QStringList Roots() const { return fRoots; }
    QStringList Files() const { return fBaseline.keys(); }
    QStringList DirtyFiles() const { return fStaged.keys(); }
    QByteArray Bytes(const QString &file) const { return fStaged.value(file, fBaseline.value(file)); }
    bool RequiresReopen() const { return fRequiresReopen; }
private:
    friend class XcfgSaveJournal;
    friend class XcfgSourceMigration;
    QStringList fRoots;
    QMap<QString,QByteArray> fBaseline, fStaged;
    quint64 fRevision = 0;
    bool fRequiresReopen = false;
    QString fActiveSave;
    struct HistoryEntry { QMap<QString,QByteArray> before,after; qint64 bytes=0; };
    QVector<HistoryEntry> fHistory;
    int fHistoryPosition = 0;
    qint64 fHistoryBytes = 0;
    static bool Budget(const QMap<QString,QByteArray> &sources, QString *error);
    bool StageSourcesImpl(const QMap<QString,QByteArray> &changes, bool recordHistory, QString *error);
    bool ApplyHistory(bool redo, QString *error);
    bool CheckSources(QString *error) const;
    static bool Build(const QStringList &roots, const QMap<QString,QByteArray> &bytes,
                      QMap<QString,XcfgDocument> *documents, QMap<QString,QByteArray> *sources, QString *error,
                      bool virtualSources=false);
};
#endif
