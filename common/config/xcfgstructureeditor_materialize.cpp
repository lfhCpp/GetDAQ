#include "xcfgstructureeditor.h"
#include "xcfgstructureeditor_p.h"
#include "configurationrange.h"

#include <algorithm>
#include <functional>
#include <stdexcept>

using namespace XcfgStructureEditorInternal;

bool XcfgStructureEditor::CreateMaterializedReplacement(const XcfgDocument &document,const Request &request,
                                                        const XcfgDocument::Node &node,const XcfgDocument::Source &source,
                                                        QString *replacement,QString *error)
{
    if(node.scalar || node.file!=document.fRootFile || node.path.contains("[*]"))
        return Fail(error,"Materialize an existing concrete object in the main file");
    if(document.Lookup(node.path,error)!=request.node)
        return Fail(error,"Object is not the concrete lookup declaration");

    int visits=0;
    std::function<QString(const QString &,int)> materialize;
    materialize=[&](const QString &path,int local) -> QString {
        if(++visits>200000)throw std::runtime_error("Materialization node budget exceeded");
        const int index=document.Lookup(path,error);
        if(index<0)throw std::runtime_error(error->toStdString());
        const auto effective=document.fNodes[index];
        if(effective.scalar) {
            if(local>=0) {
                const auto own=document.fNodes[local];
                return source.text.mid(own.tagStart,own.elementEnd-own.tagStart);
            }
            XcfgDocument::Node value;XcfgDocument::ScalarProperties info;
            if(!document.ResolveScalar(path,&value,&info,error))throw std::runtime_error(error->toStdString());
            auto attributes=value.attributes;attributes.remove("include");
            if(!attributes.contains("type"))
                attributes["type"]=QString::fromStdString(GetDAQ::ConfigurationValueType(GetDAQ::ParseConfigurationValue(value.value.toStdString())));
            return Element(value.name,attributes,value.value,true);
        }

        const auto children=document.EffectiveChildren(path,error);
        if(!error->isEmpty())throw std::runtime_error(error->toStdString());
        struct Patch {int at,size;QString value;};
        QVector<Patch> patches;QString missing;
        for(const auto &child:children) {
            const int childIndex=document.Lookup(child,error);
            if(childIndex<0)throw std::runtime_error(error->toStdString());
            const auto part=child.mid(path.size()+1);
            int own=-1;
            if(local>=0)for(int candidate:document.fNodes[local].children) {
                const auto declared=document.fNodes[candidate];
                const QString segment=declared.name+(declared.attributes.value("id").isEmpty() ? QString() : '['+declared.attributes.value("id")+']');
                if(segment==part)own=candidate;
            }
            const auto text=materialize(child,own);
            if(text.toUtf8().size()>8*1024*1024)throw std::runtime_error("Materialization exceeds document budget");
            if(own>=0) {
                const auto declared=document.fNodes[own];
                patches.append({declared.tagStart,declared.elementEnd-declared.tagStart,text});
            } else {
                missing+='\n'+text;
                if(missing.toUtf8().size()>8*1024*1024)throw std::runtime_error("Materialization exceeds document budget");
            }
        }
        if(local<0) {
            auto attributes=effective.attributes;attributes.remove("include");
            const auto part=LastSegment(path);const int bracket=part.indexOf('[');
            if(bracket<0)attributes.remove("id");
            else attributes["id"]=part.mid(bracket+1,part.size()-bracket-2);
            return Element(effective.name,attributes,QString(),false,missing);
        }

        const auto own=document.fNodes[local];
        QString result=AppendChildren(own,source,missing);
        std::sort(patches.begin(),patches.end(),[](const Patch &a,const Patch &b) {return a.at>b.at;});
        for(const auto &patch:patches)result.replace(patch.at-own.tagStart,patch.size,patch.value);
        if(own.attributes.contains("include")) {
            // Use the original opening-tag boundary because quoted attributes can contain '>'.
            const int length=own.contentStart-own.tagStart-(own.selfClosing && !missing.isEmpty() ? 1 : 0);
            result.replace(0,length,RewriteOpeningAttribute(result.left(length),"include",QString(),true));
        }
        return result;
    };

    *replacement=materialize(node.path,request.node);
    return true;
}
