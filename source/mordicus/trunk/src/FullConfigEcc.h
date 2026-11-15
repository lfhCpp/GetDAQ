/*
 * FullConfig.h
 *
 *  Created on: Mar 17, 2010
 *      Author: jchavas
 */

/**
* @file FullConfig.h
* Class FullConfig translating a CompoundConfig configuration into a MDaq structure.
*
*/

#ifndef FullConfigEcc_h_INCLUDED
#define FullConfigEcc_h_INCLUDED

//#include "mdaq/hw/server/HwControl.h"
#include <ECC.h>
#include ICE_HEADER(Communicator.h)
#include ICE_HEADER(Initialize.h)
#include ICE_HEADER(Properties.h)
//#include ICE_HEADER(Exception)
#include "CCfg/CConfig.h"
#include "CCfg/Io/Document.h"

/**
 * @brief Translates a CompoundConfig description into a MDaq description
 *
 */
class FullConfigEcc : public ecc::SystemConfig//mdaq::hw::server::NodeConfig

{
public:
	FullConfigEcc();
	virtual ~FullConfigEcc();

	mdaq::hw::server::NodeConfig nodeConfigNull;
	mdaq::hw::server::DeviceConfig deviceConfigNull;
	mdaq::hw::server::RegisterConfig registerConfigNull;
	void translateSystem(CCfg::CConfig& cfg);
	mdaq::hw::server::NodeConfig& translateNode(CCfg::CConfig& cfg);
	mdaq::hw::server::DeviceConfig& translateDevice(CCfg::CConfig& cfg, mdaq::hw::server::NodeConfig& node);
	mdaq::hw::server::RegisterConfig& translateRegister(CCfg::CConfig& cfg, mdaq::hw::server::DeviceConfig& dev);
	void translateBitField(CCfg::CConfig& cfg, mdaq::hw::server::RegisterConfig& reg);
	void fromCCfg(CCfg::CConfig& cfg);
};

#endif /* FullConfig_h_INCLUDED */
