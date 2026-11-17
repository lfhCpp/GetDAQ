// Loopback-only board fixture. Every unrequested method fails and is logged.
#include <Ice/Ice.h>
#include <mdaq/hw/Control.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>
std::string logPath;std::mutex logMutex;
void Log(const Ice::Current &current){std::lock_guard<std::mutex> lock(logMutex);std::ofstream(logPath,std::ios::app)<<current.operation<<" "<<int(current.encoding.major)<<"."<<int(current.encoding.minor)<<"\n";}
class MockDevice: public mdaq::hw::Device { public:
::std::string name(const ::Ice::Current& current) override {Log(current);throw std::runtime_error("Unexpected fixture method");}
::Ice::Long baseAddress(const ::Ice::Current& current) override {Log(current);throw std::runtime_error("Unexpected fixture method");}
::std::string registerAccess(const ::Ice::Current& current) override {Log(current);throw std::runtime_error("Unexpected fixture method");}
::Ice::Short registerWidth(const ::Ice::Current& current) override {Log(current);throw std::runtime_error("Unexpected fixture method");}
void addRegister(const ::mdaq::hw::RegisterConfig&, const ::Ice::Current& current) override {Log(current);throw std::runtime_error("Unexpected fixture method");}
void addRegisters(const ::mdaq::hw::RegisterConfigList&, const ::Ice::Current& current) override {Log(current);throw std::runtime_error("Unexpected fixture method");}
::Ice::Long readRegister(const ::std::string& name, const ::Ice::Current& current) override {Log(current);if(name=="hardwareVersion")return 5;if(name=="firmwareRelease")return 249;if(name=="firmwareDate")return 538380306;throw std::runtime_error("Unexpected register");}
::Ice::Long readField(const ::std::string& reg, const ::std::string& field, const ::Ice::Current& current) override {Log(current);if(reg=="pipeCtrl" && (field=="firmwareEnable" || field=="intEnable"))return 0;throw std::runtime_error("Unexpected field");}
void writeRegister(const ::std::string&, ::Ice::Long, const ::Ice::Current& current) override {Log(current);throw std::runtime_error("Unexpected fixture method");}
void writeField(const ::std::string&, const ::std::string&, ::Ice::Long, const ::Ice::Current& current) override {Log(current);throw std::runtime_error("Unexpected fixture method");}
void execBatch(const ::mdaq::hw::RegCmdSeq&, ::mdaq::hw::RegCmdSeq&, const ::Ice::Current& current) override {Log(current);throw std::runtime_error("Unexpected fixture method");}
void getDeviceDescription(::mdaq::hw::DeviceDescription&, const ::Ice::Current& current) override {Log(current);throw std::runtime_error("Unexpected fixture method");}
void getDeviceConfig(::mdaq::hw::DeviceConfig&, const ::Ice::Current& current) override {Log(current);throw std::runtime_error("Unexpected fixture method");}
void getListOfRegisters(::mdaq::hw::RegisterList&, const ::Ice::Current& current) override {Log(current);throw std::runtime_error("Unexpected fixture method");}
void getListOfFields(const ::std::string&, ::mdaq::hw::FieldList&, const ::Ice::Current& current) override {Log(current);throw std::runtime_error("Unexpected fixture method");}
void getRegisterDescription(const ::std::string&, ::mdaq::hw::RegisterDescription&, const ::Ice::Current& current) override {Log(current);throw std::runtime_error("Unexpected fixture method");}
void getRegisterConfig(const ::std::string&, ::mdaq::hw::RegisterConfig&, const ::Ice::Current& current) override {Log(current);throw std::runtime_error("Unexpected fixture method");}
void getFieldDescription(const ::std::string&, const ::std::string&, ::mdaq::hw::FieldDescription&, const ::Ice::Current& current) override {Log(current);throw std::runtime_error("Unexpected fixture method");}
void setVerbose(bool, const ::Ice::Current& current) override {Log(current);throw std::runtime_error("Unexpected fixture method");}
bool isVerbose(const ::Ice::Current& current) override {Log(current);throw std::runtime_error("Unexpected fixture method");}
};
class MockNode: public mdaq::hw::Node { public:
mdaq::hw::DevicePrx device;
void testConnectionToHardware(const ::Ice::Current& current) override {Log(current);throw std::runtime_error("Unexpected fixture method");}
void shutdown(const ::Ice::Current& current) override {Log(current);throw std::runtime_error("Unexpected fixture method");}
void reboot(const ::Ice::Current& current) override {Log(current);throw std::runtime_error("Unexpected fixture method");}
::std::string name(const ::Ice::Current& current) override {Log(current);return "CoBo[0]";}
void setName(const ::std::string&, const ::Ice::Current& current) override {Log(current);throw std::runtime_error("Unexpected fixture method");}
void create(const ::mdaq::hw::NodeConfig&, const ::Ice::Current& current) override {Log(current);throw std::runtime_error("Unexpected fixture method");}
void destroy(const ::Ice::Current& current) override {Log(current);throw std::runtime_error("Unexpected fixture method");}
void getNodeConfig(::mdaq::hw::NodeConfig&, const ::Ice::Current& current) override {Log(current);throw std::runtime_error("Unexpected fixture method");}
void getListOfDevices(::mdaq::hw::DeviceList& names, const ::Ice::Current& current) override {Log(current);names.push_back("ctrl");}
void getMapOfDevices(::mdaq::hw::DeviceMap&, const ::Ice::Current& current) override {Log(current);throw std::runtime_error("Unexpected fixture method");}
::mdaq::hw::DevicePrx findDevice(const ::std::string& name, const ::Ice::Current& current) override {Log(current);if(name!="ctrl")throw std::runtime_error("Unexpected device");return device;}
void removeDevice(const ::std::string&, const ::Ice::Current& current) override {Log(current);throw std::runtime_error("Unexpected fixture method");}
void removeAllDevices(const ::Ice::Current& current) override {Log(current);throw std::runtime_error("Unexpected fixture method");}
void addDevice(const ::mdaq::hw::DeviceDescription&, const ::Ice::Current& current) override {Log(current);throw std::runtime_error("Unexpected fixture method");}
};
int main(int argc,char **argv){if(argc!=3)return 2;logPath=argv[2];Ice::InitializationData init;init.properties=Ice::createProperties();auto communicator=Ice::initialize(init);auto adapter=communicator->createObjectAdapterWithEndpoints("BoardFixture","tcp -h 127.0.0.1 -p "+std::string(argv[1]));auto node=new MockNode;node->device=mdaq::hw::DevicePrx::uncheckedCast(adapter->add(new MockDevice,communicator->stringToIdentity("ctrl")));adapter->add(node,communicator->stringToIdentity("HwNode"));adapter->activate();std::cout<<"READY"<<std::endl;communicator->waitForShutdown();communicator->destroy();}
