/* $Id$ */

/*
 *   FreeBob Streaming API
 *   FreeBob = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2005,2006,2007 Pieter Palmers <pieterpalmers@users.sourceforge.net>
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

#include "SytMonitor.h"
#include "src/libstreaming/IsoStream.h"

#include <netinet/in.h>
#include <assert.h>


IMPL_DEBUG_MODULE( SytMonitor, SytMonitor, DEBUG_LEVEL_NORMAL );


/* --------------------- RECEIVE ----------------------- */

SytMonitor::SytMonitor(int port)
    : IsoStream(IsoStream::EST_Receive, port) {
    m_cinfo_buffer=freebob_ringbuffer_create(16384*sizeof(struct cycle_info));

}

SytMonitor::~SytMonitor() {
	freebob_ringbuffer_free(m_cinfo_buffer);

}

enum raw1394_iso_disposition 
SytMonitor::putPacket(unsigned char *data, unsigned int length, 
                  unsigned char channel, unsigned char tag, unsigned char sy, 
                  unsigned int cycle, unsigned int dropped) {
    enum raw1394_iso_disposition retval=RAW1394_ISO_OK;
    
    struct iec61883_packet *packet = (struct iec61883_packet *) data;
    unsigned int syt_timestamp=ntohs(packet->syt);
    unsigned int m_full_timestamp;
    
    bool wraparound_occurred=false;
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"%2u,%2u: CY=%4u, SYT=%08X (%3u secs + %4u cycles + %04u ticks)\n",
        m_port,channel,
        cycle,syt_timestamp,
        CYCLE_COUNTER_GET_SECS(syt_timestamp), 
        CYCLE_COUNTER_GET_CYCLES(syt_timestamp), 
        CYCLE_COUNTER_GET_OFFSET(syt_timestamp)
        
        );
    
    // reconstruct the full cycle
    unsigned int cc_ticks=m_handler->getCycleCounter();
    unsigned int cc_cycles=TICKS_TO_CYCLES(cc_ticks);
    
    unsigned int cc_seconds=TICKS_TO_SECS(cc_ticks);
    
    // the cyclecounter has wrapped since this packet was received
    // we want cc_seconds to reflect the 'seconds' at the point this 
    // was received
    if (cycle>cc_cycles) cc_seconds--;

     // reconstruct the top part of the timestamp using the current cycle number
    unsigned int now_cycle_masked=cycle & 0xF;
    unsigned int syt_cycle=CYCLE_COUNTER_GET_CYCLES(syt_timestamp);
    
    // if this is true, wraparound has occurred, undo this wraparound
    if(syt_cycle<now_cycle_masked) syt_cycle += 0x10;
    
    // this is the difference in cycles wrt the cycle the
    // timestamp was received
    unsigned int delta_cycles=syt_cycle-now_cycle_masked;
    
    // reconstruct the cycle part of the timestamp
    unsigned int new_cycles=cycle + delta_cycles;
    
    // if the cycles cause a wraparound of the cycle counter,
    // perform this wraparound
    if(new_cycles>7999) {
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
            "Detected wraparound: %d + %d = %d\n",
            cycle,delta_cycles,new_cycles);
        
        new_cycles-=8000; // wrap around
        wraparound_occurred=true;
        
    }
    
    m_full_timestamp = (new_cycles) << 12;
    
    // now add the offset part on top of that
    m_full_timestamp |= (syt_timestamp & 0xFFF);
    
    // and the seconds part
    // if the timestamp causes a wraparound of the cycle counter,
    // the timestamp lies in the next second
    // if it didn't, the timestamp lies in this second
    if (wraparound_occurred) {
        m_full_timestamp |= ((cc_seconds+1 << 25) & 0xFE000000);
    } else {
        m_full_timestamp |= ((cc_seconds << 25) & 0xFE000000);
    }

    struct cycle_info cif;
    cif.cycle=cycle;
    cif.seconds=cc_seconds;
    
    cif.syt=packet->syt;
    cif.full_syt=m_full_timestamp;
    
    // now we reconstruct the presentation time
    cif.pres_seconds = CYCLE_COUNTER_GET_SECS(m_full_timestamp);
    cif.pres_cycle   = CYCLE_COUNTER_GET_CYCLES(m_full_timestamp);
    cif.pres_offset  = CYCLE_COUNTER_GET_OFFSET(m_full_timestamp);
    cif.pres_ticks   = CYCLE_COUNTER_TO_TICKS(m_full_timestamp);
    
    if (wraparound_occurred) {
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"    (P: %08X, R: %08X)\n", 
            m_full_timestamp, syt_timestamp);
    }
    
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
    if(freebob_ringbuffer_write(m_cinfo_buffer,(char *)cif,sizeof(struct cycle_info))
        !=sizeof(struct cycle_info)) {
        return false;
    }
    return true;    
}

bool SytMonitor::readNextCycleInfo(struct cycle_info *cif) {
    if(freebob_ringbuffer_peek(m_cinfo_buffer,(char *)cif,sizeof(struct cycle_info))
        !=sizeof(struct cycle_info)) {
        return false;
    }
    return true;
    
}

bool SytMonitor::consumeNextCycleInfo() {
    struct cycle_info cif;
    if(freebob_ringbuffer_read(m_cinfo_buffer,(char *)&cif,sizeof(struct cycle_info))
        !=sizeof(struct cycle_info)) {
        return false;
    }
    return true;
}

void SytMonitor::dumpInfo()
{
    IsoStream::dumpInfo();
	debugOutputShort( DEBUG_LEVEL_NORMAL, "  Ringbuffer fill: %d\n",
	   freebob_ringbuffer_read_space(m_cinfo_buffer)/sizeof(struct cycle_info));
};
