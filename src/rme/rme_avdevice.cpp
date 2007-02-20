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

/* ======================================================================= */
/* Provide a mechanism for allocating iso channels and bandwidth to MOTU 
 * interfaces.
 */

// static signed int allocate_iso_channel(raw1394handle_t handle) {
// /*
//  * Allocates an iso channel for use by the interface in a similar way to
//  * libiec61883.  Returns -1 on error (due to there being no free channels)
//  * or an allocated channel number.
//  * FIXME: As in libiec61883, channel 63 is not requested; this is either a
//  * bug or it's omitted since that's the channel preferred by video devices.
//  */
// 	int c = -1;
// 	for (c = 0; c < 63; c++)
// 		if (raw1394_channel_modify (handle, c, RAW1394_MODIFY_ALLOC) == 0)
// 			break;
// 	if (c < 63)
// 		return c;
// 	return -1;
// }

// static signed int free_iso_channel(raw1394handle_t handle, signed int channel) {
// /*
//  * Deallocates an iso channel.  Returns -1 on error or 0 on success.  Silently
//  * ignores a request to deallocate a negative channel number.
//  */
// 	if (channel < 0)
// 		return 0;
// 	if (raw1394_channel_modify (handle, channel, RAW1394_MODIFY_FREE)!=0)
// 		return -1;
// 	return 0;
// }

// static signed int get_iso_bandwidth_avail(raw1394handle_t handle) {
// /*
//  * Returns the current value of the `bandwidth available' register on
//  * the IRM, or -1 on error.
//  */
// quadlet_t buffer;
// signed int result = raw1394_read (handle, raw1394_get_irm_id (handle),
// 	CSR_REGISTER_BASE + CSR_BANDWIDTH_AVAILABLE,
// 	sizeof (quadlet_t), &buffer);
// 
// 	if (result < 0)
// 		return -1;
// 	return ntohl(buffer);
// }
/* ======================================================================= */

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

// 	int samp_freq = getSamplingFrequency();

// 	raw1394handle_t handle = m_1394Service->getHandle();

	debugOutput(DEBUG_LEVEL_NORMAL, "Preparing MotuDevice...\n" );

	// Assign iso channels if not already done
//	if (m_iso_recv_channel < 0)
//		m_iso_recv_channel = allocate_iso_channel(handle);
//	if (m_iso_send_channel < 0)
//		m_iso_send_channel = allocate_iso_channel(handle);
//
//	debugOutput(DEBUG_LEVEL_VERBOSE, "recv channel = %d, send channel = %d\n",
//		m_iso_recv_channel, m_iso_send_channel);
//
//	if (m_iso_recv_channel<0 || m_iso_send_channel<0) {
//		debugFatal("Could not allocate iso channels!\n");
//		return false;
//	}

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
//	m_bandwidth = 25 + 1160;
//	debugOutput(DEBUG_LEVEL_VERBOSE, "Available bandwidth: %d\n", 
//		get_iso_bandwidth_avail(handle));
//	if (raw1394_bandwidth_modify(handle, m_bandwidth, RAW1394_MODIFY_ALLOC) < 0) {
//		debugFatal("Could not allocate bandwidth of %d\n", m_bandwidth);
//		m_bandwidth = -1;
//		return false;
//	}
//	debugOutput(DEBUG_LEVEL_VERBOSE, 
//		"allocated bandwidth of %d for MOTU device\n", m_bandwidth);
//	debugOutput(DEBUG_LEVEL_VERBOSE,
//		"remaining bandwidth: %d\n", get_iso_bandwidth_avail(handle));

//	m_receiveProcessor=new FreebobStreaming::MotuReceiveStreamProcessor(
//		m_1394Service->getPort(), samp_freq, event_size_in);

	// The first thing is to initialize the processor.  This creates the
	// data structures.
//	if(!m_receiveProcessor->init()) {
//		debugFatal("Could not initialize receive processor!\n");
//		return false;
//	}
//	m_receiveProcessor->setVerboseLevel(getDebugLevel());

	// Now we add ports to the processor
//	debugOutput(DEBUG_LEVEL_VERBOSE,"Adding ports to receive processor\n");
	
//	char *buff;
//	FreebobStreaming::Port *p=NULL;
//
//	// Add audio capture ports
//	if (!addDirPorts(FreebobStreaming::Port::E_Capture, samp_freq, optical_in_mode)) {
//		return false;
//	}

	// Add MIDI port.  The MOTU only has one MIDI input port, with each
	// MIDI byte sent using a 3 byte sequence starting at byte 4 of the
	// event data.
//	asprintf(&buff,"dev%d_cap_MIDI0",m_id);
//	p = new FreebobStreaming::MotuMidiPort(buff,
//		FreebobStreaming::Port::E_Capture, 4);
//	if (!p) {
//		debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n", buff);
//	} else {
//		if (!m_receiveProcessor->addPort(p)) {
//			debugWarning("Could not register port with stream processor\n");
//			free(buff);
//			return false;
//		} else {
//			debugOutput(DEBUG_LEVEL_VERBOSE, "Added port %s\n", buff);
//		}
//	}
//	free(buff);

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
//	m_transmitProcessor=new FreebobStreaming::MotuTransmitStreamProcessor(
//		m_1394Service->getPort(), getSamplingFrequency(), event_size_out);
//
//	m_transmitProcessor->setVerboseLevel(getDebugLevel());
//	
//	if(!m_transmitProcessor->init()) {
//		debugFatal("Could not initialize transmit processor!\n");
//		return false;
//	}

	// Connect the transmit stream ticks-per-frame hook to the
	// ticks-per-frame DLL integrator in the receive stream.
//	m_transmitProcessor->setTicksPerFrameDLL(m_receiveProcessor->getTicksPerFrameDLL());

	// Now we add ports to the processor
//	debugOutput(DEBUG_LEVEL_VERBOSE,"Adding ports to transmit processor\n");

	// Add audio playback ports
//	if (!addDirPorts(FreebobStreaming::Port::E_Playback, samp_freq, optical_out_mode)) {
//		return false;
//	}

	// Add MIDI port.  The MOTU only has one output MIDI port, with each
	// MIDI byte transmitted using a 3 byte sequence starting at byte 4
	// of the event data.
//	asprintf(&buff,"dev%d_pbk_MIDI0",m_id);
//	p = new FreebobStreaming::MotuMidiPort(buff,
//		FreebobStreaming::Port::E_Capture, 4);
//	if (!p) {
//		debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n", buff);
//	} else {
//		if (!m_receiveProcessor->addPort(p)) {
//			debugWarning("Could not register port with stream processor\n");
//			free(buff);
//			return false;
//		} else {
//			debugOutput(DEBUG_LEVEL_VERBOSE, "Added port %s\n", buff);
//		}
//	}
//	free(buff);

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
RmeDevice::getStreamCount() {
 	return 2; // one receive, one transmit
}

FreebobStreaming::StreamProcessor *
RmeDevice::getStreamProcessorByIndex(int i) {

//	switch (i) {
//	case 0:
//		return m_receiveProcessor;
//	case 1:
//		return m_transmitProcessor;
//	default:
//		return NULL;
//	}
//	return 0;
	return NULL;
}

int
RmeDevice::startStreamByIndex(int i) {

	// NOTE: this assumes that you have two streams
	switch (i) {
	case 0:
		// TODO: do the stuff that is nescessary to make the device
		// transmit a stream

		// Set the streamprocessor channel to the one obtained by 
		// the connection management
//		m_receiveProcessor->setChannel(m_iso_recv_channel);

		// Mask out current transmit settings of the MOTU and replace
		// with new ones.  Turn bit 24 on to enable changes to the
		// MOTU's iso transmit settings when the iso control register
		// is written.  Bit 23 enables iso transmit from the MOTU.
		break;
	case 1:
		// TODO: do the stuff that is nescessary to make the device
		// receive a stream

		// Set the streamprocessor channel to the one obtained by 
		// the connection management
//		m_transmitProcessor->setChannel(m_iso_send_channel);

		// Mask out current receive settings of the MOTU and replace
		// with new ones.  Turn bit 31 on to enable changes to the
		// MOTU's iso receive settings when the iso control register
		// is written.  Bit 30 enables iso receive by the MOTU.
		break;
		
	default: // Invalid stream index
		return -1;
	}

	return 0;
}

int
RmeDevice::stopStreamByIndex(int i) {

	// TODO: connection management: break connection
	// cfr the start function

	// NOTE: this assumes that you have two streams
	switch (i) {
	case 0:
		break;
	case 1:
		break;
		
	default: // Invalid stream index
		return -1;
	}

	return 0;
}

signed int RmeDevice::getIsoRecvChannel(void) {
	return m_iso_recv_channel;
}

signed int RmeDevice::getIsoSendChannel(void) {
	return m_iso_send_channel;
}

signed int RmeDevice::getEventSize(unsigned int dir) {
//
// Return the size in bytes of a single event sent to (dir==MOTUFW_OUT) or
// from (dir==MOTUFW_IN) the MOTU as part of an iso data packet.

	return 1;
}
/* ======================================================================= */

#if 0
unsigned int RmeDevice::ReadRegister(unsigned int reg) {
/*
 * Attempts to read the requested register from the RME device.
 */

quadlet_t quadlet;
assert(m_1394Service);
	
  quadlet = 0;
  // Note: 1394Service::read() expects a physical ID, not the node id
  if (m_1394Service->read(0xffc0 | m_nodeId, MOTUFW_BASE_ADDR+reg, 1, &quadlet) < 0) {
    debugError("Error doing rme read from register 0x%06x\n",reg);
  }

  return ntohl(quadlet);
}

signed int RmeDevice::WriteRegister(unsigned int reg, quadlet_t data) {
/*
 * Attempts to write the given data to the requested RME register.
 */

  unsigned int err = 0;
  data = htonl(data);

  // Note: 1394Service::write() expects a physical ID, not the node id
  if (m_1394Service->write(0xffc0 | m_nodeId, MOTUFW_BASE_ADDR+reg, 1, &data) < 0) {
    err = 1;
    debugError("Error doing rme write to register 0x%06x\n",reg);
  }

  usleep(100);
  return (err==0)?0:-1;
}
#endif

}
