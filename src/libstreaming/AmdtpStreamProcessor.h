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

#ifndef __FFADO_AMDTPSTREAMPROCESSOR__
#define __FFADO_AMDTPSTREAMPROCESSOR__

/**
 * This class implements IEC61883-6 / AM824 / AMDTP based streaming
 */

#include "../debugmodule/debugmodule.h"
#include "StreamProcessor.h"

#include "cip.h"
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
class AmdtpReceiveStreamProcessor;

/*!
\brief The Base Class for an AMDTP transmit stream processor

 This class implements a TransmitStreamProcessor that multiplexes Ports
 into AMDTP streams.

*/
class AmdtpTransmitStreamProcessor
    : public TransmitStreamProcessor
{

public:
    /**
     * Create a AMDTP transmit StreamProcessor
     * @param port 1394 port
     * @param framerate frame rate
     * @param dimension number of substreams in the ISO stream
     *                  (midi-muxed is only one stream)
     */
    AmdtpTransmitStreamProcessor(int port, int framerate, int dimension);

    virtual ~AmdtpTransmitStreamProcessor();

    enum raw1394_iso_disposition
            getPacket(unsigned char *data, unsigned int *length,
                    unsigned char *tag, unsigned char *sy,
                    int cycle, unsigned int dropped, unsigned int max_length);

    bool init();
    bool reset();
    bool prepare();

    bool prepareForStop();
    bool prepareForStart();

    bool prepareForEnable(uint64_t time_to_enable_at);

    bool putFrames(unsigned int nbframes, int64_t ts); ///< transfer the buffer contents from the client

    // We have 1 period of samples = m_period
    // this period takes m_period/m_framerate seconds of time
    // during this time, 8000 packets are sent
//     unsigned int getPacketsPerPeriod() {return (m_period*8000)/m_framerate;};

    // however, if we only count the number of used packets
    // it is m_period / m_syt_interval
    unsigned int getPacketsPerPeriod() {return (m_period)/m_syt_interval;};

    unsigned int getMaxPacketSize() {return 4 * (2 + m_syt_interval * m_dimension);};

    int getMinimalSyncDelay();

    void setVerboseLevel(int l);

protected:
    bool processWriteBlock(char *data, unsigned int nevents, unsigned int offset);

    struct iec61883_cip m_cip_status;

    int m_dimension;
    unsigned int m_syt_interval;

    int m_fdf;

    bool prefill();

    unsigned int fillNoDataPacketHeader(struct iec61883_packet *packet, unsigned int* length);
    unsigned int fillDataPacketHeader(struct iec61883_packet *packet, unsigned int* length, uint32_t ts);


    bool transferSilence(unsigned int size);

    int transmitBlock(char *data, unsigned int nevents,
                        unsigned int offset);

    bool encodePacketPorts(quadlet_t *data, unsigned int nevents, unsigned int dbc);
    int encodePortToMBLAEvents(AmdtpAudioPort *, quadlet_t *data,
                                unsigned int offset, unsigned int nevents);

    int transmitSilenceBlock(char *data, unsigned int nevents,
                        unsigned int offset);
    int encodeSilencePortToMBLAEvents(AmdtpAudioPort *, quadlet_t *data,
                                unsigned int offset, unsigned int nevents);
    void updatePreparedState();

    unsigned long m_last_timestamp;

    unsigned int m_dbc;

    unsigned int m_ringbuffer_size_frames;

    DECLARE_DEBUG_MODULE;

};
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

    virtual ~AmdtpReceiveStreamProcessor();

    enum raw1394_iso_disposition putPacket(unsigned char *data, unsigned int length,
                  unsigned char channel, unsigned char tag, unsigned char sy,
                  unsigned int cycle, unsigned int dropped);


    bool init();
    bool reset();
    bool prepare();

    bool prepareForStop();
    bool prepareForStart();

    bool getFrames(unsigned int nbframes, int64_t ts); ///< transfer the buffer contents to the client

    // We have 1 period of samples = m_period
    // this period takes m_period/m_framerate seconds of time
    // during this time, 8000 packets are sent
//     unsigned int getPacketsPerPeriod() {return (m_period*8000)/m_framerate;};

    // however, if we only count the number of used packets
    // it is m_period / m_syt_interval
    unsigned int getPacketsPerPeriod() {return (m_period)/m_syt_interval;};

    unsigned int getMaxPacketSize() {return 4 * (2 + m_syt_interval * m_dimension);};

    void dumpInfo();

    int getMinimalSyncDelay();

    void setVerboseLevel(int l);

protected:

    bool processReadBlock(char *data, unsigned int nevents, unsigned int offset);

    bool decodePacketPorts(quadlet_t *data, unsigned int nevents, unsigned int dbc);

    int decodeMBLAEventsToPort(AmdtpAudioPort *, quadlet_t *data, unsigned int offset, unsigned int nevents);
    void updatePreparedState();

    int m_dimension;
    unsigned int m_syt_interval;

    uint64_t m_last_timestamp; /// last timestamp (in ticks)
    uint64_t m_last_timestamp2; /// last timestamp (in ticks)
    uint64_t m_last_timestamp_at_period_ticks;

    DECLARE_DEBUG_MODULE;

};


} // end of namespace Streaming

#endif /* __FFADO_AMDTPSTREAMPROCESSOR__ */

