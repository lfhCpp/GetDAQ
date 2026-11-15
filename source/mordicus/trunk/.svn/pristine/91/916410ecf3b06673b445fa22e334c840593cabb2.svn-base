// =============================================================================
// ServerDevices.cpp
// Created On: 27 août 2009 15:07:57
// Author: fchateau
// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
// Developed at the CEA (Saclay, France), in the Irfu/Sedi/Lilas laboratory, by:
// Frederic Chateau (frederic.chateau@cea.fr)
// -----------------------------------------------------------------------------
//  This software is the property of CEA (Commissariat a l'Energie Atomique).
//  You may use it freely for internal and non commercial purposes. Any other
//  use is subject to a written authorization of a legal representative of CEA.
// =============================================================================

#include "mdaq/hw/server/DynDevice.hpp"
#include "mdaq/hw/server/DeviceFactory.h"
#include "mdaq/hw/MemStoragePolicy.hpp"
#include "mdaq/hw/NestedStoragePolicy.hpp"

using namespace mdaq::hw;
using namespace mdaq::hw::server;

BaseDynDevicePtr nested32bitsDeviceFactory()
{
	return BaseDynDevicePtr(new DynDevice<uint32_t, NestedStoragePolicy>);
}

REGISTER_DEVICE(nested32bitsDevice, sizeof(uint32_t), "Nest", nested32bitsDeviceFactory);

