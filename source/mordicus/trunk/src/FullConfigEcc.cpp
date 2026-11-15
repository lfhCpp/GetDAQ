/*
 * FullConfig.cpp
 *
 *  Created on: Mar 17, 2010
 *      Author: jchavas pzhao
 */

/**
 * @file FullConfig.cpp
 * Implementation of the Class FullConfig.
 */

#include "FullConfigEcc.h"
#include <iostream>
#include <stdexcept>
#include <unistd.h>

using namespace mdaq::hw::server;
using std::cout;
using std::cerr;
using std::endl;
using std::runtime_error;

FullConfigEcc::FullConfigEcc()
{
}

FullConfigEcc::~FullConfigEcc()
{
}

void FullConfigEcc::translateSystem(CCfg::CConfig& cfg)
{
	if (cfg.getName() != "SystemEcc")
	{
		throw runtime_error("The root name should be 'SystemEcc' (with a non-empty index)");
	}

	if (cfg.getIndex() == "")
	{
		throw runtime_error("Index of System can't be empty");
	}

}

/**
 * @brief translates the node
 *
 * It looks for something like "Node[NameOfNode]"
 * and translates the different "ip", "port"
 * into the MDaq description
 *
 *
 * @param cfg the CompoundConfig configuration, prealably read from a file/database
 */
NodeConfig& FullConfigEcc::translateNode(CCfg::CConfig& cfg)
{
	int n = this->nodes.size();

	if (cfg.getIndex() == "")
	{
		throw runtime_error("Index of Node can't be empty");
	}
	if (cfg.getIndex() != "*")
	{
		this->nodes.resize(n+1);
		this->nodes[n].id.ip = cfg("ip").getStringValue();
		this->nodes[n].id.port = cfg("port").getIntValue();
		//this->nodes[n].name = cfg.getIndex();
		cout<<nodes[n].id.ip<<endl;
		return this->nodes[n];
	}
	else
	{
		return nodeConfigNull;
	}
}

/**
 * @brief translates the device
 *
 * It looks for something like "Device[NameOfDevice]"
 * and translates the different "baseAddress", "registerAccess", "registerWidth"
 * into the MDaq description
 *
 * @param cfg the CompoundConfig configuration
 */
DeviceConfig& FullConfigEcc::translateDevice(CCfg::CConfig& cfg,NodeConfig& node)
{
	int n = node.devices.size();
	if (cfg.getIndex() == "")
	{
		throw runtime_error("Index of Device can't be empty");
	}

	if (cfg.getIndex() != "*")
	{
		node.devices.resize(n + 1);
		node.devices[n].descr.name = cfg.getIndex();
		node.devices[n].descr.baseAddress = cfg("baseAddress").getHexValue();
		node.devices[n].descr.registerAccess = cfg("registerAccess").getStringValue();
		node.devices[n].descr.registerWidth = cfg("registerWidth").getIntValue();
		cout<< node.devices[n].descr.baseAddress<<endl;
		return node.devices[n];
	}
	else
	{
		return deviceConfigNull;
	}

}

/**
 * @brief translates the register
 *
 * It looks for something like "Device[NameOfDevice]"
 * and translates "name", "readOnly", "offset"
 * into the MDaq description
 *
 * @param cfg The CompoundConfig configuration
 * @param dev The DeviceConfig&, which is the structure of the parent device inside MDaq
 */
RegisterConfig& FullConfigEcc::translateRegister(CCfg::CConfig& cfg, DeviceConfig& dev)
{
	int n = dev.registers.size();

	if (cfg.getIndex() == "")
	{
		throw runtime_error("Index of Register can't be empty");
	}

	if (cfg.getIndex() != "*")
	{
		dev.registers.resize(n + 1);
		dev.registers[n].descr.name = cfg.getIndex();
		dev.registers[n].descr.readOnly = false;
		dev.registers[n].descr.offset = cfg("offset").getHexValue();

		return dev.registers[n];
	}
	else
	{
		return registerConfigNull;
	}
}

/**
 * @brief translates the BitField
 *
 * It looks for something like "BitField[NameOfBitField]"
 * and translates "name", "offset", "width"
 * into the MDaq description of the BitField
 *
 * @param cfg The CompoundConfig configuration
 * @param reg The RegisterConfig&, which is the structure of the parent register inside MDaq
 */
void FullConfigEcc::translateBitField(CCfg::CConfig& cfg, RegisterConfig& reg)
{
	int n = reg.fields.size();

	if (cfg.getIndex() == "")
	{
		throw runtime_error("Index of BitField can't be empty");
	}

	if (cfg.getIndex() != "*")
	{
		reg.fields.resize(n + 1);
		reg.fields[n].name = cfg.getIndex();
		reg.fields[n].offset = cfg("offset").getIntValue();
		reg.fields[n].width = cfg("width").getIntValue();
	}
}

/**
 * @brief translates from the CompoundConfig structure to the MDaq structure
 *
 * It expects a hierarchical tree Node->Device->Register->BitField.
 * Example:
 * -# Node[CoBo]
 *  -ip
 *  -port
 * -# Device[AsAd0]
 *  - registerAccess
 *  - registerWidth
 *  - baseAddress
 * -# Register[led]
 *  - offset
 * -# BitField[l0]
 *  - offset
 *  - width
 *  - readOnly
 *
 * @param cfg The CompoundConfig configuration
 */
void FullConfigEcc::fromCCfg(CCfg::CConfig& cfg)
{
	translateSystem(cfg);
	CCfg::CConfig::Iterator iNod = cfg.iterate("Node");
	while (iNod.hasNext())
	{
		CCfg::CConfig cfgNod = iNod.next();
		NodeConfig& node = translateNode(cfgNod);
		if (node != nodeConfigNull)
		{
			CCfg::CConfig::Iterator iDev = cfg.iterate("Device");
			cout<<"Test***********************Test  "<<iDev.hasNext()<<endl;
			while (iDev.hasNext())
			{
				CCfg::CConfig cfgDev = iDev.next();
				DeviceConfig& dev = translateDevice(cfgDev,node);
				if (dev != deviceConfigNull)
				{
					CCfg::CConfig::Iterator iReg = cfgDev.iterate("Register");
					while (iReg.hasNext())
					{
						CCfg::CConfig cfgReg = iReg.next();
						RegisterConfig& reg = translateRegister(cfgReg, dev);
						if (reg != registerConfigNull)
						{
							CCfg::CConfig::Iterator iField = cfgReg.iterate("BitField");
							while (iField.hasNext())
							{
								CCfg::CConfig cfgField = iField.next();
								translateBitField(cfgField, reg);
							}
						}
					}
				}
			}
		}
	}
	if (this->nodes.size() == 0)
	{
		throw runtime_error("There should be at least one node");
	}
}
