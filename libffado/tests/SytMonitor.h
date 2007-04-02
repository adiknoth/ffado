/* $Id$ */

/*
 *   FreeBob Streaming API
 *   FreeBob = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2007 Pieter Palmers <pieterpalmers@users.sourceforge.net>
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
#ifndef __FREEBOB_SYTMONITOR__
#define __FREEBOB_SYTMONITOR__
#include "src/libstreaming/IsoStream.h"
#include "src/libstreaming/cip.h"
#include "src/libstreaming/cycletimer.h"
#include "src/libutil/ringbuffer.h"

using namespace Streaming;

struct cycle_info {
    unsigned int cycle;
    unsigned int seconds;
    unsigned int syt;
    unsigned int full_syt;
    unsigned int pres_seconds;
    unsigned int pres_cycle;
    unsigned int pres_offset;
    uint64_t     pres_ticks;
};

class SytMonitor 
    : public IsoStream
{

    public:
        SytMonitor(int port);
        virtual ~SytMonitor();
        
		virtual enum raw1394_iso_disposition 
			putPacket(unsigned char *data, unsigned int length, 
		              unsigned char channel, unsigned char tag, unsigned char sy, 
			          unsigned int cycle, unsigned int dropped);
			          
		void dumpInfo();
		unsigned int getMaxPacketSize() {return 4096;};
		unsigned int getPacketsPerPeriod() {return 1;};
		
        bool readNextCycleInfo(struct cycle_info *cif);
        bool consumeNextCycleInfo();
        
        bool putCycleInfo(struct cycle_info *cif);
        
        IsoHandler * getHandler() {return m_handler;};
        
	protected:
	   freebob_ringbuffer_t * m_cinfo_buffer;
	
	
		DECLARE_DEBUG_MODULE;
		
	   

};


#endif /* __FREEBOB_SYTMONITOR__ */


