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

#ifndef __FFADO_AMDTPRECEIVESTREAMPROCESSOR__
#define __FFADO_AMDTPRECEIVESTREAMPROCESSOR__

/**
 * This class implements IEC61883-6 / AM824 / AMDTP based streaming
 */

#include "debugmodule/debugmodule.h"

#include "../generic/StreamProcessor.h"
#include "../util/cip.h"

#include <libiec61883/iec61883.h>
#include <pthread.h>

#define AMDTP_MAX_PACKET_SIZE 2048

#define IEC61883_STREAM_TYPE_MIDI   0x0D
#define IEC61883_STREAM_TYPE_SPDIF  0x00
#define IEC61883_STREAM_TYPE_MBLA   0x06

#define IEC61883_AM824_LABEL_MASK             0xFF000000
#define IEC61883_AM824_GET_LABEL(x)         (((x) & 0xFF000000) >> 24)
#define IEC61883_AM824_SET_LABEL(x,y)         ((x) | ((y)<<24))

#define IEC61883_AM824_LABEL_MIDI_NO_DATA     0x80
#define IEC61883_AM824_LABEL_MIDI_1X          0x81
#define IEC61883_AM824_LABEL_MIDI_2X          0x82
#define IEC61883_AM824_LABEL_MIDI_3X          0x83

namespace Streaming {

class Port;
class AmdtpAudioPort;
class AmdtpMidiPort;
/*!
\brief The Base Class for an AMDTP receive stream processor

 This class implements a ReceiveStreamProcessor that demultiplexes
 AMDTP streams into Ports.

*/
class AmdtpReceiveStreamProcessor
    : public ReceiveStreamProcessor
{

public:
    /**
     * Create a AMDTP receive StreamProcessor
     * @param port 1394 port
     * @param framerate frame rate
     * @param dimension number of substreams in the ISO stream
     *                  (midi-muxed is only one stream)
     */
    AmdtpReceiveStreamProcessor(int port, int framerate, int dimension);
    virtual ~AmdtpReceiveStreamProcessor() {};

    enum raw1394_iso_disposition putPacket(unsigned char *data, unsigned int length,
                  unsigned char channel, unsigned char tag, unsigned char sy,
                  unsigned int cycle, unsigned int dropped);


    bool init();
    bool reset();
    bool prepare();

    bool prepareForStop();
    bool prepareForStart();

    bool getFrames(unsigned int nbframes, int64_t ts); ///< transfer the buffer contents to the client
    bool getFramesDry(unsigned int nbframes, int64_t ts);

    // We have 1 period of samples = m_period
    // this period takes m_period/m_framerate seconds of time
    // during this time, 8000 packets are sent
//     unsigned int getPacketsPerPeriod() {return (m_period*8000)/m_framerate;};

    // however, if we only count the number of used packets
    // it is m_period / m_syt_interval
    unsigned int getPacketsPerPeriod() {return (m_period)/m_syt_interval;};

    unsigned int getMaxPacketSize() {return 4 * (2 + m_syt_interval * m_dimension);};

    void dumpInfo();
protected:

    bool processReadBlock(char *data, unsigned int nevents, unsigned int offset);

    bool decodePacketPorts(quadlet_t *data, unsigned int nevents, unsigned int dbc);

    int decodeMBLAEventsToPort(AmdtpAudioPort *, quadlet_t *data, unsigned int offset, unsigned int nevents);
    void updatePreparedState();

    int m_dimension;
    unsigned int m_syt_interval;

    uint64_t m_dropped; /// FIXME:debug
    uint64_t m_last_dropped; /// FIXME:debug
    uint64_t m_last_syt; /// FIXME:debug
    uint64_t m_last_now; /// FIXME:debug
    int m_last_good_cycle; /// FIXME:debug
    uint64_t m_last_timestamp; /// last timestamp (in ticks)
    uint64_t m_last_timestamp2; /// last timestamp (in ticks)
    uint64_t m_last_timestamp_at_period_ticks;
};


} // end of namespace Streaming

#endif /* __FFADO_AMDTPRECEIVESTREAMPROCESSOR__ */

