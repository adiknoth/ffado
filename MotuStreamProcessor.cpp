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

#include <netinet/in.h>

namespace FreebobStreaming {

IMPL_DEBUG_MODULE( MotuTransmitStreamProcessor, MotuTransmitStreamProcessor, DEBUG_LEVEL_NORMAL );
IMPL_DEBUG_MODULE( MotuReceiveStreamProcessor, MotuReceiveStreamProcessor, DEBUG_LEVEL_NORMAL );

// A macro to extract specific bits from a native endian quadlet
#define get_bits(_d,_start,_len) (((_d)>>((_start)-(_len)+1)) & ((1<<(_len))-1))


/* transmit */
MotuTransmitStreamProcessor::MotuTransmitStreamProcessor(int port, int framerate,
		unsigned int event_size)
	: TransmitStreamProcessor(port, framerate), m_event_size(event_size) {

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

	enum raw1394_iso_disposition retval = RAW1394_ISO_OK;
	
	// signal that we are running
	// this is to allow the manager to wait untill all streams are up&running
	// it can take some time before the devices start to transmit.
	// if we would transmit ourselves, we'd have instant buffer underrun
	// this works in cooperation with the m_disabled value
	
	// TODO: add code here to detect that a stream is running
	// NOTE: xmit streams are most likely 'always' ready
	m_running=true;
	
	// don't process the stream when it is not enabled.
	// however, maybe we do have to generate (semi) valid packets
	if(m_disabled) {
		*length = 0; 
		*tag = 1; // TODO: is this correct for MOTU?
		*sy = 0;
		return RAW1394_ISO_OK;
	}

// FIXME: for now always just return NULL packets
*length = 0;
*tag = 0;
*sy = 0;
freebob_ringbuffer_read_advance(m_event_buffer, 6*m_event_size);
incrementFrameCounter(6);
return RAW1394_ISO_OK;
	
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "get packet...\n");
	
	// construct the packet cip

    // TODO: calculate read_size here.
    // note: an 'event' is one sample from all channels + possibly other midi and control data
    int nevents=0; // TODO: determine
	unsigned int read_size=nevents*m_event_size;

    // we read the packet data from a ringbuffer, because of efficiency
    // that allows us to construct the packets one period at once
	if ((freebob_ringbuffer_read(m_event_buffer,(char *)(data+8),read_size)) < 
				read_size) 
	{
        /* there is no more data in the ringbuffer */
        debugWarning("Transmit buffer underrun (cycle %d, FC=%d, PC=%d)\n", 
                 cycle, m_framecounter, m_handler->getPacketCount());
        
        // signal underrun
        m_xruns++;

        retval=RAW1394_ISO_DEFER; // make raw1394_loop_iterate exit its inner loop
        *length=0;
        nevents=0;

    } else {
        retval=RAW1394_ISO_OK;
        *length = read_size + 8;
        
        // process all ports that should be handled on a per-packet base
        // this is MIDI for AMDTP (due to the need of DBC, which is lost 
        // when putting the events in the ringbuffer)
        // for motu this might also be control data, however as control
        // data isn't time specific I would also include it in the period
        // based processing
        
        int dbc=0;//get this from your packet, if you need it. otherwise change encodePacketPorts
        if (!encodePacketPorts((quadlet_t *)(data+8), nevents, dbc)) {
            debugWarning("Problem encoding Packet Ports\n");
        }
    }
    
    *tag = 1; // TODO: is this correct for MOTU?
    *sy = 0;
    
    // update the frame counter
    incrementFrameCounter(nevents);
    // keep this at the end, because otherwise the raw1394_loop_iterate functions inner loop
    // keeps requesting packets, that are not nescessarily ready
    if(m_framecounter>(signed int)m_period) {
       retval=RAW1394_ISO_DEFER;
    }
	
    return retval;

}

bool MotuTransmitStreamProcessor::isOnePeriodReady()
{ 
     // TODO: this is the way you can implement sync
     //       only when this returns true, one period will be
     //       transferred to the audio api side.
     //       you can delay this moment as long as you
     //       want (provided that there is enough buffer space)
     
     // this implementation just waits until there is one period of samples
     // transmitted from the buffer
     return (m_framecounter > (signed int)m_period); 
}
 
bool MotuTransmitStreamProcessor::prefill() {
    // this is needed because otherwise there is no data to be 
    // sent when the streaming starts
    
    int i=m_nb_buffers;
    while(i--) {
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
    if(!TransmitStreamProcessor::reset()) {
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
    if(!TransmitStreamProcessor::prepare()) {
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

    // allocate the temporary event buffer
    // this is needed for the efficient transfer() routine
    // its size has to be equal to one 'event'
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

    // the API specific settings of the ports are already set before
    // this routine is called, therefore we can init&prepare the ports
    if(!initPorts()) {
        debugFatal("Could not initialize ports!\n");
        return false;
    }

    if(!preparePorts()) {
        debugFatal("Could not initialize ports!\n");
        return false;
    }

    // we should prefill the event buffer
    if (!prefill()) {
        debugFatal("Could not prefill buffers\n");
        return false;    
    }

    return true;

}

bool MotuTransmitStreamProcessor::transferSilence(unsigned int size) {
    
    // this function should tranfer 'size' frames of 'silence' to the event buffer
    unsigned int write_size=size*m_event_size;
    char *dummybuffer=(char *)calloc(size,m_event_size);

    transmitSilenceBlock(dummybuffer, size, 0);

    if (freebob_ringbuffer_write(m_event_buffer,(char *)(dummybuffer),write_size) < write_size) {
        debugWarning("Could not write to event buffer\n");
    }
    
    free(dummybuffer);
    
    return true;
}

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

// FIXME: just return until we've got the transmit side of things functional
return true;

    freebob_ringbuffer_data_t vec[2];
    // we received one period of frames
    // this is period_size*m_event_size of events
    unsigned int bytes2write=m_period*m_event_size;

    /* write events2write bytes to the ringbuffer 
    *  first see if it can be done in one read.
    *  if so, ok. 
    *  otherwise write up to a multiple of events directly to the buffer
    *  then do the buffer wrap around using ringbuffer_write
    *  then write the remaining data directly to the buffer in a third pass 
    *  Make sure that we cannot end up on a non-cluster aligned position!
    */
    while(bytes2write>0) {
        int byteswritten=0;
        
        unsigned int frameswritten=(m_period*m_event_size-bytes2write)/m_event_size;
        offset=frameswritten;
        
        freebob_ringbuffer_get_write_vector(m_event_buffer, vec);
            
        if(vec[0].len==0) { // this indicates a full event buffer
            debugError("XMT: Event buffer overrun in processor %p\n",this);
            break;
        }
            
        /* if we don't take care we will get stuck in an infinite loop
        * because we align to a event boundary later
        * the remaining nb of bytes in one write operation can be 
        * smaller than one event
        * this can happen because the ringbuffer size is always a power of 2
        */
        if(vec[0].len<m_event_size) {
            
            // encode to the temporary buffer
            xrun = transmitBlock(m_tmp_event_buffer, 1, offset);
            
            if(xrun<0) {
                // xrun detected
                debugError("XMT: Frame buffer underrun in processor %p\n",this);
                break;
            }
                
            // use the ringbuffer function to write one event 
            // the write function handles the wrap around.
            freebob_ringbuffer_write(m_event_buffer,
                         m_tmp_event_buffer,
                         m_event_size);
                
            // we advanced one m_event_size
            bytes2write-=m_event_size;
                
        } else { // 
            
            if(bytes2write>vec[0].len) {
                // align to an event boundary
                byteswritten=vec[0].len-(vec[0].len%m_event_size);
            } else {
                byteswritten=bytes2write;
            }
                
            xrun = transmitBlock(vec[0].buf,
                         byteswritten/m_event_size,
                         offset);
            
            if(xrun<0) {
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
                       unsigned int nevents, unsigned int offset)
{
    int problem=0;

    for ( PortVectorIterator it = m_PeriodPorts.begin();
          it != m_PeriodPorts.end();
          ++it )
    {
        // if this port is disabled, don't process it
        if((*it)->isDisabled()) {continue;};
        
        //FIXME: make this into a static_cast when not DEBUG?

        MotuPortInfo *pinfo=dynamic_cast<MotuPortInfo *>(*it);
        assert(pinfo); // this should not fail!!

/*      This is the AMDTP way, the motu way is different
        Leaving this in as reference
        
        switch(pinfo->getFormat()) {
        
        
        case MotuPortInfo::E_MBLA:
            if(encodePortToMBLAEvents(static_cast<MotuAudioPort *>(*it), (quadlet_t *)data, offset, nevents)) {
                debugWarning("Could not encode port %s to MBLA events",(*it)->getName().c_str());
                problem=1;
            }
            break;
        case MotuPortInfo::E_SPDIF: // still unimplemented
            break;
        default: // ignore
            break;
        }
*/
    }
    return problem;

}

int MotuTransmitStreamProcessor::transmitSilenceBlock(char *data, 
                       unsigned int nevents, unsigned int offset)
{
    int problem=0;

    for ( PortVectorIterator it = m_PeriodPorts.begin();
          it != m_PeriodPorts.end();
          ++it )
    {

        //FIXME: make this into a static_cast when not DEBUG?

        MotuPortInfo *pinfo=dynamic_cast<MotuPortInfo *>(*it);
        assert(pinfo); // this should not fail!!

/* this is the same as the non-silence version, except that is doesn't read from the port buffers
        switch(pinfo->getFormat()) {
        

        case MotuPortInfo::E_MBLA:
            if(encodeSilencePortToMBLAEvents(static_cast<MotuAudioPort *>(*it), (quadlet_t *)data, offset, nevents)) {
                debugWarning("Could not encode port %s to MBLA events",(*it)->getName().c_str());
                problem=1;
            }
            break;
        case MotuPortInfo::E_SPDIF: // still unimplemented
            break;
        default: // ignore
            break;
        }
        */
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

int MotuTransmitStreamProcessor::encodePortToMBLAEvents(MotuAudioPort *p, quadlet_t *data, 
                       unsigned int offset, unsigned int nevents)
{
    unsigned int j=0;

    quadlet_t *target_event;

    target_event=(quadlet_t *)(data + p->getPosition());

    switch(p->getDataType()) {
        default:
        case Port::E_Int24:
            {
                quadlet_t *buffer=(quadlet_t *)(p->getBufferAddress());

                assert(nevents + offset <= p->getBufferSize());

                buffer+=offset;

                for(j = 0; j < nevents; j += 1) { // decode max nsamples
                    *target_event = htonl((*(buffer) & 0x00FFFFFF) | 0x40000000);
                    buffer++;
                    target_event += m_dimension;
                }
            }
            break;
        case Port::E_Float:
            {
                const float multiplier = (float)(0x7FFFFF00);
                float *buffer=(float *)(p->getBufferAddress());

                assert(nevents + offset <= p->getBufferSize());

                buffer+=offset;

                for(j = 0; j < nevents; j += 1) { // decode max nsamples		
    
                    // don't care for overflow
                    float v = *buffer * multiplier;  // v: -231 .. 231
                    unsigned int tmp = ((int)v);
                    *target_event = htonl((tmp >> 8) | 0x40000000);
                    
                    buffer++;
                    target_event += m_dimension;
                }
            }
            break;
    }

    return 0;
}
*/
/*
int MotuTransmitStreamProcessor::encodeSilencePortToMBLAEvents(MotuAudioPort *p, quadlet_t *data, 
                       unsigned int offset, unsigned int nevents)
{
    unsigned int j=0;

    quadlet_t *target_event;

    target_event=(quadlet_t *)(data + p->getPosition());

    switch(p->getDataType()) {
        default:
        case Port::E_Int24:
        case Port::E_Float:
            {
                for(j = 0; j < nevents; j += 1) { // decode max nsamples
                    *target_event = htonl(0x40000000);
                    target_event += m_dimension;
                }
            }
            break;
    }

    return 0;
}
*/

/* --------------------- RECEIVE ----------------------- */

MotuReceiveStreamProcessor::MotuReceiveStreamProcessor(int port, int framerate, 
	unsigned int event_size)
    : ReceiveStreamProcessor(port, framerate), m_event_size(event_size) {

}

MotuReceiveStreamProcessor::~MotuReceiveStreamProcessor() {
    freebob_ringbuffer_free(m_event_buffer);
    free(m_tmp_event_buffer);

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
	// aligned; use of an unaligned quadlet_t may cause issues on certain
	// architectures.
	unsigned char *src_data;
	src_data = (unsigned char *)data + p->getPosition();

	switch(p->getDataType()) {
		default:
		case Port::E_Int24:
			{
				quadlet_t *buffer=(quadlet_t *)(p->getBufferAddress());

				assert(nevents + offset <= p->getBufferSize());

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
