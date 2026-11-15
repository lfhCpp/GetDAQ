#ifndef GETDAQ_XCFGSTRUCTUREEDITOR_H
#define GETDAQ_XCFGSTRUCTUREEDITOR_H
#include "xcfgdocument.h"
#include <QJsonObject>

// 单文件结构变更：预览完整有效差异，保存前重算，不向硬件发送命令。
class XcfgStructureEditor {
public:
    enum Operation { AddObject, AddScalar, RemoveDeclaration, ChangeId, MaterializeObject,
                     ChangeName, CloneDeclaration, MoveBefore, MoveAfter, ChangeScalarProperties,
                     ReplaceWithInclude, ChangeInclude, ImportConfiguration, OverrideInheritedObject };
    struct Request {
        Operation operation = AddObject;
        int node = -1;
        int sibling = -1;
        QString name, id, value, type, unit, range, includePath, path;
    };
    struct Preview {
        Request request;
        QString destinationFile, sourcePath;
        QByteArray originalBytes, updatedBytes;
        QJsonObject beforeSnapshot, afterSnapshot, report;
        QMap<QString,QByteArray> dependencyBytes; // Prospective includes/import inputs, including files not retained by import.
        QStringList createdPaths;
        QStringList localizedPaths; // Every concrete projection made local, including shared-file users.
    };
    static bool CreatePreview(const XcfgDocument &document,const Request &request,Preview *preview,QString *error);
    static bool Apply(XcfgDocument *document,const Preview &preview,QString *error);
    static QString RewriteOpeningAttribute(const QString &opening,const QString &name,const QString &value,bool remove=false);
private:
    friend class XcfgWorkspace;
    // Workspace owns disk-baseline checks; this path never saves a file.
    static bool CreatePreviewImpl(const XcfgDocument &document,const Request &request,Preview *preview,QString *error,bool verifyDisk);
    static bool CreateObjectOverride(const XcfgDocument &document,const Request &request,Preview *preview,QString *error,bool verifyDisk);
    static bool CreateIncludeReplacement(const XcfgDocument &document,const Request &request,
                                          const XcfgDocument::Node &node,const XcfgDocument::Source &source,
                                          QString *replacement,QMap<QString,QByteArray> *dependencies,
                                          QString *error,bool verifyDisk);
    static bool CreateMaterializedReplacement(const XcfgDocument &document,const Request &request,
                                              const XcfgDocument::Node &node,const XcfgDocument::Source &source,
                                              QString *replacement,QString *error);
};
#endif
