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


#include "AmdtpReceiveStreamProcessor.h"
#include "AmdtpPort.h"
#include "../StreamProcessorManager.h"
#include "devicemanager.h"

#include "libieee1394/ieee1394service.h"
#include "libieee1394/IsoHandlerManager.h"
#include "libieee1394/cycletimer.h"

#include "libutil/ByteSwap.h"
#include <assert.h>
#include "libutil/SystemTimeSource.h"
#include <cstring>

#define unlikely(x) __builtin_expect((x),0)

namespace Streaming {

/* --------------------- RECEIVE ----------------------- */

AmdtpReceiveStreamProcessor::AmdtpReceiveStreamProcessor(FFADODevice &parent, int dimension)
    : StreamProcessor(parent, ePT_Receive)
    , m_dimension( dimension )
    , m_nb_audio_ports( 0 )
    , m_nb_midi_ports( 0 )
    , mb_head( 0 )
    , mb_tail( 0 )
{}

unsigned int
AmdtpReceiveStreamProcessor::getSytInterval() {
    switch (m_StreamProcessorManager.getNominalRate()) {
        case 32000:
        case 44100:
        case 48000:
            return 8;
        case 88200:
        case 96000:
            return 16;
        case 176400:
        case 192000:
            return 32;
        default:
            debugError("Unsupported rate: %d\n", m_StreamProcessorManager.getNominalRate());
            return 0;
    }
}

bool AmdtpReceiveStreamProcessor::prepareChild() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing (%p)...\n", this);
    m_syt_interval = getSytInterval();

    if (!initPortCache()) {
        debugError("Could not init port cache\n");
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
 * @param pkt_ctr CTR value when packet was received
 * @return 
 */
enum StreamProcessor::eChildReturnValue
AmdtpReceiveStreamProcessor::processPacketHeader(unsigned char *data, unsigned int length,
                                                 unsigned char tag, unsigned char sy,
                                                 uint32_t pkt_ctr)
{
    struct iec61883_packet *packet = (struct iec61883_packet *) data;
    assert(packet);
    bool ok = (packet->syt != 0xFFFF) &&
              (packet->fdf != 0xFF) &&
              (packet->fmt == 0x10) &&
              (packet->dbs > 0) &&
              (length >= 2*sizeof(quadlet_t));
    if(ok) {
        m_last_timestamp = sytRecvToFullTicks2((uint32_t)CondSwapFromBus16(packet->syt), pkt_ctr);
        //#ifdef DEBUG
        #if 0
        uint32_t now = m_1394service.getCycleTimer();

        //=> convert the SYT to a full timestamp in ticks
        uint64_t old_last_timestamp = sytRecvToFullTicks((uint32_t)CondSwapFromBus16(packet->syt),
                                              CYCLE_TIMER_GET_CYCLES(pkt_ctr), now);
        if(m_last_timestamp != old_last_timestamp) {
            debugWarning("discepancy between timestamp calculations\n");
        }
        #endif
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
 * @param pkt_ctr 
 * @return 
 */
enum StreamProcessor::eChildReturnValue
AmdtpReceiveStreamProcessor::processPacketData(unsigned char *data, unsigned int length) {
    struct iec61883_packet *packet = (struct iec61883_packet *) data;
    assert(packet);

    /*
    Euan de Kock <euan@dekock.net> 8th February 2011

    See Ticket #310

    This is a fix to force the Echo Devices to work at
    sample rates above 48000 where the ADAT digital channels
    get multiplexed to handle higher frequencies.
    At 88200 and 96000 the ADAT channels are reduced to 4,
    and at 192000 they should be reduced to 2. (I don't have a
    device capable of 192K to verify this)

    With the Echo devices (5.5 firmware at least), the device does
    not reduce its channel count when it multiplexes the ADAT data,
    but the actual packet does contain a reduced set of channels.

    The channels are referred to as Dimensions in the firewire parlance.
    With an Audiofire Pre8, we typically receive 17 channels of data at
    44100 and 48000, being 8 Analog, 8 Digital and 1 MIDI. When we switch
    to 88200 or 96000 the ADAT (Digital) channel count is reduced to four,
    giving a total channel count (dimension) of 13. However Echo still
    reports the DBS parameter as being 17.

    To get around this, I don't bother using the DBS value at all. We know
    that the FDF is a reliable value for AM824 packets (type 0  to 7), so
    we can just use the FDF field to directly infer the size of each channel
    or dimension - 8, 16 or 32 Quadwords respectively for 41/48K, 88/96K and 192K.

    We set nevents directly to this for all AM824 packets directly, and do
    the standard calculation for any non-standard types.

    This is actually slightly less work than doing the calculation directly
    and should be portable across all devices that send AM824 data.
    */
    unsigned int nevents;

    switch(packet->fdf)
        {
        case 0x00:
        case 0x01:
        case 0x02:
	    // 8Q = 32B
            nevents=8;
	    break;
        case 0x03:
        case 0x04:
	    // 16Q = 64B
            nevents=16;
	    break;
        case 0x05:
        case 0x06:
	    // 32Q = 128B
            nevents=32;
	    break;
        default:
            nevents=((length / sizeof (quadlet_t)) - 2)/packet->dbs;
	    break;
        }

    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
                "packet->dbs %d calculated dbs %d packet->fdf %02X nevents %d\n",
                packet->dbs, (length - 8)/nevents, packet->fdf, nevents);

    // we have to keep in mind that there are also
    // some packets buffered by the ISO layer,
    // at most x=m_handler->getWakeupInterval()
    // these contain at most x*syt_interval
    // frames, meaning that we might receive
    // this packet x*syt_interval*ticks_per_frame
    // later than expected (the real receive time)
    #ifdef DEBUG
    static int64_t last_t = Util::SystemTimeSource::getCurrentTime();
    int64_t now_t = Util::SystemTimeSource::getCurrentTime();
    if(isRunning()) {
        debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                           "STMP: %"PRIu64"ticks | syt_interval=%d, tpf=%f\n",
                           m_last_timestamp, m_syt_interval, getTicksPerFrame());
/*        debugOutput(DEBUG_LEVEL_NORMAL,
                           "STMP: %12"PRIu64" ticks | delta_t: %5"PRId64" | bufferfill: %5d\n",
                           m_last_timestamp, now_t-last_t, m_data_buffer->getBufferFill());*/
    }
    last_t = now_t;

    // check whether nevents is a multiple of 8.
    if (nevents & 0x7) {
        debugError("Invalid nevents value for AMDTP (%u)\n", nevents);
    }
    #endif

    if(m_data_buffer->writeFrames(nevents, (char *)(data+8), m_last_timestamp)) {
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
    debugOutputExtreme( DEBUG_LEVEL_VERY_VERBOSE, 
                        "(%p)->processReadBlock(%u, %u)\n",
                        this, nevents, offset);

    // update the variable parts of the cache
    updatePortCache();

    // decode audio data
    switch(m_StreamProcessorManager.getAudioDataType()) {
        case StreamProcessorManager::eADT_Int24:
            decodeAudioPortsInt24((quadlet_t *)data, offset, nevents);
            break;
        case StreamProcessorManager::eADT_Float:
            decodeAudioPortsFloat((quadlet_t *)data, offset, nevents);
            break;
    }

    // do midi ports
    decodeMidiPorts((quadlet_t *)data, offset, nevents);
    return true;
}

//#ifdef __SSE2__
#if 0 // SSE code is not ready yet
#include <emmintrin.h>
#warning SSE2 build

/**
 * @brief demux events to all audio ports (int24)
 * @param data 
 * @param offset 
 * @param nevents 
 */
void
AmdtpReceiveStreamProcessor::decodeAudioPortsInt24(quadlet_t *data,
                                                    unsigned int offset,
                                                    unsigned int nevents)
{
    unsigned int j;
    quadlet_t *target_event;
    unsigned int i;

    for (i = 0; i < m_nb_audio_ports; i++) {
        struct _MBLA_port_cache &p = m_audio_ports.at(i);
        target_event = (quadlet_t *)(data + i);
        assert(nevents + offset <= p.buffer_size );

        if(p.buffer && p.enabled) {
            quadlet_t *buffer = (quadlet_t *)(p.buffer);
            buffer += offset;

            for(j = 0; j < nevents; j += 1) {
                *(buffer)=(CondSwapFromBus32((*target_event) ) & 0x00FFFFFF);
                buffer++;
                target_event+=m_dimension;
            }
        }
    }
}

/**
 * @brief demux events to all audio ports (float)
 * @param data 
 * @param offset 
 * @param nevents 
 */
void
AmdtpReceiveStreamProcessor::decodeAudioPortsFloat(quadlet_t *data,
                                                    unsigned int offset,
                                                    unsigned int nevents)
{
    unsigned int j;
    quadlet_t *target_event;
    unsigned int i;
    const float multiplier = 1.0f / (float)(0x7FFFFF);

    for (i = 0; i < m_nb_audio_ports; i++) {
        struct _MBLA_port_cache &p = m_audio_ports.at(i);
        target_event = (quadlet_t *)(data + i);
        assert(nevents + offset <= p.buffer_size );

        if(p.buffer && p.enabled) {
            float *buffer = (float *)(p.buffer);
            buffer += offset;

            for(j = 0; j < nevents; j += 1) {
                unsigned int v = CondSwapFromBus32(*target_event) & 0x00FFFFFF;
                // sign-extend highest bit of 24-bit int
                int tmp = (int)(v << 8) / 256;
                *buffer = tmp * multiplier;
                buffer++;
                target_event+=m_dimension;
            }
        }
    }
}

#else
/**
 * @brief demux events to all audio ports (int24)
 * @param data 
 * @param offset 
 * @param nevents 
 */
void
AmdtpReceiveStreamProcessor::decodeAudioPortsInt24(quadlet_t *data,
                                                    unsigned int offset,
                                                    unsigned int nevents)
{
    unsigned int j;
    quadlet_t *target_event;
    unsigned int i;

    for (i = 0; i < m_nb_audio_ports; i++) {
        struct _MBLA_port_cache &p = m_audio_ports.at(i);
        target_event = (quadlet_t *)(data + i);
        assert(nevents + offset <= p.buffer_size );

        if(p.buffer && p.enabled) {
            quadlet_t *buffer = (quadlet_t *)(p.buffer);
            buffer += offset;

            for(j = 0; j < nevents; j += 1) {
                *(buffer)=(CondSwapFromBus32((*target_event) ) & 0x00FFFFFF);
                buffer++;
                target_event+=m_dimension;
            }
        }
    }
}

/**
 * @brief demux events to all audio ports (float)
 * @param data 
 * @param offset 
 * @param nevents 
 */
void
AmdtpReceiveStreamProcessor::decodeAudioPortsFloat(quadlet_t *data,
                                                    unsigned int offset,
                                                    unsigned int nevents)
{
    unsigned int j;
    quadlet_t *target_event;
    unsigned int i;
    const float multiplier = 1.0f / (float)(0x7FFFFF);

    for (i = 0; i < m_nb_audio_ports; i++) {
        struct _MBLA_port_cache &p = m_audio_ports.at(i);
        target_event = (quadlet_t *)(data + i);
        assert(nevents + offset <= p.buffer_size );

        if(p.buffer && p.enabled) {
            float *buffer = (float *)(p.buffer);
            buffer += offset;

            for(j = 0; j < nevents; j += 1) {
                unsigned int v = CondSwapFromBus32(*target_event) & 0x00FFFFFF;
                // sign-extend highest bit of 24-bit int
                int tmp = (int)(v << 8) / 256;
                *buffer = tmp * multiplier;
                buffer++;
                target_event+=m_dimension;
            }
        }
    }
}

#endif

/**
 * @brief decode all midi ports in the cache from events
 * @param data 
 * @param offset 
 * @param nevents 
 */
void
AmdtpReceiveStreamProcessor::decodeMidiPorts(quadlet_t *data,
                                              unsigned int offset,
                                              unsigned int nevents)
{
    quadlet_t *target_event;
    quadlet_t sample_int;
    unsigned int i,j;

    for (i = 0; i < m_nb_midi_ports; i++) {
        struct _MIDI_port_cache &p = m_midi_ports.at(i);
        if (p.buffer && p.enabled) { 
            uint32_t *buffer = (quadlet_t *)(p.buffer);
            buffer += offset;

            /* clear output (to jackd) buffer for MIDI data */
            memset (buffer, 0, nevents*sizeof(*buffer));

            for (j = 0; j < nevents; j += 1) {
                target_event = (quadlet_t *) (data + ((j * m_dimension) + p.position));
                sample_int = CondSwapFromBus32(*target_event);

                // FIXME: this assumes that 2X and 3X speed isn't used,
                // because only the 1X slot is put into the ringbuffer
                if(unlikely(IEC61883_AM824_HAS_LABEL(sample_int, IEC61883_AM824_LABEL_MIDI_1X))) {
                    sample_int=(sample_int >> 16) & 0x000000FF;
                    sample_int |= 0x01000000; // flag that there is a midi event present
                    midibuffer[mb_head++] = sample_int;
                    mb_head &= RX_MIDIBUFFER_SIZE-1;
                    if (unlikely(mb_head == mb_tail)) {
                        debugWarning("MOTU rx MIDI buffer overflow\n");
                        /* Dump oldest byte.  This overflow can only happen if the
                         * rate coming in from the hardware MIDI port grossly
                         * exceeds the official MIDI baud rate of 31250 bps, so it
                         * should never occur in practice.
                         */
                        mb_tail = (mb_tail + 1) & (RX_MIDIBUFFER_SIZE-1);
                    }

                    debugOutputExtreme(DEBUG_LEVEL_VERBOSE, "(%p) MIDI [%d]: %08X\n", this,
                            i, sample_int);
                } else if(unlikely((IEC61883_AM824_HAS_LABEL(sample_int, IEC61883_AM824_LABEL_MIDI_2X)
                        || IEC61883_AM824_HAS_LABEL(sample_int, IEC61883_AM824_LABEL_MIDI_3X) ))) {
                    debugOutput(DEBUG_LEVEL_VERBOSE, "Midi mode %X not supported.\n",
                            IEC61883_AM824_GET_LABEL(sample_int));
                }
                if (unlikely(!(j & 0x07))) {
                    if (mb_head != mb_tail) {
                        *buffer = midibuffer[mb_tail++];
                        mb_tail &= RX_MIDIBUFFER_SIZE-1;
                    }
                    buffer += 8;
                }
            }
        }
    }
}

bool
AmdtpReceiveStreamProcessor::initPortCache() {
    // make use of the fact that audio ports are the first ports in
    // the cluster as per AMDTP. so we can sort the ports by position
    // and have very efficient lookups:
    // m_float_ports.at(i).buffer -> audio stream i buffer
    // for midi ports we simply cache all port info since they are (usually) not
    // that numerous
    m_nb_audio_ports = 0;
    m_audio_ports.clear();
    
    m_nb_midi_ports = 0;
    m_midi_ports.clear();
    
    for(PortVectorIterator it = m_Ports.begin();
        it != m_Ports.end();
        ++it )
    {
        AmdtpPortInfo *pinfo=dynamic_cast<AmdtpPortInfo *>(*it);
        assert(pinfo); // this should not fail!!

        switch( pinfo->getFormat() )
        {
            case AmdtpPortInfo::E_MBLA:
                m_nb_audio_ports++;
                break;
            case AmdtpPortInfo::E_SPDIF: // still unimplemented
                break;
            case AmdtpPortInfo::E_Midi:
                m_nb_midi_ports++;
                break;
            default: // ignore
                break;
        }
    }

    unsigned int idx;
    for (idx = 0; idx < m_nb_audio_ports; idx++) {
        for(PortVectorIterator it = m_Ports.begin();
            it != m_Ports.end();
            ++it )
        {
            AmdtpPortInfo *pinfo=dynamic_cast<AmdtpPortInfo *>(*it);
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
                        "idx %u: looking at port %s at position %u\n",
                        idx, (*it)->getName().c_str(), pinfo->getPosition());
            if(pinfo->getPosition() == idx) {
                struct _MBLA_port_cache p;
                p.port = dynamic_cast<AmdtpAudioPort *>(*it);
                if(p.port == NULL) {
                    debugError("Port is not an AmdtpAudioPort!\n");
                    return false;
                }
                p.buffer = NULL; // to be filled by updatePortCache
                #ifdef DEBUG
                p.buffer_size = (*it)->getBufferSize();
                #endif

                m_audio_ports.push_back(p);
                debugOutput(DEBUG_LEVEL_VERBOSE,
                            "Cached port %s at position %u\n",
                            p.port->getName().c_str(), idx);
                goto next_index;
            }
        }
        debugError("No MBLA port found for position %d\n", idx);
        return false;
next_index:
        continue;
    }

    for(PortVectorIterator it = m_Ports.begin();
        it != m_Ports.end();
        ++it )
    {
        AmdtpPortInfo *pinfo=dynamic_cast<AmdtpPortInfo *>(*it);
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
                    "idx %u: looking at port %s at position %u, location %u\n",
                    idx, (*it)->getName().c_str(), pinfo->getPosition(), pinfo->getLocation());
        if ((*it)->getPortType() == Port::E_Midi) {
            struct _MIDI_port_cache p;
            p.port = dynamic_cast<AmdtpMidiPort *>(*it);
            if(p.port == NULL) {
                debugError("Port is not an AmdtpMidiPort!\n");
                return false;
            }
            p.position = pinfo->getPosition();
            p.location = pinfo->getLocation();
            p.buffer = NULL; // to be filled by updatePortCache
            #ifdef DEBUG
            p.buffer_size = (*it)->getBufferSize();
            #endif

            m_midi_ports.push_back(p);
            debugOutput(DEBUG_LEVEL_VERBOSE,
                        "Cached port %s at position %u, location %u\n",
                        p.port->getName().c_str(), p.position, p.location);
        }
    }

    return true;
}

void
AmdtpReceiveStreamProcessor::updatePortCache() {
    unsigned int idx;
    for (idx = 0; idx < m_nb_audio_ports; idx++) {
        struct _MBLA_port_cache& p = m_audio_ports.at(idx);
        AmdtpAudioPort *port = p.port;
        p.buffer = port->getBufferAddress();
        p.enabled = !port->isDisabled();
    }
    for (idx = 0; idx < m_nb_midi_ports; idx++) {
        struct _MIDI_port_cache& p = m_midi_ports.at(idx);
        AmdtpMidiPort *port = p.port;
        p.buffer = port->getBufferAddress();
        p.enabled = !port->isDisabled();
    }
}

} // end of namespace Streaming
