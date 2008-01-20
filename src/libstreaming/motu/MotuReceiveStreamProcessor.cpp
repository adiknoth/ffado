/*
 * Copyright (C) 2005-2008 by Jonathan Woithe
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

#include "MotuReceiveStreamProcessor.h"
#include "MotuPort.h"
#include "../StreamProcessorManager.h"
#include "devicemanager.h"

#include "libieee1394/ieee1394service.h"
#include "libieee1394/IsoHandlerManager.h"
#include "libieee1394/cycletimer.h"

#include <math.h>
#include <netinet/in.h>
#include <assert.h>

namespace Streaming {

// A macro to extract specific bits from a native endian quadlet
#define get_bits(_d,_start,_len) (((_d)>>((_start)-(_len)+1)) & ((1<<(_len))-1))

// Convert an SPH timestamp as received from the MOTU to a full timestamp in ticks.
static inline uint32_t sphRecvToFullTicks(uint32_t sph, uint32_t ct_now) {

uint32_t timestamp = CYCLE_TIMER_TO_TICKS(sph & 0x1ffffff);
uint32_t now_cycles = CYCLE_TIMER_GET_CYCLES(ct_now);

uint32_t ts_sec = CYCLE_TIMER_GET_SECS(ct_now);
    // If the cycles have wrapped, correct ts_sec so it represents when timestamp
    // was received.  The timestamps sent by the MOTU are always 1 or two cycles
    // in advance of the cycle timer (reasons unknown at this stage).  In addition,
    // iso buffering can delay the arrival of packets for quite a number of cycles
    // (have seen a delay >12 cycles).
    // Every so often we also see sph wrapping ahead of ct_now, so deal with that
    // too.
    if (CYCLE_TIMER_GET_CYCLES(sph) > now_cycles + 1000) {
        if (ts_sec)
            ts_sec--;
        else
            ts_sec = 127;
    } else
    if (now_cycles > CYCLE_TIMER_GET_CYCLES(sph) + 1000) {
        if (ts_sec == 127)
            ts_sec = 0;
        else
            ts_sec++;
    }
    return timestamp + ts_sec*TICKS_PER_SECOND;
}

MotuReceiveStreamProcessor::MotuReceiveStreamProcessor(FFADODevice &parent, unsigned int event_size)
    : StreamProcessor(parent, ePT_Receive)
    , m_event_size( event_size )
{}

unsigned int
MotuReceiveStreamProcessor::getMaxPacketSize() {
    int framerate = m_Parent.getDeviceManager().getStreamProcessorManager().getNominalRate();
    return framerate<=48000?616:(framerate<=96000?1032:1160);
}

unsigned int
MotuReceiveStreamProcessor::getNominalFramesPerPacket() {
    int framerate = m_Parent.getDeviceManager().getStreamProcessorManager().getNominalRate();
    return framerate<=48000?8:(framerate<=96000?16:32);
}

bool
MotuReceiveStreamProcessor::prepareChild() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing (%p)...\n", this);

    // prepare the framerate estimate
    // FIXME: not needed anymore?
    //m_ticks_per_frame = (TICKS_PER_SECOND*1.0) / ((float)m_Parent.getDeviceManager().getStreamProcessorManager().getNominalRate());

    return true;
}


/**
 * Processes packet header to extract timestamps and check if the packet is valid
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
MotuReceiveStreamProcessor::processPacketHeader(unsigned char *data, unsigned int length,
                  unsigned char channel, unsigned char tag, unsigned char sy,
                  unsigned int cycle, unsigned int dropped)
{
    if (length > 8) {
        // The iso data blocks from the MOTUs comprise a CIP-like
        // header followed by a number of events (8 for 1x rates, 16
        // for 2x rates, 32 for 4x rates).
        quadlet_t *quadlet = (quadlet_t *)data;
        unsigned int dbs = get_bits(ntohl(quadlet[0]), 23, 8);  // Size of one event in terms of fdf_size
        unsigned int fdf_size = get_bits(ntohl(quadlet[1]), 23, 8) == 0x22 ? 32:0; // Event unit size in bits

        // Don't even attempt to process a packet if it isn't what
        // we expect from a MOTU.  Yes, an FDF value of 32 bears
        // little relationship to the actual data (24 bit integer)
        // sent by the MOTU - it's one of those areas where MOTU
        // have taken a curious detour around the standards.
        if (tag!=1 || fdf_size!=32) {
            return eCRV_Invalid;
        }

        // put this after the check because event_length can become 0 on invalid packets
        unsigned int event_length = (fdf_size * dbs) / 8;       // Event size in bytes
        unsigned int n_events = (length-8) / event_length;

        // Acquire the timestamp of the last frame in the packet just
        // received.  Since every frame from the MOTU has its own timestamp
        // we can just pick it straight from the packet.
        uint32_t last_sph = ntohl(*(quadlet_t *)(data+8+(n_events-1)*event_length));
        m_last_timestamp = sphRecvToFullTicks(last_sph, m_Parent.get1394Service().getCycleTimer());
        return eCRV_OK;
    } else {
        return eCRV_Invalid;
    }
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
MotuReceiveStreamProcessor::processPacketData(unsigned char *data, unsigned int length,
                  unsigned char channel, unsigned char tag, unsigned char sy,
                  unsigned int cycle, unsigned int dropped_cycles) {
    quadlet_t* quadlet = (quadlet_t*) data;

    unsigned int dbs = get_bits(ntohl(quadlet[0]), 23, 8);  // Size of one event in terms of fdf_size
    unsigned int fdf_size = get_bits(ntohl(quadlet[1]), 23, 8) == 0x22 ? 32:0; // Event unit size in bits
    // this is only called for packets that return eCRV_OK on processPacketHeader
    // so event_length won't become 0
    unsigned int event_length = (fdf_size * dbs) / 8;       // Event size in bytes
    unsigned int n_events = (length-8) / event_length;

    // we have to keep in mind that there are also
    // some packets buffered by the ISO layer,
    // at most x=m_handler->getWakeupInterval()
    // these contain at most x*syt_interval
    // frames, meaning that we might receive
    // this packet x*syt_interval*ticks_per_frame
    // later than expected (the real receive time)
    #ifdef DEBUG
    if(isRunning()) {
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"STMP: %lluticks | tpf=%f\n",
            m_last_timestamp, getTicksPerFrame());
    }
    #endif

    if(m_data_buffer->writeFrames(n_events, (char *)(data+8), m_last_timestamp)) {
        return eCRV_OK;
    } else {
        return eCRV_XRun;
    }
}

/***********************************************
 * Encoding/Decoding API                       *
 ***********************************************/
/**
 * \brief write received events to the port ringbuffers.
 */
bool MotuReceiveStreamProcessor::processReadBlock(char *data,
                       unsigned int nevents, unsigned int offset)
{
    bool no_problem=true;
    for ( PortVectorIterator it = m_Ports.begin();
          it != m_Ports.end();
          ++it ) {
        if((*it)->isDisabled()) {continue;};

        Port *port=(*it);

        switch(port->getPortType()) {

        case Port::E_Audio:
            if(decodeMotuEventsToPort(static_cast<MotuAudioPort *>(*it), (quadlet_t *)data, offset, nevents)) {
                debugWarning("Could not decode packet data to port %s",(*it)->getName().c_str());
                no_problem=false;
            }
            break;
        case Port::E_Midi:
//             if(decodeMotuMidiEventsToPort(static_cast<MotuMidiPort *>(*it), (quadlet_t *)data, offset, nevents)) {
//                 debugWarning("Could not decode packet midi data to port %s",(*it)->getName().c_str());
//                 no_problem=false;
//             }
            break;

        default: // ignore
            break;
        }
    }
    return no_problem;
}

signed int MotuReceiveStreamProcessor::decodeMotuEventsToPort(MotuAudioPort *p,
        quadlet_t *data, unsigned int offset, unsigned int nevents)
{
    unsigned int j=0;

    // Use char here since a port's source address won't necessarily be
    // aligned; use of an unaligned quadlet_t may cause issues on
    // certain architectures.  Besides, the source (data coming directly
    // from the MOTU) isn't structured in quadlets anyway; it mainly
    // consists of packed 24-bit integers.

    unsigned char *src_data;
    src_data = (unsigned char *)data + p->getPosition();

    switch(m_StreamProcessorManager.getAudioDataType()) {
        default:
        case StreamProcessorManager::eADT_Int24:
            {
                quadlet_t *buffer=(quadlet_t *)(p->getBufferAddress());

                assert(nevents + offset <= p->getBufferSize());

                // Offset is in frames, but each port is only a single
                // channel, so the number of frames is the same as the
                // number of quadlets to offset (assuming the port buffer
                // uses one quadlet per sample, which is the case currently).
                buffer+=offset;

                for(j = 0; j < nevents; j += 1) { // Decode nsamples
                    *buffer = (*src_data<<16)+(*(src_data+1)<<8)+*(src_data+2);
                    // Sign-extend highest bit of 24-bit int.
                    // FIXME: this isn't strictly needed since E_Int24 is a 24-bit,
                    // but doing so shouldn't break anything and makes the data
                    // easier to deal with during debugging.
                    if (*src_data & 0x80)
                        *buffer |= 0xff000000;

                    buffer++;
                    src_data+=m_event_size;
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

                    unsigned int v = (*src_data<<16)+(*(src_data+1)<<8)+*(src_data+2);

                    // sign-extend highest bit of 24-bit int
                    int tmp = (int)(v << 8) / 256;

                    *buffer = tmp * multiplier;

                    buffer++;
                    src_data+=m_event_size;
                }
            }
            break;
    }

    return 0;
}

} // end of namespace Streaming
