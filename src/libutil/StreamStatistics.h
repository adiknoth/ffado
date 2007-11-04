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

#ifndef FFADOSTREAMINGSTREAMSTATISTICS_H
#define FFADOSTREAMINGSTREAMSTATISTICS_H

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
