//
// C++ Implementation: streamstatistics
//
// Description: 
//
//
// Author: Pieter Palmers, (C) 2006
//
// Copyright: See COPYING file that comes with this distribution
//
//
#include "streamstatistics.h"
#include <stdio.h>

namespace FreebobStreaming {

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
