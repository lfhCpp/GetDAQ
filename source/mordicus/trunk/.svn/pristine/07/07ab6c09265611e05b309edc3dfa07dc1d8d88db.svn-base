/*
 * ElectronicControlImpl.h
 *
 *  Created on: Jun 9, 2010
 *      Author: pzhao
 */

#ifndef ELECTRONICS_CONTROL_CORE_IMPL_H
#define ELECTRONICS_CONTROL_CORE_IMPL_H

#include <Ice/Ice.h>
#include "ECC.h"
//#include "mdaq/hw/server/HwControl.h"
using namespace mdaq::hw::server;
#include <map>
#include <string>

namespace ecc {

class ElectronicsControlCoreImpl : public ElectronicsControlCore
{

private:
	HardwareNodePrx findNode(const NodeId& nodeId);
	static std::string makeNodeIdString(const NodeId& nodeId);
	static Ice::Identity makeNodeIdentity(const NodeId& nodeId);
	long _timestamp;
	//SystemConfig systemConfig;
	typedef std::map< NodeId, HardwareNodePrx > NodeMap;
	NodeMap _nodes;

public:
	void test(const std::string& testString,const Ice::Current& current);
	ElectronicsControlCoreImpl();
	Ice::Long timeStamp(const Ice::Current& current);
	void createNode(const NodeConfig& config,const ::Ice::Current& current);//["ami"]
	void getConfig(NodeConfigList& nodes,const ::Ice::Current& current);
	void reconfigureNode(const NodeId& nodeId,const NodeConfig& config, const ::Ice::Current& current);//["ami"]
	void removeNode(const NodeId& nodeId,const ::Ice::Current& current);


};
}

#endif /* ELECTRONICS_CONTROL_CORE_IMPL_H */
