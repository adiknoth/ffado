/* $Id$ */

/*
 *   FreeBob Streaming API
 *   FreeBob = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2005,2006 Pieter Palmers <pieterpalmers@users.sourceforge.net>
 *   Copyright (C) 2006 Jonathan Woithe <jwoithe@physics.adelaide.edu.au>
 *
 *   This program is free software {} you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation {} either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY {} without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program {} if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * 
 *
 */


#include "MotuStreamProcessor.h"
#include "Port.h"
#include "MotuPort.h"

#include <math.h>

#include <netinet/in.h>

namespace FreebobStreaming {

IMPL_DEBUG_MODULE( MotuTransmitStreamProcessor, MotuTransmitStreamProcessor, DEBUG_LEVEL_NORMAL );
IMPL_DEBUG_MODULE( MotuReceiveStreamProcessor, MotuReceiveStreamProcessor, DEBUG_LEVEL_NORMAL );

// A macro to extract specific bits from a native endian quadlet
#define get_bits(_d,_start,_len) (((_d)>>((_start)-(_len)+1)) & ((1<<(_len))-1))


/* transmit */
MotuTransmitStreamProcessor::MotuTransmitStreamProcessor(int port, int framerate,
		unsigned int event_size)
	: TransmitStreamProcessor(port, framerate), m_event_size(event_size),
	m_tx_dbc(0), m_cycle_count(-1), m_cycle_ofs(0.0), m_sph_ofs_dll(NULL),
	m_closedown_count(-1) {

}

MotuTransmitStreamProcessor::~MotuTransmitStreamProcessor() {
	freebob_ringbuffer_free(m_event_buffer);
	free(m_tmp_event_buffer);
}

bool MotuTransmitStreamProcessor::init() {

	debugOutput( DEBUG_LEVEL_VERBOSE, "Initializing (%p)...\n");
	// call the parent init
	// this has to be done before allocating the buffers, 
	// because this sets the buffersizes from the processormanager
	if(!TransmitStreamProcessor::init()) {
		debugFatal("Could not do base class init (%p)\n",this);
		return false;
	}
	m_closedown_count = -1;

	return true;
}

void MotuTransmitStreamProcessor::setVerboseLevel(int l) {
	setDebugLevel(l); // sets the debug level of the current object
	TransmitStreamProcessor::setVerboseLevel(l); // also set the level of the base class
}


enum raw1394_iso_disposition
MotuTransmitStreamProcessor::getPacket(unsigned char *data, unsigned int *length,
	              unsigned char *tag, unsigned char *sy,
	              int cycle, unsigned int dropped, unsigned int max_length) {

// FIXME: the actual delays in the system need to be worked out so
// we can get this thing synchronised.  For now this seems to work.
#define CYCLE_DELAY 1

// FIXME: every so often sync seems to be missed on startup, and because
// this code can't recover a sync the problem remains indefinitely.  The
// cause of this needs to be identified.  It may be the result of not
// running with RT privileges for this initial testing phase.  Even so, sync
// recovery needs to be implemented.

	enum raw1394_iso_disposition retval = RAW1394_ISO_OK;
	quadlet_t *quadlet = (quadlet_t *)data;
	signed int i;

	// The MOTU transmit stream is 'always' ready
	m_running = true;
	
	// Initialise the cycle counter if this is the first time
	// iso data has been requested.
	if (!m_disabled && m_cycle_count<0) {
debugOutput(DEBUG_LEVEL_VERBOSE, "tx enabled at cycle %d, dll=%g\n",cycle,
  m_sph_ofs_dll->get());
		m_cycle_count = cycle;
		m_cycle_ofs = 0.0;
	}

	// Do housekeeping expected for all packets sent to the MOTU, even
	// for packets containing no audio data.
	*sy = 0x00;
	*tag = 1;      // All MOTU packets have a CIP-like header

	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "get packet...\n");

	// Size of a single data frame in quadlets
	unsigned dbs = m_event_size / 4;

	// The number of events expected by the MOTU is solely dependent on
	// the current sample rate.  An 'event' is one sample from all channels
	// plus possibly other midi and control data.
	signed n_events = m_framerate<=48000?8:(m_framerate<=96000?16:32);

// FIXME: some tests - attempt to recover sync after loss due to missed cycles
static signed int next_cycle = -1;
//static suseconds_t last_us = -1;
//struct timeval tv;
//gettimeofday(&tv, NULL);
if (!m_disabled && next_cycle>=0 && cycle!=next_cycle) {
  debugOutput(DEBUG_LEVEL_VERBOSE, "tx cycle miss: %d requested, %d expected\n",cycle,next_cycle);
  debugOutput(DEBUG_LEVEL_VERBOSE, "tx stream: cycle=%d, ofs=%g\n",m_cycle_count, m_cycle_ofs);
//debugOutput(DEBUG_LEVEL_VERBOSE, "now=%d, last call=%d, diff=%d\n",
//  tv.tv_usec, last_us, tv.tv_usec>last_us?(tv.tv_usec-last_us):(1000000-last_us+tv.tv_usec));

#if 0
// This "simple" way out doesn't work, probably because there's no
// guarantee that ofs 0 in the current cycle is anywhere near an audio
// sample point.
m_cycle_count = cycle;
m_cycle_ofs = 0.0;
m_tx_dbc = 0;
#else
float ftmp;

// Calculate values of m_cycle_{count,ofs} which would be present
// for this cycle if the missed cycles hadn't been missed.
// These mathematical gymnastics *should* be functionally equivalent to
// the loop below.  Once it has proven correct it can replace the loop
// since it will be much faster most of the time.
signed int n_missed = (cycle>next_cycle)?cycle-next_cycle:8000-next_cycle+cycle;
signed int ma = ((n_events/2)+n_missed*3072.0/m_sph_ofs_dll->get())/n_events;
ma*=n_events;
ftmp = m_cycle_ofs+ma*m_sph_ofs_dll->get();
//m_cycle_count = (int)(m_cycle_count+ftmp/3072) % 8000;
//m_cycle_ofs = fmod(ftmp, 3072);
//m_tx_dbc = (m_tx_dbc + ma) & 0xff;
debugOutput(DEBUG_LEVEL_VERBOSE, " calc %d: count=%d, ofs=%g\n",
  ma,
  (int)(m_cycle_count+ftmp/3072) % 8000,
  fmod(ftmp,3072)
);

signed int ccount, fcount;

  ccount = next_cycle;
  while (ccount!=cycle) {
    if (ccount < m_cycle_count) {
      if (++ccount == 8000)
        ccount = 0;
      continue;
    }
    m_tx_dbc += n_events;

//    for (fcount=0; fcount<n_events; fcount++) {
//      m_cycle_ofs += m_sph_ofs_dll->get();
//      if (m_cycle_ofs >= 3072) {
//        m_cycle_ofs -= 3072;
//        if (++m_cycle_count > 7999)
//          m_cycle_count -= 8000;
//      }
//    }
// Replace the above for loop with direct calculations to
// improve efficiency.
    ftmp = m_cycle_ofs+n_events*m_sph_ofs_dll->get();
    m_cycle_count += ftmp/3072;
    m_cycle_ofs = fmod(ftmp, 3072);

    if (++ccount == 8000)
      ccount = 0;
  }
  m_tx_dbc &= 0xff;
  m_cycle_count %= 8000;

#endif
  debugOutput(DEBUG_LEVEL_VERBOSE, "  resuming with cyclecount=%d, cycleofs=%g (dll=%g)\n",
    m_cycle_count, m_cycle_ofs, m_sph_ofs_dll->get());
}
if (!m_disabled)
  next_cycle = (cycle+1)%8000;
else
  next_cycle = -1;
//last_us = tv.tv_usec;

	// Increment the dbc (data block count).  This is only done if the
	// packet will contain events - that is, we are due to send some
	// data.  Otherwise a pad packet is sent which contains the DBC of
	// the previously sent packet.  This regime also means that the very
	// first packet containing data will have a DBC of n_events, which
	// matches what is observed from other systems.
	if (!m_disabled && cycle>=m_cycle_count) {
		m_tx_dbc += n_events;
		if (m_tx_dbc > 0xff)
			m_tx_dbc -= 0x100;
	}

	// construct the packet CIP-like header.  Even if this is a data-less 
	// packet the dbs field is still set as if there were data blocks 
	// present.  For data-less packets the dbc is the same as the previously
	// transmitted block.
	*quadlet = htonl(0x00000400 | ((getNodeId()&0x3f)<<24) | m_tx_dbc | (dbs<<16));
	quadlet++;
	*quadlet = htonl(0x8222ffff);
	quadlet++;
	*length = 8;

	// If the stream is disabled or the MOTU transmission cycle count is
	// ahead of the ieee1394 cycle timer, we send a data-less packet
	// with only the 8 byte CIP-like header set up previously.
	if (m_disabled || cycle<m_cycle_count) {
		return RAW1394_ISO_OK;
	}

	// Size of data to read from the event buffer, in bytes.
	unsigned int read_size = n_events * m_event_size;

	// In the disabled state simply zero all data sent to the MOTU.  If
	// a stream of empty packets are sent once iso streaming is enabled
	// the MOTU tends to emit high-pitched audio (approx 10 kHz) for
	// some reason.  This is not completely sufficient, however (zeroed
	// packets must also be sent on iso closedown).

	// FIXME: Currently we simply send empty packets to the MOTU when
	// the stream is disabled so the "m_disabled == 0" code is never
	// executed.  However, this may change in future so it's left in 
	// for the moment for reference.
	// FIXME: Currently we don't read the buffer at all during closedown.
	// We could (and silently junk the contents) if it turned out to be
	// more helpful.
	if (!m_disabled && m_closedown_count<0) {
	        // We read the packet data from a ringbuffer because of
        	// efficiency; it allows us to construct the packets one
        	// period at once.
		i = freebob_ringbuffer_read(m_event_buffer,(char *)(data+8),read_size) <
			read_size;
	} else {
		memset(data+8, 0, read_size);
		i = 0;
	}
	if (i == 1) {
		/* there is no more data in the ringbuffer */
		debugWarning("Transmit buffer underrun (cycle %d, FC=%d, PC=%d)\n", 
			cycle, m_framecounter, m_handler->getPacketCount());

		// signal underrun
        	m_xruns++;

		retval=RAW1394_ISO_DEFER; // make raw1394_loop_iterate exit its inner loop
		n_events = 0;

	} else {
		retval=RAW1394_ISO_OK;
		*length += read_size;

// FIXME: if we choose to read the buffer even during closedown,
// here is where the data is silenced.
//		if (m_closedown_count >= 0)
//			memset(data+8, 0, read_size);
		if (m_closedown_count > 0)
			m_closedown_count--;

		// Set up each frames's SPH.  Note that the (int) typecast
		// appears to do rounding.
		// FIXME: once working, make more efficient by removing 1 of the
		// "trim to 8000" operations.
		for (i=0; i<n_events; i++, quadlet += dbs) {
			*quadlet = htonl( (((m_cycle_count+CYCLE_DELAY)%8000)<<12) + 
					(int)m_cycle_ofs);
// FIXME: remove this hacked in 1 kHz test signal to analog-1 when testing
// is complete.  Note that the tone is *not* added during closedown.
if (m_closedown_count<0) {
signed int val;
val = (int)(0x7fffff*sin(1000.0*2.0*M_PI*(m_cycle_count+((m_cycle_ofs)/3072.0))/8000.0));
*(data+8+i*m_event_size+16) = (val >> 16) & 0xff;
*(data+8+i*m_event_size+17) = (val >> 8) & 0xff;
*(data+8+i*m_event_size+18) = val & 0xff;
}
			m_cycle_ofs += m_sph_ofs_dll->get();
			if (m_cycle_ofs >= 3072) {
				m_cycle_ofs -= 3072;
				if (++m_cycle_count > 7999)
					m_cycle_count -= 8000;
			}
		}

		// Process all ports that should be handled on a per-packet base
		// this is MIDI for AMDTP (due to the need of DBC, which is lost 
		// when putting the events in the ringbuffer)
		// for motu this might also be control data, however as control
		// data isn't time specific I would also include it in the period
		// based processing
        
		// FIXME: m_tx_dbc probably needs to be initialised to a non-zero
		// value somehow so MIDI sync is possible.  For now we ignore
		// this issue.
		if (!encodePacketPorts((quadlet_t *)(data+8), n_events, m_tx_dbc)) {
			debugWarning("Problem encoding Packet Ports\n");
		}
	}
    
	// Update the frame counter
	incrementFrameCounter(n_events);

	// Keep this at the end, because otherwise the raw1394_loop_iterate
	// functions inner loop keeps requesting packets, that are not
	// nescessarily ready

// Amdtp has this commented out
	if (m_framecounter > (signed int)m_period) {
		retval=RAW1394_ISO_DEFER;
	}
	
	return retval;
}

bool MotuTransmitStreamProcessor::isOnePeriodReady() { 
	// TODO: this is the way you can implement sync
	//       only when this returns true, one period will be
	//       transferred to the audio api side.
	//       you can delay this moment as long as you
	//       want (provided that there is enough buffer space)
	
	// this implementation just waits until there is one period of samples
	// transmitted from the buffer

// Amdtp has this commented out and simply return true.
	return (m_framecounter > (signed int)m_period); 
//	return true;
}
 
bool MotuTransmitStreamProcessor::prefill() {
	// this is needed because otherwise there is no data to be 
	// sent when the streaming starts
    
	int i = m_nb_buffers;
	while (i--) {
		if(!transferSilence(m_period)) {
			debugFatal("Could not prefill transmit stream\n");
			return false;
		}
	}
	return true;
}

bool MotuTransmitStreamProcessor::reset() {

	debugOutput( DEBUG_LEVEL_VERBOSE, "Resetting...\n");

	// reset the event buffer, discard all content
	freebob_ringbuffer_reset(m_event_buffer);
    
	// reset all non-device specific stuff
	// i.e. the iso stream and the associated ports
	if (!TransmitStreamProcessor::reset()) {
		debugFatal("Could not do base class reset\n");
		return false;
	}

	// we should prefill the event buffer
	if (!prefill()) {
		debugFatal("Could not prefill buffers\n");
		return false;    
	}

	return true;
}

bool MotuTransmitStreamProcessor::prepare() {
    
	debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing...\n");
    
	// prepare all non-device specific stuff
	// i.e. the iso stream and the associated ports
	if (!TransmitStreamProcessor::prepare()) {
		debugFatal("Could not prepare base class\n");
		return false;
	}

	m_PeriodStat.setName("XMT PERIOD");
	m_PacketStat.setName("XMT PACKET");
	m_WakeupStat.setName("XMT WAKEUP");

	debugOutput( DEBUG_LEVEL_VERBOSE, "Event size: %d\n", m_event_size);
    
	// allocate the event buffer
	unsigned int ringbuffer_size_frames=m_nb_buffers * m_period;
    
	if( !(m_event_buffer=freebob_ringbuffer_create(
	  m_event_size * ringbuffer_size_frames))) {
		debugFatal("Could not allocate memory event ringbuffer");
		return false;
	}

	// Allocate the temporary event buffer.  This is needed for the
	// efficient transfer() routine.  Its size has to be equal to one
	// 'event'.
 	if( !(m_tmp_event_buffer=(char *)calloc(1,m_event_size))) {
		debugFatal("Could not allocate temporary event buffer");
		freebob_ringbuffer_free(m_event_buffer);
		return false;
	}

	// Set the parameters of ports we can: we want the audio ports to be
	// period buffered, and the midi ports to be packet buffered.
	for ( PortVectorIterator it = m_Ports.begin();
	  it != m_Ports.end();
	  ++it ) {
		debugOutput(DEBUG_LEVEL_VERBOSE, "Setting up port %s\n",(*it)->getName().c_str());
		if(!(*it)->setBufferSize(m_period)) {
			debugFatal("Could not set buffer size to %d\n",m_period);
			return false;
        	}

		switch ((*it)->getPortType()) {
		case Port::E_Audio:
			if (!(*it)->setSignalType(Port::E_PeriodSignalled)) {
				debugFatal("Could not set signal type to PeriodSignalling");
				return false;
			}
			break;

		case Port::E_Midi:
			if (!(*it)->setSignalType(Port::E_PacketSignalled)) {
				debugFatal("Could not set signal type to PacketSignalling");
				return false;
			}
			break;
                
		case Port::E_Control:
			if (!(*it)->setSignalType(Port::E_PeriodSignalled)) {
				debugFatal("Could not set signal type to PeriodSignalling");
				return false;
			}
			break;

		default:
			debugWarning("Unsupported port type specified\n");
			break;
		}
	}

	// The API specific settings of the ports are already set before
	// this routine is called, therefore we can init&prepare the ports
	if (!initPorts()) {
		debugFatal("Could not initialize ports!\n");
		return false;
	}

	if(!preparePorts()) {
		debugFatal("Could not initialize ports!\n");
		return false;
	}

	// We should prefill the event buffer
	if (!prefill()) {
		debugFatal("Could not prefill buffers\n");
		return false;    
	}

	return true;
}

bool MotuTransmitStreamProcessor::transferSilence(unsigned int size) {
    
	// This function should tranfer 'size' frames of 'silence' to the event buffer
	unsigned int write_size=size*m_event_size;
	char *dummybuffer=(char *)calloc(size,m_event_size);

	transmitSilenceBlock(dummybuffer, size, 0);

	if (freebob_ringbuffer_write(m_event_buffer,(char *)(dummybuffer),write_size) < write_size) {
		debugWarning("Could not write to event buffer\n");
	}

	free(dummybuffer);

	return true;
}

/**
 * \brief write events queued for transmission from the port ringbuffers
 * to the event buffer.
 */
bool MotuTransmitStreamProcessor::transfer() {
	m_PeriodStat.mark(freebob_ringbuffer_read_space(m_event_buffer)/m_event_size);

	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Transferring period...\n");
	// TODO: improve
/* a naive implementation would look like this:

	unsigned int write_size=m_period*m_event_size;
	char *dummybuffer=(char *)calloc(m_period,m_event_size);

	transmitBlock(dummybuffer, m_period, 0, 0);

	if (freebob_ringbuffer_write(m_event_buffer,(char *)(dummybuffer),write_size) < write_size) {
		debugWarning("Could not write to event buffer\n");
 	}

	free(dummybuffer);
*/
/* but we're not that naive anymore... */
	int xrun;
	unsigned int offset=0;

	freebob_ringbuffer_data_t vec[2];
	// There is one period of frames to transfer.  This is
	// period_size*m_event_size of events.
	unsigned int bytes2write=m_period*m_event_size;

	/* Write bytes2write bytes to the event ringbuffer.  First see if it can
	 * be done in one write; if so, ok.
	 * Otherwise write up to a multiple of events directly to the buffer
	 * then do the buffer wrap around using ringbuffer_write.  Then
	 * write the remaining data directly to the buffer in a third pass. 
	 * Make sure that we cannot end up on a non-cluster aligned
	 * position!
	 */
	while(bytes2write>0) {
		int byteswritten=0;
        
		unsigned int frameswritten=(m_period*m_event_size-bytes2write)/m_event_size;
		offset=frameswritten;

		freebob_ringbuffer_get_write_vector(m_event_buffer, vec);

		if (vec[0].len==0) { // this indicates a full event buffer
			debugError("XMT: Event buffer overrun in processor %p\n",this);
			break;
		}

		/* If we don't take care we will get stuck in an infinite
		 * loop because we align to a event boundary later.  The
		 * remaining nb of bytes in one write operation can be
		 * smaller than one event; this can happen because the
		 * ringbuffer size is always a power of 2.
		 */
		if(vec[0].len<m_event_size) {
            
			// encode to the temporary buffer
			xrun = transmitBlock(m_tmp_event_buffer, 1, offset);
            
			if (xrun<0) {
				// xrun detected
				debugError("XMT: Frame buffer underrun in processor %p\n",this);
				break;
			}

			// Use the ringbuffer function to write one event.
			// The write function handles the wrap around.
			freebob_ringbuffer_write(m_event_buffer,
				m_tmp_event_buffer, m_event_size);
                
			// we advanced one m_event_size
			bytes2write-=m_event_size;
                
		} else {
            
			if (bytes2write>vec[0].len) {
				// align to an event boundary
				byteswritten=vec[0].len-(vec[0].len%m_event_size);
			} else {
				byteswritten=bytes2write;
			}

			xrun = transmitBlock(vec[0].buf,
				byteswritten/m_event_size, offset);
            
			if (xrun<0) {
				// xrun detected
				debugError("XMT: Frame buffer underrun in processor %p\n",this);
				break;
			}

			freebob_ringbuffer_write_advance(m_event_buffer, byteswritten);
			bytes2write -= byteswritten;
		}

		// the bytes2write should always be event aligned
		assert(bytes2write%m_event_size==0);
	}

	return true;
}
/* 
 * write received events to the stream ringbuffers.
 */

int MotuTransmitStreamProcessor::transmitBlock(char *data, 
                       unsigned int nevents, unsigned int offset) {
	signed int problem=0;
	unsigned int i;

	// FIXME: ensure the MIDI and control streams are all zeroed until
	// such time as they are fully implemented.
	for (i=0; i<nevents; i++) {
		memset(data+4+i*m_event_size, 0x00, 6);
	}

	for ( PortVectorIterator it = m_PeriodPorts.begin();
	  it != m_PeriodPorts.end();
	  ++it ) {
		// If this port is disabled, don't process it
		if((*it)->isDisabled()) {continue;};
        
		//FIXME: make this into a static_cast when not DEBUG?
		Port *port=dynamic_cast<Port *>(*it);
		
		switch(port->getPortType()) {
		
		case Port::E_Audio:
			if (encodePortToMBLAEvents(static_cast<MotuAudioPort *>(*it), (quadlet_t *)data, offset, nevents)) {
				debugWarning("Could not encode port %s to MBLA events",(*it)->getName().c_str());
				problem=1;
			}
			break;
		// midi is a packet based port, don't process
		//	case MotuPortInfo::E_Midi:
		//		break;

		default: // ignore
			break;
		}
	}
	return problem;
}

int MotuTransmitStreamProcessor::transmitSilenceBlock(char *data, 
                       unsigned int nevents, unsigned int offset) {
	// This is the same as the non-silence version, except that is
	// doesn't read from the port buffers.

	int problem=0;

	for ( PortVectorIterator it = m_PeriodPorts.begin();
	  it != m_PeriodPorts.end();
	  ++it ) {
		//FIXME: make this into a static_cast when not DEBUG?
		Port *port=dynamic_cast<Port *>(*it);
		
		switch(port->getPortType()) {
		
		case Port::E_Audio:
			if (encodeSilencePortToMBLAEvents(static_cast<MotuAudioPort *>(*it), (quadlet_t *)data, offset, nevents)) {
				debugWarning("Could not encode port %s to MBLA events",(*it)->getName().c_str());
				problem=1;
			}
			break;
		// midi is a packet based port, don't process
		//	case MotuPortInfo::E_Midi:
		//		break;

		default: // ignore
			break;
		}
	}
	return problem;
}

/**
 * @brief decode a packet for the packet-based ports
 *
 * @param data Packet data
 * @param nevents number of events in data (including events of other ports & port types)
 * @param dbc DataBlockCount value for this packet
 * @return true if all successfull
 */
bool MotuTransmitStreamProcessor::encodePacketPorts(quadlet_t *data, unsigned int nevents, unsigned int dbc)
{
    bool ok=true;
    char byte;
    
    quadlet_t *target_event=NULL;
    int j;

    for ( PortVectorIterator it = m_PacketPorts.begin();
          it != m_PacketPorts.end();
          ++it )
    {

#ifdef DEBUG
        MotuPortInfo *pinfo=dynamic_cast<MotuPortInfo *>(*it);
        assert(pinfo); // this should not fail!!

        // the only packet type of events for AMDTP is MIDI in mbla
//         assert(pinfo->getFormat()==MotuPortInfo::E_Midi);
#endif
        
        MotuMidiPort *mp=static_cast<MotuMidiPort *>(*it);
        
        // TODO: decode the midi (or other type) stuff here

    }
        
    return ok;
}

/* Left in as reference, this is highly AMDTP related

basic idea:

iterate over the ports
- get port buffer address
- loop over events
  * pick right sample in event based upon PortInfo
  * convert sample from Port format (E_Int24, E_Float, ..) to native format

not that in order to use the 'efficient' transfer method, you have to make sure that
you can start from an offset (expressed in frames).
*/
#include <math.h>

int MotuTransmitStreamProcessor::encodePortToMBLAEvents(MotuAudioPort *p, quadlet_t *data, 
                       unsigned int offset, unsigned int nevents) {
	unsigned int j=0;

	// Use char here since the target address won't necessarily be 
	// aligned; use of an unaligned quadlet_t may cause issues on certain
	// architectures.  Besides, the target (data going directly to the MOTU)
	// isn't structured in quadlets anyway; it mainly consists of packed
	// 24-bit integers.
	unsigned char *target;
	target = (unsigned char *)data + p->getPosition();

	switch(p->getDataType()) {
		default:
		case Port::E_Int24:
			{
				quadlet_t *buffer=(quadlet_t *)(p->getBufferAddress());

				assert(nevents + offset <= p->getBufferSize());

				// Offset is in frames, but each port is only a single
				// channel, so the number of frames is the same as the
				// number of quadlets to offset (assuming the port buffer
				// uses one quadlet per sample, which is the case currently).
				buffer+=offset;

				for(j = 0; j < nevents; j += 1) { // Decode nsamples
					*target = (*buffer >> 16) & 0xff;
					*(target+1) = (*buffer >> 8) & 0xff;
					*(target+2) = (*buffer) & 0xff;

					buffer++;
					target+=m_event_size;
				}
			}
			break;
		case Port::E_Float:
			{
				const float multiplier = (float)(0x7FFFFF);
				float *buffer=(float *)(p->getBufferAddress());

				assert(nevents + offset <= p->getBufferSize());

				buffer+=offset;

				for(j = 0; j < nevents; j += 1) { // decode max nsamples		
					unsigned int v = (int)(*buffer * multiplier);
					*target = (v >> 16) & 0xff;
					*(target+1) = (v >> 8) & 0xff;
					*(target+2) = v & 0xff;

					buffer++;
					target+=m_event_size;
				}
			}
			break;
	}

	return 0;
}

int MotuTransmitStreamProcessor::encodeSilencePortToMBLAEvents(MotuAudioPort *p, quadlet_t *data, 
                       unsigned int offset, unsigned int nevents) {
	unsigned int j=0;
	unsigned char *target = (unsigned char *)data + p->getPosition();

	switch (p->getDataType()) {
	default:
        case Port::E_Int24:
        case Port::E_Float:
		for (j = 0; j < nevents; j++) {
			*target = *(target+1) = *(target+2) = 0;
			target += m_event_size;
		}
		break;
	}

	return 0;
}

bool MotuTransmitStreamProcessor::preparedForStop() {

	// FIXME: ideally we want to include a condition which tests if the
	// stream shutdown is in response to an xrun due to a problem at
	// startup, and unconditionally return true.  This saves a few
	// seconds delay since under these conditions the iso transmit
	// callback doesn't appear to be called and therefore
	// m_closedown_count is never decremented.  We can't just test for
	// an xrun however since sometimes these can occur "normally" during
	// shutdown.  This is probably all tied up with the sync recovery
	// issue which hasn't really been explored yet.
	if (m_disabled || !isRunning())
		return true;

	if (m_closedown_count < 0) {
		// No closedown has been initiated, so start one now.  Set
		// the closedown count to the number of zero packets which
		// will be sent to the MOTU before closing off the iso
		// streams.  FIXME: 128 packets (each containing 8 frames at
		// 48 kHz) is the experimentally-determined figure for 48
		// kHz with a period size of 1024.  It seems that at least
		// one period of zero samples need to be sent to allow for
		// inter-thread communication occuring on period boundaries. 
		// This needs to be confirmed for other rates and period
		// sizes.
		signed n_events = m_framerate<=48000?8:(m_framerate<=96000?16:32);
		m_closedown_count = m_period / n_events;
		return false;
	}

	// We are "go" for closedown once all requested zero packets
	// (initiated by a previous call to this function) have been sent to
	// the MOTU.
	return m_closedown_count == 0;
}

/* --------------------- RECEIVE ----------------------- */

MotuReceiveStreamProcessor::MotuReceiveStreamProcessor(int port, int framerate, 
	unsigned int event_size)
    : ReceiveStreamProcessor(port, framerate), m_event_size(event_size),
	m_last_cycle_ofs(-1) {

	// Set up the Delay-locked-loop to track audio frequency relative
	// to the cycle timer.  The seed value is just the difference one
	// would see if the audio clock was locked to the ieee1394 cycle
	// timer.
	// FIXME: the value for the coefficient may be optimisable; the
	// value used currently just mirrors that used in
	// AmdtpReceiveStreamProcessor::putPacket for a similar purpose.
	float coeffs[1];
	coeffs[0]=0.0005;
	m_sph_ofs_dll = new FreebobUtil::DelayLockedLoop(1, coeffs);
	m_sph_ofs_dll->setIntegrator(0, 24576000.0/framerate);
}

MotuReceiveStreamProcessor::~MotuReceiveStreamProcessor() {
	freebob_ringbuffer_free(m_event_buffer);
	free(m_tmp_event_buffer);
	delete m_sph_ofs_dll;
}

bool MotuReceiveStreamProcessor::init() {

    // call the parent init
    // this has to be done before allocating the buffers, 
    // because this sets the buffersizes from the processormanager
    if(!ReceiveStreamProcessor::init()) {
        debugFatal("Could not do base class init (%d)\n",this);
        return false;
    }

    return true;
}

enum raw1394_iso_disposition 
MotuReceiveStreamProcessor::putPacket(unsigned char *data, unsigned int length, 
                  unsigned char channel, unsigned char tag, unsigned char sy, 
                  unsigned int cycle, unsigned int dropped) {
    
    enum raw1394_iso_disposition retval=RAW1394_ISO_OK;

// FIXME: just for debugging, print out the sph ofs DLL value
// once a second
//if (cycle==0) {
//  fprintf(stderr, "sph_ofs_dll=%g\n",m_sph_ofs_dll->get());
//}

// FIXME: more debugging
static signed int last_cycle=-1;
if (last_cycle>=0 && (signed)cycle!=(last_cycle+1)%8000) {
  debugOutput(DEBUG_LEVEL_VERBOSE, "lost rx cycles; received %d, expected %d\n",
    cycle, (last_cycle+1)%8000);
}
last_cycle=cycle;

    // If the packet length is 8 bytes (ie: just a CIP-like header) there is
    // no isodata.
    if (length > 8) {
	// The iso data blocks from the MOTUs comprise a CIP-like header
	// followed by a number of events (8 for 1x rates, 16 for 2x rates,
	// 32 for 4x rates).
	quadlet_t *quadlet = (quadlet_t *)data;
	unsigned int dbs = get_bits(ntohl(quadlet[0]), 23, 8);  // Size of one event in terms of fdf_size
	unsigned int fdf_size = get_bits(ntohl(quadlet[1]), 23, 8) == 0x22 ? 32:0; // Event unit size in bits
	unsigned int event_length = (fdf_size * dbs) / 8;       // Event size in bytes
	unsigned int n_events = (length-8) / event_length;

	// Don't even attempt to process a packet if it isn't what we expect
	// from a MOTU
	if (tag!=1 || fdf_size!=32) {
		return RAW1394_ISO_OK;
	}

	// Signal that we're running
	if (n_events) m_running=true;

#if 1
	/* FIXME: test whether things are improved by doing this in the
	 * actual receive handler.  The advantage is that we don't have to
	 * wait until the stream is enabled before the DLL starts tracking. 
	 * This has the desireable side-effect of having a relatively
	 * accurate value in the DLL by the time the transmit stream is
	 * enabled.  A disadvantage of having this in here is that it
	 * increases the time spent in this function.  Whether this is
	 * important in practice remains to be seen.  Ad hoc evidence thus
	 * far seems to suggest that it does increase the chances of a
	 * faulty startup, but more tests are needed.
	 */
	/* Push cycle offset differences from each event's SPH into the DLL.
	 * If this is the very first block received, use the first event to
	 * initialise the last cycle offset.
	 * FIXME: it might be best to use differences only within the given
	 * block rather than keeping a store of the last cycle offset.
	 * Otherwise in the event of a lost incoming packet the DLL will
	 * have an abnormally large value sent to it.  Perhaps this doesn't
	 * matter?
	 */
	unsigned int ev;
	signed int sph_ofs = ntohl(*(quadlet_t *)(data+8)) & 0xfff;

//	if (m_last_cycle_ofs < 0) {
//		m_last_cycle_ofs = sph_ofs-(int)m_sph_ofs_dll->get();
//	}
	m_last_cycle_ofs = sph_ofs;
	for (ev=1; ev<n_events; ev++) {
		sph_ofs = ntohl(*(quadlet_t *)(data+8+ev*m_event_size)) & 0xfff;
		m_sph_ofs_dll->put((m_last_cycle_ofs<sph_ofs)?
			sph_ofs-m_last_cycle_ofs:sph_ofs+3072-m_last_cycle_ofs);
		m_last_cycle_ofs = sph_ofs;
	}
#endif



	// Don't process the stream when it is not enabled.
	if (m_disabled) {
		return RAW1394_ISO_OK;
	}
        
	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "put packet...\n");

        // Add the data payload (events) to the ringbuffer.  We'll just copy
	// everything including the 4 byte timestamp at the start of each
	// event (that is, everything except the CIP-like header).  The
	// demultiplexer can deal with the complexities such as the channel
	// 24-bit data.
	unsigned int write_size = length-8;
	if (freebob_ringbuffer_write(m_event_buffer,(char *)(data+8),write_size) < write_size) {
		debugWarning("Receive buffer overrun (cycle %d, FC=%d, PC=%d)\n", 
			cycle, m_framecounter, m_handler->getPacketCount());
		m_xruns++;

		retval=RAW1394_ISO_DEFER;
	} else {
		retval=RAW1394_ISO_OK;
		// Process all ports that should be handled on a per-packet basis
		// This is MIDI for AMDTP (due to the need of DBC)
		int dbc = get_bits(ntohl(quadlet[0]), 8, 8);  // Low byte of CIP quadlet 0
		if (!decodePacketPorts((quadlet_t *)(data+8), n_events, dbc)) {
			debugWarning("Problem decoding Packet Ports\n");
			retval=RAW1394_ISO_DEFER;
		}
		// time stamp processing can be done here
	}

	// update the frame counter
	incrementFrameCounter(n_events);
	// keep this at the end, because otherwise the raw1394_loop_iterate functions inner loop
	// keeps requesting packets without going to the xmit handler, leading to xmit starvation
	if(m_framecounter>(signed int)m_period) {
		retval=RAW1394_ISO_DEFER;
	}
        
    } else { // no events in packet
	// discard packet
	// can be important for sync though
    }
    
    return retval;
}

bool MotuReceiveStreamProcessor::isOnePeriodReady() { 
     // TODO: this is the way you can implement sync
     //       only when this returns true, one period will be
     //       transferred to the audio api side.
     //       you can delay this moment as long as you
     //       want (provided that there is enough buffer space)
     
     // this implementation just waits until there is one period of samples
     // received into the buffer
    if(m_framecounter > (signed int)m_period) {
        return true;
    }
    return false;
}

void MotuReceiveStreamProcessor::setVerboseLevel(int l) {
	setDebugLevel(l);
 	ReceiveStreamProcessor::setVerboseLevel(l);

}


bool MotuReceiveStreamProcessor::reset() {

	debugOutput( DEBUG_LEVEL_VERBOSE, "Resetting...\n");

	// reset the event buffer, discard all content
	freebob_ringbuffer_reset(m_event_buffer);

	// reset all non-device specific stuff
	// i.e. the iso stream and the associated ports
	if(!ReceiveStreamProcessor::reset()) {
		debugFatal("Could not do base class reset\n");
		return false;
	}
	
	return true;
}

bool MotuReceiveStreamProcessor::prepare() {

	// prepare all non-device specific stuff
	// i.e. the iso stream and the associated ports
	if(!ReceiveStreamProcessor::prepare()) {
		debugFatal("Could not prepare base class\n");
		return false;
	}

	debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing...\n");

	m_PeriodStat.setName("RCV PERIOD");
	m_PacketStat.setName("RCV PACKET");
	m_WakeupStat.setName("RCV WAKEUP");

    // setup any specific stuff here

	debugOutput( DEBUG_LEVEL_VERBOSE, "Event size: %d\n", m_event_size);
    
	// allocate the event buffer
	unsigned int ringbuffer_size_frames=m_nb_buffers * m_period;

	if( !(m_event_buffer=freebob_ringbuffer_create(
			m_event_size * ringbuffer_size_frames))) {
		debugFatal("Could not allocate memory event ringbuffer");
		return false;
	}

	// allocate the temporary event buffer
	if( !(m_tmp_event_buffer=(char *)calloc(1,m_event_size))) {
		debugFatal("Could not allocate temporary event buffer");
		freebob_ringbuffer_free(m_event_buffer);
		return false;
	}

	// set the parameters of ports we can:
	// we want the audio ports to be period buffered,
	// and the midi ports to be packet buffered
	for ( PortVectorIterator it = m_Ports.begin();
		  it != m_Ports.end();
		  ++it )
	{
		debugOutput(DEBUG_LEVEL_VERBOSE, "Setting up port %s\n",(*it)->getName().c_str());
		
		if(!(*it)->setBufferSize(m_period)) {
			debugFatal("Could not set buffer size to %d\n",m_period);
			return false;
		}

		switch ((*it)->getPortType()) {
			case Port::E_Audio:
				if(!(*it)->setSignalType(Port::E_PeriodSignalled)) {
					debugFatal("Could not set signal type to PeriodSignalling");
					return false;
				}
				break;
			case Port::E_Midi:
				if(!(*it)->setSignalType(Port::E_PacketSignalled)) {
					debugFatal("Could not set signal type to PacketSignalling");
					return false;
				}
				break;
			case Port::E_Control:
				if(!(*it)->setSignalType(Port::E_PeriodSignalled)) {
					debugFatal("Could not set signal type to PeriodSignalling");
					return false;
				}
				break;
			default:
				debugWarning("Unsupported port type specified\n");
				break;
		}

	}

	// The API specific settings of the ports are already set before
	// this routine is called, therefore we can init&prepare the ports
	if(!initPorts()) {
		debugFatal("Could not initialize ports!\n");
		return false;
	}

	if(!preparePorts()) {
		debugFatal("Could not initialize ports!\n");
		return false;
	}
	
	return true;

}

bool MotuReceiveStreamProcessor::transfer() {

    // the same idea as the transmit processor
    
	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Transferring period...\n");
	
/* another naive section:	
	unsigned int read_size=m_period*m_event_size;
	char *dummybuffer=(char *)calloc(m_period,m_event_size);
	if (freebob_ringbuffer_read(m_event_buffer,(char *)(dummybuffer),read_size) < read_size) {
		debugWarning("Could not read from event buffer\n");
	}

	receiveBlock(dummybuffer, m_period, 0);

	free(dummybuffer);
*/
	int xrun;
	unsigned int offset=0;
	
	freebob_ringbuffer_data_t vec[2];
	// We received one period of frames from each channel.
	// This is period_size*m_event_size bytes.
	unsigned int bytes2read = m_period * m_event_size;

	/* Read events2read bytes from the ringbuffer.
	*  First see if it can be done in one read.  If so, ok.
	*  Otherwise read up to a multiple of events directly from the buffer
	*  then do the buffer wrap around using ringbuffer_read
	*  then read the remaining data directly from the buffer in a third pass 
	*  Make sure that we cannot end up on a non-event aligned position!
	*/
	while(bytes2read>0) {
		unsigned int framesread=(m_period*m_event_size-bytes2read)/m_event_size;
		offset=framesread;
		
		int bytesread=0;

		freebob_ringbuffer_get_read_vector(m_event_buffer, vec);
			
		if(vec[0].len==0) { // this indicates an empty event buffer
			debugError("RCV: Event buffer underrun in processor %p\n",this);
			break;
		}
			
		/* if we don't take care we will get stuck in an infinite loop
		* because we align to an event boundary later
		* the remaining nb of bytes in one read operation can be smaller than one event
		* this can happen because the ringbuffer size is always a power of 2
			*/
		if(vec[0].len<m_event_size) {
			// use the ringbuffer function to read one event 
			// the read function handles wrap around
			freebob_ringbuffer_read(m_event_buffer,m_tmp_event_buffer,m_event_size);

			xrun = receiveBlock(m_tmp_event_buffer, 1, offset);
				
			if(xrun<0) {
				// xrun detected
				debugError("RCV: Frame buffer overrun in processor %p\n",this);
				break;
			}
				
			// We advanced one m_event_size
			bytes2read-=m_event_size;
				
		} else { // 
			
			if(bytes2read>vec[0].len) {
					// align to an event boundary
				bytesread=vec[0].len-(vec[0].len%m_event_size);
			} else {
				bytesread=bytes2read;
			}
				
			xrun = receiveBlock(vec[0].buf, bytesread/m_event_size, offset);
				
			if(xrun<0) {
				// xrun detected
				debugError("RCV: Frame buffer overrun in processor %p\n",this);
				break;
			}

			freebob_ringbuffer_read_advance(m_event_buffer, bytesread);
			bytes2read -= bytesread;
		}
			
		// the bytes2read should always be event aligned
		assert(bytes2read%m_event_size==0);
	}

	return true;
}

/**
 * \brief write received events to the port ringbuffers.
 */
int MotuReceiveStreamProcessor::receiveBlock(char *data, 
					   unsigned int nevents, unsigned int offset)
{
	int problem=0;
#if 0
	/* Push cycle offset differences from each event's SPH into the DLL.
	 * If this is the very first block received, use the first event to
	 * initialise the last cycle offset.
	 * FIXME: it might be best to use differences only within the given
	 * block rather than keeping a store of the last cycle offset.
	 * Otherwise in the event of a lost incoming packet the DLL will
	 * have an abnormally large value sent to it.  Perhaps this doesn't
	 * matter?
	 */
	unsigned int ev;
	signed int sph_ofs = ntohl(*(quadlet_t *)data) & 0xfff;

	if (m_last_cycle_ofs < 0) {
		m_last_cycle_ofs = sph_ofs-(int)m_sph_ofs_dll->get();
	}
	for (ev=0; ev<nevents; ev++) {
		sph_ofs = ntohl(*(quadlet_t *)(data+ev*m_event_size)) & 0xfff;
		m_sph_ofs_dll->put((m_last_cycle_ofs<sph_ofs)?
			sph_ofs-m_last_cycle_ofs:sph_ofs+3072-m_last_cycle_ofs);
		m_last_cycle_ofs = sph_ofs;
	}
#endif
	for ( PortVectorIterator it = m_PeriodPorts.begin();
          it != m_PeriodPorts.end();
          ++it ) {
		if((*it)->isDisabled()) {continue;};

		//FIXME: make this into a static_cast when not DEBUG?
		Port *port=dynamic_cast<Port *>(*it);
		
		switch(port->getPortType()) {
		
		case Port::E_Audio:
			if(decodeMBLAEventsToPort(static_cast<MotuAudioPort *>(*it), (quadlet_t *)data, offset, nevents)) {
				debugWarning("Could not decode packet MBLA to port %s",(*it)->getName().c_str());
				problem=1;
			}
			break;
		// midi is a packet based port, don't process
		//	case MotuPortInfo::E_Midi:
		//		break;

		default: // ignore
			break;
		}
	}
	return problem;
}

/**
 * @brief decode a packet for the packet-based ports
 *
 * @param data Packet data
 * @param nevents number of events in data (including events of other ports & port types)
 * @param dbc DataBlockCount value for this packet
 * @return true if all successfull
 */
bool MotuReceiveStreamProcessor::decodePacketPorts(quadlet_t *data, unsigned int nevents, unsigned int dbc)
{
	bool ok=true;
	
	quadlet_t *target_event=NULL;
	int j;
	
	for ( PortVectorIterator it = m_PacketPorts.begin();
	  it != m_PacketPorts.end();
	  ++it ) {

#ifdef DEBUG
		MotuPortInfo *pinfo=dynamic_cast<MotuPortInfo *>(*it);
		assert(pinfo); // this should not fail!!

		// the only packet type of events for AMDTP is MIDI in mbla
// 		assert(pinfo->getFormat()==MotuPortInfo::E_Midi);
#endif
		MotuMidiPort *mp=static_cast<MotuMidiPort *>(*it);
		

        // do decoding here

	}
    	
	return ok;
}

signed int MotuReceiveStreamProcessor::decodeMBLAEventsToPort(MotuAudioPort *p, 
		quadlet_t *data, unsigned int offset, unsigned int nevents)
{
	unsigned int j=0;

// 	printf("****************\n");
// 	hexDumpQuadlets(data,m_dimension*4);
// 	printf("****************\n");

	// Use char here since a port's source address won't necessarily be
	// aligned; use of an unaligned quadlet_t may cause issues on
	// certain architectures.  Besides, the source (data coming directly
	// from the MOTU) isn't structured in quadlets anyway; it mainly
	// consists of packed 24-bit integers.

	unsigned char *src_data;
	src_data = (unsigned char *)data + p->getPosition();

	switch(p->getDataType()) {
		default:
		case Port::E_Int24:
			{
				quadlet_t *buffer=(quadlet_t *)(p->getBufferAddress());

				assert(nevents + offset <= p->getBufferSize());

				// Offset is in frames, but each port is only a single
				// channel, so the number of frames is the same as the
				// number of quadlets to offset (assuming the port buffer
				// uses one quadlet per sample, which is the case currently).
				buffer+=offset;

				for(j = 0; j < nevents; j += 1) { // Decode nsamples
					*buffer = (*src_data<<16)+(*(src_data+1)<<8)+*(src_data+2);
					// Sign-extend highest bit of 24-bit int.
					// FIXME: this isn't strictly needed since E_Int24 is a 24-bit,
					// but doing so shouldn't break anything and makes the data
					// easier to deal with during debugging.
					if (*src_data & 0x80)
						*buffer |= 0xff000000;

					buffer++;
					src_data+=m_event_size;
				}
			}
			break;
		case Port::E_Float:
			{
				const float multiplier = 1.0f / (float)(0x7FFFFF);
				float *buffer=(float *)(p->getBufferAddress());

				assert(nevents + offset <= p->getBufferSize());

				buffer+=offset;

				for(j = 0; j < nevents; j += 1) { // decode max nsamples		
	
					unsigned int v = (*src_data<<16)+(*(src_data+1)<<8)+*(src_data+2);

					// sign-extend highest bit of 24-bit int
					int tmp = (int)(v << 8) / 256;
		
					*buffer = tmp * multiplier;
				
					buffer++;
					src_data+=m_event_size;
				}
			}
			break;
	}

	return 0;
}

signed int MotuReceiveStreamProcessor::setEventSize(unsigned int size) {
	m_event_size = size;
	return 0;
}

unsigned int MotuReceiveStreamProcessor::getEventSize(void) {
//
// Return the size of a single event sent by the MOTU as part of an iso
// data packet in bytes.
//
	return m_event_size;
}
                
} // end of namespace FreebobStreaming
