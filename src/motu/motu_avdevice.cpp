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

char *motufw_modelname[] = {"[unknown]","828MkII", "Traveler"}; 

/* ======================================================================= */
/* Keep track of iso channels assigned to different MOTU devices.  This
 * is slightly simplistic at present since it assumes there are no other
 * users of iso channels on the firewire bus.  It does however allow
 * more than one MOTU interface to exist on the same bus.  The first MOTU
 * discovered will be allocated iso channels 0 (send) and 1 (receive) which
 * mirrors what the official driver appears to do.
 */
static signed int next_iso_recv_channel_num = 1;
static signed int next_iso_send_channel_num = 0;  
      
static signed int next_iso_recv_channel(void) {
/*
 * Returns the next available channel for ISO receive.  If there are no more
 * available -1 is returned.  Currently the odd channels (starting from 1)
 * are used for iso receive.  No provision is made to reuse previously
 * allocated channels in the event that the applicable interface has been
 * removed - this may change in future.
 */
	if (next_iso_recv_channel_num < 64) {
		next_iso_recv_channel_num+=2;
		return next_iso_recv_channel_num-2;
	}
	return -1;
}
static signed int next_iso_send_channel(void) {
/*
 * Returns the next available channel for ISO send.  If there are no more
 * available -1 is returned.  Currently the even channels (starting from 0)
 * are used for iso receive.  No provision is made to reuse previously
 * allocated channels in the event that the applicable interface has been
 * removed - this may change in future.
 */
	if (next_iso_send_channel_num < 64) {
		next_iso_send_channel_num+=2;
		return next_iso_send_channel_num-2;
	}
	return -1;
}
/* ======================================================================= */

MotuDevice::MotuDevice( Ieee1394Service& ieee1394service,
                        int nodeId,
                        int verboseLevel )
    : m_1394Service( &ieee1394service )
    , m_motu_model( MOTUFW_MODEL_NONE )
    , m_nodeId( nodeId )
    , m_verboseLevel( verboseLevel )
    , m_id(0)
    , m_iso_recv_channel ( -1 )
    , m_iso_send_channel ( -1 )
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
        // Find out if this device is one we know about
        if (m_configRom->getUnitSpecifierId() == MOTUFW_VENDOR_MOTU) {
                switch (m_configRom->getUnitVersion()) {
                    case MOTUFW_UNITVER_828mkII: 
                        m_motu_model = MOTUFW_MODEL_828mkII; 
                        break;
                    case MOTUFW_UNITVER_TRAVELER:
                        m_motu_model = MOTUFW_MODEL_TRAVELER;
                        break;
                }
        }
        if (m_motu_model != MOTUFW_MODEL_NONE) {
                debugOutput( DEBUG_LEVEL_VERBOSE, "found MOTU %s\n",
                        motufw_modelname[m_motu_model]);

		// Assign iso channels if not already done
		if (m_iso_recv_channel < 0)
			m_iso_recv_channel = next_iso_recv_channel();
		if (m_iso_send_channel < 0)
			m_iso_send_channel = next_iso_send_channel();
                return true;
	}
	return false;
}

int 
MotuDevice::getSamplingFrequency( ) {
    /*
     Implement the procedure to retrieve the samplerate here
    */
    quadlet_t q = ReadRegister(MOTUFW_REG_RATECTRL);
    int rate = 0;

    switch (q & ~MOTUFW_BASE_RATE_MASK) {
        case MOTUFW_BASE_RATE_44100:
            rate = 44100;
            break;
        case MOTUFW_BASE_RATE_48000:
            rate = 48000;
            break;
    }
    switch (q & ~MOTUFW_RATE_MULTIPLIER_MASK) {
        case MOTUFW_RATE_MULTIPLIER_2X:
            rate *= 2;
            break;
        case MOTUFW_RATE_MULTIPLIER_4X:
            rate *= 4;
            break;
    }
    return rate;
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
            new_rate = MOTUFW_BASE_RATE_44100 | MOTUFW_RATE_MULTIPLIER_1X;
            break;
	case eSF_48000Hz:
            new_rate = MOTUFW_BASE_RATE_48000 | MOTUFW_RATE_MULTIPLIER_1X;
            break;
        case eSF_88200Hz:
            new_rate = MOTUFW_BASE_RATE_44100 | MOTUFW_RATE_MULTIPLIER_2X;
            break;
        case eSF_96000Hz:
            new_rate = MOTUFW_BASE_RATE_48000 | MOTUFW_RATE_MULTIPLIER_2X;
            break;
        case eSF_176400Hz:
            new_rate = MOTUFW_BASE_RATE_44100 | MOTUFW_RATE_MULTIPLIER_4X;
            break;
        case eSF_192000Hz:
            new_rate = MOTUFW_BASE_RATE_48000 | MOTUFW_RATE_MULTIPLIER_4X;
            break;
        default:
            supported=false;
    }

    // update the register.  FIXME: there's more to it than this
    if (supported) {
        quadlet_t value=ReadRegister(MOTUFW_REG_RATECTRL);
        value &= ~(MOTUFW_BASE_RATE_MASK|MOTUFW_RATE_MULTIPLIER_MASK);
        value |= new_rate;
//        value |= 0x04000000;
        if (WriteRegister(MOTUFW_REG_RATECTRL, value) == 0) {
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
    printf( "MOTU %s at node %d\n",
        motufw_modelname[m_motu_model],
        m_nodeId );
}

bool
MotuDevice::prepare() {

	int samp_freq = getSamplingFrequency();
	enum EMotuOpticalMode optical_mode = getOpticalMode();

	debugOutput(DEBUG_LEVEL_NORMAL, "Preparing MotuDevice...\n" );

	m_receiveProcessor=new FreebobStreaming::MotuReceiveStreamProcessor(
	                         m_1394Service->getPort(), samp_freq);
	                         
	// the first thing is to initialize the processor
	// this creates the data structures
	if(!m_receiveProcessor->init()) {
		debugFatal("Could not initialize receive processor!\n");
		return false;
	}
	m_receiveProcessor->setVerboseLevel(getDebugLevel());

	// now we add ports to the processor
	debugOutput(DEBUG_LEVEL_VERBOSE,"Adding ports to receive processor\n");
	
	// TODO: change this into something based upon device detection/configuration
	char *buff;
	unsigned int i;
	FreebobStreaming::Port *p=NULL;

	// For now just add the 8 analog capture ports since they are always
	// present no matter what the device configuration is.
	for (i=0; i<8; i++) {
		asprintf(&buff,"dev%d_cap_Analog%d",m_id,i+1);
		p=new FreebobStreaming::MotuAudioPort(
			buff,
			FreebobStreaming::Port::E_Capture, 
			0, 0);

		if (!p) {
			debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n",buff);
		} else {
			if (!m_receiveProcessor->addPort(p)) {
				debugWarning("Could not register port with stream processor\n");
				free(buff);
				return false;
			} else {
				debugOutput(DEBUG_LEVEL_VERBOSE, "Added port %s\n",buff);
			}
		}
		free(buff);
	}

	// example of adding an midi port:
//    asprintf(&buff,"dev%d_cap_%s",m_id,"myportnamehere");
//    p=new FreebobStreaming::MotuMidiPort(
//            buff,
//            FreebobStreaming::Port::E_Capture, 
//            0 // you can add all other port specific stuff you 
//              // need to pass by extending MotuXXXPort and MotuPortInfo
//    );
//    free(buff);
//
//    if (!p) {
//        debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n",buff);
//    } else {
//        if (!m_receiveProcessor->addPort(p)) {
//            debugWarning("Could not register port with stream processor\n");
//            return false;
//        } else {
//            debugOutput(DEBUG_LEVEL_VERBOSE, "Added port %s\n",buff);
//        }
//    }
    
	// example of adding an control port:
//    asprintf(&buff,"dev%d_cap_%s",m_id,"myportnamehere");
//    p=new FreebobStreaming::MotuControlPort(
//            buff,
//            FreebobStreaming::Port::E_Capture, 
//            0 // you can add all other port specific stuff you 
//              // need to pass by extending MotuXXXPort and MotuPortInfo
//    );
//    free(buff);
//
//    if (!p) {
//        debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n",buff);
//    } else {
//
//        if (!m_receiveProcessor->addPort(p)) {
//            debugWarning("Could not register port with stream processor\n");
//            return false;
//        } else {
//            debugOutput(DEBUG_LEVEL_VERBOSE, "Added port %s\n",buff);
//        }
//    }

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

	// For now just add the 8 analog capture ports since they are always
	// present no matter what the device configuration is.  Note the
	// direction is now E_Playback.
	for (i=0; i<8; i++) {
		asprintf(&buff,"dev%d_pbk_Analog%d",m_id,i+1);

		p=new FreebobStreaming::MotuAudioPort(
			buff,
			FreebobStreaming::Port::E_Playback, 
			0, 0);

		if (!p) {
			debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n",buff);
		} else {
			if (!m_transmitProcessor->addPort(p)) {
				debugWarning("Could not register port with stream processor\n");
				free(buff);
				return false;
			} else {
				debugOutput(DEBUG_LEVEL_VERBOSE, "Added port %s\n",buff);
			}
		}
		free(buff);
    	}

//	// example of adding an midi port:
//    asprintf(&buff,"dev%d_pbk_%s",m_id,"myportnamehere");
//    
//    p=new FreebobStreaming::MotuMidiPort(
//            buff,
//            FreebobStreaming::Port::E_Playback, 
//            0 // you can add all other port specific stuff you 
//              // need to pass by extending MotuXXXPort and MotuPortInfo
//    );
//    free(buff);
//
//    if (!p) {
//        debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n",buff);
//    } else {
//        if (!m_transmitProcessor->addPort(p)) {
//            debugWarning("Could not register port with stream processor\n");
//            return false;
//        } else {
//            debugOutput(DEBUG_LEVEL_VERBOSE, "Added port %s\n",buff);
//        }
//    }
    
	// example of adding an control port:
//    asprintf(&buff,"dev%d_pbk_%s",m_id,"myportnamehere");
//    
//    p=new FreebobStreaming::MotuControlPort(
//            buff,
//            FreebobStreaming::Port::E_Playback, 
//            0 // you can add all other port specific stuff you 
//              // need to pass by extending MotuXXXPort and MotuPortInfo
//    );
//    free(buff);
//
//    if (!p) {
//        debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n",buff);
//    } else {
//        if (!m_transmitProcessor->addPort(p)) {
//            debugWarning("Could not register port with stream processor\n");
//            return false;
//        } else {
//            debugOutput(DEBUG_LEVEL_VERBOSE, "Added port %s\n",buff);
//        }
//    }
	
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

quadlet_t isoctrl = ReadRegister(MOTUFW_REG_ISOCTRL);

	// NOTE: this assumes that you have two streams
	switch (i) {
	case 0:
		// TODO: do the stuff that is nescessary to make the device
		// transmit a stream

		// Set the streamprocessor channel to the one obtained by 
		// the connection management
		m_receiveProcessor->setChannel(m_iso_recv_channel);

		// Mask out current transmit settings of the MOTU and replace
		// with new ones.  Turn bit 24 on to enable changes to the
		// MOTU's iso transmit settings when the iso control register
		// is written.  Bit 23 enables iso transmit from the MOTU.
		isoctrl &= 0xff00ffff;
		isoctrl |= (m_iso_recv_channel << 16);
		isoctrl |= 0x00c00000;
		WriteRegister(MOTUFW_REG_ISOCTRL, isoctrl);
		break;
	case 1:
		// TODO: do the stuff that is nescessary to make the device
		// receive a stream

		// Set the streamprocessor channel to the one obtained by 
		// the connection management
		m_transmitProcessor->setChannel(m_iso_send_channel);

		// Mask out current receive settings of the MOTU and replace
		// with new ones.  Turn bit 31 on to enable changes to the
		// MOTU's iso receive settings when the iso control register
		// is written.  Bit 30 enables iso receive by the MOTU.
		isoctrl &= 0x00ffffff;
		isoctrl |= (m_iso_send_channel << 24);
		isoctrl |= 0xc0000000;
		WriteRegister(MOTUFW_REG_ISOCTRL, isoctrl);
		break;
		
	default: // Invalid stream index
		return -1;
	}

	return 0;
}

int
MotuDevice::stopStreamByIndex(int i) {

quadlet_t isoctrl = ReadRegister(MOTUFW_REG_ISOCTRL);

	// TODO: connection management: break connection
	// cfr the start function

	// NOTE: this assumes that you have two streams
	switch (i) {
	case 0:
		// Turn bit 22 off to disable iso send by the MOTU.  Turn
		// bit 23 on to enable changes to the MOTU's iso transmit
		// settings when the iso control register is written.
		isoctrl &= 0xffbfffff;
		WriteRegister(MOTUFW_REG_ISOCTRL, isoctrl);
		break;
	case 1:
		// Turn bit 30 off to disable iso receive by the MOTU.  Turn
		// bit 31 on to enable changes to the MOTU's iso receive
		// settings when the iso control register is written.
		isoctrl &= 0xbfffffff;
		WriteRegister(MOTUFW_REG_ISOCTRL, isoctrl);
		break;
		
	default: // Invalid stream index
		return -1;
	}

	return 0;
}

signed int MotuDevice::getIsoRecvChannel(void) {
	return m_iso_recv_channel;
}

signed int MotuDevice::getIsoSendChannel(void) {
	return m_iso_send_channel;
}

enum MotuDevice::EMotuOpticalMode MotuDevice::getOpticalMode(void) {
	unsigned int reg = ReadRegister(MOTUFW_REG_ROUTE_PORT_CONF);

// FIXME: remove after debugging
// Also, do we really want to mess with this enum?
fprintf(stderr,"route-port conf reg: 0x%08x\n",reg);
	return (enum EMotuOpticalMode)((reg & 0x00000300) >> 8);
}

signed int MotuDevice::setOpticalMode(enum EMotuOpticalMode mode) {
  // FIXME: needs implementing
  return -1;
}

/* ======================================================================== */

unsigned int MotuDevice::ReadRegister(unsigned int reg) {
/*
 * Attempts to read the requested register from the MOTU.
 */

quadlet_t quadlet;
assert(m_1394Service);
	
  quadlet = 0;
  // Note: 1394Service::read() expects a physical ID, not the node id
  if (m_1394Service->read(0xffc0 | m_nodeId, MOTUFW_BASE_ADDR+reg, 4, &quadlet) < 0) {
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

  // Note: 1394Service::write() expects a physical ID, not the node id
  if (m_1394Service->write(0xffc0 | m_nodeId, MOTUFW_BASE_ADDR+reg, 4, &data) < 0) {
    err = 1;
    debugError("Error doing motu write to register 0x%06x\n",reg);
  }

  usleep(100);
  return (err==0)?0:-1;
}

}
