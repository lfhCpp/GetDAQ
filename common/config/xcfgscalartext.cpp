#include "xcfgdocument.h"
#include "xcfgdocumentinternal_p.h"
#include <QXmlStreamReader>

namespace XcfgDocumentInternal {
namespace {
bool LiteralXmlSpace(QChar c)
{
    return c==' ' || c=='\t' || c=='\r' || c=='\n';
}
}
// TinyXml's default condenses literal XML whitespace before decoding character
// references. CDATA and referenced whitespace retain their exact text value.
// Source::bytes/text still hold the unchanged file for editing and history.
bool ScalarText(const QString &content,QString *value,QString *error,
                int *editStart,int *editLength,bool *cdata,
                bool *split)
{
    QString result,lastFragment; int at=0,fragments=0;
    if(editStart)*editStart=0;
    if(editLength)*editLength=0;
    if(cdata)*cdata=false;
    if(split)*split=false;
    while (at<content.size()) {
        if (content.midRef(at,4)=="<!--") {
            const int end=content.indexOf("-->",at+4);
            if (end<0) return Fail(error,"Unclosed scalar comment");
            at=end+3;
        } else if (content.midRef(at,9)=="<![CDATA[") {
            const int end=content.indexOf("]]>",at+9);
            if (end<0) return Fail(error,"Unclosed scalar CDATA");
            if(editStart)*editStart=at;
            if(editLength)*editLength=end+3-at;
            if(cdata)*cdata=true;
            lastFragment=content.mid(at+9,end-at-9);result+=lastFragment;++fragments;at=end+3;
        } else if (content.midRef(at,2)=="<?") {
            const int end=content.indexOf("?>",at+2);
            if (end<0) return Fail(error,"Unclosed scalar processing instruction");
            at=end+2;
        } else {
            int end=content.indexOf('<',at);
            if (end<0) end=content.size();
            if (end==at) return Fail(error,"Unsupported markup inside scalar text");
            QString condensed; bool pendingSpace=false;
            for (int i=at;i<end;++i) {
                if (LiteralXmlSpace(content[i])) { if (!condensed.isEmpty()) pendingSpace=true; }
                else {
                    if (pendingSpace) condensed+=' ';
                    condensed+=content[i]; pendingSpace=false;
                }
            }
            if (!condensed.isEmpty()) {
                int first=at,last=end;
                while(first<last && LiteralXmlSpace(content[first]))++first;
                while(last>first && LiteralXmlSpace(content[last-1]))--last;
                if(editStart)*editStart=first;
                if(editLength)*editLength=last-first;
                if(cdata)*cdata=false;
                if (condensed.contains('&')) {
                    QXmlStreamReader decoder("<value>"+condensed+"</value>");
                    QString decoded;
                    while (!decoder.atEnd()) { decoder.readNext(); if (decoder.isCharacters()) decoded+=decoder.text(); }
                    if (decoder.hasError()) return Fail(error,"Invalid scalar character reference");
                    lastFragment=decoded;
                } else lastFragment=condensed;
                result+=lastFragment;
                ++fragments;
            }
            at=end;
        }
    }
    // The legacy typed visitor overwrote the scalar on each text/CDATA node,
    // leaving the final fragment. Keep that read behavior while marking split
    // source markup read-only so an edit cannot discard earlier fragments.
    if(split)*split=fragments>1;
    *value=fragments>1?lastFragment:result;return true;
}
} // namespace XcfgDocumentInternal

QString XcfgDocument::EncodeScalarText(const QString &value)
{
    QString encoded;
    for (QChar c:value) {
        if (c=='&') encoded+="&amp;";
        else if (c=='<') encoded+="&lt;";
        else if (c=='>') encoded+="&gt;";
        else if (c==' ') encoded+="&#32;";
        else if (c=='\t') encoded+="&#9;";
        else if (c=='\r') encoded+="&#13;";
        else if (c=='\n') encoded+="&#10;";
        else encoded+=c;
    }
    return encoded;
}
