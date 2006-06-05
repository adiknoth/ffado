/* $Id$ */

/*
 *   FreeBob Streaming API
 *   FreeBob = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2005,2006 Pieter Palmers <pieterpalmers@users.sourceforge.net>
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

#include "AmdtpStreamProcessor.h"
#include "Port.h"
#include "AmdtpPort.h"

#include <netinet/in.h>
#include <assert.h>

#define CYCLE_COUNTER_GET_SECS(x)   (((x & 0xFE000000) >> 25))
#define CYCLE_COUNTER_GET_CYCLES(x) (((x & 0x01FFF000) >> 12))
#define CYCLE_COUNTER_GET_TICKS(x)  (((x & 0x00000FFF)))
#define CYCLE_COUNTER_TO_TICKS(x) ((CYCLE_COUNTER_GET_SECS(x)   * 24576000) +\
                                   (CYCLE_COUNTER_GET_CYCLES(x) *     3072) +\
                                   (CYCLE_COUNTER_GET_TICKS(x)            ))

// this is one milisecond of processing delay
#define TICKS_PER_SECOND 24576000
#define RECEIVE_PROCESSING_DELAY (TICKS_PER_SECOND / 500)

namespace FreebobStreaming {

IMPL_DEBUG_MODULE( AmdtpTransmitStreamProcessor, AmdtpTransmitStreamProcessor, DEBUG_LEVEL_NORMAL );
IMPL_DEBUG_MODULE( AmdtpReceiveStreamProcessor, AmdtpReceiveStreamProcessor, DEBUG_LEVEL_NORMAL );


/* transmit */
AmdtpTransmitStreamProcessor::AmdtpTransmitStreamProcessor(int port, int framerate, int dimension)
	: TransmitStreamProcessor(port, framerate), m_dimension(dimension) {


}

AmdtpTransmitStreamProcessor::~AmdtpTransmitStreamProcessor() {
	freebob_ringbuffer_free(m_event_buffer);
	free(m_cluster_buffer);
}

bool AmdtpTransmitStreamProcessor::init() {

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

void AmdtpTransmitStreamProcessor::setVerboseLevel(int l) {
	setDebugLevel(l);
	TransmitStreamProcessor::setVerboseLevel(l);
}


enum raw1394_iso_disposition
AmdtpTransmitStreamProcessor::getPacket(unsigned char *data, unsigned int *length,
	              unsigned char *tag, unsigned char *sy,
	              int cycle, unsigned int dropped, unsigned int max_length) {

	struct iec61883_packet *packet = (struct iec61883_packet *) data;
	
	
	// signal that we are running (a transmit stream is always 'runnable')
	m_running=true;
	
	// don't process the stream when it is not enabled.
	// however, we do have to generate (semi) valid packets
	// that means that we'll send NODATA packets FIXME: check!!
	if(m_disabled) {
		iec61883_cip_fill_header_nodata(getNodeId(), &m_cip_status, packet);
		*length = 0; // this is to disable sending
		*tag = IEC61883_TAG_WITH_CIP;
		*sy = 0;
		return RAW1394_ISO_OK;
	}
	
     debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "get packet...\n");
	
	// keep the old cip, in case we don't have enough events left in the buffer.
	struct iec61883_cip old_cip;
	memcpy(&old_cip,&m_cip_status,sizeof(struct iec61883_cip));
		
	// construct the packet cip
	int nevents = iec61883_cip_fill_header (getNodeId(), &m_cip_status, packet);

	enum raw1394_iso_disposition retval = RAW1394_ISO_OK;

	if (!(nevents > 0)) {
		
		if (m_cip_status.mode == IEC61883_MODE_BLOCKING_EMPTY) {
			*length = 8;
			return RAW1394_ISO_OK ;
		}
		else {
			nevents = m_cip_status.syt_interval;
		}
	}
	
	int read_size=nevents*sizeof(quadlet_t)*m_dimension;

	if ((freebob_ringbuffer_read(m_event_buffer,(char *)(data+8),read_size)) < 
				read_size) 
	{
        /* there is no more data in the ringbuffer */
        
        /* If there are already more than on period
        * of frames transfered to the XMIT buffer, there is no xrun.
        * 
        */
        if(m_framecounter>m_period) {
            // we stop processing this untill the next period boundary
            // that's when new data is ready
            
            //connection->pfd->events=0;
            
            // reset the cip to the old value
            memcpy(&m_cip_status,&old_cip,sizeof(struct iec61883_cip));

            // retry this packed 
            retval=RAW1394_ISO_AGAIN;
            nevents=0;
            debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "again!\n");
            
        } else {
            debugWarning("Transmit buffer underrun (cycle %d, FC=%d, PC=%d)\n", 
                     cycle, m_framecounter, m_handler->getPacketCount());
            
            // signal underrun
            m_xruns++;
    
            retval=RAW1394_ISO_DEFER;
            *length=0;
            nevents=0;
        }

    } else {
        retval=RAW1394_ISO_OK;
        *length = read_size + 8;
        
        // process all ports that should be handled on a per-packet base
        // this is MIDI for AMDTP (due to the need of DBC)
        if (!encodePacketPorts((quadlet_t *)(data+8), nevents, packet->dbc)) {
            debugWarning("Problem encoding Packet Ports\n");
        }
        
        if (packet->syt != 0xFFFF) {
             unsigned int m_last_timestamp=ntohs(packet->syt);
             // reconstruct the top part of the timestamp using the current cycle number
              m_last_timestamp |= ((cycle << 12) & 0x01FF0000);
             
             debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Sent packet with SYT for cycle %2d: %5u ticks (%2u cycles + %04u ticks)\n",
             cycle,
             CYCLE_COUNTER_TO_TICKS(m_last_timestamp),
             CYCLE_COUNTER_GET_CYCLES(m_last_timestamp),
             CYCLE_COUNTER_GET_TICKS(m_last_timestamp)
             );
        }
    }
    
    *tag = IEC61883_TAG_WITH_CIP;
    *sy = 0;
    
    // update the frame counter
    m_framecounter+=nevents;
    if(m_framecounter>m_period) {
       retval=RAW1394_ISO_DEFER;
    }
    
    m_PacketStat.mark(freebob_ringbuffer_read_space(m_event_buffer)/(4*m_dimension));
	
    return retval;

}

bool AmdtpTransmitStreamProcessor::isOnePeriodReady()
{ 
    //return true;
     return (m_framecounter > (int)m_period); 
}
 
bool AmdtpTransmitStreamProcessor::prefill() {
    int i=m_nb_buffers;
    while(i--) {
        if(!transferSilence(m_period)) {
            debugFatal("Could not prefill transmit stream\n");
            return false;
        }
    }
    
    // and we should also provide enough prefill for the
    // SYT processing delay
/*    if(!transferSilence((m_framerate * RECEIVE_PROCESSING_DELAY)/TICKS_PER_SECOND)) {
        debugFatal("Could not prefill transmit stream\n");
        return false;
    }*/
    
    // the framecounter should be pulled back to
    // make sure the ISO buffering is used 
    // we are using 1 period of iso buffering
//     m_framecounter=-m_period;
    
    // should this also be pre-buffered?
    //m_framecounter=-(m_framerate * RECEIVE_PROCESSING_DELAY)/TICKS_PER_SECOND;
    
    return true;
    
}

bool AmdtpTransmitStreamProcessor::reset() {

    debugOutput( DEBUG_LEVEL_VERBOSE, "Resetting...\n");

    // reset the event buffer, discard all content
    freebob_ringbuffer_reset(m_event_buffer);
    
    // reset the statistics
	m_PeriodStat.reset();
    m_PacketStat.reset();
    m_WakeupStat.reset();
    
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

bool AmdtpTransmitStreamProcessor::prepare() {
    m_PeriodStat.setName("XMT PERIOD");
    m_PacketStat.setName("XMT PACKET");
    m_WakeupStat.setName("XMT WAKEUP");

    debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing...\n");
    
    // prepare all non-device specific stuff
    // i.e. the iso stream and the associated ports
    if(!TransmitStreamProcessor::prepare()) {
        debugFatal("Could not prepare base class\n");
        return false;
    }
    
    switch (m_framerate) {
    case 32000:
        m_syt_interval = 8;
        m_fdf = IEC61883_FDF_SFC_32KHZ;
        break;
    case 44100:
        m_syt_interval = 8;
        m_fdf = IEC61883_FDF_SFC_44K1HZ;
        break;
    default:
    case 48000:
        m_syt_interval = 8;
        m_fdf = IEC61883_FDF_SFC_48KHZ;
        break;
    case 88200:
        m_syt_interval = 16;
        m_fdf = IEC61883_FDF_SFC_88K2HZ;
        break;
    case 96000:
        m_syt_interval = 16;
        m_fdf = IEC61883_FDF_SFC_96KHZ;
        break;
    case 176400:
        m_syt_interval = 32;
        m_fdf = IEC61883_FDF_SFC_176K4HZ;
        break;
    case 192000:
        m_syt_interval = 32;
        m_fdf = IEC61883_FDF_SFC_192KHZ;
        break;
    }
    
    iec61883_cip_init (
        &m_cip_status, 
        IEC61883_FMT_AMDTP, 
        m_fdf,
        m_framerate, 
        m_dimension, 
        m_syt_interval);

    // allocate the event buffer
    unsigned int ringbuffer_size_frames=m_nb_buffers * m_period;
    
    // add the processing delay
    ringbuffer_size_frames+=(m_framerate * RECEIVE_PROCESSING_DELAY)/TICKS_PER_SECOND;
    
    if( !(m_event_buffer=freebob_ringbuffer_create(
            (m_dimension * ringbuffer_size_frames) * sizeof(quadlet_t)))) {
        debugFatal("Could not allocate memory event ringbuffer");
// 		return -ENOMEM;
        return false;
    }

    // allocate the temporary cluster buffer
    if( !(m_cluster_buffer=(char *)calloc(m_dimension,sizeof(quadlet_t)))) {
        debugFatal("Could not allocate temporary cluster buffer");
        freebob_ringbuffer_free(m_event_buffer);
        return false;
// 		return -ENOMEM;
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
                debugWarning("---------------- ! Doing hardcoded test setup ! --------------\n");
                // buffertype and datatype are dependant on the API
                if(!(*it)->setBufferType(Port::E_PointerBuffer)) {
                    debugFatal("Could not set buffer type");
                    return false;
                }
                if(!(*it)->useExternalBuffer(true)) {
                    debugFatal("Could not set external buffer usage");
                    return false;
                }
                
                if(!(*it)->setDataType(Port::E_Float)) {
                    debugFatal("Could not set data type");
                    return false;
                }
                
                
                break;
            case Port::E_Midi:
                if(!(*it)->setSignalType(Port::E_PacketSignalled)) {
                    debugFatal("Could not set signal type to PeriodSignalling");
                    return false;
                }
                
                // we use a timing unit of 10ns
                // this makes sure that for the max syt interval
                // we don't have rounding, and keeps the numbers low
                // we have 1 slot every 8 events
                // we have syt_interval events per packet
                // => syt_interval/8 slots per packet
                // packet rate is 8000pkt/sec => interval=125us
                // so the slot interval is (1/8000)/(syt_interval/8)
                // or: 1/(1000 * syt_interval) sec
                // which is 1e9/(1000*syt_interval) nsec
                // or 100000/syt_interval 'units'
                // the event interval is fixed to 320us = 32000 'units'
                if(!(*it)->useRateControl(true,(100000/m_syt_interval),32000, false)) {
                    debugFatal("Could not set signal type to PeriodSignalling");
                    return false;
                }
                
                // buffertype and datatype are dependant on the API
                debugWarning("---------------- ! Doing hardcoded test setup ! --------------\n");
                // buffertype and datatype are dependant on the API
                if(!(*it)->setBufferType(Port::E_RingBuffer)) {
                    debugFatal("Could not set buffer type");
                    return false;
                }
                if(!(*it)->setDataType(Port::E_MidiEvent)) {
                    debugFatal("Could not set data type");
                    return false;
                }
                break;
            default:
                debugWarning("Unsupported port type specified\n");
                break;
        }
    }

    // the API specific settings of the ports should already be set, 
    // as this is called from the processorManager->prepare()
    // so we can init the ports
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
    
    debugOutput( DEBUG_LEVEL_VERBOSE, "Prepared for:\n");
    debugOutput( DEBUG_LEVEL_VERBOSE, " Samplerate: %d, FDF: %d, DBS: %d, SYT: %d\n",
             m_framerate,m_fdf,m_dimension,m_syt_interval);
    debugOutput( DEBUG_LEVEL_VERBOSE, " PeriodSize: %d, NbBuffers: %d\n",
             m_period,m_nb_buffers);
    debugOutput( DEBUG_LEVEL_VERBOSE, " Port: %d, Channel: %d\n",
             m_port,m_channel);

    return true;

}

bool AmdtpTransmitStreamProcessor::transferSilence(unsigned int size) {
    /* a naive implementation would look like this: */
    
    unsigned int write_size=size*sizeof(quadlet_t)*m_dimension;
    char *dummybuffer=(char *)calloc(sizeof(quadlet_t),size*m_dimension);
    transmitSilenceBlock(dummybuffer, size, 0);

    if (freebob_ringbuffer_write(m_event_buffer,(char *)(dummybuffer),write_size) < write_size) {
        debugWarning("Could not write to event buffer\n");
    }
    
    free(dummybuffer);
    
    return true;
}

bool AmdtpTransmitStreamProcessor::transfer() {
    m_PeriodStat.mark(freebob_ringbuffer_read_space(m_event_buffer)/(4*m_dimension));

    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Transferring period...\n");
    // TODO: improve
/* a naive implementation would look like this:

    unsigned int write_size=m_period*sizeof(quadlet_t)*m_dimension;
    char *dummybuffer=(char *)calloc(sizeof(quadlet_t),m_period*m_dimension);
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
    // we received one period of frames
    // this is period_size*dimension of events
    int events2write=m_period*m_dimension;
    int bytes2write=events2write*sizeof(quadlet_t);

    /* write events2write bytes to the ringbuffer 
    *  first see if it can be done in one read.
    *  if so, ok. 
    *  otherwise write up to a multiple of clusters directly to the buffer
    *  then do the buffer wrap around using ringbuffer_write
    *  then write the remaining data directly to the buffer in a third pass 
    *  Make sure that we cannot end up on a non-cluster aligned position!
    */
    int cluster_size=m_dimension*sizeof(quadlet_t);

    while(bytes2write>0) {
        int byteswritten=0;
        
        unsigned int frameswritten=(m_period*cluster_size-bytes2write)/cluster_size;
        offset=frameswritten;
        
        freebob_ringbuffer_get_write_vector(m_event_buffer, vec);
            
        if(vec[0].len==0) { // this indicates a full event buffer
            debugError("XMT: Event buffer overrun in processor %p\n",this);
            break;
        }
            
        /* if we don't take care we will get stuck in an infinite loop
        * because we align to a cluster boundary later
        * the remaining nb of bytes in one write operation can be 
        * smaller than one cluster
        * this can happen because the ringbuffer size is always a power of 2
        */
        if(vec[0].len<cluster_size) {
            
            // encode to the temporary buffer
            xrun = transmitBlock(m_cluster_buffer, 1, offset);
            
            if(xrun<0) {
                // xrun detected
                debugError("XMT: Frame buffer underrun in processor %p\n",this);
                break;
            }
                
            // use the ringbuffer function to write one cluster 
            // the write function handles the wrap around.
            freebob_ringbuffer_write(m_event_buffer,
                         m_cluster_buffer,
                         cluster_size);
                
            // we advanced one cluster_size
            bytes2write-=cluster_size;
                
        } else { // 
            
            if(bytes2write>vec[0].len) {
                // align to a cluster boundary
                byteswritten=vec[0].len-(vec[0].len%cluster_size);
            } else {
                byteswritten=bytes2write;
            }
                
            xrun = transmitBlock(vec[0].buf,
                         byteswritten/cluster_size,
                         offset);
            
            if(xrun<0) {
                    // xrun detected
                debugError("XMT: Frame buffer underrun in processor %p\n",this);
                break;
            }

            freebob_ringbuffer_write_advance(m_event_buffer, byteswritten);
            bytes2write -= byteswritten;
        }

        // the bytes2write should always be cluster aligned
        assert(bytes2write%cluster_size==0);

    }

    return true;
}
/* 
 * write received events to the stream ringbuffers.
 */

int AmdtpTransmitStreamProcessor::transmitBlock(char *data, 
                       unsigned int nevents, unsigned int offset)
{
    int problem=0;

    for ( PortVectorIterator it = m_PeriodPorts.begin();
          it != m_PeriodPorts.end();
          ++it )
    {

        if((*it)->isDisabled()) {continue;};
        
        //FIXME: make this into a static_cast when not DEBUG?

        AmdtpPortInfo *pinfo=dynamic_cast<AmdtpPortInfo *>(*it);
        assert(pinfo); // this should not fail!!

        switch(pinfo->getFormat()) {
        case AmdtpPortInfo::E_MBLA:
            if(encodePortToMBLAEvents(static_cast<AmdtpAudioPort *>(*it), (quadlet_t *)data, offset, nevents)) {
                debugWarning("Could not encode port %s to MBLA events",(*it)->getName().c_str());
                problem=1;
            }
            break;
        case AmdtpPortInfo::E_SPDIF: // still unimplemented
            break;
        default: // ignore
            break;
        }
    }
    return problem;

}

int AmdtpTransmitStreamProcessor::transmitSilenceBlock(char *data, 
                       unsigned int nevents, unsigned int offset)
{
    int problem=0;

    for ( PortVectorIterator it = m_PeriodPorts.begin();
          it != m_PeriodPorts.end();
          ++it )
    {

        //FIXME: make this into a static_cast when not DEBUG?

        AmdtpPortInfo *pinfo=dynamic_cast<AmdtpPortInfo *>(*it);
        assert(pinfo); // this should not fail!!

        switch(pinfo->getFormat()) {
        case AmdtpPortInfo::E_MBLA:
            if(encodeSilencePortToMBLAEvents(static_cast<AmdtpAudioPort *>(*it), (quadlet_t *)data, offset, nevents)) {
                debugWarning("Could not encode port %s to MBLA events",(*it)->getName().c_str());
                problem=1;
            }
            break;
        case AmdtpPortInfo::E_SPDIF: // still unimplemented
            break;
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
bool AmdtpTransmitStreamProcessor::encodePacketPorts(quadlet_t *data, unsigned int nevents, unsigned int dbc)
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
        AmdtpPortInfo *pinfo=dynamic_cast<AmdtpPortInfo *>(*it);
        assert(pinfo); // this should not fail!!

        // the only packet type of events for AMDTP is MIDI in mbla
        assert(pinfo->getFormat()==AmdtpPortInfo::E_Midi);
#endif
        
        AmdtpMidiPort *mp=static_cast<AmdtpMidiPort *>(*it);
        
        // we encode this directly (no function call) due to the high frequency
        /* idea:
        spec says: current_midi_port=(dbc+j)%8;
        => if we start at (dbc+stream->location-1)%8 [due to location_min=1], 
        we'll start at the right event for the midi port.
        => if we increment j with 8, we stay at the right event.
        */
        // FIXME: as we know in advance how big a packet is (syt_interval) we can 
        //        predict how much loops will be present here
        // first prefill the buffer with NO_DATA's on all time muxed channels
        
        for(j = (dbc & 0x07)+mp->getLocation()-1; j < nevents; j += 8) {
        
            target_event=(quadlet_t *)(data + ((j * m_dimension) + mp->getPosition()));
            
            if(mp->canRead()) { // we can send a byte
                mp->readEvent(&byte);
                *target_event=htonl(
                    IEC61883_AM824_SET_LABEL((byte)<<16,
                                             IEC61883_AM824_LABEL_MIDI_1X));
            } else { 
                // can't send a byte, either because there is no byte,
                // or because this would exceed the maximum rate
                *target_event=htonl(
                    IEC61883_AM824_SET_LABEL(0,IEC61883_AM824_LABEL_MIDI_NO_DATA));
            }
        }

    }
        
    return ok;
}


int AmdtpTransmitStreamProcessor::encodePortToMBLAEvents(AmdtpAudioPort *p, quadlet_t *data, 
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
int AmdtpTransmitStreamProcessor::encodeSilencePortToMBLAEvents(AmdtpAudioPort *p, quadlet_t *data, 
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

/* --------------------- RECEIVE ----------------------- */

AmdtpReceiveStreamProcessor::AmdtpReceiveStreamProcessor(int port, int framerate, int dimension)
    : ReceiveStreamProcessor(port, framerate), m_dimension(dimension), m_last_timestamp(0) {


}

AmdtpReceiveStreamProcessor::~AmdtpReceiveStreamProcessor() {
    freebob_ringbuffer_free(m_event_buffer);
    free(m_cluster_buffer);

}

bool AmdtpReceiveStreamProcessor::init() {
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
AmdtpReceiveStreamProcessor::putPacket(unsigned char *data, unsigned int length, 
                  unsigned char channel, unsigned char tag, unsigned char sy, 
                  unsigned int cycle, unsigned int dropped) {
    
    enum raw1394_iso_disposition retval=RAW1394_ISO_OK;
    
    struct iec61883_packet *packet = (struct iec61883_packet *) data;
    assert(packet);
    
    // how are we going to get this right???
//     m_running=true;
    
    if((packet->fmt == 0x10) && (packet->fdf != 0xFF) && (packet->dbs>0) && (length>=2*sizeof(quadlet_t))) {
        unsigned int nevents=((length / sizeof (quadlet_t)) - 2)/packet->dbs;
        
        // signal that we're running
		if(nevents) m_running=true;
        
        // don't process the stream when it is not enabled.
        if(m_disabled) {
            return RAW1394_ISO_OK;
        }
        debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "put packet...\n");
        
        unsigned int write_size=nevents*sizeof(quadlet_t)*m_dimension;
        // add the data payload to the ringbuffer
        
        if (freebob_ringbuffer_write(m_event_buffer,(char *)(data+8),write_size) < write_size) 
        {
            debugWarning("Receive buffer overrun (cycle %d, FC=%d, PC=%d)\n", 
                 cycle, m_framecounter, m_handler->getPacketCount());
            m_xruns++;

            retval=RAW1394_ISO_DEFER;
        } else {
            retval=RAW1394_ISO_OK;
            // process all ports that should be handled on a per-packet base
            // this is MIDI for AMDTP (due to the need of DBC)
            if (!decodePacketPorts((quadlet_t *)(data+8), nevents, packet->dbc)) {
                debugWarning("Problem decoding Packet Ports\n");
                retval=RAW1394_ISO_DEFER;
            }
            
            // do the time stamp processing
            // put the last time stamp a variable
            // this will allow us to determine the 
            // actual presentation time later
            if (packet->syt != 0xFFFF) {
                 
                m_last_timestamp=ntohs(packet->syt);
                 // reconstruct the top part of the timestamp using the current cycle number
//                  m_last_timestamp |= ((cycle << 12) & 0x01FF0000);
                unsigned int syt_cycles=CYCLE_COUNTER_GET_CYCLES(m_last_timestamp);
                int new_cycles=cycle+(syt_cycles-(cycle & 0xF));
//                 m_last_timestamp &= 0xFFF; // keep only the offset
//                 m_last_timestamp |= ((new_cycles << 12) & 0x01FFF000); // add the right cycle info
                 
                 debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"R-SYT for cycle (%2d %2d)=>%2d: %5uT (%04uC + %04uT) %04X %04X %d\n",
                 cycle,cycle & 0xF,new_cycles,
                 CYCLE_COUNTER_TO_TICKS(m_last_timestamp),
                 CYCLE_COUNTER_GET_CYCLES(m_last_timestamp),
                 CYCLE_COUNTER_GET_TICKS(m_last_timestamp),
                 ntohs(packet->syt),m_last_timestamp&0xFFFF, dropped
                 );
            }
        }

        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, 
            "RCV: CH = %d, FDF = %X. SYT = %6d, DBS = %3d, DBC = %3d, FMT = %3d, LEN = %4d (%2d)\n", 
            channel, packet->fdf,
            packet->syt,
            packet->dbs,
            packet->dbc,
            packet->fmt, 
            length,
            ((length / sizeof (quadlet_t)) - 2)/packet->dbs);
        
        // update the frame counter
        m_framecounter+=nevents;
        if(m_framecounter>m_period) {
           retval=RAW1394_ISO_DEFER;
           debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"defer!\n");
        }
        
    } else {
        // discard packet
        // can be important for sync though
    }
    
    m_PacketStat.mark(freebob_ringbuffer_read_space(m_event_buffer)/(4*m_dimension));
    
    return retval;
}

// this uses SYT to determine if one period is ready
bool AmdtpReceiveStreamProcessor::isOnePeriodReady() { 
#if 0 // this code is not ready yet

    // one sample will take a number off cycle counter ticks:
    // The number of ticks per second is 24576000
    // The number of samples per second is Fs
    // therefore the number of ticks per sample is 24576000 / Fs
    // NOTE: this will be rounded!!
    float ticks_per_sample=24576000.0/m_framerate;

    // we are allowed to add some constant 
    // processing delay to the transfer delay
    // being the period size and some fixed delay
    unsigned int processing_delay=ticks_per_sample*(m_period)+RECEIVE_PROCESSING_DELAY;
    
    
    // the number of events in the buffer is
    // m_framecounter

    // we have the timestamp of the last event block:
    // m_last_timestamp
    
    // the time at which the beginning of the buffer should be
    // presented to the audio side is:
    // m_last_timestamp - (m_framecounter-m_syt_interval)*ticks_per_sample
    
    // however we have to make sure that we can transfer at least one period
    // therefore we first check if this is ok
    
     if(m_framecounter > (int)m_period) {
        // we make this signed, because this can be < 0
        unsigned int m_last_timestamp_ticks = CYCLE_COUNTER_TO_TICKS(m_last_timestamp);
        
        // add the processing delay
        int ideal_presentation_time = m_last_timestamp_ticks + processing_delay;
        unsigned int buffer_content_ticks=(int)((m_framecounter-m_syt_interval)*ticks_per_sample);
        
        // if the ideal_presentation_time is smaller than buffer_content_ticks, wraparound has occurred
        // for the cycle part of m_last_timestamp. Therefore add one second worth of ticks
        // to the cycle counter, as this is the wraparound point.
        if (ideal_presentation_time < buffer_content_ticks) ideal_presentation_time += 24576000;
        // we can now safely substract these, it will always be > 0
        ideal_presentation_time -= buffer_content_ticks;
        
        // FIXME: if we are sure, make ideal_presentation_time an unsigned int
//         assert(ideal_presentation_time>=0);
        
        
/*        if(ideal_presentation_time) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Presentation time < 0 : %d\n", ideal_presentation_time);
        }*/
        
        unsigned int current_time=m_handler->getCycleCounter() & 0x1FFFFFF;
        unsigned int current_time_ticks = CYCLE_COUNTER_TO_TICKS(current_time);

        // if the last signalled period lies in the future, we know we had wraparound of the clock
        // so add one second
//         if (current_time_ticks < m_previous_signal_ticks) current_time_ticks += 24576000;
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Periods: %d\n",m_PeriodStat.m_count);
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Timestamp : %10u ticks (%3u secs + %4u cycles + %04u ticks)\n",
            m_last_timestamp_ticks,
            CYCLE_COUNTER_GET_SECS(m_last_timestamp), 
            CYCLE_COUNTER_GET_CYCLES(m_last_timestamp), 
            CYCLE_COUNTER_GET_TICKS(m_last_timestamp)
            );
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"P-TIME    : %10d ticks (%3u secs + %4u cycles + %04u ticks)\n",
            ideal_presentation_time,
            ideal_presentation_time/24576000, 
            (ideal_presentation_time/3072) % 8000,
            ideal_presentation_time%3072
            );
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Now       : %10u ticks (%3u secs + %4u cycles + %04u ticks)\n",
            current_time_ticks, 
            CYCLE_COUNTER_GET_SECS(current_time), 
            CYCLE_COUNTER_GET_CYCLES(current_time), 
            CYCLE_COUNTER_GET_TICKS(current_time)
            );
        
        int tmp=ideal_presentation_time-current_time_ticks;
        
        // if current_time_ticks wraps around while ahead of the presentation time, we have 
        // a problem.
        // we know however that we have to wait for at max one buffer + some transmit delay
        // therefore we clip this value at 0.5 seconds
        if (tmp > 24576000/2) tmp-=24576000;
        
        if(tmp<0) {
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"SYT passed (%d ticks too late)\n",-tmp);
            if (-tmp>1000000) debugWarning("SYT VERY LATE: %d!\n",-tmp);
            return true;
        } else {
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Too early wait %d ticks\n",tmp);
            return false;
        }
    } else return false;
#endif
    if(m_framecounter > (int)m_period) {
     return true;
    }

}

void AmdtpReceiveStreamProcessor::setVerboseLevel(int l) {
	setDebugLevel(l);
 	ReceiveStreamProcessor::setVerboseLevel(l);

}


bool AmdtpReceiveStreamProcessor::reset() {

	debugOutput( DEBUG_LEVEL_VERBOSE, "Resetting...\n");

	// reset the event buffer, discard all content
	freebob_ringbuffer_reset(m_event_buffer);
	
	// reset the last timestamp
	m_last_timestamp=0;
	
	m_PeriodStat.reset();
    m_PacketStat.reset();
    m_WakeupStat.reset();

	
	// reset all non-device specific stuff
	// i.e. the iso stream and the associated ports
	if(!ReceiveStreamProcessor::reset()) {
		debugFatal("Could not do base class reset\n");
		return false;
	}
	return true;
}

bool AmdtpReceiveStreamProcessor::prepare() {

    m_PeriodStat.setName("RCV PERIOD");
    m_PacketStat.setName("RCV PACKET");
    m_WakeupStat.setName("RCV WAKEUP");

	// prepare all non-device specific stuff
	// i.e. the iso stream and the associated ports
	if(!ReceiveStreamProcessor::prepare()) {
		debugFatal("Could not prepare base class\n");
		return false;
	}
	
	debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing...\n");
	switch (m_framerate) {
	case 32000:
		m_syt_interval = 8;
		break;
	case 44100:
		m_syt_interval = 8;
		break;
	default:
	case 48000:
		m_syt_interval = 8;
		break;
	case 88200:
		m_syt_interval = 16;
		break;
	case 96000:
		m_syt_interval = 16;
		break;
	case 176400:
		m_syt_interval = 32;
		break;
	case 192000:
		m_syt_interval = 32;
		break;
	}

    // allocate the event buffer
    unsigned int ringbuffer_size_frames=m_nb_buffers * m_period;
    
    // add the processing delay
    debugOutput(DEBUG_LEVEL_VERBOSE,"Adding %u frames of SYT slack buffering...\n",(m_framerate * RECEIVE_PROCESSING_DELAY)/TICKS_PER_SECOND);
    ringbuffer_size_frames+=(m_framerate * RECEIVE_PROCESSING_DELAY)/TICKS_PER_SECOND;
    
    if( !(m_event_buffer=freebob_ringbuffer_create(
            (m_dimension * ringbuffer_size_frames) * sizeof(quadlet_t)))) {
		debugFatal("Could not allocate memory event ringbuffer");
// 		return -ENOMEM;
		return false;
	}

	// allocate the temporary cluster buffer
	if( !(m_cluster_buffer=(char *)calloc(m_dimension,sizeof(quadlet_t)))) {
		debugFatal("Could not allocate temporary cluster buffer");
		freebob_ringbuffer_free(m_event_buffer);
// 		return -ENOMEM;
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
				// buffertype and datatype are dependant on the API
				debugWarning("---------------- ! Doing hardcoded dummy setup ! --------------\n");
				// buffertype and datatype are dependant on the API
				if(!(*it)->setBufferType(Port::E_PointerBuffer)) {
					debugFatal("Could not set buffer type");
					return false;
				}
				if(!(*it)->useExternalBuffer(true)) {
					debugFatal("Could not set external buffer usage");
					return false;
				}
				if(!(*it)->setDataType(Port::E_Float)) {
					debugFatal("Could not set data type");
					return false;
				}
				break;
			case Port::E_Midi:
				if(!(*it)->setSignalType(Port::E_PacketSignalled)) {
					debugFatal("Could not set signal type to PacketSignalling");
					return false;
				}
				// buffertype and datatype are dependant on the API
				// buffertype and datatype are dependant on the API
				debugWarning("---------------- ! Doing hardcoded test setup ! --------------\n");
				// buffertype and datatype are dependant on the API
				if(!(*it)->setBufferType(Port::E_RingBuffer)) {
					debugFatal("Could not set buffer type");
					return false;
				}
				if(!(*it)->setDataType(Port::E_MidiEvent)) {
					debugFatal("Could not set data type");
					return false;
				}
				break;
			default:
				debugWarning("Unsupported port type specified\n");
				break;
		}

	}

	// the API specific settings of the ports should already be set, 
	// as this is called from the processorManager->prepare()
	// so we can init the ports
	if(!initPorts()) {
		debugFatal("Could not initialize ports!\n");
		return false;
	}

	if(!preparePorts()) {
		debugFatal("Could not initialize ports!\n");
		return false;
	}


	debugOutput( DEBUG_LEVEL_VERBOSE, "Prepared for:\n");
	debugOutput( DEBUG_LEVEL_VERBOSE, " Samplerate: %d, DBS: %d, SYT: %d\n",
		     m_framerate,m_dimension,m_syt_interval);
	debugOutput( DEBUG_LEVEL_VERBOSE, " PeriodSize: %d, NbBuffers: %d\n",
		     m_period,m_nb_buffers);
	debugOutput( DEBUG_LEVEL_VERBOSE, " Port: %d, Channel: %d\n",
		     m_port,m_channel);
	return true;

}

bool AmdtpReceiveStreamProcessor::transfer() {

    m_PeriodStat.mark(freebob_ringbuffer_read_space(m_event_buffer)/(4*m_dimension));

	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Transferring period...\n");
	
/* another naive section:	
	unsigned int read_size=m_period*sizeof(quadlet_t)*m_dimension;
	char *dummybuffer=(char *)calloc(sizeof(quadlet_t),m_period*m_dimension);
	if (freebob_ringbuffer_read(m_event_buffer,(char *)(dummybuffer),read_size) < read_size) {
		debugWarning("Could not read from event buffer\n");
	}

	receiveBlock(dummybuffer, m_period, 0);

	free(dummybuffer);
*/
	int xrun;
	unsigned int offset=0;
	
	freebob_ringbuffer_data_t vec[2];
	// we received one period of frames on each connection
	// this is period_size*dimension of events

	int events2read=m_period*m_dimension;
	int bytes2read=events2read*sizeof(quadlet_t);
	/* read events2read bytes from the ringbuffer 
	*  first see if it can be done in one read. 
	*  if so, ok. 
	*  otherwise read up to a multiple of clusters directly from the buffer
	*  then do the buffer wrap around using ringbuffer_read
	*  then read the remaining data directly from the buffer in a third pass 
	*  Make sure that we cannot end up on a non-cluster aligned position!
	*/
	int cluster_size=m_dimension*sizeof(quadlet_t);
	
	while(bytes2read>0) {
		unsigned int framesread=(m_period*cluster_size-bytes2read)/cluster_size;
		offset=framesread;
		
		int bytesread=0;

		freebob_ringbuffer_get_read_vector(m_event_buffer, vec);
			
		if(vec[0].len==0) { // this indicates an empty event buffer
			debugError("RCV: Event buffer underrun in processor %p\n",this);
			break;
		}
			
		/* if we don't take care we will get stuck in an infinite loop
		* because we align to a cluster boundary later
		* the remaining nb of bytes in one read operation can be smaller than one cluster
		* this can happen because the ringbuffer size is always a power of 2
			*/
		if(vec[0].len<cluster_size) {
			// use the ringbuffer function to read one cluster 
			// the read function handles wrap around
			freebob_ringbuffer_read(m_event_buffer,m_cluster_buffer,cluster_size);

			xrun = receiveBlock(m_cluster_buffer, 1, offset);
				
			if(xrun<0) {
				// xrun detected
				debugError("RCV: Frame buffer overrun in processor %p\n",this);
				break;
			}
				
				// we advanced one cluster_size
			bytes2read-=cluster_size;
				
		} else { // 
			
			if(bytes2read>vec[0].len) {
					// align to a cluster boundary
				bytesread=vec[0].len-(vec[0].len%cluster_size);
			} else {
				bytesread=bytes2read;
			}
				
			xrun = receiveBlock(vec[0].buf, bytesread/cluster_size, offset);
				
			if(xrun<0) {
					// xrun detected
				debugError("RCV: Frame buffer overrun in processor %p\n",this);
				break;
			}

			freebob_ringbuffer_read_advance(m_event_buffer, bytesread);
			bytes2read -= bytesread;
		}
			
		// the bytes2read should always be cluster aligned
		assert(bytes2read%cluster_size==0);
	}

	return true;
}

/**
 * \brief write received events to the stream ringbuffers.
 */
int AmdtpReceiveStreamProcessor::receiveBlock(char *data, 
					   unsigned int nevents, unsigned int offset)
{
	int problem=0;

	for ( PortVectorIterator it = m_PeriodPorts.begin();
          it != m_PeriodPorts.end();
          ++it )
    {

        if((*it)->isDisabled()) {continue;};

		//FIXME: make this into a static_cast when not DEBUG?

		AmdtpPortInfo *pinfo=dynamic_cast<AmdtpPortInfo *>(*it);
		assert(pinfo); // this should not fail!!

		switch(pinfo->getFormat()) {
		case AmdtpPortInfo::E_MBLA:
			if(decodeMBLAEventsToPort(static_cast<AmdtpAudioPort *>(*it), (quadlet_t *)data, offset, nevents)) {
				debugWarning("Could not decode packet MBLA to port %s",(*it)->getName().c_str());
				problem=1;
			}
			break;
		case AmdtpPortInfo::E_SPDIF: // still unimplemented
			break;
	/* for this processor, midi is a packet based port 
		case AmdtpPortInfo::E_Midi:
			break;*/
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
bool AmdtpReceiveStreamProcessor::decodePacketPorts(quadlet_t *data, unsigned int nevents, unsigned int dbc)
{
	bool ok=true;
	
	quadlet_t *target_event=NULL;
	int j;
	
	for ( PortVectorIterator it = m_PacketPorts.begin();
          it != m_PacketPorts.end();
          ++it )
	{

#ifdef DEBUG
		AmdtpPortInfo *pinfo=dynamic_cast<AmdtpPortInfo *>(*it);
		assert(pinfo); // this should not fail!!

		// the only packet type of events for AMDTP is MIDI in mbla
		assert(pinfo->getFormat()==AmdtpPortInfo::E_Midi);
#endif
		AmdtpMidiPort *mp=static_cast<AmdtpMidiPort *>(*it);
		
		// we decode this directly (no function call) due to the high frequency
		/* idea:
		spec says: current_midi_port=(dbc+j)%8;
		=> if we start at (dbc+stream->location-1)%8 [due to location_min=1], 
		we'll start at the right event for the midi port.
		=> if we increment j with 8, we stay at the right event.
		*/
		// FIXME: as we know in advance how big a packet is (syt_interval) we can 
		//        predict how much loops will be present here
		for(j = (dbc & 0x07)+mp->getLocation()-1; j < nevents; j += 8) {
			target_event=(quadlet_t *)(data + ((j * m_dimension) + mp->getPosition()));
			quadlet_t sample_int=ntohl(*target_event);
			// FIXME: this assumes that 2X and 3X speed isn't used, 
			// because only the 1X slot is put into the ringbuffer
			if(IEC61883_AM824_GET_LABEL(sample_int) != IEC61883_AM824_LABEL_MIDI_NO_DATA) {
				sample_int=(sample_int >> 16) & 0x000000FF;
				if(!mp->writeEvent(&sample_int)) {
					debugWarning("Packet port events lost\n");
					ok=false;
				}
			}
		}

	}
    	
	return ok;
}

int AmdtpReceiveStreamProcessor::decodeMBLAEventsToPort(AmdtpAudioPort *p, quadlet_t *data, 
					   unsigned int offset, unsigned int nevents)
{
	unsigned int j=0;

// 	printf("****************\n");
// 	hexDumpQuadlets(data,m_dimension*4);
// 	printf("****************\n");

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
					*(buffer)=(ntohl((*target_event) ) & 0x00FFFFFF);
					buffer++;
					target_event+=m_dimension;
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
	
					unsigned int v = ntohl(*target_event) & 0x00FFFFFF;
					// sign-extend highest bit of 24-bit int
					int tmp = (int)(v << 8) / 256;
		
					*buffer = tmp * multiplier;
				
					buffer++;
					target_event+=m_dimension;
				}
			}
			break;
	}

	return 0;
}

} // end of namespace FreebobStreaming
