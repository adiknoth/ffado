/* bounce_avdevice.cpp
 * Copyright (C) 2006 by Pieter Palmers
 * Copyright (C) 2006 by Daniel Wagner
 *
 * This file is part of FreeBoB.
 *
 * FreeBoB is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBoB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBoB; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */

#ifdef ENABLE_BOUNCE

#include "bounce/bounce_avdevice.h"
#include "configrom.h"

#include "libfreebobavc/avc_plug_info.h"
#include "libfreebobavc/avc_extended_plug_info.h"
#include "libfreebobavc/avc_subunit_info.h"
#include "libfreebobavc/avc_extended_stream_format.h"
#include "libfreebobavc/avc_serialize.h"
#include "libfreebobavc/ieee1394service.h"
#include "libfreebobavc/avc_definitions.h"

#include "debugmodule/debugmodule.h"

#include <iostream>
#include <sstream>
#include <stdint.h>

#include <string>
#include <netinet/in.h>

namespace Bounce {

IMPL_DEBUG_MODULE( BounceDevice, BounceDevice, DEBUG_LEVEL_VERBOSE );


BounceDevice::BounceDevice( std::auto_ptr< ConfigRom >( configRom ),
                            Ieee1394Service& ieee1394service,
                            int nodeId,
                            int verboseLevel )
    : m_configRom( configRom )
    , m_1394Service( &ieee1394service )
    , m_nodeId( nodeId )
    , m_verboseLevel( verboseLevel )
    , m_samplerate (44100)
    , m_id(0)
    , m_receiveProcessor ( 0 )
    , m_receiveProcessorBandwidth ( -1 )
    , m_transmitProcessor ( 0 )
    , m_transmitProcessorBandwidth ( -1 )
{
    setDebugLevel( verboseLevel );

    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Bounce::BounceDevice (NodeID %d)\n",
                 nodeId );
}

BounceDevice::~BounceDevice()
{

}

ConfigRom&
BounceDevice::getConfigRom() const
{
    return *m_configRom;
}

struct VendorModelEntry {
    unsigned int vendor_id;
    unsigned int model_id;
};

static VendorModelEntry supportedDeviceList[] =
{
//     {0x0000, 0x000000},
};

bool
BounceDevice::probe( ConfigRom& configRom )
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
BounceDevice::discover()
{
// 	unsigned int resp_len=0;
// 	quadlet_t request[6];
// 	quadlet_t *resp;

    debugOutput( DEBUG_LEVEL_VERBOSE, "Discovering...\n" );

	std::string vendor=std::string(FREEBOB_BOUNCE_SERVER_VENDORNAME);
	std::string model=std::string(FREEBOB_BOUNCE_SERVER_MODELNAME);

	if (!(m_configRom->getVendorName().compare(0,vendor.length(),vendor,0,vendor.length())==0)
	    || !(m_configRom->getModelName().compare(0,model.length(),model,0,model.length())==0)) {
		return false;
	}
/*
// AVC1394_COMMAND_INPUT_PLUG_SIGNAL_FORMAT
	request[0] = htonl( AVC1394_CTYPE_STATUS | (AVC1394_SUBUNIT_TYPE_FREEBOB_BOUNCE_SERVER << 19) | (0 << 16)
			| AVC1394_COMMAND_INPUT_PLUG_SIGNAL_FORMAT | 0x00);

	request[1] =  0xFFFFFFFF;
        resp = m_1394Service->transactionBlock( m_nodeId,
                                                       request,
                                                       2,
		                                               &resp_len );
// 	hexDump((unsigned char *)request,6*4);
	if(resp) {
		char *buffer=(char *)&resp[1];
		resp[resp_len-1]=0;
		xmlDescription=buffer;
// 		hexDump((unsigned char *)resp,6*4);
	}
*/
	return true;
}

int BounceDevice::getSamplingFrequency( ) {
    return m_samplerate;
}

bool BounceDevice::setSamplingFrequency( ESamplingFrequency samplingFrequency ) {
    int retval=convertESamplingFrequency( samplingFrequency );
    if (retval) {
        m_samplerate=retval;
        return true;
    } else return false;
}

bool BounceDevice::setId( unsigned int id) {
    debugOutput( DEBUG_LEVEL_VERBOSE, "Set id to %d...\n", id);
    m_id=id;
    return true;
}

void
BounceDevice::showDevice() const
{
    debugOutput(DEBUG_LEVEL_NORMAL, "\nI am the bouncedevice, the bouncedevice I am...\n" );
    debugOutput(DEBUG_LEVEL_NORMAL, "Vendor            :  %s\n", m_configRom->getVendorName().c_str());
    debugOutput(DEBUG_LEVEL_NORMAL, "Model             :  %s\n", m_configRom->getModelName().c_str());
    debugOutput(DEBUG_LEVEL_NORMAL, "Node              :  %d\n", m_nodeId);
    debugOutput(DEBUG_LEVEL_NORMAL, "GUID              :  0x%016llX\n", m_configRom->getGuid());
    debugOutput(DEBUG_LEVEL_NORMAL, "AVC test response :  %s\n", xmlDescription.c_str());
    debugOutput(DEBUG_LEVEL_NORMAL, "\n" );
}

bool
BounceDevice::addXmlDescription( xmlNodePtr deviceNode )
{

    return false;

}

#define BOUNCE_NR_OF_CHANNELS 2

bool
BounceDevice::addPortsToProcessor(
	FreebobStreaming::StreamProcessor *processor,
	FreebobStreaming::AmdtpAudioPort::E_Direction direction) {

    debugOutput(DEBUG_LEVEL_VERBOSE,"Adding ports to processor\n");

    int i=0;
    for (i=0;i<BOUNCE_NR_OF_CHANNELS;i++) {
        char *buff;
        asprintf(&buff,"dev%d%s_Port%d",m_id,direction==FreebobStreaming::AmdtpAudioPort::E_Playback?"p":"c",i);

        FreebobStreaming::Port *p=NULL;
        p=new FreebobStreaming::AmdtpAudioPort(
                buff,
                direction,
                // \todo: streaming backend expects indexing starting from 0
                // but bebob reports it starting from 1. Decide where
                // and how to handle this (pp: here)
                i,
                0,
                FreebobStreaming::AmdtpPortInfo::E_MBLA,
                0
        );

        if (!p) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n",buff);
        } else {

            if (!processor->addPort(p)) {
                debugWarning("Could not register port with stream processor\n");
                free(buff);
                return false;
            } else {
                debugOutput(DEBUG_LEVEL_VERBOSE, "Added port %s\n",buff);

            }
        }

        free(buff);

     }

	return true;
}

bool
BounceDevice::prepare() {

    debugOutput(DEBUG_LEVEL_NORMAL, "Preparing BounceDevice...\n" );

	m_receiveProcessor=new FreebobStreaming::AmdtpReceiveStreamProcessor(
	                         m_1394Service->getPort(),
	                         m_samplerate,
	                         BOUNCE_NR_OF_CHANNELS);

	if(!m_receiveProcessor->init()) {
		debugFatal("Could not initialize receive processor!\n");
		return false;

	}

 	if (!addPortsToProcessor(m_receiveProcessor,
 		FreebobStreaming::AmdtpAudioPort::E_Capture)) {
 		debugFatal("Could not add ports to processor!\n");
 		return false;
 	}

	// do the transmit processor
	m_transmitProcessor=new FreebobStreaming::AmdtpTransmitStreamProcessor(
	                         m_1394Service->getPort(),
	                         m_samplerate,
	                         BOUNCE_NR_OF_CHANNELS);

	m_transmitProcessor->setVerboseLevel(getDebugLevel());

	if(!m_transmitProcessor->init()) {
		debugFatal("Could not initialize transmit processor!\n");
		return false;

	}

 	if (!addPortsToProcessor(m_transmitProcessor,
 		FreebobStreaming::AmdtpAudioPort::E_Playback)) {
 		debugFatal("Could not add ports to processor!\n");
 		return false;
 	}

	return true;
}

int
BounceDevice::getStreamCount() {
 	return 2; // one receive, one transmit
}

FreebobStreaming::StreamProcessor *
BounceDevice::getStreamProcessorByIndex(int i) {
	switch (i) {
	case 0:
		return m_receiveProcessor;
	case 1:
 		return m_transmitProcessor;
	default:
		return NULL;
	}
	return 0;
}

int
BounceDevice::startStreamByIndex(int i) {
// 	int iso_channel=0;
// 	int plug=0;
// 	int hostplug=-1;
//
 	switch (i) {
	case 0:
// 		// do connection management: make connection
// 		iso_channel = iec61883_cmp_connect(
// 			m_1394Service->getHandle(),
// 			m_nodeId | 0xffc0,
// 			&plug,
// 			raw1394_get_local_id (m_1394Service->getHandle()),
// 			&hostplug,
// 			&m_receiveProcessorBandwidth);
//
// 		// set the channel obtained by the connection management
 		m_receiveProcessor->setChannel(1);
 		break;
 	case 1:
// 		// do connection management: make connection
// 		iso_channel = iec61883_cmp_connect(
// 			m_1394Service->getHandle(),
// 			raw1394_get_local_id (m_1394Service->getHandle()),
// 			&hostplug,
// 			m_nodeId | 0xffc0,
// 			&plug,
// 			&m_transmitProcessorBandwidth);
//
// 		// set the channel obtained by the connection management
// // 		m_receiveProcessor2->setChannel(iso_channel);
  		m_transmitProcessor->setChannel(0);
 		break;
 	default:
 		return -1;
 	}

	return 0;

}

int
BounceDevice::stopStreamByIndex(int i) {
	// do connection management: break connection

// 	int plug=0;
// 	int hostplug=-1;
//
// 	switch (i) {
// 	case 0:
// 		// do connection management: break connection
// 		iec61883_cmp_disconnect(
// 			m_1394Service->getHandle(),
// 			m_nodeId | 0xffc0,
// 			plug,
// 			raw1394_get_local_id (m_1394Service->getHandle()),
// 			hostplug,
// 			m_receiveProcessor->getChannel(),
// 			m_receiveProcessorBandwidth);
//
// 		break;
// 	case 1:
// 		// do connection management: break connection
// 		iec61883_cmp_disconnect(
// 			m_1394Service->getHandle(),
// 			raw1394_get_local_id (m_1394Service->getHandle()),
// 			hostplug,
// 			m_nodeId | 0xffc0,
// 			plug,
// 			m_transmitProcessor->getChannel(),
// 			m_transmitProcessorBandwidth);
//
// 		// set the channel obtained by the connection management
// // 		m_receiveProcessor2->setChannel(iso_channel);
// 		break;
// 	default:
// 		return 0;
// 	}

	return 0;
}

} // namespace

#endif // #ifdef ENABLE_BOUNCE
