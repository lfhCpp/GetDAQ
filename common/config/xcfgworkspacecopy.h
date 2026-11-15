#ifndef GETDAQ_XCFGWORKSPACECOPY_H
#define GETDAQ_XCFGWORKSPACECOPY_H
#include "xcfgdocument.h"
#include <QJsonObject>

// Editable source workspace copy: keeps includes, optionally splits each use.
// Separate from immutable experiment publications and hardware deployment.
class XcfgWorkspaceCopy {
public:
    enum Sharing { PreserveSharedFiles, IsolateEachReference };
    struct Request { Sharing sharing=IsolateEachReference;QString directory; };
    struct Preview {
        Request request;
        QString rootFile,effectiveSha256;
        QMap<QString,QByteArray> sourceBytes,files;
        QJsonObject inventory;
    };
    static bool CreatePreview(const XcfgDocument &document,const Request &request,Preview *preview,QString *error);
    static bool Save(const XcfgDocument &document,const Preview &preview,QJsonObject *receipt,QString *error);
};
#endif
