/*
 * Copyright (C) 2005-2008 by Jonathan Woithe
 * Copyright (C) 2005-2008 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#warning RME support is currently useless (detection only)

#include "rme/rme_avdevice.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "debugmodule/debugmodule.h"

#include <string>
#include <stdint.h>
#include <assert.h>
#include <netinet/in.h>

#include <iostream>
#include <sstream>

#include <libraw1394/csr.h>

namespace Rme {

// to define the supported devices
static VendorModelEntry supportedDeviceList[] =
{
    {FW_VENDORID_RME, 0x0001, "RME", "Fireface-800"},  // RME Fireface-800
};

RmeDevice::RmeDevice( Ieee1394Service& ieee1394Service,
                      std::auto_ptr<ConfigRom>( configRom ))
    : FFADODevice( ieee1394Service, configRom )
    , m_model( NULL )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Rme::RmeDevice (NodeID %d)\n",
                 getConfigRom().getNodeId() );
}

RmeDevice::~RmeDevice()
{

}

bool
RmeDevice::probe( ConfigRom& configRom )
{
    unsigned int vendorId = configRom.getNodeVendorId();
    unsigned int modelId = configRom.getModelId();

    for ( unsigned int i = 0;
          i < ( sizeof( supportedDeviceList )/sizeof( VendorModelEntry ) );
          ++i )
    {
        if ( ( supportedDeviceList[i].vendor_id == vendorId )
             && ( supportedDeviceList[i].model_id == modelId )
           )
        {
            return true;
        }
    }

    return false;
}

FFADODevice *
RmeDevice::createDevice( Ieee1394Service& ieee1394Service,
                         std::auto_ptr<ConfigRom>( configRom ))
{
    return new RmeDevice(ieee1394Service, configRom );
}

bool
RmeDevice::discover()
{
    unsigned int vendorId = m_pConfigRom->getNodeVendorId();
    unsigned int modelId = m_pConfigRom->getModelId();

    for ( unsigned int i = 0;
          i < ( sizeof( supportedDeviceList )/sizeof( VendorModelEntry ) );
          ++i )
    {
        if ( ( supportedDeviceList[i].vendor_id == vendorId )
             && ( supportedDeviceList[i].model_id == modelId )
           )
        {
            m_model = &(supportedDeviceList[i]);
        }
    }

    if (m_model != NULL) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "found %s %s\n",
                m_model->vendor_name, m_model->model_name);
        return true;
    }

    return false;
}

int
RmeDevice::getSamplingFrequency( ) {
/*
 * Retrieve the current sample rate from the RME device.
 */
	return 48000;
}

int
RmeDevice::getConfigurationId()
{
    return 0;
}

bool
RmeDevice::setSamplingFrequency( int samplingFrequency )
{
/*
 * Set the RME device's samplerate.
 */
	if (samplingFrequency == 48000)
		return true;
	return false;
}

FFADODevice::ClockSourceVector
RmeDevice::getSupportedClockSources() {
    FFADODevice::ClockSourceVector r;
    return r;
}

bool
RmeDevice::setActiveClockSource(ClockSource s) {
    return false;
}

FFADODevice::ClockSource
RmeDevice::getActiveClockSource() {
    ClockSource s;
    return s;
}

bool
RmeDevice::lock() {

    return true;
}


bool
RmeDevice::unlock() {

    return true;
}

void
RmeDevice::showDevice()
{
	debugOutput(DEBUG_LEVEL_VERBOSE,
		"%s %s at node %d\n", m_model->vendor_name, m_model->model_name,
		getNodeId());
}

bool
RmeDevice::prepare() {

	debugOutput(DEBUG_LEVEL_NORMAL, "Preparing RmeDevice...\n" );

	return true;
}

int
RmeDevice::getStreamCount() {
 	return 0; // one receive, one transmit
}

Streaming::StreamProcessor *
RmeDevice::getStreamProcessorByIndex(int i) {
    return NULL;
}

bool
RmeDevice::startStreamByIndex(int i) {
    return false;
}

bool
RmeDevice::stopStreamByIndex(int i) {
    return false;

}

}
