/* motu_avdevice.cpp
 * Copyright (C) 2006 by Pieter Palmers
 * Copyright (C) 2006 by Jonathan Woithe
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

#include "libutil/DelayLockedLoop.h"

#include <string>
#include <stdint.h>
#include <assert.h>
#include <netinet/in.h>

namespace Motu {

IMPL_DEBUG_MODULE( MotuDevice, MotuDevice, DEBUG_LEVEL_NORMAL );

char *motufw_modelname[] = {"[unknown]","828MkII", "Traveler"}; 

/* ======================================================================= */
/* Provide a mechanism for allocating iso channels to MOTU interfaces.
 *
 * FIXME: This is overly simplistic at present since it assumes there are no
 * other users of iso channels on the firewire bus except MOTU interfaces. 
 * It does however allow more than one MOTU interface to exist on the same
 * bus.  For now the first MOTU discovered will be allocated iso channels 0
 * (send) and 1 (receive) which mirrors what the official driver appears to
 * do.  Ultimately we need code to query the IRM for iso channels.
 */
static signed int next_iso_recv_channel_num = 1;
static signed int next_iso_send_channel_num = 0;  
      
static signed int next_iso_recv_channel(void) {
/*
 * Returns the next available channel for ISO receive.  If there are no more
 * available -1 is returned.  Currently the odd channels (starting from 1)
 * are used for iso receive.  No provision is made to reuse previously
 * allocated channels in the event that the applicable interface has been
 * removed - this will change in future to use the IRM.
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
 * removed - this will all change in future to use the IRM.
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
	unsigned int optical_mode = getOpticalMode();
	unsigned int event_size = getEventSize();

	debugOutput(DEBUG_LEVEL_NORMAL, "Preparing MotuDevice...\n" );

	m_receiveProcessor=new FreebobStreaming::MotuReceiveStreamProcessor(
		m_1394Service->getPort(), samp_freq, event_size);
	                         
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

	// Add audio capture ports
	if (!addDirPorts(FreebobStreaming::Port::E_Capture, samp_freq, optical_mode)) {
		return false;
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
		m_1394Service->getPort(), getSamplingFrequency(), event_size);

	m_transmitProcessor->setVerboseLevel(getDebugLevel());
	
	if(!m_transmitProcessor->init()) {
		debugFatal("Could not initialize transmit processor!\n");
		return false;
	}

	// now we add ports to the processor
	debugOutput(DEBUG_LEVEL_VERBOSE,"Adding ports to transmit processor\n");

	// Add audio playback ports
	if (!addDirPorts(FreebobStreaming::Port::E_Playback, samp_freq, optical_mode)) {
		return false;
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

unsigned int MotuDevice::getOpticalMode(void) {
	unsigned int reg = ReadRegister(MOTUFW_REG_ROUTE_PORT_CONF);
	return reg & MOTUFW_OPTICAL_MODE_MASK;
}

signed int MotuDevice::setOpticalMode(unsigned int mode) {
	unsigned int reg = ReadRegister(MOTUFW_REG_ROUTE_PORT_CONF);

	// FIXME: there seems to be more to it than this.
	reg &= ~MOTUFW_OPTICAL_MODE_MASK;
	reg |= mode & MOTUFW_OPTICAL_MODE_MASK;
	return WriteRegister(MOTUFW_REG_ROUTE_PORT_CONF, reg);
}

signed int MotuDevice::getEventSize(void) {
//
// Return the size of a single event sent by the MOTU as part of an iso
// data packet in bytes.
//
// FIXME: for performance it may turn out best to calculate the event
// size in setOpticalMode and cache the result in a data field.  However,
// as it stands this will not adapt to dynamic changes in sample rate - we'd
// need a setFrameRate() for that.
//
// At the very least an event consists of the SPH (4 bytes), the control/MIDI
// bytes (6 bytes) and 8 analog audio channels (each 3 bytes long).  Note that
// all audio channels are sent using 3 bytes.
signed int sample_rate = getSamplingFrequency();
signed int optical_mode = getOpticalMode();
signed int size = 4+6+8*3;

        // 2 channels of AES/EBU is present if a 1x or 2x sample rate is in 
        // use
        if (sample_rate <= 96000)
                size += 2*3;

        // 2 channels of (coax) SPDIF is present for 1x or 2x sample rates so
        // long as the optical mode is not TOSLINK.  If the optical mode is  
        // TOSLINK the coax SPDIF channels are replaced by optical TOSLINK   
        // channels.  Thus between these options we always have an addition  
        // 2 channels here for 1x or 2x sample rates regardless of the optical
        // mode.
        if (sample_rate <= 96000)
                size += 2*3;

        // ADAT channels 1-4 are present for 1x or 2x sample rates so long
        // as the optical mode is ADAT.
        if (sample_rate<=96000 && optical_mode==MOTUFW_OPTICAL_MODE_ADAT)
                size += 4*3;

        // ADAT channels 5-8 are present for 1x sample rates so long as the
        // optical mode is ADAT.
        if (sample_rate<=48000 && optical_mode==MOTUFW_OPTICAL_MODE_ADAT)
                size += 4*3;

	// When 1x or 2x sample rate is active there are an additional
	// 2 channels sent in an event.  For capture it is a Mix1 return,
	// while for playback it is a separate headphone mix.
	if (sample_rate<=96000)
		size += 2*3;

        // Finally round size up to the next quadlet boundary
        return ((size+3)/4)*4;
}
/* ======================================================================= */

bool MotuDevice::addPort(FreebobStreaming::StreamProcessor *s_processor,
  char *name, enum FreebobStreaming::Port::E_Direction direction, 
  int position, int size) {
/*
 * Internal helper function to add a MOTU port to a given stream processor. 
 * This just saves the unnecessary replication of what is essentially
 * boilerplate code.  Note that the port name is freed by this function
 * prior to exit.
 */
FreebobStreaming::Port *p=NULL;

	p = new FreebobStreaming::MotuAudioPort(name, direction, position, size);

	if (!p) {
		debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n",name);
	} else {
		if (!s_processor->addPort(p)) {
			debugWarning("Could not register port with stream processor\n");
			free(name);
			return false;
		} else {
			debugOutput(DEBUG_LEVEL_VERBOSE, "Added port %s\n",name);
		}
		p->enable();
	}
	free(name);
	return true;
}
/* ======================================================================= */

bool MotuDevice::addDirPorts(
  enum FreebobStreaming::Port::E_Direction direction, 
  unsigned int sample_rate, unsigned int optical_mode) {
/*
 * Internal helper method: adds all required ports for the given direction
 * based on the indicated sample rate and optical mode.
 *
 * Notes: currently ports are not created if they are disabled due to sample
 * rate or optical mode.  However, it might be better to unconditionally
 * create all ports and just disable those which are not active.
 */
const char *mode_str = direction==FreebobStreaming::Port::E_Capture?"cap":"pbk";
const char *aux_str = direction==FreebobStreaming::Port::E_Capture?"Mix1":"Phones";
FreebobStreaming::StreamProcessor *s_processor;
unsigned int i, ofs;
char *buff;

	if (direction == FreebobStreaming::Port::E_Capture) {
		s_processor = m_receiveProcessor;
	} else {
		s_processor = m_transmitProcessor;
	}
	// Offset into an event's data of the first audio data
	ofs = 10;

	// Add ports for the Mix1 return / Phones send which is present for 
	// 1x and 2x sampling rates.
	if (sample_rate<=96000) {
		for (i=0; i<2; i++, ofs+=3) {
			asprintf(&buff,"dev%d_%s_%s-%c", m_id, mode_str,
			  aux_str, i==0?'L':'R');
			if (!addPort(s_processor, buff, direction, ofs, 0))
				return false;
		}
	}

	// Unconditionally add the 8 analog capture ports since they are
	// always present no matter what the device configuration is.
	for (i=0; i<8; i++, ofs+=3) {
		asprintf(&buff,"dev%d_%s_Analog%d", m_id, mode_str, i+1);
		if (!addPort(s_processor, buff, direction, ofs, 0))
			return false;
	}

	// AES/EBU ports are present for 1x and 2x sampling rates on the 
	// Traveler.  On earlier interfaces (for example, 828 MkII) this
	// space was taken up with a separate "main out" send.
	// FIXME: what is in this position of incoming data on an 828 MkII?
	if (sample_rate <= 96000) {
		for (i=0; i<2; i++, ofs+=3) {
			if (m_motu_model == MOTUFW_MODEL_TRAVELER) {
				asprintf(&buff,"dev%d_%s_AES/EBU%d", m_id, mode_str, i+1);
			} else {
				if (direction == FreebobStreaming::Port::E_Capture)
					asprintf(&buff,"dev%d_%s_MainOut-%c", m_id, mode_str, i==0?'L':'R');
				else
					asprintf(&buff,"dev%d_%s_????%d", m_id, mode_str, i+1);
			}
			if (!addPort(s_processor, buff, direction, ofs, 0))
				return false;
		}
	}

	// SPDIF ports are present for 1x and 2x sampling rates so long
	// as the optical mode is not TOSLINK.
	if (sample_rate<=96000 && optical_mode!=MOTUFW_OPTICAL_MODE_TOSLINK) {
		for (i=0; i<2; i++, ofs+=3) {
			asprintf(&buff,"dev%d_%s_SPDIF%d", m_id, mode_str, i+1);
			if (!addPort(s_processor, buff, direction, ofs, 0))
				return false;
		}
	}

	// TOSLINK ports are present for 1x and 2x sampling rates so long
	// as the optical mode is set to TOSLINK.
	if (sample_rate<=96000 && optical_mode==MOTUFW_OPTICAL_MODE_TOSLINK) {
		for (i=0; i<2; i++, ofs+=3) {
			asprintf(&buff,"dev%d_%s_TOSLINK%d", m_id, mode_str, i+1);
			if (!addPort(s_processor, buff, direction, ofs, 0))
				return false;
		}
	}

	// ADAT ports 1-4 are present for 1x and 2x sampling rates so long
	// as the optical mode is set to ADAT.
	if (sample_rate<=96000 && optical_mode==MOTUFW_OPTICAL_MODE_ADAT) {
		for (i=0; i<4; i++, ofs+=3) {
			asprintf(&buff,"dev%d_%s_ADAT%d", m_id, mode_str, i+1);
			if (!addPort(s_processor, buff, direction, ofs, 0))
				return false;
		}
	}

	// ADAT ports 5-8 are present for 1x sampling rates so long as the
	// optical mode is set to ADAT.
	if (sample_rate<=48000 && optical_mode==MOTUFW_OPTICAL_MODE_ADAT) {
		for (i=4; i<8; i++, ofs+=3) {
			asprintf(&buff,"dev%d_%s_ADAT%d", m_id, mode_str, i+1);
			if (!addPort(s_processor, buff, direction, ofs, 0))
				return false;
		}
	}

	return true;
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
