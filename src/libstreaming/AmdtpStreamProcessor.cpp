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
    
    m_last_cycle=cycle;
    
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Xmit handler for cycle %d, (running=%d, enabled=%d,%d)\n",
        cycle, m_running, m_disabled, m_is_disabled);
    
#ifdef DEBUG
    if(dropped>0) {
        debugWarning("Dropped %d packets on cycle %d\n",dropped, cycle);
    }
#endif
    
    // calculate & preset common values
    
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
    
    *tag = IEC61883_TAG_WITH_CIP;
    *sy = 0;
    
    // determine if we want to send a packet or not
    // note that we can't use getCycleTimer directly here,
    // because packets are queued in advance. This means that
    // we the packet we are constructing will be sent out 
    // on 'cycle', not 'now'.
    unsigned int ctr=m_handler->getCycleTimer();
    int now_cycles = (int)CYCLE_TIMER_GET_CYCLES(ctr);
    
    // the difference between 'now' and the cycle this
    // packet is intended for
    int cycle_diff = substractCycles(cycle, now_cycles);
    
#ifdef DEBUG
    if(cycle_diff < 0) {
        debugWarning("Requesting packet for cycle %04d which is in the past (now=%04dcy)\n",
            cycle, now_cycles);
    }
#endif

    // as long as the cycle parameter is not in sync with
    // the current time, the stream is considered not
    // to be 'running'
    // NOTE: this works only at startup
    if (!m_running && cycle_diff >= 0 && cycle != -1) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Xmit StreamProcessor %p started running at cycle %d\n",this, cycle);
            m_running=true;
    }
    
    uint64_t ts_head, fc;
    if (!m_disabled && m_is_disabled) {
        // this means that we are trying to enable
        if ((unsigned int)cycle == m_cycle_to_enable_at) {
            m_is_disabled=false;
            
            debugOutput(DEBUG_LEVEL_VERBOSE,"Enabling StreamProcessor %p at %u\n", this, cycle);
            
            // initialize the buffer head & tail
            m_SyncSource->m_data_buffer->getBufferHeadTimestamp(&ts_head, &fc); // thread safe
            
            // the number of cycles the sync source lags (> 0)
            // or leads (< 0)
            int sync_lag_cycles=substractCycles(cycle, m_SyncSource->getLastCycle());
            
            // account for the cycle lag between sync SP and this SP
            // the last update of the sync source's timestamps was sync_lag_cycles
            // cycles before the cycle we are calculating the timestamp for.
            // if we were to use one-frame buffers, you would expect the 
            // frame that is sent on cycle CT to have a timestamp T1.
            // ts_head however is for cycle CT-sync_lag_cycles, and lies
            // therefore sync_lag_cycles * TICKS_PER_CYCLE earlier than
            // T1.
            ts_head += sync_lag_cycles * TICKS_PER_CYCLE;

            float ticks_per_frame=m_SyncSource->getTicksPerFrame();

            // calculate the buffer tail timestamp of our buffer
            // this timestamp corresponds to the buffer head timestamp
            // of the sync source plus the 'length' of our buffer.
            // this makes the buffer head timestamp of our buffer
            // equal to (at max) the buffer head timestamp of the 
            // sync source.
            ts_head = addTicks(ts_head, (uint32_t)((float)m_ringbuffer_size_frames * ticks_per_frame));
            
            #ifdef DEBUG
            if ((unsigned int)m_data_buffer->getFrameCounter() != m_ringbuffer_size_frames) {
                debugWarning("m_data_buffer->getFrameCounter() != m_ringbuffer_size_frames\n");
            }
            #endif

            m_data_buffer->setBufferTailTimestamp(ts_head);

            debugOutput(DEBUG_LEVEL_VERBOSE,"XMIT TS SET: TS=%10lld, FC=%4d, %f\n",
                            ts_head, m_data_buffer->getFrameCounter(), ticks_per_frame);
        } else {
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
                        "will enable StreamProcessor %p at %u, now is %d\n",
                        this, m_cycle_to_enable_at, cycle);
        }
    } else if (m_disabled && !m_is_disabled) {
        // trying to disable
        debugOutput(DEBUG_LEVEL_VERBOSE,"disabling StreamProcessor %p at %u\n", 
                    this, cycle);
        m_is_disabled=true;
    }
    
    // the base timestamp is the one of the next sample in the buffer
    m_data_buffer->getBufferHeadTimestamp(&ts_head, &fc); // thread safe
    
    int64_t timestamp = ts_head;

    // we send a packet some cycles in advance, to avoid the
    // following situation:
    // suppose we are only a few ticks away from 
    // the moment to send this packet. therefore we decide
    // not to send the packet, but send it in the next cycle.
    // This means that the next time point will be 3072 ticks
    // later, making that the timestamp will be expired when the 
    // packet is sent, unless TRANSFER_DELAY > 3072.
    // this means that we need at least one cycle of extra buffering.
    uint64_t ticks_to_advance = TICKS_PER_CYCLE * TRANSMIT_ADVANCE_CYCLES;
    
    // if cycle lies cycle_diff cycles in the future, we should
    // queue this packet cycle_diff * TICKS_PER_CYCLE earlier than
    // we would if it were to be sent immediately.
    ticks_to_advance += cycle_diff * TICKS_PER_CYCLE;

    // determine the 'now' time in ticks
    uint64_t cycle_timer=CYCLE_TIMER_TO_TICKS(ctr);
    
    // time until the packet is to be sent (if > 0: send packet)
    int64_t until_next=substractTicks(timestamp, cycle_timer + ticks_to_advance);

#ifdef DEBUG
    if(!m_is_disabled) {
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "R: TS=%011llu, NOW=%011llu, CYN=%04d, CYT=%04d\n",
            timestamp, cycle_timer, now_cycles, cycle
            );
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "    UTN=%11lld\n",
            until_next
            );
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "    CY_NOW=%04d, CY_TARGET=%04d, CY_DIFF=%04d\n",
            now_cycles, cycle, cycle_diff
            );
    }
#endif

    #ifdef DEBUG
    if((cycle % 1000) == 0) {
        uint32_t timestamp_u=timestamp;
        uint32_t syt = TICKS_TO_SYT(addTicks(timestamp_u, TRANSMIT_TRANSFER_DELAY));
        uint32_t now=m_handler->getCycleTimer();
        uint32_t now_ticks=CYCLE_TIMER_TO_TICKS(now);
        
        uint32_t test_ts=sytXmitToFullTicks(syt, cycle, now);

        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "T %04d: SYT=%08X,            CY=%02d OFF=%04d\n",
            cycle, syt, CYCLE_TIMER_GET_CYCLES(syt), CYCLE_TIMER_GET_OFFSET(syt)
            );
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "T %04d: NOW=%011lu, SEC=%03u CY=%02u OFF=%04u\n",
            cycle, now_ticks, CYCLE_TIMER_GET_SECS(now), CYCLE_TIMER_GET_CYCLES(now), CYCLE_TIMER_GET_OFFSET(now)
            );
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "T %04d: TSS=%011lu, SEC=%03u CY=%02u OFF=%04u\n",
            cycle, test_ts, TICKS_TO_SECS(test_ts), TICKS_TO_CYCLES(test_ts), TICKS_TO_OFFSET(test_ts)
            );
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "T %04d: TSO=%011lu, SEC=%03u CY=%02u OFF=%04u\n",
            cycle, timestamp_u, TICKS_TO_SECS(timestamp_u), TICKS_TO_CYCLES(timestamp_u), TICKS_TO_OFFSET(timestamp_u)
            );
    }
    #endif
    // don't process the stream when it is not enabled, not running
    // or when the next sample is not due yet.
    
    // we do have to generate (semi) valid packets
    // that means that we'll send NODATA packets.
    if((until_next>0) || m_is_disabled || !m_running) {
        // no-data packets have syt=0xFFFF
        // and have the usual amount of events as dummy data (?)
        packet->fdf = IEC61883_FDF_NODATA;
        packet->syt = 0xffff;
        
        // FIXME: either make this a setting or choose 
        bool send_payload=true;
        if(send_payload) {
            // the dbc is incremented even with no data packets
            m_dbc += m_syt_interval;
    
            // this means no-data packets with payload (DICE doesn't like that)
            *length = 2*sizeof(quadlet_t) + m_syt_interval * m_dimension * sizeof(quadlet_t);
        } else {
            // dbc is not incremented
            
            // this means no-data packets without payload
            *length = 2*sizeof(quadlet_t);
        }

        return RAW1394_ISO_DEFER;
    }
    
    // construct the packet
    
    // add the transmit transfer delay to construct the playout time (=SYT timestamp)
    uint64_t ts=addTicks(timestamp, TRANSMIT_TRANSFER_DELAY);

    unsigned int nevents = m_syt_interval;
    if (m_data_buffer->readFrames(nevents, (char *)(data + 8))) {
    
        m_dbc += m_syt_interval;
        
        packet->fdf = m_fdf;

        // convert the timestamp to SYT format
        uint16_t timestamp_SYT = TICKS_TO_SYT(ts);
        packet->syt = ntohs(timestamp_SYT);
        
        *length = nevents*sizeof(quadlet_t)*m_dimension + 8;

        // process all ports that should be handled on a per-packet base
        // this is MIDI for AMDTP (due to the need of DBC)
        if (!encodePacketPorts((quadlet_t *)(data+8), nevents, packet->dbc)) {
            debugWarning("Problem encoding Packet Ports\n");
        }

        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "XMIT: CY=%04u TS=%011llu TSS=%011llu\n",
            cycle, timestamp, ts);

        return RAW1394_ISO_OK;
        
    } else { // there is no more data in the ringbuffer

        // TODO: maybe we have to be a little smarter here
        //       because we have some slack on the device side (TRANSFER_DELAY)
        //       we can allow some skipped packets
        debugWarning("Transmit buffer underrun (now %d, queue %d, target %d)\n", 
                 now_cycles, cycle, TICKS_TO_CYCLES(ts));

        // signal underrun
        m_xruns++;

        // disable the processing, will be re-enabled when
        // the xrun is handled
        m_disabled=true;
        m_is_disabled=true;

        // compose a no-data packet, we should always
        // send a valid packet
        
        // FIXME: either make this a setting or choose 
        bool send_payload=true;
        if(send_payload) {
            // the dbc is incremented even with no data packets
            m_dbc += m_syt_interval;
    
            // this means no-data packets with payload (DICE doesn't like that)
            *length = 2*sizeof(quadlet_t) + m_syt_interval * m_dimension * sizeof(quadlet_t);
        } else {
            // dbc is not incremented
            
            // this means no-data packets without payload
            *length = 2*sizeof(quadlet_t);
        }

        return RAW1394_ISO_DEFER;
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

    debugOutput( DEBUG_LEVEL_VERBOSE, "Prefill transmit buffers...\n");
    
    if(!transferSilence(m_ringbuffer_size_frames)) {
        debugFatal("Could not prefill transmit stream\n");
        return false;
    }

    return true;
}

bool AmdtpTransmitStreamProcessor::reset() {

    debugOutput( DEBUG_LEVEL_VERBOSE, "Resetting...\n");

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

    // prepare the framerate estimate
    m_ticks_per_frame = (TICKS_PER_SECOND*1.0) / ((float)m_framerate);
    
    // allocate the event buffer
    m_ringbuffer_size_frames=m_nb_buffers * m_period;

    // add the receive processing delay
//     m_ringbuffer_size_frames+=(uint)(RECEIVE_PROCESSING_DELAY/m_ticks_per_frame);

    assert(m_data_buffer);    
    m_data_buffer->setBufferSize(m_ringbuffer_size_frames);
    m_data_buffer->setEventSize(sizeof(quadlet_t));
    m_data_buffer->setEventsPerFrame(m_dimension);
    
    m_data_buffer->setUpdatePeriod(m_period);
    m_data_buffer->setNominalRate(m_ticks_per_frame);
    
    m_data_buffer->setWrapValue(128L*TICKS_PER_SECOND);
    
    m_data_buffer->prepare();

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

bool AmdtpTransmitStreamProcessor::transferSilence(unsigned int nframes) {
    bool retval;
    
    char *dummybuffer=(char *)calloc(sizeof(quadlet_t),nframes*m_dimension);
    
    transmitSilenceBlock(dummybuffer, nframes, 0);

    // add the silence data to the ringbuffer
    if(m_data_buffer->writeFrames(nframes, dummybuffer, 0)) { 
        retval=true;
    } else {
        debugWarning("Could not write to event buffer\n");
        retval=false;
    }

    free(dummybuffer);
    
    return retval;
}

bool AmdtpTransmitStreamProcessor::canClientTransferFrames(unsigned int nbframes) {
    // there has to be enough space to put the frames in
    return m_ringbuffer_size_frames - m_data_buffer->getFrameCounter() > nbframes;
}

bool AmdtpTransmitStreamProcessor::putFrames(unsigned int nbframes, int64_t ts) {
    m_PeriodStat.mark(m_data_buffer->getBufferFill());
    
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "AmdtpTransmitStreamProcessor::putFrames(%d, %llu)\n", nbframes, ts);
    
    // The timestamp passed to this function is the time
    // of the period transfer. This means that we have to
    // add our max buffer size to it to ensure causality
    // in all cases:
    // we have to make sure that the buffer HEAD timestamp
    // lies in the future for every possible buffer fill case.
    float ticks_per_frame=m_SyncSource->getTicksPerFrame();
    ts = addTicks(ts,(uint32_t)((float)m_ringbuffer_size_frames * ticks_per_frame));
    
    // transfer the data
    m_data_buffer->blockProcessWriteFrames(nbframes, ts);

    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, " New timestamp: %llu\n", ts);

    return true;
}
/* 
 * write received events to the stream ringbuffers.
 */

bool AmdtpTransmitStreamProcessor::processWriteBlock(char *data, 
                       unsigned int nevents, unsigned int offset)
{
    bool no_problem=true;

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
                no_problem=false;
            }
            break;
        case AmdtpPortInfo::E_SPDIF: // still unimplemented
            break;
        default: // ignore
            break;
        }
    }
    return no_problem;

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
    m_last_cycle=cycle;
    
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

    if((packet->syt != 0xFFFF) 
       && (packet->fdf != 0xFF) 
       && (packet->fmt == 0x10)
       && (packet->dbs>0) 
       && (length>=2*sizeof(quadlet_t))) {
       
        unsigned int nevents=((length / sizeof (quadlet_t)) - 2)/packet->dbs;

        //=> store the previous timestamp
        m_last_timestamp2=m_last_timestamp;

        //=> convert the SYT to a full timestamp in ticks
        m_last_timestamp=sytRecvToFullTicks((uint32_t)ntohs(packet->syt), 
                                        cycle, m_handler->getCycleTimer());

        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "RECV: CY=%04u TS=%011llu\n",
                cycle, m_last_timestamp);
        
        // we have to keep in mind that there are also
        // some packets buffered by the ISO layer,
        // at most x=m_handler->getWakeupInterval()
        // these contain at most x*syt_interval
        // frames, meaning that we might receive
        // this packet x*syt_interval*ticks_per_frame
        // later than expected (the real receive time)
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"STMP: %lluticks | buff=%d, syt_interval=%d, tpf=%f\n",
            m_last_timestamp, m_handler->getWakeupInterval(),m_syt_interval,m_ticks_per_frame);
        
        m_last_timestamp += (uint64_t)(((float)m_handler->getWakeupInterval())
                                       * ((float)m_syt_interval) * m_ticks_per_frame);
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE," ==> %lluticks\n", m_last_timestamp);
        
        // the receive processing delay indicates how much
        // extra time we use as slack
        m_last_timestamp += RECEIVE_PROCESSING_DELAY;
        
        // wrap if nescessary
        m_last_timestamp=wrapAtMaxTicks(m_last_timestamp);
        
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

            // integrate the error
            m_ticks_per_frame += RECEIVE_DLL_INTEGRATION_COEFFICIENT*err;
            
        }

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
                // the previous timestamp is the one we need to start with
                // because we're going to update the buffer again this loop
                // using writeframes
                m_data_buffer->setBufferTailTimestamp(m_last_timestamp2);

            } else {
                debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
                    "will enable StreamProcessor %p at %u, now is %d\n",
                     this, m_cycle_to_enable_at, cycle);
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
            ts=wrapAtMaxTicks(ts);

            // set the timestamp as if there will be a sample put into
            // the buffer by the next packet.
            m_data_buffer->setBufferTailTimestamp(ts);
            
            return RAW1394_ISO_DEFER;
        }
        
        #ifdef DEBUG
        if((cycle % 1000) == 0) {
            uint32_t syt = (uint32_t)ntohs(packet->syt);
            uint32_t now=m_handler->getCycleTimer();
            uint32_t now_ticks=CYCLE_TIMER_TO_TICKS(now);
            
            uint32_t test_ts=sytRecvToFullTicks(syt, cycle, now);

            debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "R %04d: SYT=%08X,            CY=%02d OFF=%04d\n",
                cycle, syt, CYCLE_TIMER_GET_CYCLES(syt), CYCLE_TIMER_GET_OFFSET(syt)
                );
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "R %04d: NOW=%011lu, SEC=%03u CY=%02u OFF=%04u\n",
                cycle, now_ticks, CYCLE_TIMER_GET_SECS(now), CYCLE_TIMER_GET_CYCLES(now), CYCLE_TIMER_GET_OFFSET(now)
                );
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "R %04d: TSS=%011lu, SEC=%03u CY=%02u OFF=%04u\n",
                cycle, test_ts, TICKS_TO_SECS(test_ts), TICKS_TO_CYCLES(test_ts), TICKS_TO_OFFSET(test_ts)
                );
        }
        #endif
        
        //=> process the packet
        // add the data payload to the ringbuffer
        if(m_data_buffer->writeFrames(nevents, (char *)(data+8), m_last_timestamp)) { 
            retval=RAW1394_ISO_OK;
            
            // process all ports that should be handled on a per-packet base
            // this is MIDI for AMDTP (due to the need of DBC)
            if (!decodePacketPorts((quadlet_t *)(data+8), nevents, packet->dbc)) {
                debugWarning("Problem decoding Packet Ports\n");
                retval=RAW1394_ISO_DEFER;
            }
            
        } else {
        
            debugWarning("Receive buffer overrun (cycle %d, FC=%d, PC=%d)\n", 
                 cycle, m_data_buffer->getFrameCounter(), m_handler->getPacketCount());
            
            m_xruns++;
            
            // disable the processing, will be re-enabled when
            // the xrun is handled
            m_disabled=true;
            m_is_disabled=true;

            retval=RAW1394_ISO_DEFER;
        }
    }

    return retval;
}

int64_t AmdtpReceiveStreamProcessor::getTimeUntilNextPeriodUsecs() {
    uint64_t time_at_period=getTimeAtPeriod();
    
    uint64_t cycle_timer=m_handler->getCycleTimerTicks();
    
    // calculate the time until the next period
    int64_t until_next=substractTicks(time_at_period,cycle_timer);
    
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "=> TAP=%11llu, CTR=%11llu, UTN=%11lld, TPUS=%f\n",
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
    
    int64_t fc=m_data_buffer->getFrameCounter();
    
    int64_t next_period_boundary =  m_last_timestamp;
    next_period_boundary     += (int64_t)(((int64_t)m_period
                                          - fc) * m_ticks_per_frame);
    
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "=> NPD=%11lld, LTS=%11llu, FC=%5d, TPF=%f\n",
        next_period_boundary, m_last_timestamp, fc, m_ticks_per_frame
        );
        
    // next_period_boundary too large
    // happens if the timestamp wraps around while there are a lot of 
    // frames in the buffer. We should add the wraparound value of the ticks
    // counter
    
    // next_period_boundary too small
    // happens when the last timestamp is near wrapping, and 
    // m_framecounter is low.
    // this means: m_last_timestamp is near wrapping and have just had
    // a getPackets() from the client side. the projected next_period
    // boundary lies beyond the wrap value.
    
    // the action is to wrap the value.
    next_period_boundary=wrapAtMinMaxTicks(next_period_boundary);
    
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
    
    assert(m_data_buffer);    
    m_data_buffer->setBufferSize(ringbuffer_size_frames);
    m_data_buffer->setEventSize(sizeof(quadlet_t));
    m_data_buffer->setEventsPerFrame(m_dimension);
        
    // the buffer is written every syt_interval
    m_data_buffer->setUpdatePeriod(m_syt_interval);
    m_data_buffer->setNominalRate(m_ticks_per_frame);
    
    m_data_buffer->setWrapValue(128L*TICKS_PER_SECOND);
    
    m_data_buffer->prepare();

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
    return m_data_buffer->getFrameCounter() >= (int) nbframes;
}

bool AmdtpReceiveStreamProcessor::getFrames(unsigned int nbframes) {

    m_PeriodStat.mark(m_data_buffer->getBufferFill());

    // ask the buffer to process nbframes of frames
    // using it's registered client's processReadBlock(),
    // which should be ours
    m_data_buffer->blockProcessReadFrames(nbframes);

    return true;
}

/**
 * \brief write received events to the stream ringbuffers.
 */
bool AmdtpReceiveStreamProcessor::processReadBlock(char *data, 
					   unsigned int nevents, unsigned int offset)
{
	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "(%p)->processReadBlock(%u, %u)\n",this,nevents,offset);
	
	bool no_problem=true;

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
				no_problem=false;
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
    return no_problem;

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
