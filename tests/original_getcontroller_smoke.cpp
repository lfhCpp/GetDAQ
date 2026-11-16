// Original GetController UI/workspace/plot test. No ECC, router or alarm server is started.
#include "get/GetTest.h"
#include "get/EccClient.h"
#include "mdaq/utl/Server.h"
#include "get/WorkspaceManager.h"
#include "get/ChannelMask.h"
#include "get/plot.h"
#include "mfm/FrameDictionary.h"
#include "mdaq/utl/ConsoleLoggingBackend.h"
#include "utl/Logging.h"
#include <qwt_plot_renderer.h>
#include <QApplication>
#include <QTemporaryDir>
#include <QSettings>
#include <QFileInfo>
#include <QDebug>
#include <stdexcept>
static void require(bool result,const char *message) {if(!result)throw std::runtime_error(message);}
class IdentityOnlyEcc : public Ice::Object {
public:
    bool ice_isA(const std::string &type,const Ice::Current &current) const override {
        return type==get::GetEcc::ice_staticId() || Ice::Object::ice_isA(type,current);
    }
};
int main(int argc,char **argv) {
    QApplication app(argc,argv);Q_INIT_RESOURCE(ConfigWidgets);Q_INIT_RESOURCE(GetController);
    QTemporaryDir temp;require(temp.isValid(),"temporary directory");
    QCoreApplication::setOrganizationName("GetDAQ-Original-Controller-Test");
    QCoreApplication::setApplicationName("OfflineSmoke");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path());
    try {
        require(argc>1,"frame-format file argument required");
        mfm::FrameDictionary::instance().addFormats(argv[1]);
        const auto path=(temp.path()+"/workspace.xcfg").toStdString();
        get::WorkspaceManager::createWorkspace(path);
        auto &manager=get::WorkspaceManager::instance();require(manager.loadWorkspace(path),"load workspace");
        manager.workspace()("Test","offline").create(CCfg::OBJECT_INST);
        manager.save();require(manager.loadWorkspace(path),"reopen workspace");
        require(manager.testNames().count("offline")==1,"saved test visible");
        const auto mask=get::ChannelMask::fromString("0-3,7,67");
        require(mask.size()==6 && mask.count(67) && mask.toString()=="0-3,7,67","channel mask");
        get::GetTest window;window.show();app.processEvents();
        require(window.isVisible(),"original main window");
        Plot graph("Offline waveform",nullptr);
        graph.addCurve("Channel 7","Cross",false,0,QVector<double>{0,1,2,3},QVector<double>{1,4,2,1});
        require(graph.size()==1,"original plot curve");
        QwtPlotRenderer renderer;const QString svg=temp.path()+"/waveform.svg";
        renderer.renderDocument(&graph,svg,QSizeF(120,80),85);
        require(QFileInfo(svg).size()>100,"SVG export");
        if(argc>2)require(window.grab().save(QString::fromLocal8Bit(argv[2])),"window screenshot");
        require(QMetaObject::invokeMethod(&window,"cleanUp",Qt::DirectConnection),"cleanup");
        // Separate server/client communicators force an actual loopback Ice handshake.
        // This identity-only servant cannot execute any hardware operation.
        Ice::InitializationData init;init.properties=Ice::createProperties();
        init.properties->setProperty("Ice.IPv6","0");
        auto server=Ice::initialize(init);
        auto adapter=server->createObjectAdapterWithEndpoints("OfflineSmoke","tcp -h 127.0.0.1 -p 0");
        Ice::Identity identity;identity.name="Ecc";adapter->add(new IdentityOnlyEcc,identity);adapter->activate();
        auto endpoint=Ice::TCPEndpointInfoPtr::dynamicCast(adapter->getEndpoints().front()->getInfo());
        require(bool(endpoint),"loopback endpoint");
        EccClient client("127.0.0.1:"+std::to_string(endpoint->port));
        require(bool(client.ecc()),"original ECC proxy checkedCast");client.ecc()->ice_ping();
        server->destroy();mdaq::utl::Server::ic()->destroy();
        qInfo()<<"PASS: original ECC checkedCast and ping to identity-only loopback servant";
        qInfo()<<"PASS: original formats / workspace create-save-reopen / test list / channel mask / main window / plot / SVG export";
        return 0;
    } catch(const std::exception &error) {qCritical()<<error.what();return 1;}
}
