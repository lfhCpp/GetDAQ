#include "xcfgbinary_codec_p.h"
#include "xcfgbinary.h"
#include "configurationrange.h"
#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>
#include <QXmlStreamWriter>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {
const int MaxBytes=8*1024*1024, MaxItems=100000, MaxDepth=128;
using GetDAQ::ConfigurationValue;
using GetDAQ::ConfigurationRange;
void Require(bool condition,const char *message) {if(!condition)throw std::runtime_error(message);}
QString Text(const QByteArray &bytes)
{
    const auto text=QString::fromUtf8(bytes);Require(text.toUtf8()==bytes,"Binary configuration contains invalid UTF-8");
    for(QChar c:text)Require((c.unicode()>=32 && c.unicode()!=0xfffe && c.unicode()!=0xffff) || c=='\t' || c=='\r' || c=='\n',"Binary configuration contains a forbidden XML character");
    return text;
}
QString Type(unsigned int type)
{
    static const char *types[]={"object","int","hex","real","bool","string"};
    Require(type<6,"Unknown binary attribute type");return types[type];
}
QString Literal(const ConfigurationValue &value)
{
    auto text=QString::fromStdString(GetDAQ::NormalizeConfigurationValue(value));
    if(value.type==ConfigurationValue::Real && !text.contains('.') && !text.contains('e') && !text.contains('E'))text+=".0";
    if(value.type==ConfigurationValue::String) {
        text.replace("\\","\\\\");text.replace("\"","\\\"");text.replace("\n","\\n");text.replace("\r","\\r");text.replace("\t","\\t");text='"'+text+'"';
    }
    return text;
}
struct Class {QString name,unit,range;unsigned int type=0;};
struct Node {int classId=0;QString index,value;QVector<Node> children;};
class Reader {
public:
    explicit Reader(const QByteArray &bytes):fBytes(bytes) {Require(bytes.size()<=MaxBytes,"Binary byte limit exceeded");}
    quint64 Word(int width) {
        Require(width<=8 && width>0 && fAt<=fBytes.size()-width,"Truncated binary configuration");
        quint64 word=0;for(int i=0;i<width;++i)word=(word<<8)|static_cast<unsigned char>(fBytes[fAt++]);return word;
    }
    QString String() {const auto id=Word(4);Require(id<quint64(fStrings.size()),"Binary string reference out of bounds");return fStrings[int(id)];}
    ConfigurationValue Value(unsigned int type) {
        ConfigurationValue value;Require(type>0 && type<6,"Object cannot carry a scalar range/value");value.type=ConfigurationValue::Type(type-1);
        if(type==1) {const auto bits=Word(4);value.integer=bits<=0x7fffffffULL ? qint64(bits) : qint64(bits)-0x100000000LL;}
        if(type==2) {value.hexadecimal=Word(8);Require(value.hexadecimal<=0xffffffffULL,"Legacy hex is unsigned 32-bit; high wire bits are not accepted");}
        if(type==3) {const quint64 bits=Word(8);static_assert(sizeof(double)==8,"Binary configuration requires 64-bit double");std::memcpy(&value.real,&bits,8);Require(std::numeric_limits<double>::is_iec559 && std::isfinite(value.real),"Binary real is not finite IEEE-754");}
        if(type==4) {const auto bits=Word(1);Require(bits<=1,"Binary bool is not zero or one");value.boolean=bits!=0;}
        if(type==5)value.text=String().toStdString();
        return value;
    }
    void Tables() {
        Require(Word(2)==1,"Only CompoundConfig binary version 1 is supported");const auto strings=Word(4);
        Require(strings>0 && strings<=MaxItems,"Binary string count limit exceeded");
        for(quint64 i=0;i<strings;++i) {
            const int end=fBytes.indexOf('\0',fAt);Require(end>=fAt,"Unterminated binary string");
            fStrings.append(Text(fBytes.mid(fAt,end-fAt)));fAt=end+1;
        }
        const auto classes=Word(4);Require(classes>0 && classes<=MaxItems,"Binary class count limit exceeded");
        for(quint64 i=0;i<classes;++i) {
            Class c;c.name=String();c.type=unsigned(Word(1));Type(c.type);c.unit=String();
            Require(QRegularExpression("^[A-Za-z_][A-Za-z0-9_.-]*$").match(c.name).hasMatch(),"Binary name cannot be represented by supported XML configuration");
            const auto range=Word(1);Require(range<=2,"Unknown binary range code");Require(c.type!=0 || (range==0 && c.unit.isEmpty()),"Object binary class has scalar metadata");
            if(range==1) {
                QStringList bounds;unsigned int modes[2];
                for(int b=0;b<2;++b) {modes[b]=unsigned(Word(1));Require(modes[b]<=2,"Unknown binary bound mode");const auto v=Value(c.type);const auto literal=modes[b]==2 ? QString(b ? "+inf" : "-inf") : Literal(v);Budget(literal.size());bounds.append(literal);}
                c.range=(modes[0]==1 ? "]" : "[")+bounds.join(',')+(modes[1]==1 ? "[" : "]");
            } else if(range==2) {
                const auto count=Word(4);Require(count<=4096,"Binary enumeration limit exceeded");QStringList items;
                for(quint64 j=0;j<count;++j){const auto literal=Literal(Value(c.type));Budget(literal.size());items.append(literal);}c.range='{'+items.join(',')+'}';
            }
            if(!c.range.isEmpty())GetDAQ::ParseConfigurationRange(c.range.toStdString());
            Budget(c.name.size()+c.unit.size()+c.range.size());fClasses.append(c);
        }
    }
    Node Tree(int depth=0) {
        Require(depth<MaxDepth && ++fNodes<=MaxItems,"Binary tree depth/node limit exceeded");Node node;
        const auto id=Word(4);Require(id<quint64(fClasses.size()),"Binary class reference out of bounds");node.classId=int(id);node.index=String();
        Require(!node.index.contains('[') && !node.index.contains(']') && !node.index.contains('/'),"Binary index contains unsupported path delimiters");
        const auto &c=fClasses[node.classId];if(c.type==0) {const auto count=Word(2);for(quint64 i=0;i<count;++i)node.children.append(Tree(depth+1));}
        else node.value=QString::fromStdString(GetDAQ::NormalizeConfigurationValue(Value(c.type)));
        Budget(c.name.size()+c.unit.size()+c.range.size()+node.index.size()+node.value.size());
        return node;
    }
    QByteArray Xml() {
        Tables();const auto root=Tree();Require(fClasses[root.classId].type==0,"Binary root must be an object");Require(fAt==fBytes.size(),"Trailing bytes after binary configuration");
        const auto output=(QString("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n")+WriteXml(root)+'\n').toUtf8();
        Require(output.size()<=MaxBytes,"Converted XML byte limit exceeded");return output;
    }
private:
    void Budget(qint64 characters) {Require(characters<=MaxBytes-fExpanded,"Binary expanded content limit exceeded");fExpanded+=characters;}
    QString WriteXml(const Node &node) {
        const auto &c=fClasses[node.classId];QString output;QXmlStreamWriter xml(&output);xml.writeStartElement(c.name);
        if(!node.index.isEmpty())xml.writeAttribute("id",node.index);
        if(c.type) {xml.writeAttribute("type",Type(c.type));if(!c.unit.isEmpty())xml.writeAttribute("unit",c.unit);if(!c.range.isEmpty())xml.writeAttribute("range",c.range);}
        xml.writeCharacters(QString());xml.writeEndElement();Require(!xml.hasError(),"Cannot form converted XML element");
        QString content;if(c.type)content=XcfgDocument::EncodeScalarText(node.value);
        else for(const auto &child:node.children) {const auto bytes=WriteXml(child);Require(content.size()<=MaxBytes-bytes.size(),"Converted XML character limit exceeded");content+=bytes;}
        Require(output.size()<=MaxBytes-content.size(),"Converted XML character limit exceeded");
        const int closing=output.lastIndexOf("</");Require(closing>=0,"Cannot form converted XML closing tag");output.insert(closing,content);return output;
    }
    QByteArray fBytes;int fAt=0,fNodes=0; qint64 fExpanded=0;QStringList fStrings;QVector<Class> fClasses;
};
class Writer {
public:
    QByteArray Encode(const QByteArray &xmlBytes) {
        Require(xmlBytes.size()<=MaxBytes,"XML byte limit exceeded");QTemporaryDir temporary;
        Require(temporary.isValid(),"Cannot validate XML for binary encoding");QFile file(temporary.filePath("encode.xcfg"));
        Require(file.open(QIODevice::WriteOnly|QIODevice::NewOnly) && file.write(xmlBytes)==xmlBytes.size() && file.flush(),"Cannot stage XML for binary encoding");file.close();
        XcfgDocument document;QString error;Require(document.Load(file.fileName(),&error),"Invalid XML for binary encoding");
        const auto root=ReadNode(document,document.Sources().value(document.RootFile()).root,QString(),0);
        Require(fClasses[root.classId].type==0,"Binary root must be an object");
        QByteArray nodes;WriteNode(nodes,root);QByteArray out;Word(out,1,2);Word(out,fStrings.size(),4);
        for(const auto &s:fStrings) {Append(out,s.toUtf8());Append(out,QByteArray(1,'\0'));}
        Word(out,fClasses.size(),4);Append(out,fClassBytes);Append(out,nodes);return out;
    }
private:
    void Append(QByteArray &out,const QByteArray &bytes) {Require(out.size()<=MaxBytes-bytes.size(),"Binary byte limit exceeded");out+=bytes;}
    void Word(QByteArray &out,quint64 value,int width) {QByteArray bytes(width,'\0');for(int i=width-1;i>=0;--i){bytes[i]=char(value&255);value>>=8;}Require(value==0,"Binary word overflow");Append(out,bytes);}
    int StringId(const QString &s) {
        Text(s.toUtf8());Require(!s.contains(QChar(0)),"Binary strings cannot contain NUL");auto it=fStringIds.constFind(s);if(it!=fStringIds.constEnd())return it.value();
        Require(fStrings.size()<MaxItems,"Binary string count limit exceeded");const int id=fStrings.size();fStringIds[s]=id;fStrings.append(s);return id;
    }
    void Value(QByteArray &out,const ConfigurationValue &v,unsigned int type) {
        Require(unsigned(v.type)+1==type,"Binary range type differs from scalar type");
        if(type==1) {Require(v.integer>=-2147483648LL && v.integer<=2147483647LL,"Legacy binary int is signed 32-bit");Word(out,quint32(v.integer),4);}
        else if(type==2) {Require(v.hexadecimal<=0xffffffffULL,"Legacy binary hex is unsigned 32-bit");Word(out,v.hexadecimal,8);}
        else if(type==3) {Require(std::numeric_limits<double>::is_iec559 && sizeof(double)==8 && std::isfinite(v.real),"Finite IEEE-754 double required");quint64 bits;std::memcpy(&bits,&v.real,8);Word(out,bits,8);}
        else if(type==4)Word(out,v.boolean ? 1 : 0,1);
        else if(type==5)Word(out,StringId(QString::fromStdString(v.text)),4);
        else throw std::runtime_error("Unexpected binary scalar type");
    }
    int ClassId(const Class &c,const QString &path) {
        const auto found=fClassPaths.constFind(path);
        if(found!=fClassPaths.constEnd()) {const auto &old=fClasses[found.value()];Require(old.type==c.type && old.unit==c.unit && old.range==c.range,"Inconsistent shared binary class metadata");return found.value();}
        Require(fClasses.size()<MaxItems,"Binary class count limit exceeded");const int id=fClasses.size();fClassPaths[path]=id;fClasses.append(c);
        Word(fClassBytes,StringId(c.name),4);Word(fClassBytes,c.type,1);Word(fClassBytes,StringId(c.unit),4);
        if(c.range.isEmpty())Word(fClassBytes,0,1);
        else {
            const auto range=GetDAQ::ParseConfigurationRange(c.range.toStdString());Word(fClassBytes,range.kind==ConfigurationRange::Interval ? 1 : 2,1);
            if(range.kind==ConfigurationRange::Enumeration) {Word(fClassBytes,range.values.size(),4);for(const auto &value:range.values)Value(fClassBytes,value,c.type);}
            else {
                ConfigurationValue empty;empty.type=ConfigurationValue::Type(c.type-1);
                Word(fClassBytes,range.lowerUnbounded ? 2 : (range.includeLower ? 0 : 1),1);Value(fClassBytes,range.lowerUnbounded ? empty : range.lower,c.type);
                Word(fClassBytes,range.upperUnbounded ? 2 : (range.includeUpper ? 0 : 1),1);Value(fClassBytes,range.upperUnbounded ? empty : range.upper,c.type);
            }
        }
        return id;
    }
    Node ReadNode(const XcfgDocument &document,int index,const QString &parent,int depth) {
        Require(depth<MaxDepth && ++fNodes<=MaxItems,"XML depth/node limit exceeded");const auto &source=document.Nodes()[index];Class c;c.name=source.name;
        Require(QRegularExpression("^[A-Za-z_][A-Za-z0-9_.-]*$").match(c.name).hasMatch(),"Binary cannot preserve unsupported XML names");
        Require(source.includedRoot<0,"Binary encoding requires self-contained XML");
        for(auto it=source.attributes.constBegin();it!=source.attributes.constEnd();++it)Require(it.key()=="id" || it.key()=="type" || it.key()=="unit" || it.key()=="range","Binary cannot preserve extra XML attributes");
        Node n;n.index=source.attributes.value("id");Require(!n.index.contains('[') && !n.index.contains(']') && !n.index.contains('/'),"Unsupported binary index delimiters");StringId(n.index);
        c.unit=source.attributes.value("unit");c.range=source.attributes.value("range");
        if(source.scalar) {Require(source.children.isEmpty(),"Scalar has child elements");const auto value=GetDAQ::ParseConfigurationValue(source.value.toStdString(),source.attributes.value("type").toStdString());c.type=unsigned(value.type)+1;n.value=QString::fromStdString(GetDAQ::NormalizeConfigurationValue(value));}
        else {Require(c.unit.isEmpty() && c.range.isEmpty(),"Object has scalar metadata");for(int child:source.children)n.children.append(ReadNode(document,child,parent+'/'+c.name,depth+1));}
        n.classId=ClassId(c,parent+'/'+c.name);return n;
    }
    void WriteNode(QByteArray &out,const Node &n) {
        Word(out,n.classId,4);Word(out,StringId(n.index),4);const auto type=fClasses[n.classId].type;
        if(type==0) {Require(n.children.size()<=65535,"Legacy binary child count exceeds 16-bit limit");Word(out,n.children.size(),2);for(const auto &child:n.children)WriteNode(out,child);}
        else Value(out,GetDAQ::ParseConfigurationValue(n.value.toStdString(),Type(type).toStdString()),type);
    }
    QStringList fStrings;QMap<QString,int> fStringIds,fClassPaths;QVector<Class> fClasses;QByteArray fClassBytes;int fNodes=0;
};
}
namespace XcfgBinaryCodec {
QByteArray Decode(const QByteArray &binary) { return Reader(binary).Xml(); }
QByteArray Encode(const QByteArray &xml) { return Writer().Encode(xml); }
}
