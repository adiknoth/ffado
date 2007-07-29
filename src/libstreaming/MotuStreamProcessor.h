/*
 * Copyright (C) 2005-2007 by Jonathan Woithe
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

#ifndef __FFADO_MOTUSTREAMPROCESSOR__
#define __FFADO_MOTUSTREAMPROCESSOR__
#include <assert.h>

#include "../debugmodule/debugmodule.h"
#include "StreamProcessor.h"

#include "../libutil/DelayLockedLoop.h"

namespace Streaming {

class MotuAudioPort;

/**
 * This class implements the outgoing stream processing for
 * motu devices
 */
class MotuTransmitStreamProcessor
    : public TransmitStreamProcessor
{
public:

    MotuTransmitStreamProcessor(int port, int framerate,
        unsigned int event_size);

    virtual ~MotuTransmitStreamProcessor();

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

    // These two are important to calculate the optimal ISO DMA buffers
    // size.  An estimate will do.
    unsigned int getPacketsPerPeriod() {return (m_period*8000) / m_framerate;};
    unsigned int getMaxPacketSize() {return m_framerate<=48000?616:(m_framerate<=96000?1032:1160);};

    int getMinimalSyncDelay();

    void setVerboseLevel(int l);

protected:
    /*
     * An iso packet mostly consists of multiple events.  m_event_size
     * is the size of a single 'event' in bytes.
     */
    unsigned int m_event_size;

    // Keep track of transmission data block count
    unsigned int m_tx_dbc;

    // Used to zero output data during startup while sync is established
    signed int m_startup_count;

    // Used to keep track of the close-down zeroing of output data
    signed int m_closedown_count;
    signed int m_streaming_active;

    bool prefill();

    bool transferSilence(unsigned int size);

    bool processWriteBlock(char *data, unsigned int nevents, unsigned int offset);

    bool encodePacketPorts(quadlet_t *data, unsigned int nevents, unsigned int dbc);

    int transmitSilenceBlock(char *data, unsigned int nevents,
                      unsigned int offset);

    int encodePortToMotuEvents(MotuAudioPort *p, quadlet_t *data,
        unsigned int offset, unsigned int nevents);
    int encodeSilencePortToMotuEvents(MotuAudioPort *p, quadlet_t *data,
        unsigned int offset, unsigned int nevents);

    DECLARE_DEBUG_MODULE;

};

/**
 * This class implements the incoming stream processing for
 * motu devices
 */
class MotuReceiveStreamProcessor
    : public ReceiveStreamProcessor
{

public:

    MotuReceiveStreamProcessor(int port, int framerate, unsigned int event_size);
    virtual ~MotuReceiveStreamProcessor();

    enum raw1394_iso_disposition putPacket(unsigned char *data, unsigned int length,
                  unsigned char channel, unsigned char tag, unsigned char sy,
                  unsigned int cycle, unsigned int dropped);

    bool getFrames(unsigned int nbframes, int64_t ts); ///< transfer the buffer contents to the client

    bool init();
    bool reset();
    bool prepare();

    // these two are important to calculate the optimal
    // ISO DMA buffers size
    // an estimate will do
    unsigned int getPacketsPerPeriod() {return (m_period*8000) / m_framerate;};
    unsigned int getMaxPacketSize() {return m_framerate<=48000?616:(m_framerate<=96000?1032:1160);};

    int getMinimalSyncDelay();

    virtual void setVerboseLevel(int l);

    signed int setEventSize(unsigned int size);
    unsigned int getEventSize(void);

    virtual bool prepareForStop();
    virtual bool prepareForStart();

protected:

    bool processReadBlock(char *data, unsigned int nevents, unsigned int offset);

    bool decodePacketPorts(quadlet_t *data, unsigned int nevents, unsigned int dbc);
    signed int decodeMotuEventsToPort(MotuAudioPort *p, quadlet_t *data, unsigned int offset, unsigned int nevents);

    /*
     * An iso packet mostly consists of multiple events.  m_event_size
     * is the size of a single 'event' in bytes.
     */
    unsigned int m_event_size;

    // Signifies a closedown is in progress, in which case incoming data
        // is junked.
        signed int m_closedown_active;

    uint64_t m_last_timestamp; /// last timestamp (in ticks)
    uint64_t m_last_timestamp2; /// last timestamp (in ticks)
    uint64_t m_last_timestamp_at_period_ticks;

    DECLARE_DEBUG_MODULE;

};

} // end of namespace Streaming

#endif /* __FFADO_MOTUSTREAMPROCESSOR__ */


