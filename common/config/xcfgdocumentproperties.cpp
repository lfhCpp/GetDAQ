#include "xcfgdocument.h"
#include "xcfgdocumentinternal_p.h"
#include "configurationrange.h"
#include <exception>
#include <functional>

using namespace XcfgDocumentInternal;

namespace {
bool EquivalentRanges(const QString &left, const QString &right) {
    if (left.isEmpty() || right.isEmpty()) return left == right;
    return GetDAQ::ParseConfigurationRange(left.toStdString()).Equivalent(GetDAQ::ParseConfigurationRange(right.toStdString()));
}
}

bool XcfgDocument::BuildProperties(QString *error) const
{
    if (fPropertiesBuilt) { *error=fPropertiesError; return error->isEmpty(); }
    fPropertiesBuilt=true;
    struct Member { int source; QString path; bool scalar; };
    struct View {
        QVector<Member> members;
        QMap<QString,ScalarProperties> classes;
        QMap<QString,bool> kinds;
    };
    QMap<int,View> documents; int visits=0; qint64 projectedBytes=0;
    QString problem;
    std::function<View(int)> document;
    document=[&](int root) -> View {
        if (documents.contains(root)) return documents.value(root);
        View view;
        auto append=[&](int source,const QString &path) -> bool {
            if (++visits>200000) { problem="Shared class projection limit exceeded"; return false; }
            const int bytes=path.toUtf8().size(); projectedBytes+=bytes;
            if (bytes>65536 || path.count('/')>=128 || projectedBytes>64*1024*1024) {
                problem="Shared class path depth/byte limit exceeded"; return false;
            }
            const bool scalar=fNodes[source].scalar;
            if (view.kinds.contains(path) && view.kinds[path]!=scalar) {
                problem="Object/scalar conflict in shared class "+path; return false;
            }
            view.kinds[path]=scalar; view.members.append({source,path,scalar}); return true;
        };
        std::function<void(int,const QString &)> walk;
        walk=[&](int source,const QString &prefix) {
            if (!problem.isEmpty()) return;
            const auto &node=fNodes[source]; const QString path=prefix.isEmpty() ? node.name : prefix+'/'+node.name;
            if (!append(source,path)) return;
            if (node.scalar) {
                auto &info=view.classes[path]; info.classPath=path;
                // XML载入依次setUnit/setRange，共享类中后出现的非空属性更新整类。
                if (!node.attributes.value("unit").isEmpty()) { info.unit=node.attributes["unit"]; info.unitSource=source; info.hasClassInfo=true; }
                if (!node.attributes.value("range").isEmpty()) { info.range=node.attributes["range"]; info.rangeSource=source; info.hasClassInfo=true; }
                return;
            }
            // 旧Document在对象进入时解析独立include，再import，然后读本地子元素。
            if (node.includedRoot>=0) {
                const View included=document(node.includedRoot);
                if (!problem.isEmpty()) return;
                const QString includedName=fNodes[node.includedRoot].name;
                for (const auto &member:included.members) {
                    if (member.path==includedName) continue;
                    const QString importedPath=path+member.path.mid(includedName.size());
                    if (!append(member.source,importedPath)) return;
                    if (!member.scalar) continue;
                    ScalarProperties incoming=included.classes.value(member.path); incoming.classPath=importedPath;
                    if (view.classes.contains(importedPath)) {
                        auto &existing=view.classes[importedPath];
                        try {
                            if (incoming.hasClassInfo && existing.hasClassInfo &&
                                (incoming.unit!=existing.unit || !EquivalentRanges(incoming.range,existing.range))) {
                                problem="Imported shared class metadata conflict at "+importedPath; return;
                            }
                            // CompoundConfig lets a class member with no local
                            // ClassInfo adopt metadata carried by an imported
                            // member of the same generic path.
                            if (incoming.hasClassInfo && !existing.hasClassInfo) existing=incoming;
                        } catch (const std::exception &e) { problem=importedPath+": "+QString::fromUtf8(e.what()); return; }
                    } else view.classes[importedPath]=incoming;
                }
            }
            for (int child:node.children) walk(child,path);
        };
        walk(root,QString());
        if (problem.isEmpty()) documents[root]=view;
        return view;
    };
    if (!fSources.contains(fRootFile)) problem="No configuration loaded";
    else {
        const View view=document(fSources.value(fRootFile).root);
        if (problem.isEmpty()) {
            fProperties=view.classes;
            for (const auto &member:view.members) if (member.scalar && !fDeclarationClasses[member.source].contains(member.path))
                fDeclarationClasses[member.source].append(member.path);
        }
    }
    fPropertiesError=problem; *error=problem; return problem.isEmpty();
}

bool XcfgDocument::ResolveScalar(const QString &path, Node *value, ScalarProperties *properties, QString *error) const
{
    const int index=Lookup(path,error); if (index<0) return false;
    if (!fNodes[index].scalar) return Fail(error,"Expected scalar: "+path);
    if (!BuildProperties(error)) return false;
    const QStringList parts=PathParts(path,error); if (!error->isEmpty()) return false;
    QStringList names;
    for (const auto &part:parts) names.append(part.left(part.contains('[') ? part.indexOf('[') : part.size()));
    const QString classPath=names.join('/');
    if (!fProperties.contains(classPath)) return Fail(error,"Shared scalar class not projected: "+classPath);
    Node result=fNodes[index]; const auto info=fProperties.value(classPath);
    result.attributes.remove("unit"); result.attributes.remove("range");
    if (info.unitSource>=0) result.attributes["unit"]=info.unit;
    if (info.rangeSource>=0) result.attributes["range"]=info.range;
    *value=result; *properties=info; error->clear(); return true;
}

bool XcfgDocument::DeclarationProperties(int node, QVector<ScalarProperties> *properties, QString *error) const
{
    if (node<0 || node>=fNodes.size() || !fNodes[node].scalar) return Fail(error,"Expected scalar declaration");
    if (!BuildProperties(error)) return false;
    QVector<ScalarProperties> result;
    for (const auto &path:fDeclarationClasses.value(node)) result.append(fProperties.value(path));
    if (result.isEmpty()) return Fail(error,"Declaration is not part of the loaded document projection");
    *properties=result; error->clear(); return true;
}

bool XcfgDocument::CheckValue(const Node &node, const QString &value, QString *error)
{
    try {
        const auto declared=node.attributes.value("type").toStdString();
        const auto type=declared.empty() ? GetDAQ::ConfigurationValueType(GetDAQ::ParseConfigurationValue(node.value.toStdString())) : declared;
        const auto parsed=GetDAQ::ParseConfigurationValue(value.toStdString(),type);
        const QString range=node.attributes.value("range").trimmed();
        if (!range.isEmpty()) {
            const auto constraint=GetDAQ::ParseConfigurationRange(range.toStdString());
            if (!constraint.Contains(parsed)) return Fail(error,"Value is outside declared "+
                QString(constraint.kind==GetDAQ::ConfigurationRange::Interval ? "interval" : "enumeration")+
                " (scalar and bounds must have matching types): "+range);
        }
        error->clear(); return true;
    } catch (const std::exception &e) { return Fail(error,QString::fromUtf8(e.what())); }
}
