#ifndef GETDAQ_CATALOGTRANSFER_H
#define GETDAQ_CATALOGTRANSFER_H
#include <string>
#include <functional>
#include <cstdint>
namespace GetDAQ {
// Offline, separate native bulk task. Never deletes a source or changes ECC/Run state.
typedef std::function<void(const std::string &,std::uint64_t)> CatalogTransferProgress;
std::string CopyReceiverCatalog(const std::string &source,const std::string &newDestination,
                                CatalogTransferProgress progress=CatalogTransferProgress(),const std::string &expectedSourceRequestKey=std::string());
// Reads the copied payload with bounded buffers; works without the original disk.
std::string VerifyTransferredCatalog(const std::string &destination);
// Explicit continuation: checks original source binding and every existing
// destination prefix before appending missing bytes. Never truncates a file.
std::string ResumeReceiverCatalog(const std::string &source,const std::string &destination,
                                  CatalogTransferProgress progress=CatalogTransferProgress(),const std::string &expectedSourceRequestKey=std::string());
// Cross-host archive copy. Explicit resume reuses the original source/root
// identity and appends only after remote prefixes are checked against source.
std::string SendReceiverCatalogOverSsh(const std::string &source,const std::string &destination,
                                       const std::string &remoteCatalogProgram,const std::string &remoteRoot,
                                       CatalogTransferProgress progress=CatalogTransferProgress());
std::string ResumeReceiverCatalogOverSsh(const std::string &source,const std::string &destination,
                                         const std::string &remoteCatalogProgram,const std::string &remoteRoot,
                                         CatalogTransferProgress progress=CatalogTransferProgress());
std::string SendReceiverCatalogStream(const std::string &source,int outputFd,int inputFd,
                                      CatalogTransferProgress progress=CatalogTransferProgress(),bool resume=false,
                                      const std::string &transport="openssh-stdio-v1");
// Remote OpenSSH command endpoint. Reads/writes only the framed archive stream
// on the supplied descriptors; destination root must already be private (0700).
std::string ReceiveReceiverCatalogStream(const std::string &destinationRoot,int inputFd,int outputFd,bool resume=false,
                                         const std::string &transport="openssh-stdio-v1",bool acceptEitherMode=false);
std::string SendReceiverCatalogOverTcp(const std::string &source,const std::string &address,unsigned int port,
                                       CatalogTransferProgress progress=CatalogTransferProgress(),bool resume=false);
void ListenReceiverCatalogOverTcp(const std::string &bindAddress,unsigned int port,const std::string &destinationRoot,
                                  const std::function<void(unsigned int)> &ready,bool once=false);
}
#endif
