/* motu_avdevice.cpp
 * Copyright (C) 2006 by Pieter Palmers
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

#include "motu/motu_avdevice.h"
#include "configrom.h"

#include "libfreebobavc/ieee1394service.h"
#include "libfreebobavc/avc_definitions.h"

#include "debugmodule/debugmodule.h"

#include "libstreaming/MotuStreamProcessor.h"
#include "libstreaming/MotuPort.h"

#include <string>
#include <stdint.h>
#include <assert.h>
#include <netinet/in.h>

namespace Motu {

IMPL_DEBUG_MODULE( MotuDevice, MotuDevice, DEBUG_LEVEL_NORMAL );

MotuDevice::MotuDevice( Ieee1394Service& ieee1394service,
                        int nodeId,
                        int verboseLevel )
    : m_1394Service( &ieee1394service )
    , m_nodeId( nodeId )
    , m_verboseLevel( verboseLevel )
    , m_id(0)
    , m_receiveProcessor ( 0 )
    , m_transmitProcessor ( 0 )
    
{
    if ( m_verboseLevel ) {
        setDebugLevel( DEBUG_LEVEL_VERBOSE );
    }
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Motu::MotuDevice (NodeID %d)\n",
                 nodeId );
    m_configRom = new ConfigRom( m_1394Service, m_nodeId );
    m_configRom->initialize();

}

MotuDevice::~MotuDevice()
{
	delete m_configRom;
}

ConfigRom&
MotuDevice::getConfigRom() const
{
    return *m_configRom;
}

bool
MotuDevice::discover()
{
	signed int i, is_motu_fw_audio;

	/* A list of all IEEE1394 Vendor IDs used by MOTU.  One would expect this to
	* include only one, but stranger things have happened in this world.  The
	* list is terminated with a 0xffffffff value.
	*/
	const unsigned int motu_vendor_ids[] = {
		VENDOR_MOTU,
		VENDOR_MOTU_TEST,
		0xffffffff,
	};
	
	/* A list of all valid IEEE1394 model IDs for MOTU firewire audio devices,
	* terminated by 0xffffffff.
	*/
	const unsigned int motu_fw_audio_model_ids[] = {
		MOTU_828mkII, MOTU_TRAVELER, MOTU_TEST,
		0xffffffff,
	};
	
	/* Find out if this device is one we know about */
	is_motu_fw_audio = i = 0;

	while ((motu_vendor_ids[i]!=0xffffffff) 
			&& (m_configRom->getVendorId() != motu_vendor_ids[i]))
		i++;

	if (motu_vendor_ids[i] != 0xffffffff) {
		/* Device is made by MOTU.  See if the model is one we know about */
		i = 0;

		while ((motu_fw_audio_model_ids[i]!=0xffffffff) 
				&& (m_configRom->getModelId() != motu_fw_audio_model_ids[i]))
			i++;

		if (motu_fw_audio_model_ids[i]!=0xffffffff)
			is_motu_fw_audio = 1;
	}

	if (is_motu_fw_audio) {
		debugOutput( DEBUG_LEVEL_VERBOSE, "found %s %s\n",
			m_configRom->getVendorName(), m_configRom->getModelName());
		return true;
	}

    return false;
}

int 
MotuDevice::getSamplingFrequency( ) {
    /*
     Implement the procedure to retrieve the samplerate here
    */
    // FIXME: 
    return 44100;

}

bool
MotuDevice::setSamplingFrequency( ESamplingFrequency samplingFrequency )
{
    /*
     Implement the procedure to set the samplerate here
    */

    quadlet_t new_rate=0;
	int supported=true;

	switch ( samplingFrequency ) {
    case eSF_22050Hz:
		supported=false;
        break;
    case eSF_24000Hz:
		supported=false;
        break;
    case eSF_32000Hz:
		supported=false;
        break;
    case eSF_44100Hz:
        new_rate = MOTU_BASE_RATE_44100 | MOTU_RATE_MULTIPLIER_1X;
        break;
    case eSF_48000Hz:
        new_rate = MOTU_BASE_RATE_48000 | MOTU_RATE_MULTIPLIER_1X;
        break;
    case eSF_88200Hz:
        new_rate = MOTU_BASE_RATE_44100 | MOTU_RATE_MULTIPLIER_2X;
        break;
    case eSF_96000Hz:
        new_rate = MOTU_BASE_RATE_48000 | MOTU_RATE_MULTIPLIER_2X;
        break;
    case eSF_176400Hz:
        new_rate = MOTU_BASE_RATE_44100 | MOTU_RATE_MULTIPLIER_4X;
        break;
    case eSF_192000Hz:
        new_rate = MOTU_BASE_RATE_48000 | MOTU_RATE_MULTIPLIER_4X;
        break;
    default:
        supported=false;
    }

	// update the register
	if(supported) {
 		quadlet_t value=ReadRegister(0x0B14);
		value &= ~(MOTU_RATE_MASK);
		value |= new_rate;

		if(WriteRegister(0x0B14,value) == 0) {
			supported=true;
		} else {
			supported=false;
		}
	}

    return supported;
}

bool MotuDevice::setId( unsigned int id) {
    debugOutput( DEBUG_LEVEL_VERBOSE, "Set id to %d...\n", id);
    m_id=id;
    return true;
}

void
MotuDevice::showDevice() const
{
    printf( "%s %s at node %d\n",
	        m_configRom->getVendorName().c_str(), 
	        m_configRom->getModelName().c_str(),
	        m_nodeId );

}

bool
MotuDevice::prepare() {

    debugOutput(DEBUG_LEVEL_NORMAL, "Preparing MotuDevice...\n" );

	m_receiveProcessor=new FreebobStreaming::MotuReceiveStreamProcessor(
	                         m_1394Service->getPort(),
	                         getSamplingFrequency());
	                         
	// the first thing is to initialize the processor
	// this creates the data structures
	if(!m_receiveProcessor->init()) {
		debugFatal("Could not initialize receive processor!\n");
		return false;
	
	}

    // now we add ports to the processor
    debugOutput(DEBUG_LEVEL_VERBOSE,"Adding ports to receive processor\n");
	
	// TODO: change this into something based upon device detection/configuration
	
    char *buff;
    FreebobStreaming::Port *p=NULL;
    
	// example of adding an audio port:
    asprintf(&buff,"dev%d_cap_%s",m_id,"myportnamehere");
    
    p=new FreebobStreaming::MotuAudioPort(
            buff,
            FreebobStreaming::Port::E_Capture, 
            0 // you can add all other port specific stuff you 
              // need to pass by extending MotuXXXPort and MotuPortInfo
    );
    
    free(buff);

    if (!p) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n",buff);
    } else {

        if (!m_receiveProcessor->addPort(p)) {
            debugWarning("Could not register port with stream processor\n");
            return false;
        } else {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Added port %s\n",buff);
        
        }
    }
    
	// example of adding an midi port:
    asprintf(&buff,"dev%d_cap_%s",m_id,"myportnamehere");
    
    p=new FreebobStreaming::MotuMidiPort(
            buff,
            FreebobStreaming::Port::E_Capture, 
            0 // you can add all other port specific stuff you 
              // need to pass by extending MotuXXXPort and MotuPortInfo
    );
    
    free(buff);

    if (!p) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n",buff);
    } else {

        if (!m_receiveProcessor->addPort(p)) {
            debugWarning("Could not register port with stream processor\n");
            return false;
        } else {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Added port %s\n",buff);
        
        }
    }
    
	// example of adding an control port:
    asprintf(&buff,"dev%d_cap_%s",m_id,"myportnamehere");
    
    p=new FreebobStreaming::MotuControlPort(
            buff,
            FreebobStreaming::Port::E_Capture, 
            0 // you can add all other port specific stuff you 
              // need to pass by extending MotuXXXPort and MotuPortInfo
    );
    
    free(buff);

    if (!p) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n",buff);
    } else {

        if (!m_receiveProcessor->addPort(p)) {
            debugWarning("Could not register port with stream processor\n");
            return false;
        } else {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Added port %s\n",buff);
        
        }
    }


	// do the same for the transmit processor
	m_transmitProcessor=new FreebobStreaming::MotuTransmitStreamProcessor(
	                         m_1394Service->getPort(),
	                         getSamplingFrequency());
	                         
	m_transmitProcessor->setVerboseLevel(getDebugLevel());
	
	if(!m_transmitProcessor->init()) {
		debugFatal("Could not initialize transmit processor!\n");
		return false;
	
	}

    // now we add ports to the processor
    debugOutput(DEBUG_LEVEL_VERBOSE,"Adding ports to transmit processor\n");
    
	// example of adding an audio port:
	// we change the naming and the direction to E_Playback
    asprintf(&buff,"dev%d_pbk_%s",m_id,"myportnamehere");
    
    p=new FreebobStreaming::MotuAudioPort(
            buff,
            FreebobStreaming::Port::E_Playback, 
            0 // you can add all other port specific stuff you 
              // need to pass by extending MotuXXXPort and MotuPortInfo
    );
    
    free(buff);

    if (!p) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n",buff);
    } else {

        if (!m_transmitProcessor->addPort(p)) {
            debugWarning("Could not register port with stream processor\n");
            return false;
        } else {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Added port %s\n",buff);
        
        }
    }
    
	// example of adding an midi port:
    asprintf(&buff,"dev%d_pbk_%s",m_id,"myportnamehere");
    
    p=new FreebobStreaming::MotuMidiPort(
            buff,
            FreebobStreaming::Port::E_Playback, 
            0 // you can add all other port specific stuff you 
              // need to pass by extending MotuXXXPort and MotuPortInfo
    );
    
    free(buff);

    if (!p) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n",buff);
    } else {

        if (!m_transmitProcessor->addPort(p)) {
            debugWarning("Could not register port with stream processor\n");
            return false;
        } else {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Added port %s\n",buff);
        
        }
    }
    
	// example of adding an control port:
    asprintf(&buff,"dev%d_pbk_%s",m_id,"myportnamehere");
    
    p=new FreebobStreaming::MotuControlPort(
            buff,
            FreebobStreaming::Port::E_Playback, 
            0 // you can add all other port specific stuff you 
              // need to pass by extending MotuXXXPort and MotuPortInfo
    );
    
    free(buff);

    if (!p) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n",buff);
    } else {

        if (!m_transmitProcessor->addPort(p)) {
            debugWarning("Could not register port with stream processor\n");
            return false;
        } else {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Added port %s\n",buff);
        
        }
    }
	
	return true;
}

int 
MotuDevice::getStreamCount() {
 	return 2; // one receive, one transmit
}

FreebobStreaming::StreamProcessor *
MotuDevice::getStreamProcessorByIndex(int i) {
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
MotuDevice::startStreamByIndex(int i) {

    // NOTE: this assumes that you have two streams
 	switch (i) {
	case 0:
        // TODO: do the stuff that is nescessary to make the device
        // transmit a stream


		// set the streamprocessor channel to the one obtained by 
		// the connection management
		// IMPORTANT!!
 		m_receiveProcessor->setChannel(1); // TODO: change the channel NR
 		break;
 	case 1:
        // TODO: do the stuff that is nescessary to make the device
        // receive a stream

		// set the streamprocessor channel to the one obtained by 
		// the connection management
		// IMPORTANT!!
  		m_transmitProcessor->setChannel(0); // TODO: change the channel NR
 		break;
 		
 	default: // invalid index
 		return -1;
 	}

	return 0;

}

int
MotuDevice::stopStreamByIndex(int i) {
	// TODO: connection management: break connection
	// cfr the start function

	return 0;
}




/* ======================================================================== */

unsigned int MotuDevice::ReadRegister(unsigned int reg) {
/*
 * Attempts to read the requested register from the MOTU.
 */

	quadlet_t quadlet;
	assert(m_1394Service);
	
	quadlet = 0;
	if (m_1394Service->read(m_nodeId, MOTU_BASE_ADDR+reg, 4, &quadlet) < 0) {
		debugError("Error doing motu read from register 0x%06x\n",reg);
	}


	return ntohl(quadlet);
}

signed int MotuDevice::WriteRegister(unsigned int reg, quadlet_t data) {
/*
 * Attempts to write the given data to the requested MOTU register.
 */

  unsigned int err = 0;

  data = htonl(data);

  if (m_1394Service->write(m_nodeId, MOTU_BASE_ADDR+reg, 4, &data) < 0) {
    err = 1;
    debugError("Error doing motu write to register 0x%06x\n",reg);
  }

  usleep(100);

  return (err==0)?0:-1;
}

}
