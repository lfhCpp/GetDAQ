// Test-only service generated from the historical GetBench ecc.hh contract.
// This translation unit and generated bindings are not product components.
#include "ecceccObject.h"
#include <fstream>

SOAP_NMAC struct Namespace namespaces[] = {
    { "SOAP-ENV", "http://schemas.xmlsoap.org/soap/envelope/", "http://www.w3.org/*/soap-envelope", NULL },
    { "SOAP-ENC", "http://schemas.xmlsoap.org/soap/encoding/", "http://www.w3.org/*/soap-encoding", NULL },
    { "xsi", "http://www.w3.org/2001/XMLSchema-instance", "http://www.w3.org/*/XMLSchema-instance", NULL },
    { "xsd", "http://www.w3.org/2001/XMLSchema", "http://www.w3.org/*/XMLSchema", NULL },
    // The old GetBench client and server both override this mapping.
    { "ecc", "urn:http://localhost:8083/", NULL, NULL },
    { NULL, NULL, NULL, NULL }
};

namespace {

void LogRequest(struct soap *soap, const char *operation,
                const std::string &configID = std::string(),
                const std::string &table = std::string())
{
    std::ofstream *requestLog = static_cast<std::ofstream *>(soap->user);
    if (!requestLog)
        return;
    *requestLog << "OPERATION " << operation << "\n";
    if (!configID.empty())
        *requestLog << "CONFIG_BEGIN\n" << configID << "\nTABLE_BEGIN\n" << table << "\n";
    requestLog->flush();
}

}

int ecc__ServerExit(struct soap *, ecc__Response &response)
{
    response.ErrorCode = 0;
    response.ErrorMessage.clear();
    return SOAP_OK;
}

int ecc__GetConfigIDs(struct soap *, ecc__ResponseText &response)
{
    response.ErrorCode = 0;
    response.ErrorMessage.clear();
    response.Text = "<ConfigIdSet/>";
    return SOAP_OK;
}

int ecc__Describe(struct soap *soap, std::string configID, std::string table,
                  ecc__ResponseText &response)
{
    LogRequest(soap, "Describe", configID, table);
    response.ErrorCode = 0;
    response.ErrorMessage.clear();
    response.Text = "<Get><State>1</State></Get>";
    return SOAP_OK;
}

#define GETDAQ_TEXT_OPERATION(name) \
    int ecc__##name(struct soap *soap, std::string configID, std::string table, ecc__ResponseText &response) \
    { LogRequest(soap, #name, configID, table); response.ErrorCode = 0; response.ErrorMessage.clear(); response.Text.clear(); return SOAP_OK; }
GETDAQ_TEXT_OPERATION(Prepare)
GETDAQ_TEXT_OPERATION(Configure)
GETDAQ_TEXT_OPERATION(Start)
GETDAQ_TEXT_OPERATION(Stop)
GETDAQ_TEXT_OPERATION(Pause)
GETDAQ_TEXT_OPERATION(Resume)
GETDAQ_TEXT_OPERATION(Breakup)
GETDAQ_TEXT_OPERATION(Undo)
GETDAQ_TEXT_OPERATION(Special)
#undef GETDAQ_TEXT_OPERATION

int ecc__GetState(struct soap *soap, ecc__ResponseState &response)
{
    LogRequest(soap, "GetState");
    response.ErrorCode = 0;
    response.ErrorMessage.clear();
    response.State = 4;
    response.Transition = 0;
    return SOAP_OK;
}

int main(int argc, char **argv)
{
    if (argc != 3)
        return 2;
    const int port = std::atoi(argv[1]);
    std::ofstream requestLog(argv[2], std::ios::binary | std::ios::trunc);
    if (!requestLog)
        return 3;

    eccService service;
    service.user = &requestLog;
    service.namespaces = namespaces;
    if (service.bind("127.0.0.1", port, 8) == SOAP_INVALID_SOCKET)
        return 4;
    std::cout << "READY " << port << std::endl;

    while (service.accept() != SOAP_INVALID_SOCKET) {
        const int result = service.serve();
        soap_destroy(&service);
        soap_end(&service);
        if (result != SOAP_OK && service.error < SOAP_STOP)
            return 5;
    }
    return 0;
}
