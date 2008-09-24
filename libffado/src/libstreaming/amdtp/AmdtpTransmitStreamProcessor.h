/*
 * Copyright (C) 2005-2008 by Pieter Palmers
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

#ifndef __FFADO_AMDTPTRANSMITSTREAMPROCESSOR__
#define __FFADO_AMDTPTRANSMITSTREAMPROCESSOR__

/**
 * This class implements IEC61883-6 / AM824 / AMDTP based streaming
 */
#include "config.h"

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
\brief The Base Class for an AMDTP transmit stream processor

 This class implements a TransmitStreamProcessor that multiplexes Ports
 into AMDTP streams.

*/
class AmdtpTransmitStreamProcessor
    : public StreamProcessor
{

public:
    /**
     * Create a AMDTP transmit StreamProcessor
     * @param port 1394 port
     * @param framerate frame rate
     * @param dimension number of substreams in the ISO stream
     *                  (midi-muxed is only one stream)
     */
    AmdtpTransmitStreamProcessor(FFADODevice &parent, int dimension);
    virtual ~AmdtpTransmitStreamProcessor() {};

    enum eChildReturnValue generatePacketHeader(unsigned char *data, unsigned int *length,
                                                unsigned char *tag, unsigned char *sy,
                                                uint32_t pkt_ctr);
    enum eChildReturnValue generatePacketData(unsigned char *data, unsigned int *length);
    enum eChildReturnValue generateEmptyPacketHeader(unsigned char *data, unsigned int *length,
                                                     unsigned char *tag, unsigned char *sy,
                                                     uint32_t pkt_ctr);
    enum eChildReturnValue generateEmptyPacketData(unsigned char *data, unsigned int *length);
    enum eChildReturnValue generateSilentPacketHeader(unsigned char *data, unsigned int *length,
                                                      unsigned char *tag, unsigned char *sy,
                                                      uint32_t pkt_ctr);
    enum eChildReturnValue generateSilentPacketData(unsigned char *data, unsigned int *length);
    virtual bool prepareChild();

#if AMDTP_ALLOW_PAYLOAD_IN_NODATA_XMIT
public:
    void sendPayloadForNoDataPackets(bool b) {m_send_nodata_payload = b;};
#endif

public:
    virtual unsigned int getEventSize()
                    {return 4;};
    virtual unsigned int getMaxPacketSize()
                    {return 4 * (2 + getSytInterval() * m_dimension);};
    virtual unsigned int getAveragePacketSize();
    virtual unsigned int getEventsPerFrame()
                    { return m_dimension; };
    virtual unsigned int getNominalFramesPerPacket()
                    {return getSytInterval();};

protected:
    bool processWriteBlock(char *data, unsigned int nevents, unsigned int offset);
    bool transmitSilenceBlock(char *data, unsigned int nevents, unsigned int offset);

private:
    unsigned int fillNoDataPacketHeader(struct iec61883_packet *packet, unsigned int* length);
    unsigned int fillDataPacketHeader(struct iec61883_packet *packet, unsigned int* length, uint32_t ts);

    int transmitBlock(char *data, unsigned int nevents,
                        unsigned int offset);

    void encodeAudioPortsSilence(quadlet_t *data, unsigned int offset, unsigned int nevents);
    void encodeAudioPortsFloat(quadlet_t *data, unsigned int offset, unsigned int nevents);
    void encodeAudioPortsInt24(quadlet_t *data, unsigned int offset, unsigned int nevents);
    void encodeMidiPortsSilence(quadlet_t *data, unsigned int offset, unsigned int nevents);
    void encodeMidiPorts(quadlet_t *data, unsigned int offset, unsigned int nevents);

    unsigned int getFDF();
    unsigned int getSytInterval();

    struct iec61883_cip m_cip_status;
    int m_dimension;
    unsigned int m_syt_interval;
    int m_fdf;
    unsigned int m_dbc;

#if AMDTP_ALLOW_PAYLOAD_IN_NODATA_XMIT
private:
    bool m_send_nodata_payload;
#endif

private: // local port caching for performance
    struct _MBLA_port_cache {
        AmdtpAudioPort*     port;
        void*               buffer;
        bool                enabled;
#ifdef DEBUG
        unsigned int        buffer_size;
#endif
    };
    std::vector<struct _MBLA_port_cache> m_audio_ports;
    int m_nb_audio_ports;

    struct _MIDI_port_cache {
        AmdtpMidiPort*      port;
        void*               buffer;
        bool                enabled;
        unsigned int        position;
        unsigned int        location;
#ifdef DEBUG
        unsigned int        buffer_size;
#endif
    };
    std::vector<struct _MIDI_port_cache> m_midi_ports;
    int m_nb_midi_ports;

    bool initPortCache();
    void updatePortCache();
};

} // end of namespace Streaming

#endif /* __FFADO_AMDTPTRANSMITSTREAMPROCESSOR__ */

