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
