/*
 * Copyright (C) 2005-2007 by Jonathan Woithe
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

#ifndef __FFADO_MOTUTRANSMITSTREAMPROCESSOR__
#define __FFADO_MOTUTRANSMITSTREAMPROCESSOR__

/**
 * This class implements MOTU based streaming
 */

#include "debugmodule/debugmodule.h"

#include "../generic/StreamProcessor.h"
#include "../util/cip.h"

namespace Streaming {

class Port;
class MotuAudioPort;

/*!
\brief The Base Class for an MOTU transmit stream processor

 This class implements a TransmitStreamProcessor that multiplexes Ports
 into MOTU streams.

*/
class MotuTransmitStreamProcessor
    : public StreamProcessor
{

public:
    /**
     * Create a MOTU transmit StreamProcessor
     */
    MotuTransmitStreamProcessor(FFADODevice &parent, unsigned int event_size);
    virtual ~MotuTransmitStreamProcessor() {};

    enum eChildReturnValue generatePacketHeader(unsigned char *data, unsigned int *length,
                              unsigned char *tag, unsigned char *sy,
                              int cycle, unsigned int dropped, unsigned int max_length);
    enum eChildReturnValue generatePacketData(unsigned char *data, unsigned int *length,
                            unsigned char *tag, unsigned char *sy,
                            int cycle, unsigned int dropped, unsigned int max_length);
    enum eChildReturnValue generateSilentPacketHeader(unsigned char *data, unsigned int *length,
                                    unsigned char *tag, unsigned char *sy,
                                    int cycle, unsigned int dropped, unsigned int max_length);
    enum eChildReturnValue generateSilentPacketData(unsigned char *data, unsigned int *length,
                                  unsigned char *tag, unsigned char *sy,
                                  int cycle, unsigned int dropped, unsigned int max_length);
    virtual bool prepareChild();

public:
    virtual unsigned int getEventSize() 
                {return m_event_size;};
    virtual unsigned int getMaxPacketSize();
    virtual unsigned int getEventsPerFrame() 
                    { return 1; }; // FIXME: check
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

    int encodePortToMotuEvents(MotuAudioPort *, quadlet_t *data,
                                unsigned int offset, unsigned int nevents);
    int encodeSilencePortToMotuEvents(MotuAudioPort *, quadlet_t *data,
                                unsigned int offset, unsigned int nevents);

    /*
     * An iso packet mostly consists of multiple events.  m_event_size
     * is the size of a single 'event' in bytes.
     */
    unsigned int m_event_size;

    // Keep track of transmission data block count
    unsigned int m_tx_dbc;

};

} // end of namespace Streaming

#endif /* __FFADO_MOTUTRANSMITSTREAMPROCESSOR__ */

