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

#ifndef __FFADO_MOTURECEIVESTREAMPROCESSOR__
#define __FFADO_MOTURECEIVESTREAMPROCESSOR__

/**
 * This class implements MOTU streaming
 */

#include "debugmodule/debugmodule.h"

#include "../generic/StreamProcessor.h"
#include "../util/cip.h"

namespace Streaming {

class MotuAudioPort;
/*!
 * \brief The Base Class for a MOTU receive stream processor
 *
 * This class implements the outgoing stream processing for
 * motu devices
 *
 */
class MotuReceiveStreamProcessor
    : public StreamProcessor
{

public:
    /**
     * Create a MOTU receive StreamProcessor
     * @param port 1394 port
     * @param dimension number of substreams in the ISO stream
     *                  (midi-muxed is only one stream)
     */
    MotuReceiveStreamProcessor(int port, unsigned int event_size);
    virtual ~MotuReceiveStreamProcessor() {};

    enum eChildReturnValue processPacketHeader(unsigned char *data, unsigned int length,
                  unsigned char channel, unsigned char tag, unsigned char sy,
                  unsigned int cycle, unsigned int dropped);
    enum eChildReturnValue processPacketData(unsigned char *data, unsigned int length,
                  unsigned char channel, unsigned char tag, unsigned char sy,
                  unsigned int cycle, unsigned int dropped);

    virtual bool prepareChild();

public:
    virtual unsigned int getEventSize() 
                {return m_event_size;};
    virtual unsigned int getMaxPacketSize();
    virtual unsigned int getEventsPerFrame() 
                    { return 1; }; // FIXME: check
    virtual unsigned int getNominalFramesPerPacket();

protected:
    bool processReadBlock(char *data, unsigned int nevents, unsigned int offset);

private:
    bool decodePacketPorts(quadlet_t *data, unsigned int nevents, unsigned int dbc);

    int decodeMotuEventsToPort(MotuAudioPort *, quadlet_t *data, unsigned int offset, unsigned int nevents);

    /*
     * An iso packet mostly consists of multiple events.  m_event_size
     * is the size of a single 'event' in bytes.
     */
    unsigned int m_event_size;
};


} // end of namespace Streaming

#endif /* __FFADO_MOTURECEIVESTREAMPROCESSOR__ */

