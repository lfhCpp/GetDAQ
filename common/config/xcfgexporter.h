#ifndef GETDAQ_XCFGEXPORTER_H
#define GETDAQ_XCFGEXPORTER_H
#include "xcfgdocument.h"

// Self-contained XML copy. Source documents/history/hardware remain separate.
class XcfgExporter {
public:
    enum Mode { FlattenDeclarations, EffectiveConcreteObject };
    struct Request {
        Mode mode=FlattenDeclarations;
        QString path, destination;
    };
    struct Preview {
        Request request;
        QString rootFile, effectiveSha256;
        QByteArray xml;
        QMap<QString,QByteArray> sourceBytes;
        int objectCount=0,scalarCount=0;
    };
    static bool CreatePreview(const XcfgDocument &document,const Request &request,Preview *preview,QString *error);
    static bool Save(const XcfgDocument &document,const Preview &preview,QString *error);
};
#endif
