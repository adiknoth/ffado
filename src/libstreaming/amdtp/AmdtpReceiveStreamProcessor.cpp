/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software Foundation;
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#include "AmdtpReceiveStreamProcessor.h"
#include "AmdtpPort.h"

#include "../util/cycletimer.h"

#include <netinet/in.h>
#include <assert.h>

// in ticks
// as per AMDTP2.1:
// 354.17us + 125us @ 24.576ticks/usec = 11776.08192 ticks
#define DEFAULT_TRANSFER_DELAY (11776U)

#define TRANSMIT_TRANSFER_DELAY DEFAULT_TRANSFER_DELAY

namespace Streaming {

/* --------------------- RECEIVE ----------------------- */

AmdtpReceiveStreamProcessor::AmdtpReceiveStreamProcessor(int port, int framerate, int dimension)
    : ReceiveStreamProcessor(port, framerate)
    , m_dimension(dimension)
    , m_last_timestamp(0)
    , m_last_timestamp2(0)
    , m_dropped(0) 
{}

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

    int dropped_cycles=diffCycles(cycle, m_last_cycle) - 1;
    if (dropped_cycles < 0) debugWarning("(%p) dropped < 1 (%d)\n", this, dropped_cycles);
    else m_dropped += dropped_cycles;
    if (dropped_cycles > 0) debugWarning("(%p) dropped %d packets on cycle %u\n", this, dropped_cycles, cycle);

    m_last_cycle=cycle;

    struct iec61883_packet *packet = (struct iec61883_packet *) data;
    assert(packet);

#ifdef DEBUG
    if(dropped>0) {
        debugWarning("(%p) Dropped %d packets on cycle %d\n", this, dropped, cycle);
    }

    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"ch%2u: CY=%4u, SYT=%08X (%4ucy + %04uticks) (running=%d)\n",
        channel, cycle, ntohs(packet->syt),
        CYCLE_TIMER_GET_CYCLES(ntohs(packet->syt)), CYCLE_TIMER_GET_OFFSET(ntohs(packet->syt)),
        m_running);

    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
        "RCV: CH = %d, FDF = %X. SYT = %6d, DBS = %3d, DBC = %3d, FMT = %3d, LEN = %4d\n",
        channel, packet->fdf,
        packet->syt,
        packet->dbs,
        packet->dbc,
        packet->fmt,
        length);

#endif

    // check if this is a valid packet
    if((packet->syt != 0xFFFF)
       && (packet->fdf != 0xFF)
       && (packet->fmt == 0x10)
       && (packet->dbs>0)
       && (length>=2*sizeof(quadlet_t))) {

        unsigned int nevents=((length / sizeof (quadlet_t)) - 2)/packet->dbs;

        //=> store the previous timestamp
        m_last_timestamp2=m_last_timestamp;

        uint64_t nowX = m_handler->getCycleTimer();
        //=> convert the SYT to a full timestamp in ticks
        m_last_timestamp=sytRecvToFullTicks((uint32_t)ntohs(packet->syt),
                                        cycle, nowX);

        int64_t diffx = diffTicks(m_last_timestamp, m_last_timestamp2);
        if (abs(diffx) > m_syt_interval * m_data_buffer->getRate() * 1.1) {
            uint32_t now=m_handler->getCycleTimer();
            uint32_t syt = (uint32_t)ntohs(packet->syt);
            uint32_t now_ticks=CYCLE_TIMER_TO_TICKS(now);
            
            debugOutput(DEBUG_LEVEL_VERBOSE, "diff=%06lld TS=%011llu TS2=%011llu\n",
                diffx, m_last_timestamp, m_last_timestamp2);
            debugOutput(DEBUG_LEVEL_VERBOSE, "[1] cy=%04d dropped=%05llu syt=%04llX NOW=%08llX => TS=%011llu\n",
                m_last_good_cycle, m_last_dropped, m_last_syt, m_last_now, m_last_timestamp2);
            debugOutput(DEBUG_LEVEL_VERBOSE, "[2] cy=%04d dropped=%05d syt=%04X NOW=%08llX => TS=%011llu\n",
                cycle, dropped_cycles, ntohs(packet->syt), nowX, m_last_timestamp);

            uint32_t test_ts=sytRecvToFullTicks(syt, cycle, now);

            debugOutput(DEBUG_LEVEL_VERBOSE, "R %04d: SYT=%08X,            CY=%04d OFF=%04d\n",
                cycle, syt, CYCLE_TIMER_GET_CYCLES(syt), CYCLE_TIMER_GET_OFFSET(syt)
                );
            debugOutput(DEBUG_LEVEL_VERBOSE, "R %04d: NOW=%011lu, SEC=%03u CY=%04u OFF=%04u\n",
                cycle, now_ticks, CYCLE_TIMER_GET_SECS(now), CYCLE_TIMER_GET_CYCLES(now), CYCLE_TIMER_GET_OFFSET(now)
                );
            debugOutput(DEBUG_LEVEL_VERBOSE, "R %04d: TSS=%011lu, SEC=%03u CY=%04u OFF=%04u\n",
                cycle, test_ts, TICKS_TO_SECS(test_ts), TICKS_TO_CYCLES(test_ts), TICKS_TO_OFFSET(test_ts)
                );
                
            int64_t diff_ts = diffTicks(now_ticks, test_ts);
            debugOutput(DEBUG_LEVEL_VERBOSE, "DIFF  : TCK=%011lld, SEC=%03llu CY=%04llu OFF=%04llu\n",
                diff_ts, 
                TICKS_TO_SECS((uint64_t)diff_ts),
                TICKS_TO_CYCLES((uint64_t)diff_ts),
                TICKS_TO_OFFSET((uint64_t)diff_ts)
                );
        }
        m_last_syt = ntohs(packet->syt);
        m_last_now = nowX;
        m_last_good_cycle = cycle;
        m_last_dropped = dropped_cycles;

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
            m_last_timestamp, m_handler->getWakeupInterval(),m_syt_interval,getTicksPerFrame());

        //=> signal that we're running (if we are)
        if(!m_running && nevents && m_last_timestamp2 && m_last_timestamp) {
            debugOutput(DEBUG_LEVEL_VERBOSE,"Receive StreamProcessor %p started running at %d\n", this, cycle);
            m_running=true;
            m_data_buffer->setBufferTailTimestamp(m_last_timestamp);
            // we don't want this first sample to be written
            return RAW1394_ISO_OK;
        }

        // if we are not running yet, there is nothing more to do
        if(!m_running) {
            return RAW1394_ISO_OK;
        }
        #ifdef DEBUG_OFF
        if((cycle % 1000) == 0) {
            uint32_t now=m_handler->getCycleTimer();
            uint32_t syt = (uint32_t)ntohs(packet->syt);
            uint32_t now_ticks=CYCLE_TIMER_TO_TICKS(now);

            uint32_t test_ts=sytRecvToFullTicks(syt, cycle, now);

            debugOutput(DEBUG_LEVEL_VERBOSE, "R %04d: SYT=%08X,            CY=%02d OFF=%04d\n",
                cycle, syt, CYCLE_TIMER_GET_CYCLES(syt), CYCLE_TIMER_GET_OFFSET(syt)
                );
            debugOutput(DEBUG_LEVEL_VERBOSE, "R %04d: NOW=%011lu, SEC=%03u CY=%02u OFF=%04u\n",
                cycle, now_ticks, CYCLE_TIMER_GET_SECS(now), CYCLE_TIMER_GET_CYCLES(now), CYCLE_TIMER_GET_OFFSET(now)
                );
            debugOutput(DEBUG_LEVEL_VERBOSE, "R %04d: TSS=%011lu, SEC=%03u CY=%02u OFF=%04u\n",
                cycle, test_ts, TICKS_TO_SECS(test_ts), TICKS_TO_CYCLES(test_ts), TICKS_TO_OFFSET(test_ts)
                );
        }
        #endif

        #ifdef DEBUG
            // keep track of the lag
            uint32_t now=m_handler->getCycleTimer();
            int32_t diff = diffCycles( cycle,  ((int)CYCLE_TIMER_GET_CYCLES(now)) );
            m_PacketStat.mark(diff);
        #endif

        //=> process the packet
        // add the data payload to the ringbuffer
        
        if(dropped_cycles) {
            debugWarning("(%p) Correcting timestamp for dropped cycles, discarding packet...\n", this);
            m_data_buffer->setBufferTailTimestamp(m_last_timestamp);
            // we don't want this first sample to be written
            return RAW1394_ISO_OK;
        }
        
        if(m_data_buffer->writeFrames(nevents, (char *)(data+8), m_last_timestamp)) {
            retval=RAW1394_ISO_OK;

            // process all ports that should be handled on a per-packet base
            // this is MIDI for AMDTP (due to the need of DBC)
            if (!decodePacketPorts((quadlet_t *)(data+8), nevents, packet->dbc)) {
                debugWarning("Problem decoding Packet Ports\n");
                retval=RAW1394_ISO_DEFER;
            }

        } else {

//             debugWarning("Receive buffer overrun (cycle %d, FC=%d, PC=%d)\n",
//                  cycle, m_data_buffer->getFrameCounter(), m_handler->getPacketCount());

            m_xruns++;

            retval=RAW1394_ISO_DEFER;
        }
    }

    return retval;
}

void AmdtpReceiveStreamProcessor::dumpInfo() {
    StreamProcessor::dumpInfo();
}

bool AmdtpReceiveStreamProcessor::reset() {

    debugOutput( DEBUG_LEVEL_VERBOSE, "Resetting...\n");

    m_PeriodStat.reset();
    m_PacketStat.reset();
    m_WakeupStat.reset();

    m_data_buffer->setTickOffset(0);

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

    debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing (%p)...\n", this);

    // prepare all non-device specific stuff
    // i.e. the iso stream and the associated ports
    if(!ReceiveStreamProcessor::prepare()) {
        debugFatal("Could not prepare base class\n");
        return false;
    }

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
    float ticks_per_frame = (TICKS_PER_SECOND*1.0) / ((float)m_framerate);
    m_ticks_per_frame=ticks_per_frame;

    debugOutput(DEBUG_LEVEL_VERBOSE,"Initializing remote ticks/frame to %f\n",ticks_per_frame);

    // initialize internal buffer
    unsigned int ringbuffer_size_frames=m_nb_buffers * m_period;

    assert(m_data_buffer);
    m_data_buffer->setBufferSize(ringbuffer_size_frames * 2);
    m_data_buffer->setEventSize(sizeof(quadlet_t));
    m_data_buffer->setEventsPerFrame(m_dimension);

    // the buffer is written every syt_interval
    m_data_buffer->setUpdatePeriod(m_syt_interval);
    m_data_buffer->setNominalRate(ticks_per_frame);

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

bool AmdtpReceiveStreamProcessor::getFrames(unsigned int nbframes, int64_t ts) {
    m_PeriodStat.mark(m_data_buffer->getBufferFill());

#ifdef DEBUG
    uint64_t ts_head;
    signed int fc;
    int32_t lag_ticks;
    float lag_frames;

    // in order to sync up multiple received streams, we should 
    // use the ts parameter. It specifies the time of the block's 
    // first sample.
    
    ffado_timestamp_t ts_head_tmp;
    m_data_buffer->getBufferHeadTimestamp(&ts_head_tmp, &fc);
    ts_head=(uint64_t)ts_head_tmp;
    lag_ticks=diffTicks(ts, ts_head);
    float rate=m_data_buffer->getRate();
    
    assert(rate!=0.0);
    
    lag_frames=(((float)lag_ticks)/rate);
    
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "stream (%p): drifts %6d ticks = %10.5f frames (rate=%10.5f), %lld, %llu, %d\n",
                 this, lag_ticks, lag_frames,rate, ts, ts_head, fc);

    if (lag_frames>=1.0) {
        // the stream lags
        debugWarning( "stream (%p): lags  with %6d ticks = %10.5f frames (rate=%10.5f), %lld, %llu, %d\n",
                      this, lag_ticks, lag_frames,rate, ts, ts_head, fc);
    } else if (lag_frames<=-1.0) {
        // the stream leads
        debugWarning( "stream (%p): leads with %6d ticks = %10.5f frames (rate=%10.5f), %lld, %llu, %d\n",
                      this, lag_ticks, lag_frames,rate, ts, ts_head, fc);
    }
#endif
    // ask the buffer to process nbframes of frames
    // using it's registered client's processReadBlock(),
    // which should be ours
    m_data_buffer->blockProcessReadFrames(nbframes);

    return true;
}

bool AmdtpReceiveStreamProcessor::getFramesDry(unsigned int nbframes, int64_t ts) {
    m_PeriodStat.mark(m_data_buffer->getBufferFill());
    int frames_to_ditch=(int)(nbframes);
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "stream (%p): dry run %d frames (@ ts=%lld)\n",
                 this, frames_to_ditch, ts);
    char dummy[m_data_buffer->getBytesPerFrame()]; // one frame of garbage

    while (frames_to_ditch--) {
        m_data_buffer->readFrames(1, dummy);
    }
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
        => if we start at (dbc+stream->location-1)%8,
        we'll start at the right event for the midi port.
        => if we increment j with 8, we stay at the right event.
        */
        // FIXME: as we know in advance how big a packet is (syt_interval) we can
        //        predict how much loops will be present here
        for(j = (dbc & 0x07)+mp->getLocation(); j < nevents; j += 8) {
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

//     printf("****************\n");
//     hexDumpQuadlets(data,m_dimension*4);
//     printf("****************\n");

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

} // end of namespace Streaming
