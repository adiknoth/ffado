//
// C++ Interface: streamstatistics
//
// Description: 
//
//
// Author: Pieter Palmers, (C) 2006
//
// Copyright: See COPYING file that comes with this distribution
//
//
#ifndef FREEBOBSTREAMINGSTREAMSTATISTICS_H
#define FREEBOBSTREAMINGSTREAMSTATISTICS_H

#include <string>

namespace FreebobStreaming {

class StreamStatistics {
public:
    StreamStatistics();

    ~StreamStatistics();

    void setName(std::string n) {m_name=n;};
    
    void mark(int value);
    
    void dumpInfo();
    void reset();

    std::string m_name;
    
    long m_count;
    float m_average;
    long m_min;
    long m_max;
    long m_sum;
    
private:
    
};

}

#endif
