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
#include "config.h"

#include "AmdtpReceiveStreamProcessor.h"
#include "AmdtpPort.h"
#include "../StreamProcessorManager.h"
#include "devicemanager.h"

#include "libieee1394/ieee1394service.h"
#include "libieee1394/IsoHandlerManager.h"
#include "libieee1394/cycletimer.h"

#include <netinet/in.h>
#include <assert.h>

namespace Streaming {

/* --------------------- RECEIVE ----------------------- */

AmdtpReceiveStreamProcessor::AmdtpReceiveStreamProcessor(FFADODevice &parent, int dimension)
    : StreamProcessor(parent, ePT_Receive)
    , m_dimension( dimension )
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
    #ifdef DEBUG
    static uint32_t now_prev=0;
    static uint64_t now_prev_ticks=0;
    #endif

    struct iec61883_packet *packet = (struct iec61883_packet *) data;
    assert(packet);
    bool ok = (packet->syt != 0xFFFF) &&
                  (packet->fdf != 0xFF) &&
                  (packet->fmt == 0x10) &&
                  (packet->dbs > 0) &&
                  (length >= 2*sizeof(quadlet_t));
    if(ok) {
        uint32_t now = m_1394service.getCycleTimer();

        #ifdef DEBUG
        uint64_t now_ticks = CYCLE_TIMER_TO_TICKS(now);

        if (diffTicks(now_ticks, now_prev_ticks) < 0) {
            debugWarning("non-monotonic CTR on cycle %04u: %llu -> %llu\n", cycle, now_prev_ticks, now_ticks);
            debugWarning("                               : %08X -> %08X\n", now_prev, now);
            debugOutput ( DEBUG_LEVEL_VERBOSE,
                        " current: %011llu (%03us %04ucy %04uticks)\n",
                        now,
                        (unsigned int)TICKS_TO_SECS( now ),
                        (unsigned int)TICKS_TO_CYCLES( now ),
                        (unsigned int)TICKS_TO_OFFSET( now ) );
            debugOutput ( DEBUG_LEVEL_VERBOSE,
                        " prev   : %011llu (%03us %04ucy %04uticks)\n",
                        now_prev,
                        (unsigned int)TICKS_TO_SECS( now_prev ),
                        (unsigned int)TICKS_TO_CYCLES( now_prev ),
                        (unsigned int)TICKS_TO_OFFSET( now_prev ) );
        }
        now_prev = now;
        now_prev_ticks=now_ticks;
        #endif

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
        debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                           "STMP: %lluticks | syt_interval=%d, tpf=%f\n",
                           m_last_timestamp, m_syt_interval, getTicksPerFrame());
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
                        this,nevents,offset);

    bool no_problem=true;

    for ( PortVectorIterator it = m_Ports.begin();
          it != m_Ports.end();
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
        case AmdtpPortInfo::E_Midi:
            if(decodeMidiEventsToPort(static_cast<AmdtpMidiPort *>(*it), (quadlet_t *)data, offset, nevents)) {
                debugWarning("Could not decode packet Midi to port %s",(*it)->getName().c_str());
                no_problem=false;
            }
            break;
        default: // ignore
            break;
        }
    }
    return no_problem;
}

#if USE_SSE
#error broken
typedef float v4sf __attribute__ ((vector_size (16)));
typedef int v4si __attribute__ ((vector_size (16)));
typedef int v2si __attribute__ ((vector_size (8)));

int
AmdtpReceiveStreamProcessor::decodeMBLAEventsToPort(
                       AmdtpAudioPort *p, quadlet_t *data,
                       unsigned int offset, unsigned int nevents)
{
    unsigned int j=0;
    quadlet_t *target_event;

    target_event=(quadlet_t *)(data + p->getPosition());

    static const float multiplier = 1.0f / (float)(0x7FFFFF);
    static const float sse_multiplier[4] __attribute__((aligned(16))) = {
            1.0f / (float)(0x7FFFFF),
            1.0f / (float)(0x7FFFFF),
            1.0f / (float)(0x7FFFFF),
            1.0f / (float)(0x7FFFFF)
    };
    unsigned int tmp[4];

    switch(p->getDataType()) {
        default:
            debugError("bad type: %d\n", p->getDataType());
            return -1;
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
                float *buffer=(float *)(p->getBufferAddress());

                assert(nevents + offset <= p->getBufferSize());

                buffer += offset;
                j = 0;
                if(nevents > 3) {
                    for(j = 0; j < nevents-3; j += 4) {
                        tmp[0] = ntohl(*target_event);
                        target_event += m_dimension;
                        tmp[1] = ntohl(*target_event);
                        target_event += m_dimension;
                        tmp[2] = ntohl(*target_event);
                        target_event += m_dimension;
                        tmp[3] = ntohl(*target_event);
                        target_event += m_dimension;
                        asm("pslld $8, %[in2]\n\t" // sign extend 24th bit
                                "pslld $8, %[in1]\n\t"
                                "psrad $8, %[in2]\n\t"
                                "psrad $8, %[in1]\n\t"
                                "cvtpi2ps %[in2], %%xmm0\n\t"
                                "movlhps %%xmm0, %%xmm0\n\t"
                                "cvtpi2ps %[in1], %%xmm0\n\t"
                                "mulps %[ssemult], %%xmm0\n\t"
                                "movups %%xmm0, %[floatbuff]"
                            : [floatbuff] "=m" (*(v4sf*)buffer)
                            : [in1] "y" (*(v2si*)tmp),
                        [in2] "y" (*(v2si*)(tmp+2)),
                        [ssemult] "x" (*(v4sf*)sse_multiplier)
                            : "xmm0");
                        buffer += 4;
                    }
                }
                for(; j < nevents; ++j) { // decode max nsamples
                    unsigned int v = ntohl(*target_event) & 0x00FFFFFF;
                    // sign-extend highest bit of 24-bit int
                    int tmp = (int)(v << 8) / 256;
                    *buffer = tmp * multiplier;

                    buffer++;
                    target_event += m_dimension;
                }
                asm volatile("emms");
                break;
            }
            break;
    }

    return 0;
}

int
AmdtpReceiveStreamProcessor::decodeMidiEventsToPort(
                       AmdtpMidiPort *p, quadlet_t *data,
                       unsigned int offset, unsigned int nevents)
{
    #warning implement
}

#else

int
AmdtpReceiveStreamProcessor::decodeMBLAEventsToPort(
                       AmdtpAudioPort *p, quadlet_t *data,
                       unsigned int offset, unsigned int nevents)
{
    unsigned int j=0;
    quadlet_t *target_event;

    target_event=(quadlet_t *)(data + p->getPosition());

    switch(m_StreamProcessorManager.getAudioDataType()) {
        default:
            debugError("bad type: %d\n", m_StreamProcessorManager.getAudioDataType());
            return -1;
        case StreamProcessorManager::eADT_Int24:
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
        case StreamProcessorManager::eADT_Float:
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

int
AmdtpReceiveStreamProcessor::decodeMidiEventsToPort(
                       AmdtpMidiPort *p, quadlet_t *data,
                       unsigned int offset, unsigned int nevents)
{
    unsigned int j=0;
    quadlet_t *target_event;
    quadlet_t sample_int;
    unsigned int position = p->getPosition();
    unsigned int location = p->getLocation();

    quadlet_t *buffer=(quadlet_t *)(p->getBufferAddress());

    assert(nevents + offset <= p->getBufferSize());

    buffer+=offset;

    // clear
    memset(buffer, 0, nevents * 4);

    // assumes that dbc%8 == 0, which is always true if data points to the
    // start of a packet in blocking mode
    // midi events that belong to the same time mpx-ed block should all be
    // timed at the SYT timestamp of the packet. This basically means that they
    // all correspond to the first audio frame in the packet.
    for(j = location; j < nevents; j += 8) {
        target_event=(quadlet_t *)(data + ((j * m_dimension) + position));
        sample_int=ntohl(*target_event);
        // FIXME: this assumes that 2X and 3X speed isn't used,
        // because only the 1X slot is put into the ringbuffer
        if(IEC61883_AM824_GET_LABEL(sample_int) != IEC61883_AM824_LABEL_MIDI_NO_DATA) {
            sample_int=(sample_int >> 16) & 0x000000FF;
            sample_int |= 0x01000000; // flag that there is a midi event present
            *buffer = sample_int;
            debugOutput(DEBUG_LEVEL_VERBOSE, "Received midi byte %08X on port %p index %d\n", sample_int, p, j-location);
        }
        buffer += 8; // skip 8 frames
    }

    return 0;
}

#endif
} // end of namespace Streaming
