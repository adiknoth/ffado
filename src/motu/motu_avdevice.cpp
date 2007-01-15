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

#include <libraw1394/csr.h>

namespace Motu {

IMPL_DEBUG_MODULE( MotuDevice, MotuDevice, DEBUG_LEVEL_NORMAL );

char *motufw_modelname[] = {"[unknown]","828MkII", "Traveler"}; 

/* ======================================================================= */
/* Provide a mechanism for allocating iso channels and bandwidth to MOTU 
 * interfaces.
 */

static signed int allocate_iso_channel(raw1394handle_t handle) {
/*
 * Allocates an iso channel for use by the interface in a similar way to
 * libiec61883.  Returns -1 on error (due to there being no free channels)
 * or an allocated channel number.
 * FIXME: As in libiec61883, channel 63 is not requested; this is either a
 * bug or it's omitted since that's the channel preferred by video devices.
 */
	int c = -1;
	for (c = 0; c < 63; c++)
		if (raw1394_channel_modify (handle, c, RAW1394_MODIFY_ALLOC) == 0)
			break;
	if (c < 63)
		return c;
	return -1;
}

static signed int free_iso_channel(raw1394handle_t handle, signed int channel) {
/*
 * Deallocates an iso channel.  Returns -1 on error or 0 on success.  Silently
 * ignores a request to deallocate a negative channel number.
 */
	if (channel < 0)
		return 0;
	if (raw1394_channel_modify (handle, channel, RAW1394_MODIFY_FREE)!=0)
		return -1;
	return 0;
}

static signed int get_iso_bandwidth_avail(raw1394handle_t handle) {
/*
 * Returns the current value of the `bandwidth available' register on
 * the IRM, or -1 on error.
 */
quadlet_t buffer;
signed int result = raw1394_read (handle, raw1394_get_irm_id (handle),
	CSR_REGISTER_BASE + CSR_BANDWIDTH_AVAILABLE,
	sizeof (quadlet_t), &buffer);

	if (result < 0)
		return -1;
	return ntohl(buffer);
}
/* ======================================================================= */

MotuDevice::MotuDevice( std::auto_ptr< ConfigRom >( configRom ),
                    Ieee1394Service& ieee1394service,
                    int nodeId,
                    int verboseLevel )
    : m_configRom( configRom )
    , m_1394Service( &ieee1394service )
    , m_motu_model( MOTUFW_MODEL_NONE )
    , m_nodeId( nodeId )
    , m_verboseLevel( verboseLevel )
    , m_id(0)
    , m_iso_recv_channel ( -1 )
    , m_iso_send_channel ( -1 )
    , m_bandwidth ( -1 )
    , m_receiveProcessor ( 0 )
    , m_transmitProcessor ( 0 )
    
{
    setDebugLevel( verboseLevel );
    
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Motu::MotuDevice (NodeID %d)\n",
                 nodeId );

}

MotuDevice::~MotuDevice()
{
	// Free ieee1394 bus resources if they have been allocated
	if (m_1394Service != NULL) {
		raw1394handle_t handle = m_1394Service->getHandle();
		if (m_bandwidth >= 0)
			if (raw1394_bandwidth_modify(handle, m_bandwidth, RAW1394_MODIFY_FREE) < 0)
				debugOutput(DEBUG_LEVEL_VERBOSE, "Could not free bandwidth of %d\n", m_bandwidth);
		if (m_iso_recv_channel >= 0)
			if (raw1394_channel_modify(handle, m_iso_recv_channel, RAW1394_MODIFY_FREE) < 0)
				debugOutput(DEBUG_LEVEL_VERBOSE, "Could not free recv iso channel %d\n", m_iso_recv_channel);
		if (m_iso_send_channel >= 0)
			if (raw1394_channel_modify(handle, m_iso_send_channel, RAW1394_MODIFY_FREE) < 0)
				debugOutput(DEBUG_LEVEL_VERBOSE, "Could not free send iso channel %d\n", m_iso_send_channel);
	}
}

ConfigRom&
MotuDevice::getConfigRom() const
{
    return *m_configRom;
}

struct VendorModelEntry {
    unsigned int vendor_id;
    unsigned int model_id;
};

static VendorModelEntry supportedDeviceList[] =
{
    {0x0001f2, 0x00000000},  // Motu device, model entry not used

};

bool
MotuDevice::probe( ConfigRom& configRom )
{
    unsigned int vendorId = configRom.getNodeVendorId();
    unsigned int modelId = configRom.getModelId();

    for ( unsigned int i = 0;
          i < ( sizeof( supportedDeviceList )/sizeof( VendorModelEntry ) );
          ++i )
    {
        if ( ( supportedDeviceList[i].vendor_id == vendorId )
             //&& ( supportedDeviceList[i].model_id == modelId ) 
             )
        {
            switch (configRom.getUnitVersion()) {
                case MOTUFW_UNITVER_828mkII: 
                    return true;
                case MOTUFW_UNITVER_TRAVELER:
                    return true;
            }
        }
    }

    return false;
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
                return true;
	}

	return false;
}

int 
MotuDevice::getSamplingFrequency( ) {
/*
 * Retrieve the current sample rate from the MOTU device.
 */
	quadlet_t q = ReadRegister(MOTUFW_REG_CLK_CTRL);
	int rate = 0;

	switch (q & MOTUFW_RATE_BASE_MASK) {
		case MOTUFW_RATE_BASE_44100:
			rate = 44100;
			break;
		case MOTUFW_RATE_BASE_48000:
			rate = 48000;
			break;
	}
	switch (q & MOTUFW_RATE_MULTIPLIER_MASK) {
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
 * Set the MOTU device's samplerate.
 */
	char *src_name;
	quadlet_t q, new_rate=0;
	int i, supported=true, cancel_adat=false;

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
			new_rate = MOTUFW_RATE_BASE_44100 | MOTUFW_RATE_MULTIPLIER_1X;
			break;
		case eSF_48000Hz:
			new_rate = MOTUFW_RATE_BASE_48000 | MOTUFW_RATE_MULTIPLIER_1X;
			break;
		case eSF_88200Hz:
			new_rate = MOTUFW_RATE_BASE_44100 | MOTUFW_RATE_MULTIPLIER_2X;
			break;
		case eSF_96000Hz:
			new_rate = MOTUFW_RATE_BASE_48000 | MOTUFW_RATE_MULTIPLIER_2X;
			break;
		case eSF_176400Hz:
			// Currently only the Traveler supports 4x sample rates
			if (m_motu_model == MOTUFW_MODEL_TRAVELER) {
				new_rate = MOTUFW_RATE_BASE_44100 | MOTUFW_RATE_MULTIPLIER_4X;
				cancel_adat = true;
			} else
				supported=false;
			break;
		case eSF_192000Hz:
			// Currently only the Traveler supports 4x sample rates
			if (m_motu_model == MOTUFW_MODEL_TRAVELER) {
				new_rate = MOTUFW_RATE_BASE_48000 | MOTUFW_RATE_MULTIPLIER_4X;
				cancel_adat = true;
			} else
				supported=false;
			break;
		default:
			supported=false;
	}

	// Update the clock control register.  FIXME: while this is now rather
	// comprehensive there may still be a need to manipulate MOTUFW_REG_CLK_CTRL
	// a little more than we do.
	if (supported) {
		quadlet_t value=ReadRegister(MOTUFW_REG_CLK_CTRL);

		// If optical port must be disabled (because a 4x sample rate has
		// been selected) then do so before changing the sample rate.  At
		// this stage it will be up to the user to re-enable the optical
		// port if the sample rate is set to a 1x or 2x rate later.
		if (cancel_adat) {
			setOpticalMode(MOTUFW_DIR_INOUT, MOTUFW_OPTICAL_MODE_OFF);
		}

		value &= ~(MOTUFW_RATE_BASE_MASK|MOTUFW_RATE_MULTIPLIER_MASK);
		value |= new_rate;

		// In other OSes bit 26 of MOTUFW_REG_CLK_CTRL always seems
		// to be set when this register is written to although the
		// reason isn't currently known.  When we set it, it appears
		// to prevent output being produced so we'll leave it unset
		// until we work out what's going on.  Other systems write
		// to MOTUFW_REG_CLK_CTRL multiple times, so that may be
		// part of the mystery.
		//   value |= 0x04000000;
		if (WriteRegister(MOTUFW_REG_CLK_CTRL, value) == 0) {
			supported=true;
		} else {
			supported=false;
		}
		// A write to the rate/clock control register requires the 
		// textual name of the current clock source be sent to the 
		// clock source name registers.
		switch (value & MOTUFW_CLKSRC_MASK) {
			case MOTUFW_CLKSRC_INTERNAL:
				src_name = "Internal        ";
				break;
			case MOTUFW_CLKSRC_ADAT_OPTICAL:
				src_name = "ADAT Optical    ";
				break;
			case MOTUFW_CLKSRC_SPDIF_TOSLINK:
				if (getOpticalMode(MOTUFW_DIR_IN)  == MOTUFW_OPTICAL_MODE_TOSLINK)
					src_name = "TOSLink         ";
				else
					src_name = "SPDIF           ";
				break;
			case MOTUFW_CLKSRC_SMTPE:
				src_name = "SMPTE           ";
				break;
			case MOTUFW_CLKSRC_WORDCLOCK:
				src_name = "Word Clock In   ";
				break;
			case MOTUFW_CLKSRC_ADAT_9PIN:
				src_name = "ADAT 9-pin      ";
				break;
			case MOTUFW_CLKSRC_AES_EBU:
				src_name = "AES-EBU         ";
				break;
			default:
				src_name = "Unknown         ";
		}
		for (i=0; i<16; i+=4) {
			q = (src_name[i]<<24) | (src_name[i+1]<<16) | 
				(src_name[i+2]<<8) | src_name[i+3];
			WriteRegister(MOTUFW_REG_CLKSRC_NAME0+i, q);
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
	debugOutput(DEBUG_LEVEL_VERBOSE,
		"MOTU %s at node %d\n", motufw_modelname[m_motu_model],
		m_nodeId);
}

bool
MotuDevice::prepare() {

	int samp_freq = getSamplingFrequency();
	unsigned int optical_in_mode = getOpticalMode(MOTUFW_DIR_IN);
	unsigned int optical_out_mode = getOpticalMode(MOTUFW_DIR_OUT);
	unsigned int event_size_in = getEventSize(MOTUFW_DIR_IN);
	unsigned int event_size_out= getEventSize(MOTUFW_DIR_OUT);

	raw1394handle_t handle = m_1394Service->getHandle();

	debugOutput(DEBUG_LEVEL_NORMAL, "Preparing MotuDevice...\n" );

	// Assign iso channels if not already done
	if (m_iso_recv_channel < 0)
		m_iso_recv_channel = allocate_iso_channel(handle);
	if (m_iso_send_channel < 0)
		m_iso_send_channel = allocate_iso_channel(handle);

	debugOutput(DEBUG_LEVEL_VERBOSE, "recv channel = %d, send channel = %d\n",
		m_iso_recv_channel, m_iso_send_channel);

	if (m_iso_recv_channel<0 || m_iso_send_channel<0) {
		debugFatal("Could not allocate iso channels!\n");
		return false;
	}

	// Allocate bandwidth if not previously done.
	// FIXME: The bandwidth allocation calculation can probably be
	// refined somewhat since this is currently based on a rudimentary
	// understanding of the iso protocol.
	// Currently we assume the following.
	//   * Ack/iso gap = 0.05 us
	//   * DATA_PREFIX = 0.16 us
	//   * DATA_END    = 0.26 us
	// These numbers are the worst-case figures given in the ieee1394
	// standard.  This gives approximately 0.5 us of overheads per
	// packet - around 25 bandwidth allocation units (from the ieee1394
	// standard 1 bandwidth allocation unit is 125/6144 us).  We further
	// assume the MOTU is running at S400 (which it should be) so one
	// allocation unit is equivalent to 1 transmitted byte; thus the
	// bandwidth allocation required for the packets themselves is just
	// the size of the packet.  We allocate based on the maximum packet
	// size (1160 bytes at 192 kHz) so the sampling frequency can be
	// changed dynamically if this ends up being useful in future.
	m_bandwidth = 25 + 1160;
	debugOutput(DEBUG_LEVEL_VERBOSE, "Available bandwidth: %d\n", 
		get_iso_bandwidth_avail(handle));
	if (raw1394_bandwidth_modify(handle, m_bandwidth, RAW1394_MODIFY_ALLOC) < 0) {
		debugFatal("Could not allocate bandwidth of %d\n", m_bandwidth);
		m_bandwidth = -1;
		return false;
	}
	debugOutput(DEBUG_LEVEL_VERBOSE, 
		"allocated bandwidth of %d for MOTU device\n", m_bandwidth);
	debugOutput(DEBUG_LEVEL_VERBOSE,
		"remaining bandwidth: %d\n", get_iso_bandwidth_avail(handle));

	m_receiveProcessor=new FreebobStreaming::MotuReceiveStreamProcessor(
		m_1394Service->getPort(), samp_freq, event_size_in);

	// The first thing is to initialize the processor.  This creates the
	// data structures.
	if(!m_receiveProcessor->init()) {
		debugFatal("Could not initialize receive processor!\n");
		return false;
	}
	m_receiveProcessor->setVerboseLevel(getDebugLevel());

	// Now we add ports to the processor
	debugOutput(DEBUG_LEVEL_VERBOSE,"Adding ports to receive processor\n");
	
	char *buff;
	FreebobStreaming::Port *p=NULL;

	// Add audio capture ports
	if (!addDirPorts(FreebobStreaming::Port::E_Capture, samp_freq, optical_in_mode)) {
		return false;
	}

	// Add MIDI port.  The MOTU only has one MIDI input port, with each
	// MIDI byte sent using a 3 byte sequence starting at byte 4 of the
	// event data.
	asprintf(&buff,"dev%d_cap_MIDI0",m_id);
	p = new FreebobStreaming::MotuMidiPort(buff,
		FreebobStreaming::Port::E_Capture, 4);
	if (!p) {
		debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n", buff);
	} else {
		if (!m_receiveProcessor->addPort(p)) {
			debugWarning("Could not register port with stream processor\n");
			free(buff);
			return false;
		} else {
			debugOutput(DEBUG_LEVEL_VERBOSE, "Added port %s\n", buff);
		}
	}
	free(buff);

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

	// Do the same for the transmit processor
	m_transmitProcessor=new FreebobStreaming::MotuTransmitStreamProcessor(
		m_1394Service->getPort(), getSamplingFrequency(), event_size_out);

	m_transmitProcessor->setVerboseLevel(getDebugLevel());
	
	if(!m_transmitProcessor->init()) {
		debugFatal("Could not initialize transmit processor!\n");
		return false;
	}

	// Connect the transmit stream ticks-per-frame hook to the
	// ticks-per-frame DLL integrator in the receive stream.
	m_transmitProcessor->setTicksPerFrameDLL(m_receiveProcessor->getTicksPerFrameDLL());

	// Now we add ports to the processor
	debugOutput(DEBUG_LEVEL_VERBOSE,"Adding ports to transmit processor\n");

	// Add audio playback ports
	if (!addDirPorts(FreebobStreaming::Port::E_Playback, samp_freq, optical_out_mode)) {
		return false;
	}

	// Add MIDI port.  The MOTU only has one output MIDI port, with each
	// MIDI byte transmitted using a 3 byte sequence starting at byte 4
	// of the event data.
	asprintf(&buff,"dev%d_pbk_MIDI0",m_id);
	p = new FreebobStreaming::MotuMidiPort(buff,
		FreebobStreaming::Port::E_Capture, 4);
	if (!p) {
		debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n", buff);
	} else {
		if (!m_receiveProcessor->addPort(p)) {
			debugWarning("Could not register port with stream processor\n");
			free(buff);
			return false;
		} else {
			debugOutput(DEBUG_LEVEL_VERBOSE, "Added port %s\n", buff);
		}
	}
	free(buff);

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
		// receive a stream

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
		// transmit a stream

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
		isoctrl |= 0x00800000;
		WriteRegister(MOTUFW_REG_ISOCTRL, isoctrl);
		break;
	case 1:
		// Turn bit 30 off to disable iso receive by the MOTU.  Turn
		// bit 31 on to enable changes to the MOTU's iso receive
		// settings when the iso control register is written.
		isoctrl &= 0xbfffffff;
		isoctrl |= 0x80000000;
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

unsigned int MotuDevice::getOpticalMode(unsigned int dir) {
	unsigned int reg = ReadRegister(MOTUFW_REG_ROUTE_PORT_CONF);

debugOutput(DEBUG_LEVEL_VERBOSE, "optical mode: %x %x %x %x\n",dir, reg, reg & MOTUFW_OPTICAL_IN_MODE_MASK,
reg & MOTUFW_OPTICAL_OUT_MODE_MASK);

	if (dir == MOTUFW_DIR_IN)
		return (reg & MOTUFW_OPTICAL_IN_MODE_MASK) >> 8;
	else
		return (reg & MOTUFW_OPTICAL_OUT_MODE_MASK) >> 10;
}

signed int MotuDevice::setOpticalMode(unsigned int dir, unsigned int mode) {
	unsigned int reg = ReadRegister(MOTUFW_REG_ROUTE_PORT_CONF);
	unsigned int opt_ctrl = 0x0000002;

	// Set up the optical control register value according to the current
	// optical port modes.  At this stage it's not completely understood 
	// what the "Optical control" register does, so the values it's set to
	// are more or less "magic" numbers.
	if (reg & MOTUFW_OPTICAL_IN_MODE_MASK != (MOTUFW_OPTICAL_MODE_ADAT<<8))
		opt_ctrl |= 0x00000080;
	if (reg & MOTUFW_OPTICAL_OUT_MODE_MASK != (MOTUFW_OPTICAL_MODE_ADAT<<10))
		opt_ctrl |= 0x00000040;

	if (mode & MOTUFW_DIR_IN) {
		reg &= ~MOTUFW_OPTICAL_IN_MODE_MASK;
		reg |= (mode << 8) & MOTUFW_OPTICAL_IN_MODE_MASK;
		if (mode != MOTUFW_OPTICAL_MODE_ADAT)
			opt_ctrl |= 0x00000080;
		else
			opt_ctrl &= ~0x00000080;
	}
	if (mode & MOTUFW_DIR_OUT) {
		reg &= ~MOTUFW_OPTICAL_OUT_MODE_MASK;
		reg |= (mode <<10) & MOTUFW_OPTICAL_OUT_MODE_MASK;
		if (mode != MOTUFW_OPTICAL_MODE_ADAT)
			opt_ctrl |= 0x00000040;
		else
			opt_ctrl &= ~0x00000040;
	}

	// FIXME: there seems to be more to it than this, but for 
	// the moment at least this seems to work.
	WriteRegister(MOTUFW_REG_ROUTE_PORT_CONF, reg);
	return WriteRegister(MOTUFW_REG_OPTICAL_CTRL, opt_ctrl);
}

signed int MotuDevice::getEventSize(unsigned int dir) {
//
// Return the size in bytes of a single event sent to (dir==MOTUFW_OUT) or
// from (dir==MOTUFW_IN) the MOTU as part of an iso data packet.
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
signed int optical_mode = getOpticalMode(dir);
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
					asprintf(&buff,"dev%d_%s_Mic%d", m_id, mode_str, i+1);
				else
					asprintf(&buff,"dev%d_%s_MainOut-%c", m_id, mode_str, i==0?'L':'R');
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
  if (m_1394Service->read(0xffc0 | m_nodeId, MOTUFW_BASE_ADDR+reg, 1, &quadlet) < 0) {
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
  if (m_1394Service->write(0xffc0 | m_nodeId, MOTUFW_BASE_ADDR+reg, 1, &data) < 0) {
    err = 1;
    debugError("Error doing motu write to register 0x%06x\n",reg);
  }

  usleep(100);
  return (err==0)?0:-1;
}

}
