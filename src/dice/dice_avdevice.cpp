/* dice_avdevice.cpp
 * Copyright (C) 2007 by Pieter Palmers
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
#warning DICE support is currently useless

#include "dice/dice_avdevice.h"
#include "dice/dice_defines.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "libstreaming/AmdtpStreamProcessor.h"

#include "debugmodule/debugmodule.h"

#include <string>
#include <stdint.h>
#include <assert.h>
#include <netinet/in.h>
#include <libraw1394/csr.h>


namespace Dice {

IMPL_DEBUG_MODULE( DiceAvDevice, DiceAvDevice, DEBUG_LEVEL_VERBOSE );

// to define the supported devices
static VendorModelEntry supportedDeviceList[] =
{
    {0x00000000, 0x0000, "DICE VENDOR", "XXX"},
};

DiceAvDevice::DiceAvDevice( std::auto_ptr< ConfigRom >( configRom ),
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
{
    setDebugLevel( verboseLevel );
    
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Dice::DiceAvDevice (NodeID %d)\n",
                 nodeId );

}

DiceAvDevice::~DiceAvDevice()
{

}

ConfigRom&
DiceAvDevice::getConfigRom() const
{
    return *m_configRom;
}

bool
DiceAvDevice::probe( ConfigRom& configRom )
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
DiceAvDevice::discover()
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
DiceAvDevice::getSamplingFrequency( ) {
    return 0;
}

bool
DiceAvDevice::setSamplingFrequency( ESamplingFrequency samplingFrequency )
{

    return false;
}

bool DiceAvDevice::setId( unsigned int id) {
    debugOutput( DEBUG_LEVEL_VERBOSE, "Set id to %d...\n", id);
    m_id=id;
    return true;
}

void
DiceAvDevice::showDevice() const
{
    debugOutput(DEBUG_LEVEL_VERBOSE,
        "%s %s at node %d\n", m_model->vendor_name, m_model->model_name,
        m_nodeId);
}

bool
DiceAvDevice::prepare() {

    return true;
}

bool
DiceAvDevice::lock() {

    return true;
}


bool
DiceAvDevice::unlock() {

    return true;
}

int 
DiceAvDevice::getStreamCount() {
    return 0; // one receive, one transmit
}

Streaming::StreamProcessor *
DiceAvDevice::getStreamProcessorByIndex(int i) {

//    switch (i) {
//    case 0:
//        return m_receiveProcessor;
//    case 1:
//        return m_transmitProcessor;
//    default:
//        return NULL;
//    }
//    return 0;
    return NULL;
}

bool
DiceAvDevice::startStreamByIndex(int i) {

    // NOTE: this assumes that you have two streams
    switch (i) {
    case 0:
        // TODO: do the stuff that is nescessary to make the device
        // transmit a stream

        // Set the streamprocessor channel to the one obtained by 
        // the connection management
//        m_receiveProcessor->setChannel(m_iso_recv_channel);

        break;
    case 1:
        // TODO: do the stuff that is nescessary to make the device
        // receive a stream

        // Set the streamprocessor channel to the one obtained by 
        // the connection management
//        m_transmitProcessor->setChannel(m_iso_send_channel);

        break;
        
    default: // Invalid stream index
        return false;
    }

    return true;
}

bool
DiceAvDevice::stopStreamByIndex(int i) {

    // TODO: connection management: break connection
    // cfr the start function

    // NOTE: this assumes that you have two streams
    switch (i) {
    case 0:
        break;
    case 1:
        break;
        
    default: // Invalid stream index
        return false;
    }

    return true;
}

signed int DiceAvDevice::getIsoRecvChannel(void) {
    return m_iso_recv_channel;
}

signed int DiceAvDevice::getIsoSendChannel(void) {
    return m_iso_send_channel;
}

}
