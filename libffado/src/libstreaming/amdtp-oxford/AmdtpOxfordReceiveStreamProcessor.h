/*
 * Copyright (C) 2005-2009 by Pieter Palmers
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

#ifndef __FFADO_AMDTPOXFORDRECEIVESTREAMPROCESSOR__
#define __FFADO_AMDTPOXFORDRECEIVESTREAMPROCESSOR__

/**
 * This class implements IEC61883-6 / AM824 / AMDTP based streaming
 * 
 * but with all quircks required to make the oxford FW-92x devices working
 */

#include "debugmodule/debugmodule.h"

#include "../amdtp/AmdtpReceiveStreamProcessor.h"
#include "libutil/ringbuffer.h"

namespace Streaming {

/*!
\brief The Base Class for an Oxford AMDTP receive stream processor

 This class implements a ReceiveStreamProcessor that demultiplexes
 AMDTP streams into Ports. (Oxford style)

*/
class AmdtpOxfordReceiveStreamProcessor
    : public AmdtpReceiveStreamProcessor
{

public:
    /**
     * Create a Oxford AMDTP receive StreamProcessor
     * @param port 1394 port
     * @param dimension number of substreams in the ISO stream
     *                  (midi-muxed is only one stream)
     */
    AmdtpOxfordReceiveStreamProcessor(FFADODevice &parent, int dimension);
    virtual ~AmdtpOxfordReceiveStreamProcessor();

    virtual enum eChildReturnValue processPacketHeader(unsigned char *data, unsigned int length,
                                                       unsigned char tag, unsigned char sy,
                                                       uint32_t pkt_ctr);
    virtual enum eChildReturnValue processPacketData(unsigned char *data, unsigned int length);

    virtual bool prepareChild();

protected:
    // packet start timestamp
    uint64_t m_next_packet_timestamp;
    
    // this contains the payload data
    ffado_ringbuffer_t *m_temp_buffer;
    
    unsigned int m_packet_size_bytes;
    char *m_payload_buffer;
    
    // dll stuff
    double m_dll_e2;
    float m_dll_b;
    float m_dll_c;
    uint64_t m_expected_time_of_receive;
    float m_nominal_ticks_per_frame;
};


} // end of namespace Streaming

#endif /* __FFADO_AMDTPOXFORDRECEIVESTREAMPROCESSOR__ */

