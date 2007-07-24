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

#include "AmdtpSlaveStreamProcessor.h"

#include "cycletimer.h"

#include <netinet/in.h>
#include <assert.h>

// in ticks
#define TRANSMIT_TRANSFER_DELAY 9000U
// the number of cycles to send a packet in advance of it's timestamp
#define TRANSMIT_ADVANCE_CYCLES 1U

namespace Streaming {

/* transmit */
AmdtpSlaveTransmitStreamProcessor::AmdtpSlaveTransmitStreamProcessor(int port, int framerate, int dimension)
        : AmdtpTransmitStreamProcessor(port, framerate, dimension)
{

}

AmdtpSlaveTransmitStreamProcessor::~AmdtpSlaveTransmitStreamProcessor() {

}

enum raw1394_iso_disposition
AmdtpSlaveTransmitStreamProcessor::getPacket(unsigned char *data, unsigned int *length,
                  unsigned char *tag, unsigned char *sy,
                  int cycle, unsigned int dropped, unsigned int max_length) {

    struct iec61883_packet *packet = (struct iec61883_packet *) data;
    if (cycle<0) return RAW1394_ISO_OK;

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

    // the difference between the cycle this
    // packet is intended for and 'now'
    int cycle_diff = diffCycles(cycle, now_cycles);

#ifdef DEBUG
    if(m_running && (cycle_diff < 0)) {
        debugWarning("Requesting packet for cycle %04d which is in the past (now=%04dcy)\n",
            cycle, now_cycles);
    }
#endif

    // as long as the cycle parameter is not in sync with
    // the current time, the stream is considered not
    // to be 'running'
    // NOTE: this works only at startup
    if (!m_running && cycle_diff >= 0 && cycle >= 0) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Xmit StreamProcessor %p started running at cycle %d\n",this, cycle);
        m_running=true;

        uint32_t ticks_to_advance = TICKS_PER_CYCLE * TRANSMIT_ADVANCE_CYCLES;

        // if cycle lies cycle_diff cycles in the future, the
        // timestamp for it corresponds to
        // now+cycle_diff * TICKS_PER_CYCLE
        ticks_to_advance += cycle_diff * TICKS_PER_CYCLE;

        // determine the 'now' time in ticks
        uint32_t cycle_timer=CYCLE_TIMER_TO_TICKS(ctr);

        cycle_timer = addTicks(cycle_timer, ticks_to_advance);
        m_data_buffer->setBufferHeadTimestamp(cycle_timer);
    }

    uint64_t ts_head, fc;
    if (!m_disabled && m_is_disabled) { // this means that we are trying to enable
        // check if we are on or past the enable point
        int cycles_past_enable=diffCycles(cycle, m_cycle_to_enable_at);

        if (cycles_past_enable >= 0) {
            m_is_disabled=false;

            debugOutput(DEBUG_LEVEL_VERBOSE,"Enabling StreamProcessor %p at %u\n", this, cycle);

            // initialize the buffer head & tail
            m_SyncSource->m_data_buffer->getBufferHeadTimestamp(&ts_head, &fc); // thread safe

            // the number of cycles the sync source lags (> 0)
            // or leads (< 0)
            int sync_lag_cycles=diffCycles(cycle, m_SyncSource->getLastCycle());

            // account for the cycle lag between sync SP and this SP
            // the last update of the sync source's timestamps was sync_lag_cycles
            // cycles before the cycle we are calculating the timestamp for.
            // if we were to use one-frame buffers, you would expect the
            // frame that is sent on cycle CT to have a timestamp T1.
            // ts_head however is for cycle CT-sync_lag_cycles, and lies
            // therefore sync_lag_cycles * TICKS_PER_CYCLE earlier than
            // T1.
            ts_head = addTicks(ts_head, (sync_lag_cycles) * TICKS_PER_CYCLE);

            ts_head = substractTicks(ts_head, TICKS_PER_CYCLE);

            // account for the number of cycles we are too late to enable
            ts_head = addTicks(ts_head, cycles_past_enable * TICKS_PER_CYCLE);

            // account for one extra packet of frames
            ts_head = substractTicks(ts_head,
                        (uint32_t)((float)m_syt_interval * m_SyncSource->m_data_buffer->getRate()));

            m_data_buffer->setBufferHeadTimestamp(ts_head);

            #ifdef DEBUG
            if ((unsigned int)m_data_buffer->getFrameCounter() != m_data_buffer->getBufferSize()) {
                debugWarning("m_data_buffer->getFrameCounter() != m_data_buffer->getBufferSize()\n");
            }
            #endif
            debugOutput(DEBUG_LEVEL_VERBOSE,"XMIT TS SET: TS=%10llu, LAG=%03d, FC=%4d\n",
                            ts_head, sync_lag_cycles, m_data_buffer->getFrameCounter());
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

    // we send a packet some cycles in advance, to avoid the
    // following situation:
    // suppose we are only a few ticks away from
    // the moment to send this packet. therefore we decide
    // not to send the packet, but send it in the next cycle.
    // This means that the next time point will be 3072 ticks
    // later, making that the timestamp will be expired when the
    // packet is sent, unless TRANSFER_DELAY > 3072.
    // this means that we need at least one cycle of extra buffering.
    uint32_t ticks_to_advance = TICKS_PER_CYCLE * TRANSMIT_ADVANCE_CYCLES;

    // if cycle lies cycle_diff cycles in the future, we should
    // queue this packet cycle_diff * TICKS_PER_CYCLE earlier than
    // we would if it were to be sent immediately.
    ticks_to_advance += cycle_diff * TICKS_PER_CYCLE;

    // determine the 'now' time in ticks
    uint32_t cycle_timer=CYCLE_TIMER_TO_TICKS(ctr);

    cycle_timer = addTicks(cycle_timer, ticks_to_advance);

    // time until the packet is to be sent (if > 0: send packet)
    int32_t until_next=diffTicks(ts_head, cycle_timer);

    // if until_next < 0 we should send a filled packet
    // otherwise we should send a NO-DATA packet
    if((until_next<0) && (m_running)) {
        // add the transmit transfer delay to construct the playout time (=SYT timestamp)
        uint32_t ts_packet=addTicks(ts_head, TRANSMIT_TRANSFER_DELAY);

        // if we are disabled, send a silent packet
        // and advance the buffer head timestamp
        if(m_is_disabled) {

            transmitSilenceBlock((char *)(data+8), m_syt_interval, 0);
            m_dbc += fillDataPacketHeader(packet, length, ts_packet);

            debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "XMIT SYNC: CY=%04u TSH=%011llu TSP=%011lu\n",
                cycle, ts_head, ts_packet);

            // update the base timestamp
            uint32_t ts_step=(uint32_t)((float)(m_syt_interval)
                             *m_SyncSource->m_data_buffer->getRate());

            // the next buffer head timestamp
            ts_head=addTicks(ts_head,ts_step);
            m_data_buffer->setBufferHeadTimestamp(ts_head);

            // defer to make sure we get to be enabled asap
            return RAW1394_ISO_DEFER;

        } else { // enabled & packet due, read from the buffer
            if (m_data_buffer->readFrames(m_syt_interval, (char *)(data + 8))) {
                m_dbc += fillDataPacketHeader(packet, length, ts_packet);

                // process all ports that should be handled on a per-packet base
                // this is MIDI for AMDTP (due to the need of DBC)
                if (!encodePacketPorts((quadlet_t *)(data+8), m_syt_interval, packet->dbc)) {
                    debugWarning("Problem encoding Packet Ports\n");
                }

                debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "XMIT DATA: CY=%04u TSH=%011llu TSP=%011lu\n",
                    cycle, ts_head, ts_packet);

                return RAW1394_ISO_OK;

            } else if (now_cycles<cycle) {
                // we can still postpone the queueing of the packets
                // because the ISO transmit packet buffer is not empty yet
                return RAW1394_ISO_AGAIN;

            } else { // there is no more data in the ringbuffer
                // compose a silent packet, we should always
                // send a valid packet
                transmitSilenceBlock((char *)(data+8), m_syt_interval, 0);
                m_dbc += fillDataPacketHeader(packet, length, ts_packet);

                debugWarning("Transmit buffer underrun (now %d, queue %d, target %d)\n",
                        now_cycles, cycle, TICKS_TO_CYCLES(ts_packet));
                // signal underrun
                m_xruns++;
                // disable the processing, will be re-enabled when
                // the xrun is handled
                m_disabled=true;
                m_is_disabled=true;

                return RAW1394_ISO_DEFER;
            }
        }

    } else { // no packet due, send no-data packet
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "XMIT NONE: CY=%04u TSH=%011llu\n",
                cycle, ts_head);

        m_dbc += fillNoDataPacketHeader(packet, length);
        return RAW1394_ISO_DEFER;
    }

    // we shouldn't get here
    return RAW1394_ISO_ERROR;

}

/* --------------------- RECEIVE ----------------------- */

AmdtpSlaveReceiveStreamProcessor::AmdtpSlaveReceiveStreamProcessor(int port, int framerate, int dimension)
    : AmdtpReceiveStreamProcessor(port, framerate, dimension)
{}

AmdtpSlaveReceiveStreamProcessor::~AmdtpSlaveReceiveStreamProcessor()
{}

enum raw1394_iso_disposition
AmdtpSlaveReceiveStreamProcessor::putPacket(unsigned char *data, unsigned int length,
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

    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"ch%2u: CY=%4u, SYT=%08X (%4ucy + %04uticks) (running=%d, disabled=%d,%d)\n",
        channel, cycle,ntohs(packet->syt),
        CYCLE_TIMER_GET_CYCLES(ntohs(packet->syt)), CYCLE_TIMER_GET_OFFSET(ntohs(packet->syt)),
        m_running,m_disabled,m_is_disabled);

    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
        "RCV: CH = %d, FDF = %X. SYT = %6d, DBS = %3d, DBC = %3d, FMT = %3d, LEN = %4d\n",
        channel, packet->fdf,
        packet->syt,
        packet->dbs,
        packet->dbc,
        packet->fmt,
        length);

#endif
    // check our enable status
    if (!m_disabled && m_is_disabled) { // this means that we are trying to enable
        // check if we are on or past the enable point
        int cycles_past_enable=diffCycles(cycle, m_cycle_to_enable_at);

        if (cycles_past_enable >= 0) {
            m_is_disabled=false;
            debugOutput(DEBUG_LEVEL_VERBOSE,"Enabling StreamProcessor %p at %d (SYT=%04X)\n",
                this, cycle, ntohs(packet->syt));
            // the previous timestamp is the one we need to start with
            // because we're going to update the buffer again this loop
            // using writeframes
            m_data_buffer->setBufferTailTimestamp(m_last_timestamp);

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

    // check if this is a valid packet
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
            m_last_timestamp, m_handler->getWakeupInterval(),m_syt_interval,getTicksPerFrame());

        //=> signal that we're running (if we are)
        if(!m_running && nevents && m_last_timestamp2 && m_last_timestamp) {
            debugOutput(DEBUG_LEVEL_VERBOSE,"Receive StreamProcessor %p started running at %d\n", this, cycle);
            m_running=true;
        }

        //=> don't process the stream samples when it is not enabled.
        if(m_is_disabled) {

            // we keep track of the timestamp here
            // this makes sure that we will have a somewhat accurate
            // estimate as to when a period might be ready. i.e. it will not
            // be ready earlier than this timestamp + period time

            // the next (possible) sample is not this one, but lies
            // SYT_INTERVAL * rate later
            uint64_t ts=addTicks(m_last_timestamp,
                                 (uint64_t)((float)m_syt_interval * getTicksPerFrame()));

            // set the timestamp as if there will be a sample put into
            // the buffer by the next packet.
            m_data_buffer->setBufferTailTimestamp(ts);

            return RAW1394_ISO_DEFER;
        }

        #ifdef DEBUG_OFF
        if((cycle % 1000) == 0)
        {
            uint32_t syt = (uint32_t)ntohs(packet->syt);
            uint32_t now=m_handler->getCycleTimer();
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

} // end of namespace Streaming
