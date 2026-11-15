

#include <HwControl.ice>
module ecc
{
//____________________________________________________________________________//


interface ElectronicsControlCore;

sequence<mdaq::hw::server::NodeConfig> NodeConfigList;

struct SystemConfig
{
	NodeConfigList nodes;
};

exception NodeAlreadyExists
{
	string reason;
};

exception ReconfigFailure
{
	string reason;
};

//____________________________________________________________________________//


interface ElectronicsControlCore 
{	
	idempotent void test(string testString);
	long timeStamp();
	idempotent void createNode(mdaq::hw::server::NodeConfig config) throws NodeAlreadyExists;//["ami"]
	idempotent void getConfig(out NodeConfigList nodes);
	idempotent void reconfigureNode(mdaq::hw::server::NodeId id, mdaq::hw::server::NodeConfig config) throws ReconfigFailure;//["ami"]
	idempotent void removeNode(mdaq::hw::server::NodeId id);
};

//____________________________________________________________________________//

}; //ecc
