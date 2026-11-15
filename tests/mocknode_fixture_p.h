#ifndef GETDAQ_MOCKNODE_FIXTURE_P_H
#define GETDAQ_MOCKNODE_FIXTURE_P_H

#include <QTcpServer>
#include <QStringList>

namespace GetDAQTestFixtures {

class MockNode : public QTcpServer
{
public:
    int state = 1;
    int transition = 0;
    bool failStart = false;
    bool hangState = false;
    bool hangCatalog = false;
    bool deferState = false;
    bool deferStop = false;
    int catalogCount = 0;
    int replacementCatalogCount = 0;
    int replacementFinalizeCount = 0;
    QString replacementOperationId;
    QStringList soapActionHeaders;
    QString role;
    QStringList *trace;

    MockNode(QString role, QStringList *trace);
};

} // namespace GetDAQTestFixtures

#endif
