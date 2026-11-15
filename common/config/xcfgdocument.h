#ifndef GETDAQ_XCFGDOCUMENT_H
#define GETDAQ_XCFGDOCUMENT_H
#include <QByteArray>
#include <QMap>
#include <QStringList>
#include <QVector>

// 原始配置文档层：保留原字节与 include 来源，不把展开后的树反写到原文件。
class XcfgDocument {
public:
    struct Node {
        QString name, value, file, path;
        QMap<QString, QString> attributes;
        QVector<int> children;
        int line = 0, tagStart = -1, contentStart = -1, contentEnd = -1, elementEnd = -1;
        int includedRoot = -1;
        bool scalar = false;
        // Legacy typed scalars may contain several text/CDATA nodes. Their
        // value is readable, but the raw markup cannot be edited safely in place.
        bool valueEditable = true;
        bool selfClosing = false;
    };
    struct Source { QByteArray bytes; QString text; int root = -1; };
    struct ScalarProperties {
        QString classPath, unit, range;
        int unitSource = -1, rangeSource = -1;
        bool hasClassInfo = false;
    };
    struct OverridePreview {
        QString path, destinationFile, previousValue, value;
        QByteArray originalBytes, updatedBytes;
    };
    bool Load(const QString &path, QString *error);
    const QVector<Node> &Nodes() const { return fNodes; }
    const QMap<QString, Source> &Sources() const { return fSources; }
    const QString &RootFile() const { return fRootFile; }
    // 从具体路径查找声明来源；每层先具体 ID 后默认 ID，未声明的实例不凭空生成。
    int Lookup(const QString &path, QString *error) const;
    // 枚举继承后可见的具体子对象路径，不把 '*' 本身作为硬件实例。
    QStringList Children(const QString &path, const QString &name, QString *error) const;
    // ClassInfo按元素名路径共享，忽略所有ID；具体值和type仍属于原声明。
    bool ResolveScalar(const QString &path, Node *value, ScalarProperties *properties, QString *error) const;
    bool DeclarationProperties(int node, QVector<ScalarProperties> *properties, QString *error) const;
    // 只更改选中来源文件中的现有 scalar；不会自动创建覆盖或改动被继承的其他对象。
    bool SetValue(int node, const QString &value, QString *error);
    bool PreviewOverride(const QString &path, const QString &value, OverridePreview *preview, QString *error) const;
    bool ApplyOverride(const OverridePreview &preview, QString *error);
    // 捕获已加载有效树；先核对所有来源，不凭空生成默认实例。
    bool CaptureEffectiveTree(QMap<QString,Node> *values, QString *error) const;
    bool SourcesUnchanged(QString *error) const;
    static bool CheckValue(const Node &node, const QString &value, QString *error);
    // Encode an intended text value without TinyXml-style whitespace condensation.
    // Character references also preserve leading/trailing spaces on reload.
    static QString EncodeScalarText(const QString &value);
private:
    friend class XcfgStructureEditor;
    friend class XcfgEditHistory;
    friend class EffectiveConfigurationSnapshot;
    friend class XcfgExporter;
    friend class XcfgWorkspace;
    friend class XcfgSourceMigration;
    int Read(const QString &path, QStringList stack, QString *error);
    int LookupImpl(const QString &path,bool *inherited,QString *error) const;
    static QStringList SplitPath(const QString &path,QString *error);
    QVector<Node> fNodes;
    QMap<QString, Source> fSources;
    QString fRootFile;
    qint64 fTotalBytes = 0;
    QMap<QString,QByteArray> fReadBytes;
    // 仅迁移器的封闭内存来源图可使用不存在的目标路径，普通Load不启用。
    bool fVirtualSources = false;
    mutable bool fPropertiesBuilt = false;
    mutable QString fPropertiesError;
    mutable QMap<QString, ScalarProperties> fProperties;
    mutable QMap<int, QStringList> fDeclarationClasses;
    mutable QMap<int,QMap<QString,int>> fChildIndices;
    mutable QMap<int,QString> fChildIndexErrors;
    mutable qint64 fChildIndexEntries=0;
    bool ChildIndex(int parent,QMap<QString,int> *children,QString *error) const;
    bool BuildProperties(QString *error) const;
    QStringList EffectiveChildren(const QString &path, QString *error,bool includeDefaults=false) const;
    bool EffectiveScalars(QMap<QString,Node> *values, QString *error) const;
    bool EffectiveNodes(QMap<QString,Node> *values, QString *error) const;
    bool CheckSources(QString *error) const;
    bool ReloadEdited(const QString &destination,const QByteArray &bytes,const QMap<QString,Source> &before,
                      const QString &label,QString *error);
};
#endif
