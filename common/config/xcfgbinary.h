#ifndef GETDAQ_XCFGBINARY_H
#define GETDAQ_XCFGBINARY_H
#include "xcfgexporter.h"

// Legacy CompoundConfig version-1 binary configuration, for offline conversion.
// Not a receiver format and never linked into the native raw-data executables.
class XcfgBinary {
public:
    struct Preview {
        QString source, destination, objectPath;
        XcfgExporter::Mode mode = XcfgExporter::FlattenDeclarations;
        bool importing = false;
        QByteArray bytes;
        QMap<QString,QByteArray> sourceBytes;
        int objects = 0, scalars = 0;
    };
    static bool Decode(const QByteArray &binary,QByteArray *xml,QString *error);
    static bool Encode(const QByteArray &selfContainedXml,QByteArray *binary,QString *error);
    static bool PreviewImport(const QString &source,const QString &destination,Preview *preview,QString *error);
    static bool PreviewExport(const XcfgDocument &document,const XcfgExporter::Request &request,Preview *preview,QString *error);
    static bool Save(const Preview &preview,QString *error);
};
#endif
