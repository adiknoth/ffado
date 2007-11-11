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

#include "AmdtpTransmitStreamProcessor.h"
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

/* transmit */
AmdtpTransmitStreamProcessor::AmdtpTransmitStreamProcessor(int port, int framerate, int dimension)
        : StreamProcessor(ePT_Transmit, port, framerate)
        , m_dimension(dimension)
        , m_last_timestamp(0)
        , m_dbc(0)
        , m_ringbuffer_size_frames(0)
{}

/**
 * @return
 */
bool AmdtpTransmitStreamProcessor::init() {

    debugOutput( DEBUG_LEVEL_VERBOSE, "Initializing (%p)...\n", this);
    // call the parent init
    // this has to be done before allocating the buffers,
    // because this sets the buffersizes from the processormanager
    if(!StreamProcessor::init()) {
        debugFatal("Could not do base class init (%p)\n",this);
        return false;
    }
    return true;
}

enum raw1394_iso_disposition
AmdtpTransmitStreamProcessor::getPacket(unsigned char *data, unsigned int *length,
                  unsigned char *tag, unsigned char *sy,
                  int cycle, unsigned int dropped, unsigned int max_length) {
    struct iec61883_packet *packet = (struct iec61883_packet *) data;

    if (cycle<0) {
        debugOutput(DEBUG_LEVEL_ULTRA_VERBOSE,"Xmit handler for cycle %d, (running=%d)\n",
            cycle, m_running);
        *tag = 0;
        *sy = 0;
        *length=0;
        return RAW1394_ISO_OK;
    }

    debugOutput(DEBUG_LEVEL_ULTRA_VERBOSE,"Xmit handler for cycle %d, (running=%d)\n",
        cycle, m_running);

    if (addCycles(m_last_cycle, 1) != cycle) {
        debugWarning("(%p) Dropped %d packets on cycle %d\n", diffCycles(cycle,m_last_cycle)-1, cycle);
    }

    m_last_cycle=cycle;

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

    // the difference between the cycle this
    // packet is intended for and 'now'
    int cycle_diff = diffCycles(cycle, now_cycles);

#ifdef DEBUG
    if(m_running && (cycle_diff < 0)) {
        debugWarning("Requesting packet for cycle %04d which is in the past (now=%04dcy)\n",
            cycle, now_cycles);
    }

    // keep track of the lag
    m_PacketStat.mark(cycle_diff);
#endif

    // as long as the cycle parameter is not in sync with
    // the current time, the stream is considered not
    // to be 'running'
    // NOTE: this works only at startup
    if (!m_running && cycle_diff >= 0 && cycle >= 0) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Xmit StreamProcessor %p started running at cycle %d\n",this, cycle);
            m_running=true;
    }

    signed int fc;
    uint64_t presentation_time;
    unsigned int presentation_cycle;
    int cycles_until_presentation;

    uint64_t transmit_at_time;
    unsigned int transmit_at_cycle;
    int cycles_until_transmit;

    // FIXME: should become a define
    // the absolute minimum number of cycles we want to transmit
    // a packet ahead of the presentation time. The nominal time
    // the packet is transmitted ahead of the presentation time is
    // given by TRANSMIT_TRANSFER_DELAY (in ticks), but in case we
    // are too late for that, this constant defines how late we can
    // be.
    const int min_cycles_before_presentation = 1;
    // FIXME: should become a define
    // the absolute maximum number of cycles we want to transmit
    // a packet ahead of the ideal transmit time. The nominal time
    // the packet is transmitted ahead of the presentation time is
    // given by TRANSMIT_TRANSFER_DELAY (in ticks), but we can send
    // packets early if we want to. (not completely according to spec)
    const int max_cycles_to_transmit_early = 5;

    if( !m_running || !m_data_buffer->isEnabled() ) {
        debugOutput(DEBUG_LEVEL_ULTRA_VERBOSE,
                    "Not running (%d) or buffer not enabled (enabled=%d)\n",
                    m_running, m_data_buffer->isEnabled());

        // not running or not enabled
        goto send_empty_packet;
    }

try_block_of_frames:
    debugOutput(DEBUG_LEVEL_ULTRA_VERBOSE, "Try for cycle %d\n", cycle);
    // check whether the packet buffer has packets for us to send.
    // the base timestamp is the one of the next sample in the buffer
    ffado_timestamp_t ts_head_tmp;
    m_data_buffer->getBufferHeadTimestamp(&ts_head_tmp, &fc); // thread safe

    // the timestamp gives us the time at which we want the sample block
    // to be output by the device
    presentation_time=(uint64_t)ts_head_tmp;

    // now we calculate the time when we have to transmit the sample block
    transmit_at_time = substractTicks(presentation_time, TRANSMIT_TRANSFER_DELAY);

    // calculate the cycle this block should be presented in
    // (this is just a virtual calculation since at that time it should
    //  already be in the device's buffer)
    presentation_cycle = (unsigned int)(TICKS_TO_CYCLES( presentation_time ));

    // calculate the cycle this block should be transmitted in
    transmit_at_cycle = (unsigned int)(TICKS_TO_CYCLES( transmit_at_time ));

    // we can check whether this cycle is within the 'window' we have
    // to send this packet.
    // first calculate the number of cycles left before presentation time
    cycles_until_presentation = diffCycles( presentation_cycle, cycle );

    // we can check whether this cycle is within the 'window' we have
    // to send this packet.
    // first calculate the number of cycles left before presentation time
    cycles_until_transmit = diffCycles( transmit_at_cycle, cycle );

    // two different options:
    // 1) there are not enough frames for one packet 
    //      => determine wether this is a problem, since we might still
    //         have some time to send it
    // 2) there are enough packets
    //      => determine whether we have to send them in this packet
    if (fc < (signed int)m_syt_interval) {
        m_PacketStat.signal(0);
        // not enough frames in the buffer,
        debugOutput(DEBUG_LEVEL_VERBOSE, 
                    "Insufficient frames: N=%02d, CY=%04u, TC=%04u, CUT=%04d\n",
                    fc, cycle, transmit_at_cycle, cycles_until_transmit);
        // we can still postpone the queueing of the packets
        // if we are far enough ahead of the presentation time
        if( cycles_until_presentation <= min_cycles_before_presentation ) {
            m_PacketStat.signal(1);
            // we are too late
            // meaning that we in some sort of xrun state
            // signal xrun situation ??HERE??
            m_xruns++;
            // we send an empty packet on this cycle
            goto send_empty_packet; // UGLY but effective
        } else {
            m_PacketStat.signal(2);
            // there is still time left to send the packet
            // we want the system to give this packet another go
//             goto try_packet_again; // UGLY but effective
            // unfortunatly the try_again doesn't work very well,
            // so we'll have to either usleep(one cycle) and goto try_block_of_frames
            
            // or just fill this with an empty packet
            // if we have to do this too often, the presentation time will
            // get too close and we're in trouble
            goto send_empty_packet; // UGLY but effective
        }
    } else {
        m_PacketStat.signal(3);
        // there are enough frames, so check the time they are intended for
        // all frames have a certain 'time window' in which they can be sent
        // this corresponds to the range of the timestamp mechanism:
        // we can send a packet 15 cycles in advance of the 'presentation time'
        // in theory we can send the packet up till one cycle before the presentation time,
        // however this is not very smart.
        
        // There are 3 options:
        // 1) the frame block is too early
        //      => send an empty packet
        // 2) the frame block is within the window
        //      => send it
        // 3) the frame block is too late
        //      => discard (and raise xrun?)
        //         get next block of frames and repeat
        
        if (cycles_until_transmit <= max_cycles_to_transmit_early) {
            m_PacketStat.signal(4);
            // it's time send the packet
            goto send_packet; // UGLY but effective
        } else if (cycles_until_transmit < 0) {
            // we are too late
            debugOutput(DEBUG_LEVEL_VERBOSE, 
                        "Too late: CY=%04u, TC=%04u, CUT=%04d, TSP=%011llu (%04u)\n",
                        cycle,
                        transmit_at_cycle, cycles_until_transmit,
                        presentation_time, (unsigned int)TICKS_TO_CYCLES(presentation_time));

            // however, if we can send this sufficiently before the presentation
            // time, it could be harmless.
            // NOTE: dangerous since the device has no way of reporting that it didn't get
            //       this packet on time.
            if ( cycles_until_presentation <= min_cycles_before_presentation ) {
                m_PacketStat.signal(5);
                // we are not that late and can still try to transmit the packet
                goto send_packet; // UGLY but effective
            } else { // definitely too late
                m_PacketStat.signal(6);
                // remove the samples
                m_data_buffer->dropFrames(m_syt_interval);
                // signal some xrun situation ??HERE??
                m_xruns++;
                // try a new block of frames
                goto try_block_of_frames; // UGLY but effective
            }
        } else {
            m_PacketStat.signal(7);
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE, 
                        "Too early: CY=%04u, TC=%04u, CUT=%04d, TST=%011llu (%04u), TSP=%011llu (%04u)\n",
                        cycle,
                        transmit_at_cycle, cycles_until_transmit,
                        transmit_at_time, (unsigned int)TICKS_TO_CYCLES(transmit_at_time),
                        presentation_time, (unsigned int)TICKS_TO_CYCLES(presentation_time));
            #ifdef DEBUG
            if (cycles_until_transmit > max_cycles_to_transmit_early + 1) {
                debugOutput(DEBUG_LEVEL_VERBOSE, 
                            "Way too early: CY=%04u, TC=%04u, CUT=%04d, TST=%011llu (%04u), TSP=%011llu (%04u)\n",
                            cycle,
                            transmit_at_cycle, cycles_until_transmit,
                            transmit_at_time, (unsigned int)TICKS_TO_CYCLES(transmit_at_time),
                            presentation_time, (unsigned int)TICKS_TO_CYCLES(presentation_time));
            }
            #endif
            // we are too early, send only an empty packet
            goto send_empty_packet; // UGLY but effective
        }
    }

    debugFatal("Should never reach this code!\n");
    return RAW1394_ISO_ERROR;

send_empty_packet:
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "XMIT NONE: CY=%04u, TSP=%011llu (%04u)\n",
            cycle,
            presentation_time, (unsigned int)TICKS_TO_CYCLES(presentation_time));

    m_dbc += fillNoDataPacketHeader(packet, length);
    return RAW1394_ISO_DEFER;

send_packet:
    if (m_data_buffer->readFrames(m_syt_interval, (char *)(data + 8))) {
        m_dbc += fillDataPacketHeader(packet, length, presentation_time);

        // process all ports that should be handled on a per-packet base
        // this is MIDI for AMDTP (due to the need of DBC)
        if (!encodePacketPorts((quadlet_t *)(data+8), m_syt_interval, packet->dbc)) {
            debugWarning("Problem encoding Packet Ports\n");
        }

        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "XMIT DATA: CY=%04u, TST=%011llu (%04u), TSP=%011llu (%04u)\n",
            cycle,
            transmit_at_time, (unsigned int)TICKS_TO_CYCLES(transmit_at_time),
            presentation_time, (unsigned int)TICKS_TO_CYCLES(presentation_time));

        return RAW1394_ISO_OK;
    }

// the ISO AGAIN does not work very well...
// try_packet_again:
// 
//     debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "XMIT RETRY: CY=%04u, TSP=%011llu (%04u)\n",
//             cycle,
//             presentation_time, (unsigned int)TICKS_TO_CYCLES(presentation_time));
//     return RAW1394_ISO_AGAIN;

    // else:
    debugFatal("This is impossible, since we checked the buffer size before!\n");
    return RAW1394_ISO_ERROR;
}

unsigned int AmdtpTransmitStreamProcessor::fillDataPacketHeader(
        struct iec61883_packet *packet, unsigned int* length,
        uint32_t ts) {

    packet->fdf = m_fdf;

    // convert the timestamp to SYT format
    uint16_t timestamp_SYT = TICKS_TO_SYT(ts);
    packet->syt = ntohs(timestamp_SYT);

    *length = m_syt_interval*sizeof(quadlet_t)*m_dimension + 8;

    return m_syt_interval;
}

unsigned int AmdtpTransmitStreamProcessor::fillNoDataPacketHeader(
        struct iec61883_packet *packet, unsigned int* length) {

    // no-data packets have syt=0xFFFF
    // and have the usual amount of events as dummy data (?)
    packet->fdf = IEC61883_FDF_NODATA;
    packet->syt = 0xffff;

    // FIXME: either make this a setting or choose
    bool send_payload=true;
    if(send_payload) {
        // this means no-data packets with payload (DICE doesn't like that)
        *length = 2*sizeof(quadlet_t) + m_syt_interval * m_dimension * sizeof(quadlet_t);
        return m_syt_interval;
    } else {
        // dbc is not incremented
        // this means no-data packets without payload
        *length = 2*sizeof(quadlet_t);
        return 0;
    }
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

    m_data_buffer->setTickOffset(0);

    // reset all non-device specific stuff
    // i.e. the iso stream and the associated ports
    if(!StreamProcessor::reset()) {
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

    debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing (%p)...\n", this);

    // prepare all non-device specific stuff
    // i.e. the iso stream and the associated ports
    if(!StreamProcessor::prepare()) {
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
    float ticks_per_frame = (TICKS_PER_SECOND*1.0) / ((float)m_framerate);
    m_ticks_per_frame=ticks_per_frame;

    // initialize internal buffer
    m_ringbuffer_size_frames=m_nb_buffers * m_period;

    assert(m_data_buffer);
    m_data_buffer->setBufferSize(m_ringbuffer_size_frames * 2);
    m_data_buffer->setEventSize(sizeof(quadlet_t));
    m_data_buffer->setEventsPerFrame(m_dimension);

    m_data_buffer->setUpdatePeriod(m_period);
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
    return true;
}

bool AmdtpTransmitStreamProcessor::prepareForEnable(uint64_t time_to_enable_at) {

    if (!StreamProcessor::prepareForEnable(time_to_enable_at)) {
        debugError("StreamProcessor::prepareForEnable failed\n");
        return false;
    }

    return true;
}

bool AmdtpTransmitStreamProcessor::transferSilence(unsigned int nframes) {
    bool retval;
    signed int fc;
    ffado_timestamp_t ts_tail_tmp;
    uint64_t ts_tail;
    
    // prepare a buffer of silence
    char *dummybuffer=(char *)calloc(sizeof(quadlet_t),nframes*m_dimension);
    transmitSilenceBlock(dummybuffer, nframes, 0);

    
    m_data_buffer->getBufferTailTimestamp(&ts_tail_tmp, &fc);
    if (fc != 0) {
        debugWarning("Prefilling a buffer that already contains %d frames\n", fc);
    }

    ts_tail = (uint64_t)ts_tail_tmp;
    // modify the timestamp such that it makes sense
    ts_tail = addTicks(ts_tail, (uint64_t)(nframes * getTicksPerFrame()));
    // add the silence data to the ringbuffer
    if(m_data_buffer->writeFrames(nframes, dummybuffer, ts_tail)) {
        retval=true;
    } else {
        debugWarning("Could not write to event buffer\n");
        retval=false;
    }

    free(dummybuffer);

    return retval;
}

bool AmdtpTransmitStreamProcessor::putFrames(unsigned int nbframes, int64_t ts) {
    m_PeriodStat.mark(m_data_buffer->getBufferFill());
    debugOutput(DEBUG_LEVEL_ULTRA_VERBOSE, "AmdtpTransmitStreamProcessor::putFrames(%d, %llu)\n", nbframes, ts);

    // transfer the data
    m_data_buffer->blockProcessWriteFrames(nbframes, ts);

    debugOutput(DEBUG_LEVEL_ULTRA_VERBOSE, " New timestamp: %llu\n", ts);

    return true; // FIXME: what about failure?
}

bool AmdtpTransmitStreamProcessor::putFramesDry(unsigned int nbframes, int64_t ts) {
    m_PeriodStat.mark(m_data_buffer->getBufferFill());
    debugOutput(DEBUG_LEVEL_ULTRA_VERBOSE, "AmdtpTransmitStreamProcessor::putFramesDry(%d, %llu)\n", nbframes, ts);

    bool retval;
    char dummybuffer[sizeof(quadlet_t)*nbframes*m_dimension];

    transmitSilenceBlock(dummybuffer, nbframes, 0);
    // add the silence data to the ringbuffer
    if(m_data_buffer->writeFrames(nbframes, dummybuffer, ts)) {
        retval=true;
    } else {
        debugWarning("Could not write %u events to event buffer\n", nbframes);
        retval=false;
    }

    debugOutput(DEBUG_LEVEL_ULTRA_VERBOSE, " New timestamp: %llu\n", ts);
    return retval;
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
    quadlet_t byte;

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
        => if we start at (dbc+stream->location-1)%8,
        we'll start at the right event for the midi port.
        => if we increment j with 8, we stay at the right event.
        */
        // FIXME: as we know in advance how big a packet is (syt_interval) we can
        //        predict how much loops will be present here
        // first prefill the buffer with NO_DATA's on all time muxed channels

        for(j = (dbc & 0x07)+mp->getLocation(); j < nevents; j += 8) {
            
            quadlet_t tmpval;
            
            target_event=(quadlet_t *)(data + ((j * m_dimension) + mp->getPosition()));
            
            if(mp->canRead()) { // we can send a byte
                mp->readEvent(&byte);
                byte &= 0xFF;
                tmpval=htonl(
                    IEC61883_AM824_SET_LABEL((byte)<<16,
                                             IEC61883_AM824_LABEL_MIDI_1X));

                debugOutput(DEBUG_LEVEL_ULTRA_VERBOSE, "MIDI port %s, pos=%d, loc=%d, dbc=%d, nevents=%d, dim=%d\n",
                    mp->getName().c_str(), mp->getPosition(), mp->getLocation(), dbc, nevents, m_dimension);
                debugOutput(DEBUG_LEVEL_ULTRA_VERBOSE, "base=%p, target=%p, value=%08X\n",
                    data, target_event, tmpval);
                    
            } else {
                // can't send a byte, either because there is no byte,
                // or because this would exceed the maximum rate
                tmpval=htonl(
                    IEC61883_AM824_SET_LABEL(0,IEC61883_AM824_LABEL_MIDI_NO_DATA));
            }
            
            *target_event=tmpval;
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

} // end of namespace Streaming
