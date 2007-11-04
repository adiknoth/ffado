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

#ifndef __FFADO_SYTMONITOR__
#define __FFADO_SYTMONITOR__
#include "src/libstreaming/generic/IsoStream.h"
#include "src/libstreaming/util/cip.h"
#include "src/libstreaming/util/cycletimer.h"
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
        ffado_ringbuffer_t * m_cinfo_buffer;

        DECLARE_DEBUG_MODULE;
};


#endif /* __FFADO_SYTMONITOR__ */
