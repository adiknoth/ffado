/*
 * Copyright (C) 2005-2009 by Jonathan Woithe
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

#ifndef __FFADO_RMETRANSMITSTREAMPROCESSOR__
#define __FFADO_RMETRANSMITSTREAMPROCESSOR__

/**
 * This class implements RME based streaming
 */

#include "debugmodule/debugmodule.h"

#include "../generic/StreamProcessor.h"
#include "../util/cip.h"

namespace Streaming {

class Port;
class RmeAudioPort;
class RmeMidiPort;
/*!
\brief The Base Class for an RME transmit stream processor

 This class implements a TransmitStreamProcessor that multiplexes Ports
 into RME streams.

*/
class RmeTransmitStreamProcessor
    : public StreamProcessor
{

public:
    /**
     * Create a RME transmit StreamProcessor
     */
    RmeTransmitStreamProcessor(FFADODevice &parent, 
                               unsigned int model, unsigned int event_size);
    virtual ~RmeTransmitStreamProcessor() {};

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

public:
    virtual unsigned int getEventSize() 
                {return m_event_size;};
    virtual unsigned int getMaxPacketSize();
    virtual unsigned int getEventsPerFrame() 
                    { return 1; };
    virtual unsigned int getNominalFramesPerPacket();

protected:
    bool processWriteBlock(char *data, unsigned int nevents, unsigned int offset);
    bool transmitSilenceBlock(char *data, unsigned int nevents, unsigned int offset);

private:
    unsigned int fillNoDataPacketHeader(quadlet_t *data, unsigned int* length);
    unsigned int fillDataPacketHeader(quadlet_t *data, unsigned int* length, uint32_t ts);

    int transmitBlock(char *data, unsigned int nevents,
                        unsigned int offset);

    bool encodePacketPorts(quadlet_t *data, unsigned int nevents,
                           unsigned int dbc);

    int encodePortToRmeEvents(RmeAudioPort *, quadlet_t *data,
                                unsigned int offset, unsigned int nevents);
    int encodeSilencePortToRmeEvents(RmeAudioPort *, quadlet_t *data,
                                unsigned int offset, unsigned int nevents);

    int encodePortToRmeMidiEvents(
                       RmeMidiPort *p, quadlet_t *data,
                       unsigned int offset, unsigned int nevents);
    int encodeSilencePortToRmeMidiEvents(
                       RmeMidiPort *p, quadlet_t *data,
                       unsigned int offset, unsigned int nevents);

    unsigned int m_rme_model;

    /*
     * An iso packet mostly consists of multiple events.  m_event_size
     * is the size of a single 'event' in bytes.
     */
    unsigned int m_event_size;

    // Keep track of transmission data block count
    unsigned int m_tx_dbc;

    // A simple circular buffer for outgoing MIDI data to allow
    // a rate control to be implemented on the data to suit the RME
    // devices.  Note that this buffer's size is forced to be a power
    // of 2 to allow for buffer manipulation optimisations.
    // FIXME: it is yet to be determined whether this is necessary for
    // the RME device.
    #define MIDIBUFFER_SIZE_EXP 10
    #define MIDIBUFFER_SIZE     (1<<MIDIBUFFER_SIZE_EXP)
    unsigned int midibuffer[MIDIBUFFER_SIZE];
    unsigned int mb_head, mb_tail;
    unsigned int midi_lock;
    unsigned int midi_tx_period; /* Measured in audio clock periods */
};

} // end of namespace Streaming

#endif /* __FFADO_RMETRANSMITSTREAMPROCESSOR__ */

