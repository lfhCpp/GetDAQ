#include "xcfgtransaction.h"
#include "xcfgdocument.h"
#include "xcfgpending.h"
#include "xcfgdocumentinternal_p.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QUrl>
#include <QXmlStreamReader>
#include <exception>

using namespace XcfgDocumentInternal;


bool XcfgDocument::Load(const QString &path, QString *error)
{
    XcfgIoTransaction ioTransaction(error);if(!ioTransaction.IsLocked())return false;

    XcfgDocument next;
    next.fRootFile = QFileInfo(path).canonicalFilePath();
    if (next.fRootFile.isEmpty()) {
        const QFileInfo file(path);const auto parent=QFileInfo(file.absolutePath()).canonicalFilePath();
        if(!parent.isEmpty() && !XcfgNoPendingSave(QDir(parent).filePath(file.fileName()),error))return false;
        return Fail(error, "Configuration file does not exist: " + path);
    }
    if (next.Read(next.fRootFile, {}, error) < 0) return false;
    if(!next.CheckSources(error))return false;
    *this = next; error->clear(); return true;
}
int XcfgDocument::Read(const QString &path, QStringList stack, QString *error)
{
    XcfgIoTransaction ioTransaction(error);if(!ioTransaction.IsLocked())return -1;

    QString canonical = QFileInfo(path).canonicalFilePath();
    if(canonical.isEmpty() && fVirtualSources)
        canonical=QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    auto fail = [error, path](const QString &message) { *error = path + ": " + message; return -1; };
    if (canonical.isEmpty()) {
        const QFileInfo file(path);const auto parent=QFileInfo(file.absolutePath()).canonicalFilePath();
        if(!parent.isEmpty() && !XcfgNoPendingSave(QDir(parent).filePath(file.fileName()),error))return -1;
        return fail("Missing include/file");
    }
    if(fVirtualSources && !fReadBytes.contains(canonical))return fail("Migration include escapes the reviewed source set");
    if (stack.contains(canonical)) return fail("Recursive include: " + stack.join(" -> "));
    if (fSources.contains(canonical)) return fSources[canonical].root;
    if (stack.size() >= 32 || fSources.size() >= 256) return fail("Include depth/file limit exceeded");
    QFile input(canonical); Source source;
    if (fReadBytes.contains(canonical)) source.bytes=fReadBytes.value(canonical);
    else {
        if(!XcfgNoPendingSave(canonical,error))return -1;
        if (!input.open(QIODevice::ReadOnly)) return fail(input.errorString());
        if (input.size() > 8 * 1024 * 1024 || fTotalBytes + input.size() > 64 * 1024 * 1024) return fail("Configuration byte limit exceeded");
        source.bytes=input.read(8 * 1024 * 1024 + 1);
        if (input.error()!=QFile::NoError) return fail("Unreadable configuration");
    }
    source.text = QString::fromUtf8(source.bytes);
    if (source.bytes.size() > 8 * 1024 * 1024 || fTotalBytes + source.bytes.size() > 64 * 1024 * 1024) return fail("Configuration grew beyond byte limit");
    if (source.text.toUtf8() != source.bytes) return fail("Non-UTF-8 configuration");
    fTotalBytes += source.bytes.size();
    QXmlStreamReader xml(source.text); QVector<int> parents; QVector<int> ownNodes;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.tokenType() == QXmlStreamReader::DTD) return fail("DTD/entity declarations are not supported");
        if (xml.isStartElement()) {
            if (parents.size() >= 128 || fNodes.size() >= 100000) return fail("XML depth/node limit exceeded");
            Node node; node.name = xml.qualifiedName().toString(); node.file = canonical; node.line = int(xml.lineNumber());
            for (const auto &a : xml.attributes()) node.attributes[a.qualifiedName().toString()] = a.value().toString();
            const QString segment = node.name + (node.attributes.contains("id") ? "[" + node.attributes["id"] + "]" : "");
            node.path = parents.isEmpty() ? segment : fNodes[parents.last()].path + "/" + segment;
            node.contentStart = int(xml.characterOffset());
            node.tagStart=source.text.lastIndexOf('<',node.contentStart-1);
            node.selfClosing=source.text.mid(node.tagStart,node.contentStart-node.tagStart).trimmed().endsWith("/>");
            const int id = fNodes.size(); fNodes.append(node); ownNodes.append(id);
            if (parents.isEmpty()) source.root = id;
            else fNodes[parents.last()].children.append(id);
            parents.append(id);
        } else if (xml.isCharacters() && !parents.isEmpty()) {
            fNodes[parents.last()].value += xml.text().toString();
        } else if (xml.isEndElement() && !parents.isEmpty()) {
            Node &node = fNodes[parents.takeLast()];
            const int end = int(xml.characterOffset());
            node.elementEnd=end;
            node.contentEnd = source.text.lastIndexOf("</", end - 1);
            // 自闭合元素没有可直接替换的文本区域。
            if (node.contentEnd < node.contentStart) node.contentEnd = -1;
            if (node.children.isEmpty()) {
                if (!node.selfClosing && node.contentEnd>=node.contentStart) {
                    QString textError;bool splitText=false;
                    if (!ScalarText(source.text.mid(node.contentStart,node.contentEnd-node.contentStart),&node.value,&textError,
                                    nullptr,nullptr,nullptr,&splitText))
                        return fail(textError+" at "+node.path);
                    if(splitText) {
                        if(!node.attributes.contains("type"))return fail("Untyped scalar text is split by markup at "+node.path);
                        node.valueEditable=false;
                    }
                }
                node.scalar=!node.value.isEmpty() || node.attributes.contains("type");
            } else {
                if (!node.value.trimmed().isEmpty()) return fail("Mixed scalar text and child elements at " + node.path);
                node.value.clear();
            }
        }
    }
    if (xml.hasError() || source.root < 0) return fail(QString("XML line %1: %2").arg(xml.lineNumber()).arg(xml.errorString()));
    fSources.insert(canonical, source); stack.append(canonical);
    // include 导入的是被包含文件根节点的内容；保留独立来源，既不展开写回也不丢未知节点。
    for (int id : ownNodes) {
        QString include = fNodes[id].attributes.value("include");
        if (include.isEmpty()) continue;
        if (fNodes[id].scalar) return fail("Scalar cannot include a document: " + fNodes[id].path);
        if (include.startsWith("file:")) include = QUrl(include).toLocalFile();
        else if (include.contains("://")) return fail("Only local file includes are supported");
        if (include.isEmpty()) return fail("Empty include path");
        const int included = Read(QDir(QFileInfo(canonical).absolutePath()).absoluteFilePath(include), stack, error);
        if (included < 0) return -1;
        fNodes[id].includedRoot = included;
    }
    return source.root;
}

bool XcfgDocument::SetValue(int index, const QString &value, QString *error)
{
    XcfgIoTransaction ioTransaction(error,XcfgIoTransaction::Write);if(!ioTransaction.IsLocked())return false;

    if (value.size() > 1024 * 1024) return Fail(error, "Scalar value exceeds 1 MiB character limit");
    if (index < 0 || index >= fNodes.size()) return Fail(error, "No selected configuration value");
    const Node node = fNodes[index];
    if (!node.scalar || node.contentEnd < node.contentStart) return Fail(error, "Only existing scalar text can be edited");
    if (!node.valueEditable) return Fail(error, "Legacy split scalar is read-only; normalize its XML text explicitly before editing");
    if (value.isEmpty() && !node.attributes.contains("type")) return Fail(error, "Empty untyped text would change scalar/object semantics");
    QVector<ScalarProperties> properties;
    if (!DeclarationProperties(index,&properties,error)) return false;
    for (const auto &info:properties) {
        Node effective=node; effective.attributes.remove("range");
        if (info.rangeSource>=0) effective.attributes["range"]=info.range;
        if (!CheckValue(effective,value,error)) { *error=info.classPath+": "+*error; return false; }
    }
    const Source source = fSources.value(node.file);
    // 共享范围和投影结构可来自其他文件；保存前核对整个已加载来源集合。
    if (!CheckSources(error)) return false;
    const QString original = source.text.mid(node.contentStart, node.contentEnd - node.contentStart);
    int first=0,length=0;bool cdata=false;QString previous;
    if(!ScalarText(original,&previous,error,&first,&length,&cdata))return false;
    if(previous!=node.value)return Fail(error,"Scalar token value differs from the loaded declaration");
    // Preserve surrounding comments/instructions and literal XML indentation.
    // CDATA cannot represent its terminator, or preserve CR in other readers;
    // one escaped text section preserves those values without split text nodes.
    const QString escaped=cdata && !value.contains("]]>") && !value.contains('\r')
        ? "<![CDATA["+value+"]]>" : EncodeScalarText(value);
    QString updated = source.text;
    updated.replace(node.contentStart + first, length, escaped);
    QString intended;
    if(!ScalarText(updated.mid(node.contentStart,node.contentEnd-node.contentStart-length+escaped.size()),&intended,error) || intended!=value)
        return Fail(error,"Edited scalar tokens do not preserve the requested value");
    QXmlStreamReader check(updated);
    while (!check.atEnd()) check.readNext();
    if (check.hasError()) return Fail(error, "Updated value is not valid XML: " + check.errorString());
    QSaveFile output(node.file); const QByteArray bytes = updated.toUtf8();
    const auto before=fSources;
    if (bytes.size() > 8 * 1024 * 1024 || fTotalBytes - source.bytes.size() + bytes.size() > 64 * 1024 * 1024)
        return Fail(error, "Updated configuration exceeds document limits");
    if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size() || !output.commit()) return Fail(error, output.errorString());
    // 写入成功但并发改动了 include 时，明确报重载错误，不复用旧索引。
    return ReloadEdited(node.file,bytes,before,"Value",error);
}

bool XcfgDocument::ReloadEdited(const QString &destination,const QByteArray &bytes,const QMap<QString,Source> &before,
                               const QString &label,QString *error)
{
    const auto root=fRootFile;
    if(!Load(root,error)) {fNodes.clear();fSources.clear();*error=label+" saved, but reload failed: "+*error;return false;}
    bool same=fSources.size()==before.size();
    for(auto it=before.constBegin();same && it!=before.constEnd();++it)
        same=fSources.contains(it.key()) && fSources.value(it.key()).bytes==(it.key()==destination ? bytes : it->bytes);
    if(!same) {
        fNodes.clear();fSources.clear();
        *error=label+" saved, but reloaded sources differ; reopen and inspect";return false;
    }
    error->clear();return true;
}

bool XcfgDocument::CheckSources(QString *error) const
{
    XcfgIoTransaction ioTransaction(error);if(!ioTransaction.IsLocked())return false;

    for (auto it=fSources.constBegin(); it!=fSources.constEnd(); ++it) {
        if(!XcfgNoPendingSave(it.key(),error))return false;
        QFile current(it.key());
        if (!current.open(QIODevice::ReadOnly) || current.size()!=it->bytes.size() || current.read(it->bytes.size()+1)!=it->bytes)
            return Fail(error,"Source changed since loading; reload before editing: "+it.key());
    }
    error->clear(); return true;
}
bool XcfgDocument::SourcesUnchanged(QString *error) const { return CheckSources(error); }

