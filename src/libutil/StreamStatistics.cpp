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

#include "StreamStatistics.h"
#include <stdio.h>

namespace Streaming {

StreamStatistics::StreamStatistics()
    : m_name("")
    , m_count(0)
    , m_average(0.0)
    , m_min(0x7FFFFFFF)
    , m_max(0)
    , m_sum(0)
{

}


StreamStatistics::~StreamStatistics()
{
}

void StreamStatistics::mark(int value) {
    if(value>m_max) m_max=value;
    if(value<m_min) m_min=value;
    m_count++;
    m_sum+=value;
    m_average=(1.0*m_sum)/(1.0*m_count);
}

void StreamStatistics::dumpInfo() {
     printf("--- Stats for %s: min=%ld avg=%f max=%ld cnt=%ld sum=%ld\n",m_name.c_str(),
         m_min,m_average,m_max,m_count,m_sum);
}

void StreamStatistics::reset() {
    m_count=0;
    m_average= 0.0;
    m_min=0x7FFFFFFF;
    m_max=0;
    m_sum=0;
}

}
