/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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

#ifndef FFADOSTREAMINGSTREAMSTATISTICS_H
#define FFADOSTREAMINGSTREAMSTATISTICS_H

#include <string>

#include "debugmodule/debugmodule.h"

#define MAX_SIGNAL_VALUE 7
namespace Streaming {

class StreamStatistics {
public:
    StreamStatistics();

    ~StreamStatistics() {};

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

    // some tools to do run statistics
    // will keep a histogram of the number of times a certain value
    // is added.
    void signal(unsigned int val);

    unsigned int m_signalled[MAX_SIGNAL_VALUE+1];

private:
    DECLARE_DEBUG_MODULE;
};

}

#endif
