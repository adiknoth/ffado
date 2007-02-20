/* rme_avdevice.cpp
 * Copyright (C) 2006 by Jonathan Woithe
 * Copyright (C) 2006,2007 by Pieter Palmers
 *
 * This file is part of FreeBob.
 *
 * FreeBob is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBob is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBob; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */
#warning RME support is currently useless (detection only)

#include "rme/rme_avdevice.h"
#include "configrom.h"

#include "libfreebobavc/ieee1394service.h"
#include "libfreebobavc/avc_definitions.h"

#include "debugmodule/debugmodule.h"

//#include "libstreaming/MotuStreamProcessor.h"
//#include "libstreaming/MotuPort.h"

#include "libutil/DelayLockedLoop.h"

#include <string>
#include <stdint.h>
#include <assert.h>
#include <netinet/in.h>

#include <libraw1394/csr.h>

namespace Rme {

IMPL_DEBUG_MODULE( RmeDevice, RmeDevice, DEBUG_LEVEL_NORMAL );

// to define the supported devices
static VendorModelEntry supportedDeviceList[] =
{
    {0x00000a35, 0x0001, "RME", "Fireface-800"},  // RME Fireface-800
};

RmeDevice::RmeDevice( std::auto_ptr< ConfigRom >( configRom ),
                    Ieee1394Service& ieee1394service,
                    int nodeId,
                    int verboseLevel )
    : m_configRom( configRom )
    , m_1394Service( &ieee1394service )
    , m_model( NULL )
    , m_nodeId( nodeId )
    , m_verboseLevel( verboseLevel )
    , m_id(0)
    , m_iso_recv_channel ( -1 )
    , m_iso_send_channel ( -1 )
    , m_bandwidth ( -1 )
//    , m_receiveProcessor ( 0 )
//    , m_transmitProcessor ( 0 )
    
{
    setDebugLevel( verboseLevel );
    
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Rme::RmeDevice (NodeID %d)\n",
                 nodeId );

}

RmeDevice::~RmeDevice()
{
    // Free ieee1394 bus resources if they have been allocated
    if (m_1394Service != NULL) {
        if(m_1394Service->freeIsoChannel(m_iso_recv_channel)) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Could not free recv iso channel %d\n", m_iso_recv_channel);
            
        }
        if(m_1394Service->freeIsoChannel(m_iso_send_channel)) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Could not free send iso channel %d\n", m_iso_send_channel);
            
        }
    }
}

ConfigRom&
RmeDevice::getConfigRom() const
{
    return *m_configRom;
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

bool
RmeDevice::discover()
{
    unsigned int vendorId = m_configRom->getNodeVendorId();
    unsigned int modelId = m_configRom->getModelId();

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

bool
RmeDevice::setSamplingFrequency( ESamplingFrequency samplingFrequency )
{
/*
 * Set the RME device's samplerate.
 */
	if (samplingFrequency == eSF_48000Hz)
		return true;
	return false;
}

bool RmeDevice::setId( unsigned int id) {
	debugOutput( DEBUG_LEVEL_VERBOSE, "Set id to %d...\n", id);
	m_id=id;
	return true;
}

void
RmeDevice::showDevice() const
{
	debugOutput(DEBUG_LEVEL_VERBOSE,
		"%s %s at node %d\n", m_model->vendor_name, m_model->model_name,
		m_nodeId);
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

FreebobStreaming::StreamProcessor *
RmeDevice::getStreamProcessorByIndex(int i) {
    return NULL;
}

int
RmeDevice::startStreamByIndex(int i) {
    return -1;
}

int
RmeDevice::stopStreamByIndex(int i) {
    return -1;

}

signed int RmeDevice::getIsoRecvChannel(void) {
	return m_iso_recv_channel;
}

signed int RmeDevice::getIsoSendChannel(void) {
	return m_iso_send_channel;
}

signed int RmeDevice::getEventSize(unsigned int dir) {
    return 0;
}

}
