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

#include "AmdtpReceiveStreamProcessor.h"
#include "AmdtpPort.h"
#include "../StreamProcessorManager.h"

#include "libieee1394/cycletimer.h"

#include <netinet/in.h>
#include <assert.h>

namespace Streaming {

/* --------------------- RECEIVE ----------------------- */

AmdtpReceiveStreamProcessor::AmdtpReceiveStreamProcessor(FFADODevice &parent, int dimension)
    : StreamProcessor(parent, ePT_Receive)
    , m_dimension( dimension )
{}

bool AmdtpReceiveStreamProcessor::prepareChild() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing (%p)...\n", this);

    switch (m_manager->getNominalRate()) {
        case 32000:
        case 44100:
        case 48000:
            m_syt_interval = 8;
            break;
        case 88200:
        case 96000:
            m_syt_interval = 16;
            break;
        case 176400:
        case 192000:
            m_syt_interval = 32;
            break;
        default:
            debugError("Unsupported rate: %d\n", m_manager->getNominalRate());
            return false;
    }
    return true;
}


/**
 * Processes packet header to extract timestamps and so on
 * @param data 
 * @param length 
 * @param channel 
 * @param tag 
 * @param sy 
 * @param cycle 
 * @param dropped 
 * @return 
 */
enum StreamProcessor::eChildReturnValue
AmdtpReceiveStreamProcessor::processPacketHeader(unsigned char *data, unsigned int length,
                  unsigned char channel, unsigned char tag, unsigned char sy,
                  unsigned int cycle, unsigned int dropped)
{
    struct iec61883_packet *packet = (struct iec61883_packet *) data;
    assert(packet);
    bool ok = (packet->syt != 0xFFFF) &&
                  (packet->fdf != 0xFF) &&
                  (packet->fmt == 0x10) &&
                  (packet->dbs > 0) &&
                  (length >= 2*sizeof(quadlet_t));
    if(ok) {
        uint64_t now = m_parent.get1394Service().getCycleTimer();
        //=> convert the SYT to a full timestamp in ticks
        m_last_timestamp = sytRecvToFullTicks((uint32_t)ntohs(packet->syt),
                                              cycle, now);
    }
    return (ok ? eCRV_OK : eCRV_Invalid );
}

/**
 * extract the data from the packet
 * @pre the IEC61883 packet is valid according to isValidPacket
 * @param data 
 * @param length 
 * @param channel 
 * @param tag 
 * @param sy 
 * @param cycle 
 * @param dropped 
 * @return 
 */
enum StreamProcessor::eChildReturnValue
AmdtpReceiveStreamProcessor::processPacketData(unsigned char *data, unsigned int length,
                  unsigned char channel, unsigned char tag, unsigned char sy,
                  unsigned int cycle, unsigned int dropped_cycles) {
    struct iec61883_packet *packet = (struct iec61883_packet *) data;
    assert(packet);

    unsigned int nevents=((length / sizeof (quadlet_t)) - 2)/packet->dbs;

    // we have to keep in mind that there are also
    // some packets buffered by the ISO layer,
    // at most x=m_handler->getWakeupInterval()
    // these contain at most x*syt_interval
    // frames, meaning that we might receive
    // this packet x*syt_interval*ticks_per_frame
    // later than expected (the real receive time)
    #ifdef DEBUG
    if(isRunning()) {
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"STMP: %lluticks | syt_interval=%d, tpf=%f\n",
            m_last_timestamp, m_syt_interval, getTicksPerFrame());
    }
    #endif

    if(m_data_buffer->writeFrames(nevents, (char *)(data+8), m_last_timestamp)) {
        // process all ports that should be handled on a per-packet base
        // this is MIDI for AMDTP (due to the need of DBC)
        if(isRunning()) {
            if (!decodePacketPorts((quadlet_t *)(data+8), nevents, packet->dbc)) {
                debugWarning("Problem decoding Packet Ports\n");
            }
        }
        return eCRV_OK;
    } else {
        return eCRV_XRun;
    }
}

/***********************************************
 * Encoding/Decoding API                       *
 ***********************************************/
/**
 * @brief write received events to the stream ringbuffers.
 */
bool AmdtpReceiveStreamProcessor::processReadBlock(char *data,
                       unsigned int nevents, unsigned int offset)
{
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "(%p)->processReadBlock(%u, %u)\n",this,nevents,offset);

    bool no_problem=true;

    for ( PortVectorIterator it = m_PeriodPorts.begin();
          it != m_PeriodPorts.end();
          ++it )
    {
        if((*it)->isDisabled()) {continue;};

        //FIXME: make this into a static_cast when not DEBUG?

        AmdtpPortInfo *pinfo=dynamic_cast<AmdtpPortInfo *>(*it);
        assert(pinfo); // this should not fail!!

        switch(pinfo->getFormat()) {
        case AmdtpPortInfo::E_MBLA:
            if(decodeMBLAEventsToPort(static_cast<AmdtpAudioPort *>(*it), (quadlet_t *)data, offset, nevents)) {
                debugWarning("Could not decode packet MBLA to port %s",(*it)->getName().c_str());
                no_problem=false;
            }
            break;
        case AmdtpPortInfo::E_SPDIF: // still unimplemented
            break;
    /* for this processor, midi is a packet based port
        case AmdtpPortInfo::E_Midi:
            break;*/
        default: // ignore
            break;
        }
    }
    return no_problem;
}

/**
 * @brief decode a packet for the packet-based ports
 *
 * @param data Packet data
 * @param nevents number of events in data (including events of other ports & port types)
 * @param dbc DataBlockCount value for this packet
 * @return true if all successfull
 */
bool AmdtpReceiveStreamProcessor::decodePacketPorts(quadlet_t *data, unsigned int nevents, unsigned int dbc)
{
    bool ok=true;

    quadlet_t *target_event=NULL;
    unsigned int j;

    for ( PortVectorIterator it = m_PacketPorts.begin();
          it != m_PacketPorts.end();
          ++it )
    {

#ifdef DEBUG
        AmdtpPortInfo *pinfo=dynamic_cast<AmdtpPortInfo *>(*it);
        assert(pinfo); // this should not fail!!

        // the only packet type of events for AMDTP is MIDI in mbla
        assert(pinfo->getFormat()==AmdtpPortInfo::E_Midi);
#endif
        AmdtpMidiPort *mp=static_cast<AmdtpMidiPort *>(*it);

        // we decode this directly (no function call) due to the high frequency
        /* idea:
        spec says: current_midi_port=(dbc+j)%8;
        => if we start at (dbc+stream->location-1)%8,
        we'll start at the right event for the midi port.
        => if we increment j with 8, we stay at the right event.
        */
        // FIXME: as we know in advance how big a packet is (syt_interval) we can
        //        predict how much loops will be present here
        for(j = (dbc & 0x07)+mp->getLocation(); j < nevents; j += 8) {
            target_event=(quadlet_t *)(data + ((j * m_dimension) + mp->getPosition()));
            quadlet_t sample_int=ntohl(*target_event);
            // FIXME: this assumes that 2X and 3X speed isn't used,
            // because only the 1X slot is put into the ringbuffer
            if(IEC61883_AM824_GET_LABEL(sample_int) != IEC61883_AM824_LABEL_MIDI_NO_DATA) {
                sample_int=(sample_int >> 16) & 0x000000FF;
                if(!mp->writeEvent(&sample_int)) {
                    debugWarning("Packet port events lost\n");
                    ok=false;
                }
            }
        }

    }

    return ok;
}

int
AmdtpReceiveStreamProcessor::decodeMBLAEventsToPort(
                       AmdtpAudioPort *p, quadlet_t *data,
                       unsigned int offset, unsigned int nevents)
{
    unsigned int j=0;
    quadlet_t *target_event;

    target_event=(quadlet_t *)(data + p->getPosition());

    switch(p->getDataType()) {
        default:
        case Port::E_Int24:
            {
                quadlet_t *buffer=(quadlet_t *)(p->getBufferAddress());

                assert(nevents + offset <= p->getBufferSize());

                buffer+=offset;

                for(j = 0; j < nevents; j += 1) { // decode max nsamples
                    *(buffer)=(ntohl((*target_event) ) & 0x00FFFFFF);
                    buffer++;
                    target_event+=m_dimension;
                }
            }
            break;
        case Port::E_Float:
            {
                const float multiplier = 1.0f / (float)(0x7FFFFF);
                float *buffer=(float *)(p->getBufferAddress());

                assert(nevents + offset <= p->getBufferSize());

                buffer+=offset;

                for(j = 0; j < nevents; j += 1) { // decode max nsamples

                    unsigned int v = ntohl(*target_event) & 0x00FFFFFF;
                    // sign-extend highest bit of 24-bit int
                    int tmp = (int)(v << 8) / 256;

                    *buffer = tmp * multiplier;

                    buffer++;
                    target_event+=m_dimension;
                }
            }
            break;
    }

    return 0;
}
} // end of namespace Streaming
