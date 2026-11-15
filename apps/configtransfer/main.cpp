#include "configurationrepositoryclient.h"
#include <stdexcept>
#include "selectionfile.h"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QTextStream>
#include <QJsonDocument>
#include <QTimer>
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);QCommandLineParser parser;parser.addHelpOption();
    parser.addOption({"host","Canonical IPv4 configuration server address.","address","127.0.0.1"});parser.addOption({"port","Configuration server port.","number","24002"});
    parser.addOption({"credential","Private credential file, never a token value.","path"});parser.addOption({"repository-id","Reviewed repository SHA-256 identity.","sha256"});
    parser.addPositionalArgument("command","identity, list, upload or selection");parser.addPositionalArgument("arguments","upload MANIFEST; selection VERSION_SHA NEW_SELECTION.json","[arguments...]");parser.process(app);
    const auto args=parser.positionalArguments();if (args.isEmpty() || !parser.isSet("credential")) {QTextStream(stderr)<<"Supply command and credential file\n";return 2;}
    try {
        bool ok=false;ConfigurationRepositoryEndpoint endpoint;endpoint.address=parser.value("host");endpoint.port=parser.value("port").toInt(&ok);endpoint.credentialFile=parser.value("credential");endpoint.repositoryId=parser.value("repository-id");
        if (!ok) throw std::invalid_argument("Invalid repository port");
        ConfigurationRepositoryClient client(endpoint);
        const auto finish=[&](const QJsonObject &result,const QString &error) {
            if (error.isEmpty() && args.size()==3 && args[0]=="selection") {
                const auto bytes=client.SelectionBytes();QString saveError;
                if (bytes.isEmpty() || !PublishNewSelectionFile(args[2],bytes,&saveError)) {
                    QTextStream(stderr)<<"Cannot save new read-only selection; existing files are never replaced";
                    if (!saveError.isEmpty()) QTextStream(stderr)<<": "<<saveError;
                    QTextStream(stderr)<<'\n';app.exit(1);return;
                }
                QTextStream(stdout)<<QString::fromUtf8(bytes);app.exit(0);return;
            }
            if (error.isEmpty()) QTextStream(stdout)<<QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Indented));else QTextStream(stderr)<<error<<'\n';app.exit(error.isEmpty() ? 0 : 1);
        };
        QObject::connect(&client,&ConfigurationRepositoryClient::IdentitySignal,&app,finish);QObject::connect(&client,&ConfigurationRepositoryClient::VersionsSignal,&app,finish);QObject::connect(&client,&ConfigurationRepositoryClient::FinishedSignal,&app,finish);
        QTimer::singleShot(0,&app,[&] {
            bool started=false;
            if (args.size()==1 && args[0]=="identity") started=client.QueryIdentity();
            else if (args.size()==1 && args[0]=="list") started=client.QueryVersions();
            else if (args.size()==2 && args[0]=="upload") started=client.Upload(args[1]);
            else if (args.size()==3 && args[0]=="selection") started=client.QueryVersion(args[1]);
            else {QTextStream(stderr)<<"Use identity, list, upload manifest, or selection VERSION_SHA NEW_FILE\n";app.exit(2);return;}
            if (!started) app.exit(1);
        });return app.exec();
    } catch (const std::exception &e) {QTextStream(stderr)<<e.what()<<'\n';return 1;}
}
