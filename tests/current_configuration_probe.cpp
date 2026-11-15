// Independent executable boundary for legacy-vs-current integration tests.
// Uses the production reader and scalar/range core, with no legacy dependencies.
#include "xcfgdocument.h"
#include "configurationrange.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <exception>

int main(int argc,char** argv)
{
    QCoreApplication application(argc,argv);
    const QStringList args=application.arguments();
    if (args.size()<3) return 2;
    XcfgDocument document;
    QString error;
    if (!document.Load(args[1],&error)) {
        QTextStream(stdout)<<QJsonDocument(QJsonObject{{"loaded",false},{"error",error}}).toJson(QJsonDocument::Compact)<<'\n';
        return 1;
    }
    QJsonArray queries;
    for (int i=2;i<args.size();++i) {
        QJsonObject query{{"path",args[i]}};
        const int id=document.Lookup(args[i],&error);
        if (id<0) { query["ok"]=false; query["error"]=error; }
        else if (!document.Nodes()[id].scalar) { query["ok"]=true; query["object"]=true; }
        else {
            XcfgDocument::Node node;
            XcfgDocument::ScalarProperties properties;
            if (!document.ResolveScalar(args[i],&node,&properties,&error)) {
                query["ok"]=false; query["error"]=error;
            } else try {
                const auto value=GetDAQ::ParseConfigurationValue(node.value.toStdString(),node.attributes.value("type").toStdString());
                query["ok"]=true; query["object"]=false;
                query["type"]=QString::fromStdString(GetDAQ::ConfigurationValueType(value));
                query["value"]=QString::fromStdString(GetDAQ::NormalizeConfigurationValue(value));
                query["unit"]=properties.unit;
                query["range"]=properties.range;
                query["in_range"]=properties.range.isEmpty() || GetDAQ::ParseConfigurationRange(properties.range.toStdString()).Contains(value);
            } catch (const std::exception& failure) {
                query["ok"]=false; query["error"]=QString::fromUtf8(failure.what());
            }
        }
        queries.append(query);
    }
    QTextStream(stdout)<<QJsonDocument(QJsonObject{{"loaded",true},{"queries",queries}}).toJson(QJsonDocument::Compact)<<'\n';
    return 0;
}
