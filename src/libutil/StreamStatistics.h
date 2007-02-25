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

#ifndef FREEBOBSTREAMINGSTREAMSTATISTICS_H
#define FREEBOBSTREAMINGSTREAMSTATISTICS_H

#include <string>

namespace Streaming {

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
