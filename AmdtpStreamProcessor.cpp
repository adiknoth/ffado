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

#include "cycletimer.h"

#include <netinet/in.h>
#include <assert.h>

#define RECEIVE_DLL_INTEGRATION_COEFFICIENT 0.015

#define RECEIVE_PROCESSING_DELAY (10000U)

// in ticks
#define TRANSMIT_TRANSFER_DELAY 9000U
// the number of cycles to send a packet in advance of it's timestamp
#define TRANSMIT_ADVANCE_CYCLES 1U

namespace FreebobStreaming {

IMPL_DEBUG_MODULE( AmdtpTransmitStreamProcessor, AmdtpTransmitStreamProcessor, DEBUG_LEVEL_NORMAL );
IMPL_DEBUG_MODULE( AmdtpReceiveStreamProcessor, AmdtpReceiveStreamProcessor, DEBUG_LEVEL_NORMAL );


/* transmit */
AmdtpTransmitStreamProcessor::AmdtpTransmitStreamProcessor(int port, int framerate, int dimension)
	: TransmitStreamProcessor(port, framerate), m_dimension(dimension)
	, m_last_timestamp(0), m_dbc(0), m_ringbuffer_size_frames(0)
	{


}

AmdtpTransmitStreamProcessor::~AmdtpTransmitStreamProcessor() {
	freebob_ringbuffer_free(m_event_buffer);
	free(m_cluster_buffer);
}

/**
 * @return 
 */
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
    unsigned int nevents=0;
    
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Xmit handler for cycle %d, (running=%d, enabled=%d,%d)\n",
        cycle, m_running, m_disabled, m_is_disabled);
    
#ifdef DEBUG
    if(dropped>0) {
        debugWarning("Dropped %d packets on cycle %d\n",dropped, cycle);
    }
#endif
    
    /* Our node ID can change after a bus reset, so it is best to fetch
     * our node ID for each packet. */
    packet->sid = getNodeId() & 0x3f;

    packet->dbs = m_dimension;
    packet->fn = 0;
    packet->qpc = 0;
    packet->sph = 0;
    packet->reserved = 0;
    packet->dbc = m_dbc;
    packet->eoh1 = 2;
    packet->fmt = IEC61883_FMT_AMDTP;

    // recalculate the buffer head timestamp
    float ticks_per_frame=m_SyncSource->getTicksPerFrame();
    
    // the base timestamp is the one of the last sample in the buffer
    uint64_t ts_tail;
    uint64_t fc;
    
    getBufferTailTimestamp(&ts_tail, &fc); // thread safe
    
    int64_t timestamp = ts_tail;
    
    // meaning that the first sample in the buffer lies m_framecounter * rate
    // earlier. This gives the next equation:
    //   timestamp = m_last_timestamp - m_framecounter * rate
    timestamp -= (int64_t)((float)fc * ticks_per_frame);
    
    // FIXME: test
    // substract the receive transfer delay
    timestamp -= RECEIVE_PROCESSING_DELAY;
    
    // this happens if m_buffer_tail_timestamp wraps around while there are 
    // not much frames in the buffer. We should add the wraparound value of the ticks
    // counter
    if (timestamp < 0) {
        timestamp += TICKS_PER_SECOND * 128L;
    }
    // this happens when the last timestamp is near wrapping, and 
    // m_framecounter is low.
    // this means: m_last_timestamp is near wrapping and have just had
    // a getPackets() from the client side. the projected next_period
    // boundary lies beyond the wrap value.
    // the action is to wrap the value.
    else if (timestamp >= TICKS_PER_SECOND * 128L) {
        timestamp -= TICKS_PER_SECOND * 128L;
    }
    
    // determine if we want to send a packet or not
    // note that we can't use getCycleTimer directly here,
    // because packets are queued in advance. This means that
    // we the packet we are constructing will be sent out 
    // on 'cycle', not 'now'.
    unsigned int ctr=m_handler->getCycleTimer();
    int now_cycles = (int)CYCLE_TIMER_GET_CYCLES(ctr);
    
    // the difference between 'now' and the cycle this
    // packet is intended for
    int cycle_diff = cycle - now_cycles;
    
    // detect wraparounds
    if(cycle_diff < -(int)(CYCLES_PER_SECOND/2)) {
        cycle_diff += CYCLES_PER_SECOND;
    } else if(cycle_diff > (int)(CYCLES_PER_SECOND/2)) {
        cycle_diff -= CYCLES_PER_SECOND;
    }

    // as long as the cycle parameter is not in sync with
    // the current time, the stream is considered not
    // to be 'running'
    if (!m_running && cycle_diff >= 0 && cycle != -1) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Xmit StreamProcessor %p started running at cycle %d\n",this, cycle);
            m_running=true;
    }
    
#ifdef DEBUG
    if(cycle_diff < 0) {
        debugWarning("Requesting packet for cycle %04d which is in the past (now=%04dcy)\n",
            cycle, now_cycles);
    }
#endif
    // if cycle lies cycle_diff cycles in the future, we should
    // queue this packet cycle_diff * TICKS_PER_CYCLE earlier than
    // we would if it were to be sent immediately.
    
    // determine the 'now' time in ticks
    uint64_t cycle_timer=CYCLE_TIMER_TO_TICKS(ctr);
    
    // time until the packet is to be sent (if > 0: send packet)
    int64_t until_next=timestamp-(int64_t)cycle_timer;
    
#ifdef DEBUG
    int64_t utn2=until_next; // debug!!
#endif

    // we send a packet some cycles in advance, to avoid the
    // following situation:
    // suppose we are only a few ticks away from 
    // the moment to send this packet. therefore we decide
    // not to send the packet, but send it in the next cycle.
    // This means that the next time point will be 3072 ticks
    // later, making that the timestamp will be expired when the 
    // packet is sent, unless TRANSFER_DELAY > 3072.
    // this means that we need at least one cycle of extra buffering.
    until_next -= TICKS_PER_CYCLE * TRANSMIT_ADVANCE_CYCLES;
    
    // we have to queue it cycle_diff * TICKS_PER_CYCLE earlier
    until_next -= cycle_diff * TICKS_PER_CYCLE;

    // the maximal difference we can allow (64secs)
    const int64_t max=TICKS_PER_SECOND*64L;

#ifdef DEBUG
    if(!m_is_disabled) {
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "=> TS=%11llu, CTR=%11llu, FC=%5d\n",
            timestamp, cycle_timer, fc
            );
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "    UTN=%11lld, UTN2=%11lld\n",
            until_next, utn2
            );
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "    CY_NOW=%04d, CY_TARGET=%04d, CY_DIFF=%04d\n",
            now_cycles, cycle, cycle_diff
            );
    }
#endif

    if(until_next > max) {
        // this means that cycle_timer has wrapped, but
        // timestamp has not. we should unwrap cycle_timer
        // by adding TICKS_PER_SECOND*128L, meaning that we should substract
        // this value from until_next            
        until_next -= TICKS_PER_SECOND*128L;
    } else if (until_next < -max) {
        // this means that timestamp has wrapped, but
        // cycle_timer has not. we should unwrap timestamp
        // by adding TICKS_PER_SECOND*128L, meaning that we should add
        // this value from until_next
        until_next += TICKS_PER_SECOND*128L;
    }

#ifdef DEBUG
    if(!m_is_disabled) {
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, " > TS=%11llu, CTR=%11llu, FC=%5d\n",
            timestamp, cycle_timer, fc
            );
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "    UTN=%11lld, UTN2=%11lld\n",
            until_next, utn2
            );
    }
#endif

    if (!m_disabled && m_is_disabled) {
        // this means that we are trying to enable
        if ((unsigned int)cycle == m_cycle_to_enable_at) {
            m_is_disabled=false;
            debugOutput(DEBUG_LEVEL_VERBOSE,"Enabling StreamProcessor %p at %u\n", this, cycle);
            
            // initialize the buffer head & tail
            uint64_t ts;
            uint64_t fc;
            
            debugOutput(DEBUG_LEVEL_VERBOSE,"Preparing to enable...\n");
            
            m_SyncSource->getBufferHeadTimestamp(&ts, &fc); // thread safe
            
            // recalculate the buffer head timestamp
            float ticks_per_frame=m_SyncSource->getTicksPerFrame();

            // set buffer head timestamp
            // this makes that the next sample to be sent out
            // has the same timestamp as the last one received
            // plus one frame
            ts += (uint64_t)ticks_per_frame;
            if (ts >= TICKS_PER_SECOND * 128L) {
                ts -= TICKS_PER_SECOND * 128L;
            }
            
            setBufferHeadTimestamp(ts);
            int64_t timestamp = ts;
        
            // since we have frames_in_buffer frames in the buffer, 
            // we know that the buffer tail lies
            // frames_in_buffer * rate 
            // later
            int frames_in_buffer=getFrameCounter();
            timestamp += (int64_t)((float)frames_in_buffer * ticks_per_frame);
            
            // this happens when the last timestamp is near wrapping, and 
            // m_framecounter is low.
            // this means: m_last_timestamp is near wrapping and have just had
            // a getPackets() from the client side. the projected next_period
            // boundary lies beyond the wrap value.
            // the action is to wrap the value.
            if (timestamp >= TICKS_PER_SECOND * 128L) {
                timestamp -= TICKS_PER_SECOND * 128L;
            }
        
            StreamProcessor::setBufferTailTimestamp(timestamp);
            
            debugOutput(DEBUG_LEVEL_VERBOSE,"XMIT TS SET: TS=%10lld, TSTMP=%10llu, FC=%4d, %f\n",
                            ts, timestamp, frames_in_buffer, ticks_per_frame);

        } else {
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"will enable StreamProcessor %p at %u, now is %d\n", this, m_cycle_to_enable_at, cycle);
        }
    } else if (m_disabled && !m_is_disabled) {
        // trying to disable
        debugOutput(DEBUG_LEVEL_VERBOSE,"disabling StreamProcessor %p at %u\n", this, cycle);
        m_is_disabled=true;
    }

    // don't process the stream when it is not enabled, not running
    // or when the next sample is not due yet.
    
    // we do have to generate (semi) valid packets
    // that means that we'll send NODATA packets.
    // we don't add payload because DICE devices don't like that.
    if((until_next>0) || m_is_disabled || !m_running) {
        // no-data packets have syt=0xFFFF
        // and have the usual amount of events as dummy data (?)
        packet->fdf = IEC61883_FDF_NODATA;
        packet->syt = 0xffff;
        
        // the dbc is incremented even with no data packets
        m_dbc += m_syt_interval;

        // this means no-data packets with payload (DICE doesn't like that)
        *length = 2*sizeof(quadlet_t) + m_syt_interval * m_dimension * sizeof(quadlet_t);
        
        // this means no-data packets without payload
        //*length = 2*sizeof(quadlet_t);
        
        *tag = IEC61883_TAG_WITH_CIP;
        *sy = 0;
        
        return RAW1394_ISO_DEFER;
    }
    
    // construct the packet
    nevents = m_syt_interval;
    m_dbc += m_syt_interval;
    
    *tag = IEC61883_TAG_WITH_CIP;
    *sy = 0;
     
    unsigned int read_size=nevents*sizeof(quadlet_t)*m_dimension;

    if ((freebob_ringbuffer_read(m_event_buffer,(char *)(data+8),read_size)) < 
                            read_size) 
    {
        /* there is no more data in the ringbuffer */
        // convert the timestamp to SYT format
        uint64_t ts=timestamp + TRANSMIT_TRANSFER_DELAY;
        
        // check if it wrapped
        if (ts >= TICKS_PER_SECOND * 128L) {
            ts -= TICKS_PER_SECOND * 128L;
        }

        debugWarning("Transmit buffer underrun (now %d, queue %d, target %d)\n", 
                 now_cycles, cycle, TICKS_TO_CYCLES(ts));

        nevents=0;

        // TODO: we have to be a little smarter here
        //       because we have some slack on the device side (TRANSFER_DELAY)
        //       we can allow some skipped packets
        // signal underrun
        m_xruns++;

        // disable the processing, will be re-enabled when
        // the xrun is handled
        m_disabled=true;
        m_is_disabled=true;

        // compose a no-data packet, we should always
        // send a valid packet
        packet->fdf = IEC61883_FDF_NODATA;
        packet->syt = 0xffff;

        // this means no-data packets with payload (DICE doesn't like that)
        *length = 2*sizeof(quadlet_t) + m_syt_interval * m_dimension * sizeof(quadlet_t);

        // this means no-data packets without payload
        //*length = 2*sizeof(quadlet_t);

        return RAW1394_ISO_DEFER;
    } else {
        *length = read_size + 8;

        // process all ports that should be handled on a per-packet base
        // this is MIDI for AMDTP (due to the need of DBC)
        if (!encodePacketPorts((quadlet_t *)(data+8), nevents, packet->dbc)) {
            debugWarning("Problem encoding Packet Ports\n");
        }

        packet->fdf = m_fdf;

        // convert the timestamp to SYT format
        uint64_t ts=timestamp + TRANSMIT_TRANSFER_DELAY;
        
        // check if it wrapped
        if (ts >= TICKS_PER_SECOND * 128L) {
            ts -= TICKS_PER_SECOND * 128L;
        }
        
        unsigned int timestamp_SYT = TICKS_TO_SYT(ts);
        packet->syt = ntohs(timestamp_SYT);

        // calculate the new buffer head timestamp. this is
        // the previous buffer head timestamp plus
        // the number of frames sent * ticks_per_frame
        timestamp += (int64_t)((float)nevents * ticks_per_frame );
        
        // check if it wrapped
        if (timestamp >= TICKS_PER_SECOND * 128L) {
            timestamp -= TICKS_PER_SECOND * 128L;
        }
        
        // update the frame counter such that it reflects the new value
        // also update the buffer head timestamp
        // done in the SP base class
        if (!StreamProcessor::getFrames(nevents, timestamp)) {
            debugError("Could not do StreamProcessor::getFrames(%d, %llu)\n",nevents, timestamp);
             return RAW1394_ISO_ERROR;
        }
        
        return RAW1394_ISO_OK;
    }

    // we shouldn't get here
    return RAW1394_ISO_ERROR;

}

int64_t AmdtpTransmitStreamProcessor::getTimeUntilNextPeriodUsecs() {
    debugFatal("IMPLEMENT ME!");
    return 0;
}

uint64_t AmdtpTransmitStreamProcessor::getTimeAtPeriodUsecs() {
    // then we should convert this into usecs
    // FIXME: we assume that the TimeSource of the IsoHandler is
    //        in usecs.
    return m_handler->mapToTimeSource(getTimeAtPeriod());
}

uint64_t AmdtpTransmitStreamProcessor::getTimeAtPeriod() {
    debugFatal("IMPLEMENT ME!");
    
    return 0;
}

bool AmdtpTransmitStreamProcessor::prefill() {
    if(!transferSilence(m_ringbuffer_size_frames)) {
        debugFatal("Could not prefill transmit stream\n");
        return false;
    }

    // when the buffer is prefilled, we should
    // also initialize the base timestamp
    // this base timestamp is the timestamp of the
    // last buffer transfer.
    uint64_t ts;
    uint64_t fc;
    m_SyncSource->getBufferHeadTimestamp(&ts, &fc); // thread safe

    // update the frame counter such that it reflects the buffer content,
    // the buffer tail timestamp is initialized when the SP is enabled
    // done in the SP base class
    if (!StreamProcessor::putFrames(m_ringbuffer_size_frames, ts)) {
        debugError("Could not do StreamProcessor::putFrames(%d, %0)\n",
            m_ringbuffer_size_frames);
        return false;
    }

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
    m_ringbuffer_size_frames=m_nb_buffers * m_period;
    
    // prepare the framerate estimate
    m_ticks_per_frame = (TICKS_PER_SECOND*1.0) / ((float)m_framerate);
    
    // add the receive processing delay
//     m_ringbuffer_size_frames+=(uint)(RECEIVE_PROCESSING_DELAY/m_ticks_per_frame);
    
    if( !(m_event_buffer=freebob_ringbuffer_create(
            (m_dimension * m_ringbuffer_size_frames) * sizeof(quadlet_t)))) {
        debugFatal("Could not allocate memory event ringbuffer");
        return false;
    }

    // allocate the temporary cluster buffer
    if( !(m_cluster_buffer=(char *)calloc(m_dimension,sizeof(quadlet_t)))) {
        debugFatal("Could not allocate temporary cluster buffer");
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
    
    // prefilling is done in ...()
    // because at that point the streams are running, 
    // while here they are not.
    
    // prefill the event buffer
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

bool AmdtpTransmitStreamProcessor::prepareForStart() {

    return true;
}

bool AmdtpTransmitStreamProcessor::prepareForStop() {
    disable();
    return true;
}

bool AmdtpTransmitStreamProcessor::prepareForEnable() {

    debugOutput(DEBUG_LEVEL_VERBOSE,"Preparing to enable...\n");

    if (!StreamProcessor::prepareForEnable()) {
        debugError("StreamProcessor::prepareForEnable failed\n");
        return false;
    }

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

bool AmdtpTransmitStreamProcessor::canClientTransferFrames(unsigned int nbframes) {
    // there has to be enough space to put the frames in
    return m_ringbuffer_size_frames - getFrameCounter() > nbframes;
}

bool AmdtpTransmitStreamProcessor::putFrames(unsigned int nbframes, int64_t ts) {
    m_PeriodStat.mark(freebob_ringbuffer_read_space(m_event_buffer)/(4*m_dimension));

    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Transferring period...\n");
    int xrun;
    unsigned int offset=0;
    
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "AmdtpTransmitStreamProcessor::putFrames(%d, %llu)\n",nbframes, ts);
    
    freebob_ringbuffer_data_t vec[2];
    // we received one period of frames
    // this is period_size*dimension of events
    unsigned int events2write=nbframes*m_dimension;
    unsigned int bytes2write=events2write*sizeof(quadlet_t);

    /* write events2write bytes to the ringbuffer 
    *  first see if it can be done in one read.
    *  if so, ok. 
    *  otherwise write up to a multiple of clusters directly to the buffer
    *  then do the buffer wrap around using ringbuffer_write
    *  then write the remaining data directly to the buffer in a third pass 
    *  Make sure that we cannot end up on a non-cluster aligned position!
    */
    unsigned int cluster_size=m_dimension*sizeof(quadlet_t);

    while(bytes2write>0) {
        int byteswritten=0;
        
        unsigned int frameswritten=(nbframes*cluster_size-bytes2write)/cluster_size;
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
                break; // FIXME: return false ?
            }

            freebob_ringbuffer_write_advance(m_event_buffer, byteswritten);
            bytes2write -= byteswritten;
        }

        // the bytes2write should always be cluster aligned
        assert(bytes2write%cluster_size==0);

    }
    
    // recalculate the buffer tail timestamp
    float ticks_per_frame=m_SyncSource->getTicksPerFrame();
    
    // this makes that the last sample to be sent out on ISO
    // has the same timestamp as the last one transfered
    // to the client
    // plus one frame
    ts += (uint64_t)ticks_per_frame;
    int64_t timestamp = ts;
    
    // however we have to preserve causality, meaning that we have to make
    // sure that the worst-case buffer head timestamp still lies in the future.
    // this worst case timestamp occurs when the xmit buffer is completely full.
    // therefore we add m_ringbuffer_size_frames * ticks_per_frame to the timestamp.
    // this will make sure that the buffer head timestamp lies in the future.
    // the netto effect of this is that the system works as if the buffer processing
    // by the client doesn't take time.
    
    timestamp += (int64_t)((float)m_ringbuffer_size_frames * ticks_per_frame);
    
    // wrap the timestamp if nescessary
    if (timestamp >= TICKS_PER_SECOND * 128L) {
        timestamp -= TICKS_PER_SECOND * 128L;
    }

    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "StreamProcessor::putFrames(%d, %llu)\n",nbframes, timestamp);

    // update the frame counter such that it reflects the new value,
    // and also update the buffer tail timestamp
    // done in the SP base class
    if (!StreamProcessor::putFrames(nbframes, timestamp)) {
        debugError("Could not do StreamProcessor::putFrames(%d, %llu)\n",nbframes, timestamp);
        return false;
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
    unsigned int j;
    
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
    : ReceiveStreamProcessor(port, framerate), m_dimension(dimension), m_last_timestamp(0), m_last_timestamp2(0) {


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

#ifdef DEBUG
    if(dropped>0) {
        debugWarning("Dropped %d packets on cycle %d\n",dropped, cycle);
    }
#endif

    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"ch%2u: CY=%4u, SYT=%08X (%4ucy + %04uticks) (running=%d, disabled=%d,%d)\n",
        channel, cycle,ntohs(packet->syt),  
        CYCLE_TIMER_GET_CYCLES(ntohs(packet->syt)), CYCLE_TIMER_GET_OFFSET(ntohs(packet->syt)),
        m_running,m_disabled,m_is_disabled);

    if((packet->fmt == 0x10) && (packet->fdf != 0xFF) && (packet->syt != 0xFFFF) && (packet->dbs>0) && (length>=2*sizeof(quadlet_t))) {
        unsigned int nevents=((length / sizeof (quadlet_t)) - 2)/packet->dbs;

        //=> store the previous timestamp
        m_last_timestamp2=m_last_timestamp;

        //=> convert the SYT to ticks
        unsigned int syt_timestamp=ntohs(packet->syt);

        debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"ch%2u: CY=%4u, SYT=%08X (%4u cycles + %04u ticks), FC=%04d, %d\n",
            channel, cycle,syt_timestamp,  
            CYCLE_TIMER_GET_CYCLES(syt_timestamp), CYCLE_TIMER_GET_OFFSET(syt_timestamp),
            getFrameCounter(), m_is_disabled);
        
        // reconstruct the full cycle
        unsigned int cc=m_handler->getCycleTimer();
        unsigned int cc_cycles=CYCLE_TIMER_GET_CYCLES(cc);
        unsigned int cc_seconds=CYCLE_TIMER_GET_SECS(cc);
        
        // the cycletimer has wrapped since this packet was received
        // we want cc_seconds to reflect the 'seconds' at the point this 
        // was received
        if (cycle>cc_cycles) {
            if (cc_seconds) {
                cc_seconds--;
            } else {
                // seconds has wrapped around, so we'd better not substract 1
                // the good value is 127
                cc_seconds=127;
            }
        }
        
        // reconstruct the top part of the timestamp using the current cycle number
        unsigned int now_cycle_masked=cycle & 0xF;
        unsigned int syt_cycle=CYCLE_TIMER_GET_CYCLES(syt_timestamp);
        
        // if this is true, wraparound has occurred, undo this wraparound
        if(syt_cycle<now_cycle_masked) syt_cycle += 0x10;
        
        // this is the difference in cycles wrt the cycle the
        // timestamp was received
        unsigned int delta_cycles=syt_cycle-now_cycle_masked;
        
        // reconstruct the cycle part of the timestamp
        unsigned int new_cycles=cycle + delta_cycles;
        
        // if the cycles cause a wraparound of the cycle timer,
        // perform this wraparound
        // and convert the timestamp into ticks
        if(new_cycles<8000) {
            m_last_timestamp  = new_cycles * TICKS_PER_CYCLE;
        } else {
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
                "Detected wraparound: %d + %d = %d\n",
                cycle,delta_cycles,new_cycles);
            
            new_cycles-=8000; // wrap around
            m_last_timestamp  = new_cycles * TICKS_PER_CYCLE;
            // add one second due to wraparound
            m_last_timestamp += TICKS_PER_SECOND;
        }
        
        m_last_timestamp += CYCLE_TIMER_GET_OFFSET(syt_timestamp);
        m_last_timestamp += cc_seconds * TICKS_PER_SECOND;
        
        // we have to keep in mind that there are also
        // some packets buffered by the ISO layer
        // at most x=m_handler->getNbBuffers()
        // these contain at most x*syt_interval
        // frames, meaning that we might receive
        // this packet x*syt_interval*ticks_per_frame
        // later than expected (the real receive time)
        m_last_timestamp += (uint64_t)(((float)m_handler->getNbBuffers())
                                       * m_syt_interval * m_ticks_per_frame);
        
        // the receive processing delay indicates how much
        // extra time we need as slack
        m_last_timestamp += RECEIVE_PROCESSING_DELAY;

        //=> now estimate the device frame rate
        if (m_last_timestamp2 && m_last_timestamp) {
            // try and estimate the frame rate from the device
            
            // first get the measured difference between both 
            // timestamps
            int64_t measured_difference;
            measured_difference=((int64_t)(m_last_timestamp))
                               -((int64_t)(m_last_timestamp2));
            // correct for seconds wraparound
            if (m_last_timestamp<m_last_timestamp2) {
                measured_difference+=128L*TICKS_PER_SECOND;
            }

            // implement a 1st order DLL to estimate the framerate
            // this is the number of ticks between two samples
            float f=measured_difference;
            float err = f / (1.0*m_syt_interval) - m_ticks_per_frame;
            
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"SYT: %08X | STMP: %lluticks | DLL: in=%f, current=%f, err=%e\n",syt_timestamp, m_last_timestamp, f,m_ticks_per_frame,err);

#ifdef DEBUG
            // this helps to detect wraparound issues
            if(f > 1.5*((TICKS_PER_SECOND*1.0) / m_framerate)*m_syt_interval) {
                debugWarning("Timestamp diff more than 50%% of the nominal diff too large!\n");
                debugWarning(" SYT: %08X | STMP: %llu,%llu | DLL: in=%f, current=%f, err=%e\n",syt_timestamp, m_last_timestamp, m_last_timestamp2, f,m_ticks_per_frame,err);
                debugWarning(" CC: %08X | CC_CY: %u | CC_SEC: %u | SYT_CY: %u | NEW_CY: %u\n",
                    cc, cc_cycles, cc_seconds, syt_cycle,new_cycles);
                
            }
            if(f < 0.5*((TICKS_PER_SECOND*1.0) / m_framerate)*m_syt_interval) {
                debugWarning("Timestamp diff more than 50%% of the nominal diff too small!\n");
                debugWarning(" SYT: %08X | STMP: %llu,%llu | DLL: in=%f, current=%f, err=%e\n",syt_timestamp, m_last_timestamp, m_last_timestamp2, f,m_ticks_per_frame,err);
            }
#endif
            // integrate the error
            m_ticks_per_frame += RECEIVE_DLL_INTEGRATION_COEFFICIENT*err;
            
        }
        
         debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"R-SYT for cycle (%2d %2d)=>%2d: %5uT (%04uC + %04uT) %04X %04X %d\n",
            cycle,now_cycle_masked,delta_cycles,
            (m_last_timestamp),
            TICKS_TO_CYCLES(m_last_timestamp),
            TICKS_TO_OFFSET(m_last_timestamp),
            ntohs(packet->syt),TICKS_TO_CYCLE_TIMER(m_last_timestamp)&0xFFFF, dropped
         );

        //=> signal that we're running (if we are)
        if(!m_running && nevents && m_last_timestamp2 && m_last_timestamp) {
            debugOutput(DEBUG_LEVEL_VERBOSE,"Receive StreamProcessor %p started running at %d\n", this, cycle);
            m_running=true;
        }

        //=> don't process the stream samples when it is not enabled.
        if (!m_disabled && m_is_disabled) {
            // this means that we are trying to enable
            if (cycle == m_cycle_to_enable_at) {
                m_is_disabled=false;
                debugOutput(DEBUG_LEVEL_VERBOSE,"enabling StreamProcessor %p at %d\n", this, cycle);
            } else {
                debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"will enable StreamProcessor %p at %u, now is %d\n", this, m_cycle_to_enable_at, cycle);
            }
        } else if (m_disabled && !m_is_disabled) {
            // trying to disable
            debugOutput(DEBUG_LEVEL_VERBOSE,"disabling StreamProcessor %p at %u\n", this, cycle);
            m_is_disabled=true;
        }
        
        if(m_is_disabled) {

            // we keep track of the timestamp here
            // this makes sure that we will have a somewhat accurate
            // estimate as to when a period might be ready. i.e. it will not
            // be ready earlier than this timestamp + period time
            
            // the next (possible) sample is not this one, but lies 
            // SYT_INTERVAL * rate later
            uint64_t ts=m_last_timestamp+(uint64_t)((float)m_syt_interval * m_ticks_per_frame);
            
            // wrap if nescessary
            if (ts >= TICKS_PER_SECOND * 128L) {
                ts -= TICKS_PER_SECOND * 128L;
            }
            // set the timestamps
            StreamProcessor::setBufferTimestamps(ts,ts);
            
            return RAW1394_ISO_DEFER;
        }
        
        //=> process the packet
        unsigned int write_size=nevents*sizeof(quadlet_t)*m_dimension;
        
        // add the data payload to the ringbuffer
        if (freebob_ringbuffer_write(m_event_buffer,(char *)(data+8),write_size) < write_size) 
        {
            debugWarning("Receive buffer overrun (cycle %d, FC=%d, PC=%d)\n", 
                 cycle, getFrameCounter(), m_handler->getPacketCount());
            
            m_xruns++;
            
            // disable the processing, will be re-enabled when
            // the xrun is handled
            m_disabled=true;
            m_is_disabled=true;

            retval=RAW1394_ISO_DEFER;
        } else {
            retval=RAW1394_ISO_OK;
            // process all ports that should be handled on a per-packet base
            // this is MIDI for AMDTP (due to the need of DBC)
            if (!decodePacketPorts((quadlet_t *)(data+8), nevents, packet->dbc)) {
                debugWarning("Problem decoding Packet Ports\n");
                retval=RAW1394_ISO_DEFER;
            }
        }

#ifdef DEBUG
        if(packet->dbs) {
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE, 
                "RCV %04d: CH = %d, FDF = %X. SYT = %6d, DBS = %3d, DBC = %3d, FMT = %3d, LEN = %4d (%2d)\n", 
                cycle, channel, packet->fdf,
                packet->syt,
                packet->dbs,
                packet->dbc,
                packet->fmt, 
                length,
                ((length / sizeof (quadlet_t)) - 2)/packet->dbs);
        }
#endif

        // update the frame counter such that it reflects the new value,
        // and also update the buffer tail timestamp, as we add new frames
        // done in the SP base class
        if (!StreamProcessor::putFrames(nevents, m_last_timestamp)) {
            debugError("Could not do StreamProcessor::putFrames(%d, %llu)\n",nevents, m_last_timestamp);
            return RAW1394_ISO_ERROR;
        }

    } 
    
    return retval;
}

int64_t AmdtpReceiveStreamProcessor::getTimeUntilNextPeriodUsecs() {
    uint64_t time_at_period=getTimeAtPeriod();
    
    uint64_t cycle_timer=m_handler->getCycleTimerTicks();
    
    int64_t until_next=time_at_period-cycle_timer;
    
    // the maximal difference we can allow (64secs)
    const int64_t max=TICKS_PER_SECOND*64L;
    
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "=> TAP=%11llu, CTR=%11llu, UTN=%11lld, TPUS=%f\n",
        time_at_period, cycle_timer, until_next, m_handler->getTicksPerUsec()
        );
        
    if(until_next > max) {
        // this means that cycle_timer has wrapped, but
        // time_at_period has not. we should unwrap cycle_timer
        // by adding TICKS_PER_SECOND*128L, meaning that we should substract
        // this value from until_next            
        until_next -= TICKS_PER_SECOND*128L;
    } else if (until_next < -max) {
        // this means that time_at_period has wrapped, but
        // cycle_timer has not. we should unwrap time_at_period
        // by adding TICKS_PER_SECOND*128L, meaning that we should add
        // this value from until_next
        until_next += TICKS_PER_SECOND*128L;
    }
    
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "   TAP=%11llu, CTR=%11llu, UTN=%11lld, TPUS=%f\n",
        time_at_period, cycle_timer, until_next, m_handler->getTicksPerUsec()
        );
    
    // now convert to usecs
    // don't use the mapping function because it only works
    // for absolute times, not the relative time we are
    // using here (which can also be negative).
    return (int64_t)(((float)until_next) / m_handler->getTicksPerUsec());
}

uint64_t AmdtpReceiveStreamProcessor::getTimeAtPeriodUsecs() {
    // then we should convert this into usecs
    // FIXME: we assume that the TimeSource of the IsoHandler is
    //        in usecs.
    return m_handler->mapToTimeSource(getTimeAtPeriod());
}

uint64_t AmdtpReceiveStreamProcessor::getTimeAtPeriod() {

    // every time a packet is received both the framecounter and the base
    // timestamp are updated. This means that at any instance of time, the 
    // front of the buffer (latest sample) timestamp is known.
    // As we know the number of frames in the buffer, and we now the rate
    // in ticks/frame, we can calculate the back of buffer timestamp as:
    //    back_of_buffer_time = front_time - nbframes * rate
    // the next period boundary time lies m_period frames later:
    //    next_period_boundary = back_of_buffer_time + m_period * rate
    
    // NOTE: we should account for the fact that the timestamp is not for
    //       the latest sample, but for the latest sample minus syt_interval-1
    //       because it is the timestamp for the first sample in the packet,
    //       while the complete packet contains SYT_INTERVAL samples.
    //       this makes the equation:
    //          back_of_buffer_time = front_time - (nbframes - (syt_interval - 1)) * rate
    //          next_period_boundary = back_of_buffer_time + m_period * rate

    // NOTE: where do we add the processing delay?
    //       if we add it here:
    //          next_period_boundary += RECEIVE_PROCESSING_DELAY
    
    // the complete equation now is:
    // next_period_boundary = front_time - (nbframes - (syt_interval - 1)) * rate
    //                        + m_period * rate + RECEIVE_PROCESSING_DELAY
    // since syt_interval is a constant value, we can equally well ignore it, as
    // if it were already included in RECEIVE_PROCESSING_DELAY
    // making the equation (simplified:
    // next_period_boundary = front_time + (-nbframes + m_period) * rate
    //                        + RECEIVE_PROCESSING_DELAY
    // currently this is in ticks
    
    int64_t fc=getFrameCounter();
    
    int64_t next_period_boundary =  m_last_timestamp;
    next_period_boundary     += (int64_t)(((int64_t)m_period
                                          - fc) * m_ticks_per_frame);
    
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "=> NPD=%11lld, LTS=%11llu, FC=%5d, TPF=%f\n",
        next_period_boundary, m_last_timestamp, fc, m_ticks_per_frame
        );
    
    // this happens if the timestamp wraps around while there are a lot of 
    // frames in the buffer. We should add the wraparound value of the ticks
    // counter
    if (next_period_boundary < 0) {
        next_period_boundary += TICKS_PER_SECOND * 128L;
    }
    // this happens when the last timestamp is near wrapping, and 
    // m_framecounter is low.
    // this means: m_last_timestamp is near wrapping and have just had
    // a getPackets() from the client side. the projected next_period
    // boundary lies beyond the wrap value.
    // the action is to wrap the value.
    else if (next_period_boundary >= TICKS_PER_SECOND * 128L) {
        next_period_boundary -= TICKS_PER_SECOND * 128L;
    }
    
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "   NPD=%11lld, LTS=%11llu, FC=%5d, TPF=%f\n",
        next_period_boundary, m_last_timestamp, fc, m_ticks_per_frame
        );

    return next_period_boundary;
}

void AmdtpReceiveStreamProcessor::dumpInfo()
{

    StreamProcessor::dumpInfo();
    
	debugOutputShort( DEBUG_LEVEL_NORMAL, "  Device framerate  : %f\n", 24576000.0/m_ticks_per_frame);

}


void AmdtpReceiveStreamProcessor::setVerboseLevel(int l) {
	setDebugLevel(l);
 	ReceiveStreamProcessor::setVerboseLevel(l);

}


bool AmdtpReceiveStreamProcessor::reset() {

    debugOutput( DEBUG_LEVEL_VERBOSE, "Resetting...\n");

    // reset the event buffer, discard all content
    freebob_ringbuffer_reset(m_event_buffer);
    
    m_PeriodStat.reset();
    m_PacketStat.reset();
    m_WakeupStat.reset();
    
    // this needs to be reset to the nominal value
    // because xruns can cause the DLL value to shift a lot
    // making that we run into problems when trying to re-enable 
    // streaming
    m_ticks_per_frame = (TICKS_PER_SECOND*1.0) / ((float)m_framerate);

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

    // prepare the framerate estimate
    m_ticks_per_frame = (TICKS_PER_SECOND*1.0) / ((float)m_framerate);

    debugOutput(DEBUG_LEVEL_VERBOSE,"Initializing remote ticks/frame to %f\n",m_ticks_per_frame);

    // allocate the event buffer
    unsigned int ringbuffer_size_frames=m_nb_buffers * m_period;
    
    // add the processing delay
    debugOutput(DEBUG_LEVEL_VERBOSE,"Adding %u frames of SYT slack buffering...\n",
        (uint)(RECEIVE_PROCESSING_DELAY/m_ticks_per_frame));
    ringbuffer_size_frames+=(uint)(RECEIVE_PROCESSING_DELAY/m_ticks_per_frame);
    
    if( !(m_event_buffer=freebob_ringbuffer_create(
            (m_dimension * ringbuffer_size_frames) * sizeof(quadlet_t)))) {
		debugFatal("Could not allocate memory event ringbuffer");
		return false;
	}

	// allocate the temporary cluster buffer
	if( !(m_cluster_buffer=(char *)calloc(m_dimension,sizeof(quadlet_t)))) {
		debugFatal("Could not allocate temporary cluster buffer");
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

bool AmdtpReceiveStreamProcessor::prepareForStart() {
    disable();
    return true;
}

bool AmdtpReceiveStreamProcessor::prepareForStop() {
    disable();
    return true;
}

bool AmdtpReceiveStreamProcessor::canClientTransferFrames(unsigned int nbframes) {
    return getFrameCounter() >= (int) nbframes;
}

bool AmdtpReceiveStreamProcessor::getFrames(unsigned int nbframes, int64_t ts) {

    m_PeriodStat.mark(freebob_ringbuffer_read_space(m_event_buffer)/(4*m_dimension));

	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Transferring period...\n");
	
	int xrun;
	unsigned int offset=0;
	
	freebob_ringbuffer_data_t vec[2];
	// we received one period of frames on each connection
	// this is period_size*dimension of events

	unsigned int events2read=nbframes*m_dimension;
	unsigned int bytes2read=events2read*sizeof(quadlet_t);
	/* read events2read bytes from the ringbuffer 
	*  first see if it can be done in one read. 
	*  if so, ok. 
	*  otherwise read up to a multiple of clusters directly from the buffer
	*  then do the buffer wrap around using ringbuffer_read
	*  then read the remaining data directly from the buffer in a third pass 
	*  Make sure that we cannot end up on a non-cluster aligned position!
	*/
	unsigned int cluster_size=m_dimension*sizeof(quadlet_t);
	
	while(bytes2read>0) {
		unsigned int framesread=(nbframes*cluster_size-bytes2read)/cluster_size;
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
	
    // update the frame counter such that it reflects the new value,
    // and also update the buffer head timestamp as we pull frames
    // done in the SP base class

    // wrap the timestamp if nescessary
    if (ts < 0) {
        ts += TICKS_PER_SECOND * 128L;
    } else if (ts >= TICKS_PER_SECOND * 128L) {
        ts -= TICKS_PER_SECOND * 128L;
    }
    
    if (!StreamProcessor::getFrames(nbframes, ts)) {
        debugError("Could not do StreamProcessor::getFrames(%d, %llu)\n", nbframes, ts);
        return false;
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
	unsigned int j;
	
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
