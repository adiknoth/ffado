/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * FFADO is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FFADO is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FFADO; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 *
 */

#include "SytMonitor.h"
#include "src/libstreaming/IsoStream.h"

#include <netinet/in.h>
#include <assert.h>


IMPL_DEBUG_MODULE( SytMonitor, SytMonitor, DEBUG_LEVEL_VERBOSE );


/* --------------------- RECEIVE ----------------------- */

SytMonitor::SytMonitor(int port)
    : IsoStream(IsoStream::EST_Receive, port) {
    m_cinfo_buffer=ffado_ringbuffer_create(16384*sizeof(struct cycle_info));

}

SytMonitor::~SytMonitor() {
    ffado_ringbuffer_free(m_cinfo_buffer);

}

enum raw1394_iso_disposition
SytMonitor::putPacket(unsigned char *data, unsigned int length,
                  unsigned char channel, unsigned char tag, unsigned char sy,
                  unsigned int cycle, unsigned int dropped) {
    enum raw1394_iso_disposition retval=RAW1394_ISO_OK;

    struct iec61883_packet *packet = (struct iec61883_packet *) data;
    unsigned int syt_timestamp=ntohs(packet->syt);

    if (syt_timestamp == 0xFFFF) {
        return RAW1394_ISO_OK;
    }

    uint32_t m_full_timestamp;
    uint32_t m_full_timestamp_ticks;

    debugOutput(DEBUG_LEVEL_VERBOSE,"%2u,%2u: CY=%4u, SYT=%08X (%3u secs + %4u cycles + %04u ticks)\n",
        m_port,channel,
        cycle,syt_timestamp,
        CYCLE_TIMER_GET_SECS(syt_timestamp),
        CYCLE_TIMER_GET_CYCLES(syt_timestamp),
        CYCLE_TIMER_GET_OFFSET(syt_timestamp)

        );

    // reconstruct the full cycle
    unsigned int cc=m_handler->getCycleTimer();

    unsigned int cc_cycles=CYCLE_TIMER_GET_CYCLES(cc);

    unsigned int cc_seconds=CYCLE_TIMER_GET_SECS(cc);


    // the cycletimer has wrapped since this packet was received
    // we want cc_seconds to reflect the 'seconds' at the point this
    // was received
    if (cycle>cc_cycles) cc_seconds--;

    m_full_timestamp_ticks=sytRecvToFullTicks((uint32_t)syt_timestamp, cycle, cc);
    m_full_timestamp=TICKS_TO_CYCLE_TIMER(m_full_timestamp_ticks);

    struct cycle_info cif;
    cif.cycle=cycle;
    cif.seconds=cc_seconds;

    cif.syt=packet->syt;
    cif.full_syt=m_full_timestamp;

    // now we reconstruct the presentation time
    cif.pres_seconds = CYCLE_TIMER_GET_SECS(m_full_timestamp);
    cif.pres_cycle   = CYCLE_TIMER_GET_CYCLES(m_full_timestamp);
    cif.pres_offset  = CYCLE_TIMER_GET_OFFSET(m_full_timestamp);
    cif.pres_ticks   = m_full_timestamp_ticks;

    if (cif.pres_offset != (syt_timestamp & 0xFFF)) {
        debugOutput(DEBUG_LEVEL_NORMAL,"P-offset error: %04X != %04X (P: %08X, R: %08X)\n",
        cif.pres_offset, syt_timestamp & 0xFFF, m_full_timestamp, syt_timestamp);
    }
    if ((cif.pres_cycle & 0xF) != ((syt_timestamp & 0xF000)>>12)) {
        debugOutput(DEBUG_LEVEL_NORMAL,"P-cycle error: %01X != %01X (P: %08X, R: %08X)\n",
            cif.pres_cycle &0xF, (syt_timestamp & 0xF000)>>12, m_full_timestamp, syt_timestamp);
    }

    putCycleInfo(&cif);

    return retval;
}

bool SytMonitor::putCycleInfo(struct cycle_info *cif) {
    if(ffado_ringbuffer_write(m_cinfo_buffer,(char *)cif,sizeof(struct cycle_info))
        !=sizeof(struct cycle_info)) {
        return false;
    }
    return true;
}

bool SytMonitor::readNextCycleInfo(struct cycle_info *cif) {
    if(ffado_ringbuffer_peek(m_cinfo_buffer,(char *)cif,sizeof(struct cycle_info))
        !=sizeof(struct cycle_info)) {
        return false;
    }
    return true;

}

bool SytMonitor::consumeNextCycleInfo() {
    struct cycle_info cif;
    if(ffado_ringbuffer_read(m_cinfo_buffer,(char *)&cif,sizeof(struct cycle_info))
        !=sizeof(struct cycle_info)) {
        return false;
    }
    return true;
}

void SytMonitor::dumpInfo()
{
    IsoStream::dumpInfo();
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Ringbuffer fill: %d\n",
       ffado_ringbuffer_read_space(m_cinfo_buffer)/sizeof(struct cycle_info));
};
