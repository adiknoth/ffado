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

#ifndef __FFADO_PACKETBUFFER__
#define __FFADO_PACKETBUFFER__

#include "../debugmodule/debugmodule.h"
#include <libraw1394/raw1394.h>
#include "libutil/ringbuffer.h"

namespace Streaming {

class PacketBuffer {
// note: all sizes in quadlets
public:

    PacketBuffer(int headersize, int buffersize, int max_packetsize)
       : m_headersize(headersize), m_buffersize(buffersize), m_max_packetsize(max_packetsize),
         payload_buffer(0), header_buffer(0), len_buffer(0)
    {};

    virtual ~PacketBuffer();
    void setVerboseLevel(int l) { setDebugLevel( l ); };

    int initialize();

    void flush();

    int addPacket(quadlet_t *packet, int packet_len);

    int getNextPacket(quadlet_t *packet, int packet_len);
    int getBufferFillPackets();
    int getBufferFillPayload();

protected:
    int m_headersize;
    int m_buffersize;
    int m_max_packetsize;

    ffado_ringbuffer_t *payload_buffer;
    ffado_ringbuffer_t *header_buffer;
    ffado_ringbuffer_t *len_buffer;

    DECLARE_DEBUG_MODULE;

};

}

#endif /* __FFADO_PACKETBUFFER__ */


