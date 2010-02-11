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


#include "libutil/float_cast.h"

#include "MotuReceiveStreamProcessor.h"
#include "MotuPort.h"
#include "../StreamProcessorManager.h"
#include "devicemanager.h"

#include "libieee1394/ieee1394service.h"
#include "libieee1394/IsoHandlerManager.h"
#include "libieee1394/cycletimer.h"

#include "libutil/ByteSwap.h"

#include <cstring>
#include <math.h>
#include <assert.h>

/* Provide more intuitive access to GCC's branch predition built-ins */
#define likely(x)   __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)


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
    if (unlikely(CYCLE_TIMER_GET_CYCLES(sph) > now_cycles + 1000)) {
        if (likely(ts_sec))
            ts_sec--;
        else
            ts_sec = 127;
    } else
    if (unlikely(now_cycles > CYCLE_TIMER_GET_CYCLES(sph) + 1000)) {
        if (unlikely(ts_sec == 127))
            ts_sec = 0;
        else
            ts_sec++;
    }
    return timestamp + ts_sec*TICKS_PER_SECOND;
}

MotuReceiveStreamProcessor::MotuReceiveStreamProcessor(FFADODevice &parent, unsigned int event_size)
    : StreamProcessor(parent, ePT_Receive)
    , m_event_size( event_size )
    , mb_head ( 0 )
    , mb_tail ( 0 )
{
    memset(&m_devctrls, 0, sizeof(m_devctrls));
}

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
 * @return 
 */
enum StreamProcessor::eChildReturnValue
MotuReceiveStreamProcessor::processPacketHeader(unsigned char *data, unsigned int length, 
                                                unsigned char tag, unsigned char sy,
                                                uint32_t pkt_ctr)
{
    static int len_shown = 0;

    if (length > 8) {
        // The iso data blocks from the MOTUs comprise a CIP-like
        // header followed by a number of events (8 for 1x rates, 16
        // for 2x rates, 32 for 4x rates).
        quadlet_t *quadlet = (quadlet_t *)data;
        unsigned int dbs = get_bits(CondSwapFromBus32(quadlet[0]), 23, 8);  // Size of one event in terms of fdf_size
        unsigned int fdf_size = get_bits(CondSwapFromBus32(quadlet[1]), 23, 8) == 0x22 ? 32:0; // Event unit size in bits

        // Don't even attempt to process a packet if it isn't what we expect
        // from a MOTU.  Yes, an FDF value of 32 bears little relationship
        // to the actual data (24 bit integer) sent by the MOTU - it's one
        // of those areas where MOTU have taken a curious detour around the
        // standards.  Do this check early on because for invalid packets
        // dbs may not be what we expect, potentially causing issues later
        // on.
        if (tag!=1 || fdf_size!=32 || dbs==0) {
            return eCRV_Invalid;
        }

        // m_event_size is the event size in bytes
        unsigned int n_events = (length-8) / m_event_size;

        // Acquire the timestamp of the last frame in the packet just
        // received.  Since every frame from the MOTU has its own timestamp
        // we can just pick it straight from the packet.
        uint32_t last_sph = CondSwapFromBus32(*(quadlet_t *)(data+8+(n_events-1)*m_event_size));
        m_last_timestamp = sphRecvToFullTicks(last_sph, m_Parent.get1394Service().getCycleTimer());

        // For debugging only - to be removed once 896HD issues have been 
        // resolved.  JMW, 6 Jan 2010.
        if (!len_shown) {
            debugOutput(DEBUG_LEVEL_VERBOSE,"Packet from MOTU: length = %d\n", length);
            len_shown = 1;
        }

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
MotuReceiveStreamProcessor::processPacketData(unsigned char *data, unsigned int length) {
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
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"STMP: %"PRIu64"ticks | tpf=%f\n",
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

    /* Scan incoming block for device control events */
    decodeMotuCtrlEvents(data, nevents);

    for ( PortVectorIterator it = m_Ports.begin();
          it != m_Ports.end();
          ++it ) {
        if((*it)->isDisabled()) {continue;};

        Port *port=(*it);

        switch(port->getPortType()) {

        case Port::E_Audio:
            if(decodeMotuEventsToPort(static_cast<MotuAudioPort *>(*it), (quadlet_t *)data, offset, nevents)) {
                debugWarning("Could not decode packet data to port %s\n",(*it)->getName().c_str());
                no_problem=false;
            }
            break;
        case Port::E_Midi:
             if(decodeMotuMidiEventsToPort(static_cast<MotuMidiPort *>(*it), (quadlet_t *)data, offset, nevents)) {
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
MotuReceiveStreamProcessor::decodeMotuMidiEventsToPort(
                      MotuMidiPort *p, quadlet_t *data,
                      unsigned int offset, unsigned int nevents)
{
    unsigned int j = 0;
    unsigned char *src = NULL;

    quadlet_t *buffer = (quadlet_t *)(p->getBufferAddress());
    assert(nevents + offset <= p->getBufferSize());
    buffer += offset;

    // Zero the buffer
    memset(buffer, 0, nevents*sizeof(*buffer));

    // Get MIDI bytes if present in any frames within the packet.  MOTU MIDI
    // data is sent as part of a 3-byte sequence starting at the port's
    // position.  Some MOTUs (eg: the 828MkII) send more than one MIDI byte
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
        if (unlikely((*src & MOTU_KEY_MASK_MIDI) == MOTU_KEY_MASK_MIDI)) {
            // A MIDI byte is in *(src+2).  Bit 24 is used to flag MIDI data
            // as present once the data makes it to the output buffer.
            midibuffer[mb_head++] = 0x01000000 | *(src+2);
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

int
MotuReceiveStreamProcessor::decodeMotuCtrlEvents(
                      char *data, unsigned int nevents)
{
    unsigned int j = 0;
    unsigned char *src = NULL;
    unsigned char *arg = NULL;

    // Get control bytes if present in any frames within the packet.  The
    // device control messages start at (zero-based) byte 0x04 in the data
    // stream.
    src = (unsigned char *)data + 0x04;
    arg = src+1;
    while (j < nevents) {
        unsigned int control_key = *src & ~MOTU_KEY_MASK_MIDI;
        
        if (m_devctrls.status == MOTU_DEVCTRL_INVALID) {
            // Start syncing on reception of the sequence sync key which indicates
            // mix bus 1 values are pending.  Acquisition will start when we see the
            // first channel gain key after this.
            if (control_key==MOTU_KEY_SEQ_SYNC && *arg==MOTU_KEY_SEQ_SYNC_MIXBUS1) {
                 debugOutput(DEBUG_LEVEL_VERBOSE, "syncing device control status stream\n");
                 m_devctrls.status = MOTU_DEVCTRL_SYNCING;
            }
        } else
        if (m_devctrls.status == MOTU_DEVCTRL_SYNCING) {
            // Start acquiring when we see a channel gain key for mixbus 1.
            if (control_key == MOTU_KEY_SEQ_SYNC) {
                // Keep mixbus index updated since we don't execute the main parser until
                // we move to the initialising state.  Since we don't dereference this until
                // we know it's equal to 0 there's no need for bounds checking here.
                m_devctrls.mixbus_index = *arg;
            } else
            if (control_key==MOTU_KEY_CHANNEL_GAIN && m_devctrls.mixbus_index==0) {
              debugOutput(DEBUG_LEVEL_VERBOSE, "initialising device control status\n");
              m_devctrls.status = MOTU_DEVCTRL_INIT;
            }
        } else
        if (m_devctrls.status == MOTU_DEVCTRL_INIT) {
            // Consider ourselves fully initialised when a control sequence sync key
            // arrives which takes things back to mixbus 1.
            if (control_key==MOTU_KEY_SEQ_SYNC && *arg==MOTU_KEY_SEQ_SYNC_MIXBUS1 && m_devctrls.mixbus_index>0) {
                debugOutput(DEBUG_LEVEL_VERBOSE, "device control status valid: n_mixbuses=%d, n_channels=%d\n",
                    m_devctrls.n_mixbuses, m_devctrls.n_channels);
                m_devctrls.status = MOTU_DEVCTRL_VALID;
            }
        }

        if (m_devctrls.status==MOTU_DEVCTRL_INIT || m_devctrls.status==MOTU_DEVCTRL_VALID) {
            unsigned int i;
            switch (control_key) {
                case MOTU_KEY_SEQ_SYNC:
                    if (m_devctrls.mixbus_index < MOTUFW_MAX_MIXBUSES) {
                        if (m_devctrls.n_channels==0 && m_devctrls.mixbus[m_devctrls.mixbus_index].channel_gain_index!=0) {
                            m_devctrls.n_channels = m_devctrls.mixbus[m_devctrls.mixbus_index].channel_gain_index;
                        }
                    }
                    /* Mix bus to configure next is in bits 5-7 of the argument */
                    m_devctrls.mixbus_index = (*arg >> 5);
                    if (m_devctrls.mixbus_index >= MOTUFW_MAX_MIXBUSES) {
                        debugWarning("MOTU cuemix value parser error: mix bus index %d exceeded maximum %d\n",
                            m_devctrls.mixbus_index, MOTUFW_MAX_MIXBUSES);
                    } else {
                        if (m_devctrls.n_mixbuses < m_devctrls.mixbus_index+1) {
                            m_devctrls.n_mixbuses = m_devctrls.mixbus_index+1;
                        }
                        m_devctrls.mixbus[m_devctrls.mixbus_index].channel_gain_index =
                            m_devctrls.mixbus[m_devctrls.mixbus_index].channel_pan_index =
                            m_devctrls.mixbus[m_devctrls.mixbus_index].channel_control_index = 0;
                        }
                    break;
                case MOTU_KEY_CHANNEL_GAIN:
                    i = m_devctrls.mixbus[m_devctrls.mixbus_index].channel_gain_index++;
                    if (m_devctrls.mixbus_index<MOTUFW_MAX_MIXBUSES && i<MOTUFW_MAX_MIXBUS_CHANNELS) {
                        m_devctrls.mixbus[m_devctrls.mixbus_index].channel_gain[i] = *arg;
                    }
                    if (i >= MOTUFW_MAX_MIXBUS_CHANNELS) {
                        debugWarning("MOTU cuemix value parser error: channel gain index %d exceeded maximum %d\n",
                            i, MOTUFW_MAX_MIXBUS_CHANNELS);
                    }
                    break;
                case MOTU_KEY_CHANNEL_PAN:
                    i = m_devctrls.mixbus[m_devctrls.mixbus_index].channel_pan_index++;
                    if (m_devctrls.mixbus_index<MOTUFW_MAX_MIXBUSES && i<MOTUFW_MAX_MIXBUS_CHANNELS) {
                        m_devctrls.mixbus[m_devctrls.mixbus_index].channel_pan[i] = *arg;
                    }
                    if (i >= MOTUFW_MAX_MIXBUS_CHANNELS) {
                        debugWarning("MOTU cuemix value parser error: channel pan index %d exceeded maximum %d\n",
                            i, MOTUFW_MAX_MIXBUS_CHANNELS);
                    }
                    break;
                case MOTU_KEY_CHANNEL_CTRL:
                    i = m_devctrls.mixbus[m_devctrls.mixbus_index].channel_control_index++;
                    if (m_devctrls.mixbus_index<MOTUFW_MAX_MIXBUSES && i<MOTUFW_MAX_MIXBUS_CHANNELS) {
                        m_devctrls.mixbus[m_devctrls.mixbus_index].channel_control[i] = *arg;
                    }
                    if (i >= MOTUFW_MAX_MIXBUS_CHANNELS) {
                        debugWarning("MOTU cuemix value parser error: channel control index %d exceeded maximum %d\n",
                            i, MOTUFW_MAX_MIXBUS_CHANNELS);
                    }
                    break;
                case MOTU_KEY_MIXBUS_GAIN:
                    if (m_devctrls.mixbus_index < MOTUFW_MAX_MIXBUSES) {
                        m_devctrls.mixbus[m_devctrls.mixbus_index].bus_gain = *arg;
                    }
                    break;
                case MOTU_KEY_MIXBUS_DEST:
                    if (m_devctrls.mixbus_index < MOTUFW_MAX_MIXBUSES) {
                        m_devctrls.mixbus[m_devctrls.mixbus_index].bus_dest = *arg;
                    }
                    break;
                case MOTU_KEY_MAINOUT_VOL:
                    m_devctrls.main_out_volume = *arg;
                    break;
                case MOTU_KEY_PHONES_VOL:
                    m_devctrls.phones_volume = *arg;
                    break;
                case MOTU_KEY_PHONES_DEST:
                    m_devctrls.phones_assign = *arg;
                    break;
                case MOTU_KEY_INPUT_6dB_BOOST:
                    m_devctrls.input_6dB_boost = *arg;
                    break;
                case MOTU_KEY_INPUT_REF_LEVEL:
                    m_devctrls.input_ref_level = *arg;
                    break;
                case MOTU_KEY_MIDI:
                    // MIDI is dealt with elsewhere, so just pass it over
                    break;
                default:
                    // Uncomment to debug unknown keys
                    // debugOutput(DEBUG_LEVEL_VERBOSE, "Unknown MOTU key %d, value %d\n",
                    //   control_key, (signed int)(*arg));

                    // Ignore any unknown keys or those we don't care about, at
                    // least for now.
                    break;
            }
        }
        j++;
        src += m_event_size;
        arg += m_event_size;
    }

    return 0;    
}

} // end of namespace Streaming
