/*
 * ElectronicControlImpl.cpp
 *
 *  Created on: Jun 9, 2010
 *      Author: pzhao
 */

#include "ElectronicsControlCoreImpl.h"
#include "HardwareNodeImpl.h"
#include "DeviceImpl.h"
#include <Ice/ObjectAdapter.h>
#include <algorithm>
#include <sstream>
#include <stdlib.h>
#include <stdexcept>
#include <iostream>
#include <vector>

using namespace mdaq::hw::server;
using namespace std;

namespace ecc {

ElectronicsControlCoreImpl::ElectronicsControlCoreImpl()
{

}

Ice::Long ElectronicsControlCoreImpl::timeStamp(const Ice::Current& current)
{
	return _timestamp;
}

void ElectronicsControlCoreImpl::createNode(const NodeConfig& config, const Ice::Current& current)
{

	if(not findNode(config.id))
	{
		HardwareNodePrx node = 0;

#ifdef SIMUL
		/*for(DeviceConfigList::const_iterator it = config.devices.begin(),
					end = config.devices.end(); it != end; ++it)
				{
					//it->descr.registerAccess = "Nest";
				}*/

		HardwareNodeImpl* nodeImpl = new HardwareNodeImpl;
		HardwareNodePtr nodePtr(nodeImpl);
		node = HardwareNodePrx::checkedCast(current.adapter->add(nodePtr, makeNodeIdentity(config.id)));
#else
		ostringstream proxyStr;
		proxyStr << "HardwareNode: default -h " << config.id.ip << " -p " << config.id.port;
		node = HardwareNodePrx::checkedCast(current.adapter->getCommunicator()->stringToProxy(proxyStr.str()));
#endif
		_nodes[config.id] = node;
		node->create(config);
		cout<<"Successfull creation of node: "+node->id().ip+":"<<node->id().port<<endl;

		//cout<<"Device:"+node->findDevice("device1")->registerAccess()<<endl;
	}
	else
	{
		ostringstream stream;
		stream << "Node " << config.id.ip << ":" << config.id.port << " has already existed!" <<endl;
			throw NodeAlreadyExists(stream.str());
	}
}

void ElectronicsControlCoreImpl::getConfig(NodeConfigList& nodes,const ::Ice::Current& current)
{
	NodeMap::iterator itNodeMap = _nodes.begin();
	if(itNodeMap != _nodes.end())
	{
		for(itNodeMap; itNodeMap != _nodes.end(); itNodeMap++)
		{
			NodeConfig nodeConfig ;//= new NodeConfig();
			nodeConfig.id = itNodeMap->first;
			{
				DeviceList deviceList;
				itNodeMap->second->enumerateDevices(deviceList);
				DeviceList::iterator itNameOfDevice = deviceList.begin();
				for(itNameOfDevice; itNameOfDevice != deviceList.end(); itNameOfDevice++)
				{
					DeviceConfig deviceConfig;// = new DeviceConfig();
					deviceConfig.descr.name = itNodeMap->second->findDevice(*itNameOfDevice)->name();
					deviceConfig.descr.baseAddress = itNodeMap->second->findDevice(*itNameOfDevice)->baseAddress();
					deviceConfig.descr.registerAccess = itNodeMap->second->findDevice(*itNameOfDevice)->registerAccess();
					deviceConfig.descr.registerWidth = itNodeMap->second->findDevice(*itNameOfDevice)->registerWidth();
					{
						RegisterList registerList;
						itNodeMap->second->findDevice(*itNameOfDevice)->getListOfRegisters(registerList);
						RegisterList::iterator itNameOfRegister = registerList.begin();
						for(itNameOfRegister; itNameOfRegister != registerList.end(); itNameOfRegister++)
						{
							RegisterConfig registerConfig;// = new RegisterConfig();
							itNodeMap->second->findDevice(*itNameOfDevice)->getRegisterDescription(*itNameOfRegister, registerConfig.descr);
							{
								FieldList fieldList;
								itNodeMap->second->findDevice(*itNameOfDevice)->getListOfFields(*itNameOfRegister,fieldList);
								FieldList::iterator itNameOfField = fieldList.begin();
								for(itNameOfField; itNameOfField != fieldList.end(); itNameOfField++)
								{
									FieldConfig fieldConfig;// = new FieldConfig();
									itNodeMap->second->findDevice(*itNameOfDevice)->getFieldDescription(*itNameOfRegister, *itNameOfField, fieldConfig);
									registerConfig.fields.push_back(fieldConfig);
								}
							}
							deviceConfig.registers.push_back(registerConfig);
						}
					}
					nodeConfig.devices.push_back(deviceConfig);
				}
			}
			//nodeConfig.devices = getDevices();
			nodes.push_back(nodeConfig);
		}
	}
	else
	{
		cout<<"No node existed."<<endl;
	}
	//cout<<"Success getListOfNodes"<<endl;

}

void ElectronicsControlCoreImpl::reconfigureNode(const NodeId& nodeId,const NodeConfig& config, const ::Ice::Current& current)//["ami"]
{
	HardwareNodePrx node = findNode(nodeId);
	if (node)
	{
		node->create(config);
		cout<<"Success reconfig"<<endl;	
	}
	else
	{
		cout<< "No node " << nodeId.ip <<":" << nodeId.port <<" exists yet!" <<endl;
	}
}

HardwareNodePrx ElectronicsControlCoreImpl::findNode(const NodeId& nodeId)
{
	HardwareNodePrx node ;
	NodeMap::iterator nodeIter = _nodes.find(nodeId);
	if(nodeIter != _nodes.end())
		node = nodeIter->second;
	cout<<"Test:"<<node<<endl;
	return node;
}

void ElectronicsControlCoreImpl::removeNode(const NodeId& nodeId,const ::Ice::Current& current)
{
	HardwareNodePrx node = findNode(nodeId);

	if (node)
	{
		node->removeAllDevices();
		node->destroy();
#ifdef SIMUL
		current.adapter->remove(makeNodeIdentity(nodeId));
#endif
		NodeMap::iterator nodeIter = _nodes.find(nodeId);
		_nodes.erase(nodeIter);
	}

	cout<<"Success removeNode"<<endl;

}

string ElectronicsControlCoreImpl::makeNodeIdString(const NodeId& nodeId)
{
	ostringstream label;
	label << nodeId.ip << ':' << nodeId.port;
	return label.str();
}

Ice::Identity ElectronicsControlCoreImpl::makeNodeIdentity(const NodeId& nodeId)
{
	Ice::Identity id;
	id.name  = makeNodeIdString(nodeId);
	cout<<"Success makeNodeIdentity"<<endl;
	return id;



}

void ElectronicsControlCoreImpl::test(const string& testString,const Ice::Current& current)
{
	cout << testString << endl;
}

}//ecc

