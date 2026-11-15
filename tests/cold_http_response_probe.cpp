#include "httpclient.h"
#include "httpserver.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <QUrl>
#include <iostream>
#include <stdexcept>
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    try {
        HttpServer server;const QByteArray metadata(2*1024*1024,'x');int requests=0;
        QObject::connect(&server,&HttpServer::RequestSignal,[&](QString,QString,QByteArray,HttpConnection *c){++requests;c->Reply(200,metadata);});
        if(!server.listen(QHostAddress::LocalHost,0))throw std::runtime_error("Cannot bind local HTTP metadata fixture");
        QUrl url;url.setScheme("http");url.setHost("127.0.0.1");url.setPort(server.serverPort());url.setPath("/metadata");HttpClient client;
        const auto run=[&](int budget,bool success) {
            bool done=false,okay=false;
            client.Request(url,{},"",5000,[&](const QByteArray &body,const QString &error){okay=success ? error.isEmpty() && body==metadata : !error.isEmpty();done=true;},{},budget);
            QElapsedTimer timer;timer.start();while(!done){if(timer.elapsed()>6000)throw std::runtime_error("HTTP budget deadline");QCoreApplication::processEvents();QThread::msleep(1);}
            if(!okay)throw std::runtime_error("HTTP metadata response budget did not preserve bounded exact bytes");
        };
        run(8*1024*1024,true);run(1024*1024,false);
        if(requests!=2)throw std::runtime_error("Unexpected HTTP repeat");
        std::cout<<"PASS exact 2MiB metadata response and preserved ordinary 1MiB rejection; two GETs, no payload files\n";return 0;
    }catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
