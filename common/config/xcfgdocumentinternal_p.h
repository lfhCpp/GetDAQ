#ifndef GETDAQ_XCFGDOCUMENTINTERNAL_P_H
#define GETDAQ_XCFGDOCUMENTINTERNAL_P_H

#include <QString>
#include <QStringList>

namespace XcfgDocumentInternal {
bool ScalarText(const QString &content,QString *value,QString *error,
                int *editStart=nullptr,int *editLength=nullptr,bool *cdata=nullptr,
                bool *split=nullptr);
inline bool Fail(QString *error, const QString &message)
{
    *error = message;
    return false;
}

inline QStringList PathParts(const QString &path, QString *error)
{
    QStringList parts;
    bool bracket = false;
    int begin = 0;
    if (path.size() > 65536) {
        *error = "Configuration path exceeds limit";
        return {};
    }
    for (int i = 0; i < path.size(); ++i) {
        if (path[i] == '[') {
            if (bracket) {
                *error = "Nested '[' in configuration path";
                return {};
            }
            bracket = true;
        } else if (path[i] == ']') {
            if (!bracket) {
                *error = "Unmatched ']' in configuration path";
                return {};
            }
            bracket = false;
        } else if (path[i] == '/' && !bracket) {
            if (i > begin) parts.append(path.mid(begin, i - begin));
            begin = i + 1;
        }
    }
    if (bracket) {
        *error = "Unclosed '[' in configuration path";
        return {};
    }
    if (begin < path.size()) parts.append(path.mid(begin));
    error->clear();
    return parts;
}
}

#endif
