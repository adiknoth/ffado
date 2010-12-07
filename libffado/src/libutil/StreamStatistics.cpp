/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "StreamStatistics.h"
#include <stdio.h>

namespace Streaming {
IMPL_DEBUG_MODULE( StreamStatistics, StreamStatistics, DEBUG_LEVEL_VERBOSE );

StreamStatistics::StreamStatistics()
    : m_name("")
    , m_count(0)
    , m_average(0.0)
    , m_min(0x7FFFFFFF)
    , m_max(0)
    , m_sum(0)
{
    reset();
}

void StreamStatistics::mark(int value) {
    if(value>m_max) m_max=value;
    if(value<m_min) m_min=value;
    m_count++;
    m_sum+=value;
    m_average=(1.0*m_sum)/(1.0*m_count);
}

void StreamStatistics::signal(unsigned int val) {
    if (val <= MAX_SIGNAL_VALUE) {
        m_signalled[val]++;
    }
}

void StreamStatistics::dumpInfo() {
    debugOutputShort( DEBUG_LEVEL_VERBOSE, 
                      "--- Stats for %s: min=%"PRId64" avg=%f max=%"PRId64" cnt=%"PRId64" sum=%"PRId64"\n",
                      m_name.c_str(), m_min, m_average, m_max, m_count, m_sum);
    debugOutputShort( DEBUG_LEVEL_VERBOSE, "    Signal stats\n");
    for (unsigned int i=0;i <= MAX_SIGNAL_VALUE; i++) {
        debugOutputShort(DEBUG_LEVEL_VERBOSE, 
                         "     Stats for %3u: %8u\n",
                         i, m_signalled[i]);
    }
}

void StreamStatistics::reset() {
    m_count=0;
    m_average= 0.0;
    m_min=0x7FFFFFFF;
    m_max=0;
    m_sum=0;

    for (unsigned int i=0;i <= MAX_SIGNAL_VALUE; i++) {
        m_signalled[i]=0;
    }
}

}
