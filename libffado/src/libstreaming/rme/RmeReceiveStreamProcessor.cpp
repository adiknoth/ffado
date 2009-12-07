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

/* CAUTION: this module is under active development.  It has been templated    
 * from the MOTU driver and many MOTU-specific details remain.  Do not use
 * this file as a reference for RME devices until initial development has     
 * been completed.
  */

#include "libutil/float_cast.h"

#include "RmeReceiveStreamProcessor.h"
#include "RmePort.h"
#include "../StreamProcessorManager.h"
#include "devicemanager.h"

#include "libieee1394/ieee1394service.h"
#include "libieee1394/IsoHandlerManager.h"
#include "libieee1394/cycletimer.h"

#include "libutil/ByteSwap.h"

// This is to pick up the RME_MODEL_* constants.  There's probably a better
// way ...
#include "../../rme/rme_avdevice.h"

#include <cstring>
#include <math.h>
#include <assert.h>

/* Provide more intuitive access to GCC's branch predition built-ins */
#define likely(x)   __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)


namespace Streaming {

// A macro to extract specific bits from a native endian quadlet
#define get_bits(_d,_start,_len) (((_d)>>((_start)-(_len)+1)) & ((1<<(_len))-1))

RmeReceiveStreamProcessor::RmeReceiveStreamProcessor(FFADODevice &parent, 
  unsigned int model, unsigned int event_size)
    : StreamProcessor(parent, ePT_Receive)
    , m_rme_model( model )
    , m_event_size( event_size )
    , mb_head ( 0 )
    , mb_tail ( 0 )
{
}

unsigned int
RmeReceiveStreamProcessor::getMaxPacketSize() {
    int framerate = m_Parent.getDeviceManager().getStreamProcessorManager().getNominalRate();
    if (m_rme_model == Rme::RME_MODEL_FIREFACE800)
        return 8 + (framerate<=48000?784:(framerate<=96000?1200:1200));
    else
        return 8 + (framerate<=48000?504:(framerate<=96000?840:1000));
}

unsigned int
RmeReceiveStreamProcessor::getNominalFramesPerPacket() {
    int framerate = m_Parent.getDeviceManager().getStreamProcessorManager().getNominalRate();
    return framerate<=48000?7:(framerate<=96000?15:25);
}

bool
RmeReceiveStreamProcessor::prepareChild() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing (%p)...\n", this);

    // prepare the framerate estimate
    // FIXME: not needed anymore?
    //m_ticks_per_frame = (TICKS_PER_SECOND*1.0) / ((float)m_Parent.getDeviceManager().getStreamProcessorManager().getNominalRate());

    return true;
}


/*
 * Processes packet header to extract timestamps and check if the packet is 
 * valid.
 */
enum StreamProcessor::eChildReturnValue
RmeReceiveStreamProcessor::processPacketHeader(unsigned char *data, unsigned int length, 
                                                unsigned char tag, unsigned char sy,
                                                uint32_t pkt_ctr)
{
// For testing
debugOutput(DEBUG_LEVEL_VERBOSE, "data packet header\n");

    if (length > 8) {
        // The iso data blocks from the RMEs comprise 24-bit audio
        // data encoded in 32-bit integers.  The LSB of the 32-bit integers
        // of certain channels are used for house-keeping information.
        // The number of samples for each channel present in a packet
        // varies: 7 for 1x rates, 15 for 2x rates and 25 for 4x rates.
        // quadlet_t *quadlet = (quadlet_t *)data;

        // Don't even attempt to process a packet if it isn't what we expect
        // from an RME.  For now the only condition seems to be a tag of 0.
        // This will be fleshed out in due course.
//        if (tag!=1) {
//            return eCRV_Invalid;
//        }

        // Timestamps are not transmitted explicitly by the RME interfaces
        // so we'll have to fake it somehow in order to fit in with the
        // rest of the FFADO infrastructure.  For now just take the current
        // value of the cycle timer as the "last timestamp".  In practice 
        // there a fixed offset that we'll have to include eventually.
        m_last_timestamp = CYCLE_TIMER_TO_TICKS(m_Parent.get1394Service().getCycleTimer());

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
 * @param pkt_ctr 
 * @return 
 */
enum StreamProcessor::eChildReturnValue
RmeReceiveStreamProcessor::processPacketData(unsigned char *data, unsigned int length) {
    // m_event_size should never be zero
    unsigned int n_events = (length-8) / m_event_size;

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

// For testing
debugOutput(DEBUG_LEVEL_VERBOSE, "data packet data\n");

    if(m_data_buffer->writeFrames(n_events, (char *)data, m_last_timestamp)) {
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
bool RmeReceiveStreamProcessor::processReadBlock(char *data,
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
            if(decodeRmeEventsToPort(static_cast<RmeAudioPort *>(*it), (quadlet_t *)data, offset, nevents)) {
                debugWarning("Could not decode packet data to port %s\n",(*it)->getName().c_str());
                no_problem=false;
            }
            break;
        case Port::E_Midi:
             if(decodeRmeMidiEventsToPort(static_cast<RmeMidiPort *>(*it), (quadlet_t *)data, offset, nevents)) {
                 debugWarning("Could not decode packet midi data to port %s\n",(*it)->getName().c_str());
                 no_problem=false;
             }
            break;

        default: // ignore
            break;
        }
    }
    return no_problem;
}

signed int RmeReceiveStreamProcessor::decodeRmeEventsToPort(RmeAudioPort *p,
        quadlet_t *data, unsigned int offset, unsigned int nevents)
{
    unsigned int j=0;

    // Use char here since a port's source address won't necessarily be
    // aligned; use of an unaligned quadlet_t may cause issues on certain
    // architectures.  Besides, the source (data coming directly from the
    // RME) isn't structured in quadlets anyway; it consists 24-bit integers
    // within 32-bit quadlets with the LSB being a housekeeping byte.

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
                    // This isn't strictly needed since E_Int24 is a 24-bit,
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

                    signed int v = (*src_data<<16)+(*(src_data+1)<<8)+*(src_data+2);
                    /* Sign-extend highest bit of incoming 24-bit integer */
                    if (*src_data & 0x80)
                      v |= 0xff000000;
                    *buffer = v * multiplier;
                    buffer++;
                    src_data+=m_event_size;
                }
            }
            break;
    }

    return 0;
}

int
RmeReceiveStreamProcessor::decodeRmeMidiEventsToPort(
                      RmeMidiPort *p, quadlet_t *data,
                      unsigned int offset, unsigned int nevents)
{
    unsigned int j = 0;
    unsigned char *src = NULL;

    quadlet_t *buffer = (quadlet_t *)(p->getBufferAddress());
    assert(nevents + offset <= p->getBufferSize());
    buffer += offset;

    // Zero the buffer
    memset(buffer, 0, nevents*sizeof(*buffer));

    // Get MIDI bytes if present in any frames within the packet.  RME MIDI
    // data is sent as part of a 3-byte sequence starting at the port's
    // position.  Some RMEs (eg: the 828MkII) send more than one MIDI byte
    // in some packets.  Since the FFADO MIDI layer requires a MIDI byte in
    // only every 8th buffer position we allow for this by buffering the
    // incoming data.  The buffer is small since it only has to cover for
    // short-term excursions in the data rate.  Since the MIDI data
    // originates on a physical MIDI bus the overall data rate is limited by
    // the baud rate of that bus (31250), which is no more than one byte in
    // 8 even for 1x sample rates.
    src = (unsigned char *)data + p->getPosition();
    // We assume that the buffer has been set up in such a way that the first
    // element is correctly aligned for FFADOs MIDI layer.  The requirement
    // is that actual MIDI bytes must be aligned to multiples of 8 samples.  

    while (j < nevents) {
        /* Most events don't have MIDI data bytes */
//        if (unlikely((*src & RME_KEY_MASK_MIDI) == RME_KEY_MASK_MIDI)) {
        if (0) {
            // A MIDI byte is in *(src+2).  Bit 24 is used to flag MIDI data
            // as present once the data makes it to the output buffer.
            midibuffer[mb_head++] = 0x01000000 | *(src+2);
            mb_head &= RX_MIDIBUFFER_SIZE-1;
            if (unlikely(mb_head == mb_tail)) {
                debugWarning("RME rx MIDI buffer overflow\n");
                /* Dump oldest byte.  This overflow can only happen if the
                 * rate coming in from the hardware MIDI port grossly
                 * exceeds the official MIDI baud rate of 31250 bps, so it
                 * should never occur in practice.
                 */
                mb_tail = (mb_tail + 1) & (RX_MIDIBUFFER_SIZE-1);
            }
        }
        /* Write to the buffer if we're at an 8-sample boundary */
        if (unlikely(!(j & 0x07))) {
            if (mb_head != mb_tail) {
                *buffer = midibuffer[mb_tail++];
                mb_tail &= RX_MIDIBUFFER_SIZE-1;
            }
            buffer += 8;
        }
        j++;
        src += m_event_size;
    }

    return 0;    
}

} // end of namespace Streaming
