#include "datasupervisor.h"
#include "datasupervisor_p.h"
#include "receiverprocessposix_p.h"
#include "receivercycleevidence.h"
#include "controlprotocol.h"
#include "supervisorio.h"
#include "configurationsha256.h"
#include "systemexperimentconfiguration.h"
#include "receivercatalog.h"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <dirent.h>
#include <fcntl.h>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <sys/file.h>
#include <sys/statvfs.h>
#include <thread>
#include <unistd.h>
namespace GetDAQ {
using namespace DataSupervisorInternal;
namespace {
using namespace SupervisorIO;
using namespace ReceiverProcessInternal;
using Clock=std::chrono::steady_clock;
}

DataSupervisor::DataSupervisor(const DataSupervisorOptions &o) : fImpl(new Impl(o)) {}
DataSupervisor::~DataSupervisor() {}
DataSupervisorOptions DataSupervisor::Configuration() const { return fImpl->options; }
std::string DataSupervisor::PlanBytes() const { return fImpl->manifest; }
DataSupervisorStatus DataSupervisor::Status() const { std::lock_guard<std::mutex> lock(fImpl->statusMutex); return fImpl->snapshot; }

}
