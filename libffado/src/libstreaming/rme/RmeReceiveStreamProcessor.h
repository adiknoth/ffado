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

#ifndef __FFADO_RMERECEIVESTREAMPROCESSOR__
#define __FFADO_RMERECEIVESTREAMPROCESSOR__

/**
 * This class implements RME streaming
 */

#include "debugmodule/debugmodule.h"

#include "../generic/StreamProcessor.h"
#include "../util/cip.h"

namespace Streaming {

class RmeAudioPort;
class RmeMidiPort;
/*!
 * \brief The Base Class for a RME receive stream processor
 *
 * This class implements the outgoing stream processing for
 * motu devices
 *
 */
class RmeReceiveStreamProcessor
    : public StreamProcessor
{

public:
    /**
     * Create a RME receive StreamProcessor
     * @param port 1394 port
     * @param dimension number of substreams in the ISO stream
     *                  (midi-muxed is only one stream)
     */
    RmeReceiveStreamProcessor(FFADODevice &parent, unsigned int event_size);
    virtual ~RmeReceiveStreamProcessor() {};

    enum eChildReturnValue processPacketHeader(unsigned char *data, unsigned int length,
                                               unsigned char tag, unsigned char sy,
                                               uint32_t pkt_ctr);
    enum eChildReturnValue processPacketData(unsigned char *data, unsigned int length);

    virtual bool prepareChild();

public:
    virtual unsigned int getEventSize() 
                {return m_event_size;};
    virtual unsigned int getMaxPacketSize();
    virtual unsigned int getEventsPerFrame() 
                    { return 1; };
    virtual unsigned int getNominalFramesPerPacket();

protected:
    bool processReadBlock(char *data, unsigned int nevents, unsigned int offset);

private:
    bool decodePacketPorts(quadlet_t *data, unsigned int nevents, unsigned int dbc);

    int decodeRmeEventsToPort(RmeAudioPort *, quadlet_t *data, unsigned int offset, unsigned int nevents);
    int decodeRmeMidiEventsToPort(RmeMidiPort *, quadlet_t *data, unsigned int offset, unsigned int nevents);

    /*
     * An iso packet mostly consists of multiple events.  m_event_size
     * is the size of a single 'event' in bytes.
     */
    unsigned int m_event_size;

    /* A small MIDI buffer to cover for the case where we need to span a
     * period - that is, if more than one MIDI byte is sent per packet. 
     * Since the long-term average data rate must be close to the MIDI spec
     * (as it's coming from a physical MIDI port_ this buffer doesn't have
     * to be particularly large.  The size is a power of 2 for optimisation
     * reasons.
     *
     * FIXME: it is yet to be determined whether this is required for RME
     * devices.
     */
    #define RX_MIDIBUFFER_SIZE_EXP 6
    #define RX_MIDIBUFFER_SIZE     (1<<RX_MIDIBUFFER_SIZE_EXP)
    unsigned int midibuffer[RX_MIDIBUFFER_SIZE];
    unsigned mb_head, mb_tail;
};


} // end of namespace Streaming

#endif /* __FFADO_RMERECEIVESTREAMPROCESSOR__ */

