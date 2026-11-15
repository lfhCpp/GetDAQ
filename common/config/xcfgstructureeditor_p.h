#ifndef GETDAQ_XCFGSTRUCTUREEDITOR_P_H
#define GETDAQ_XCFGSTRUCTUREEDITOR_P_H

#include "xcfgdocument.h"
#include <QMap>
#include <QString>
#include <QXmlStreamWriter>
#include <stdexcept>

namespace XcfgStructureEditorInternal {
inline bool Fail(QString *error, const QString &message)
{
    *error = message;
    return false;
}

inline QString Element(const QString &name, const QMap<QString, QString> &attributes,
                       const QString &value, bool scalar,
                       const QString &children = QString())
{
    QString output;
    QXmlStreamWriter xml(&output);
    xml.writeStartElement(name);
    for (auto it = attributes.constBegin(); it != attributes.constEnd(); ++it)
        xml.writeAttribute(it.key(), it.value());
    // Force a start/end pair, then insert encoded text or original child pieces.
    // Literal spaces written by QXmlStreamWriter would condense on legacy reload.
    xml.writeCharacters(QString());
    xml.writeEndElement();
    if (scalar) output.insert(output.lastIndexOf("</"), XcfgDocument::EncodeScalarText(value));
    else if (!children.isEmpty()) {
        const int close = output.lastIndexOf("</");
        if (close >= 0) output.insert(close, children);
        else {
            const int slash = output.lastIndexOf("/>");
            output.replace(slash, 2, ">" + children + "</" + name + ">");
        }
    }
    return output;
}

inline QString LastSegment(const QString &path)
{
    bool bracket = false;
    for (int i = path.size() - 1; i >= 0; --i) {
        if (path[i] == ']') bracket = true;
        else if (path[i] == '[') bracket = false;
        else if (path[i] == '/' && !bracket) return path.mid(i + 1);
    }
    return path;
}

inline QString AppendChildren(const XcfgDocument::Node &node, const XcfgDocument::Source &source,
                              const QString &children)
{
    QString result = source.text.mid(node.tagStart, node.elementEnd - node.tagStart);
    if (children.isEmpty()) return result;
    if (node.selfClosing) {
        const int slash = result.lastIndexOf("/>");
        if (slash < 0) throw std::runtime_error("Invalid self-closing offsets");
        result.replace(slash, 2, ">\n" + children + "\n</" + node.name + ">");
    } else {
        if (node.contentEnd < node.contentStart) throw std::runtime_error("Invalid parent insertion offsets");
        result.insert(node.contentEnd - node.tagStart, '\n' + children + '\n');
    }
    return result;
}
}

#endif
