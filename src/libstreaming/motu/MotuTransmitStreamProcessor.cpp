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

#include "config.h"

#include "MotuTransmitStreamProcessor.h"
#include "MotuPort.h"
#include "../StreamProcessorManager.h"
#include "devicemanager.h"

#include "libieee1394/ieee1394service.h"
#include "libieee1394/IsoHandlerManager.h"
#include "libieee1394/cycletimer.h"

#include <netinet/in.h>
#include <assert.h>

// Set to 1 to enable the generation of a 1 kHz test tone in analog output 1
#define TESTTONE 1

#if TESTTONE
#include <math.h>
#endif

namespace Streaming
{

// A macro to extract specific bits from a native endian quadlet
#define get_bits(_d,_start,_len) (((_d)>>((_start)-(_len)+1)) & ((1<<(_len))-1))

// Convert a full timestamp into an SPH timestamp as required by the MOTU
static inline uint32_t fullTicksToSph(int64_t timestamp) {
    return TICKS_TO_CYCLE_TIMER(timestamp) & 0x1ffffff;
}

/* transmit */
MotuTransmitStreamProcessor::MotuTransmitStreamProcessor(FFADODevice &parent, unsigned int event_size )
        : StreamProcessor(parent, ePT_Transmit )
        , m_event_size( event_size )
        , m_tx_dbc( 0 )
{}

unsigned int
MotuTransmitStreamProcessor::getMaxPacketSize() {
    int framerate = m_Parent.getDeviceManager().getStreamProcessorManager().getNominalRate();
    return framerate<=48000?616:(framerate<=96000?1032:1160);
}

unsigned int
MotuTransmitStreamProcessor::getNominalFramesPerPacket() {
    int framerate = m_Parent.getDeviceManager().getStreamProcessorManager().getNominalRate();
    return framerate<=48000?8:(framerate<=96000?16:32);
}

enum StreamProcessor::eChildReturnValue
MotuTransmitStreamProcessor::generatePacketHeader (
    unsigned char *data, unsigned int *length,
    unsigned char *tag, unsigned char *sy,
    int cycle, unsigned int dropped, unsigned int max_length )
{
    // The number of events per packet expected by the MOTU is solely
    // dependent on the current sample rate.  An 'event' is one sample from
    // all channels plus possibly other midi and control data.
    signed n_events = getNominalFramesPerPacket();

    // Do housekeeping expected for all packets sent to the MOTU, even
    // for packets containing no audio data.
    *sy = 0x00;
    *tag = 1;      // All MOTU packets have a CIP-like header
    *length = n_events*m_event_size + 8;

    signed int fc;
    uint64_t presentation_time;
    unsigned int presentation_cycle;
    int cycles_until_presentation;

    uint64_t transmit_at_time;
    unsigned int transmit_at_cycle;
    int cycles_until_transmit;

    // FIXME: should become a define
    // the absolute minimum number of cycles we want to transmit
    // a packet ahead of the presentation time. The nominal time
    // the packet is transmitted ahead of the presentation time is
    // given by MOTU_TRANSMIT_TRANSFER_DELAY (in ticks), but in case we
    // are too late for that, this constant defines how late we can
    // be.
    const int min_cycles_before_presentation = 1;
    // FIXME: should become a define
    // the absolute maximum number of cycles we want to transmit
    // a packet ahead of the ideal transmit time. The nominal time
    // the packet is transmitted ahead of the presentation time is
    // given by MOTU_TRANSMIT_TRANSFER_DELAY (in ticks), but we can send
    // packets early if we want to. (not completely according to spec)
    const int max_cycles_to_transmit_early = 2;

    debugOutput ( DEBUG_LEVEL_ULTRA_VERBOSE, "Try for cycle %d\n", cycle );
    // check whether the packet buffer has packets for us to send.
    // the base timestamp is the one of the next sample in the buffer
    ffado_timestamp_t ts_head_tmp;
    m_data_buffer->getBufferHeadTimestamp ( &ts_head_tmp, &fc ); // thread safe

    // the timestamp gives us the time at which we want the sample block
    // to be output by the device
    presentation_time = ( uint64_t ) ts_head_tmp;
    m_last_timestamp = presentation_time;

    // now we calculate the time when we have to transmit the sample block
    transmit_at_time = substractTicks ( presentation_time, MOTU_TRANSMIT_TRANSFER_DELAY );

    // calculate the cycle this block should be presented in
    // (this is just a virtual calculation since at that time it should
    //  already be in the device's buffer)
    presentation_cycle = ( unsigned int ) ( TICKS_TO_CYCLES ( presentation_time ) );

    // calculate the cycle this block should be transmitted in
    transmit_at_cycle = ( unsigned int ) ( TICKS_TO_CYCLES ( transmit_at_time ) );

    // we can check whether this cycle is within the 'window' we have
    // to send this packet.
    // first calculate the number of cycles left before presentation time
    cycles_until_presentation = diffCycles ( presentation_cycle, cycle );

    // we can check whether this cycle is within the 'window' we have
    // to send this packet.
    // first calculate the number of cycles left before presentation time
    cycles_until_transmit = diffCycles ( transmit_at_cycle, cycle );

    if (dropped) {
        debugOutput ( DEBUG_LEVEL_VERBOSE,
                    "Gen HDR: CY=%04u, TC=%04u, CUT=%04d, TST=%011llu (%04u), TSP=%011llu (%04u)\n",
                    cycle,
                    transmit_at_cycle, cycles_until_transmit,
                    transmit_at_time, ( unsigned int ) TICKS_TO_CYCLES ( transmit_at_time ),
                    presentation_time, ( unsigned int ) TICKS_TO_CYCLES ( presentation_time ) );
    }
    // two different options:
    // 1) there are not enough frames for one packet
    //      => determine wether this is a problem, since we might still
    //         have some time to send it
    // 2) there are enough packets
    //      => determine whether we have to send them in this packet
    if ( fc < ( signed int ) getNominalFramesPerPacket() )
    {
        // not enough frames in the buffer,

        // we can still postpone the queueing of the packets
        // if we are far enough ahead of the presentation time
        if ( cycles_until_presentation <= min_cycles_before_presentation )
        {
            debugOutput ( DEBUG_LEVEL_VERBOSE,
                        "Insufficient frames (P): N=%02d, CY=%04u, TC=%04u, CUT=%04d\n",
                        fc, cycle, transmit_at_cycle, cycles_until_transmit );
            // we are too late
            return eCRV_XRun;
        }
        else
        {
            debugOutput ( DEBUG_LEVEL_VERY_VERBOSE,
                        "Insufficient frames (NP): N=%02d, CY=%04u, TC=%04u, CUT=%04d\n",
                        fc, cycle, transmit_at_cycle, cycles_until_transmit );
            // there is still time left to send the packet
            // we want the system to give this packet another go at a later time instant
            return eCRV_Again;
        }
    }
    else
    {
        // there are enough frames, so check the time they are intended for
        // all frames have a certain 'time window' in which they can be sent
        // this corresponds to the range of the timestamp mechanism:
        // we can send a packet 15 cycles in advance of the 'presentation time'
        // in theory we can send the packet up till one cycle before the presentation time,
        // however this is not very smart.

        // There are 3 options:
        // 1) the frame block is too early
        //      => send an empty packet
        // 2) the frame block is within the window
        //      => send it
        // 3) the frame block is too late
        //      => discard (and raise xrun?)
        //         get next block of frames and repeat

        if(cycles_until_transmit < 0)
        {
            // we are too late
            debugOutput(DEBUG_LEVEL_VERBOSE,
                        "Too late: CY=%04u, TC=%04u, CUT=%04d, TSP=%011llu (%04u)\n",
                        cycle,
                        transmit_at_cycle, cycles_until_transmit,
                        presentation_time, (unsigned int)TICKS_TO_CYCLES(presentation_time) );

            // however, if we can send this sufficiently before the presentation
            // time, it could be harmless.
            // NOTE: dangerous since the device has no way of reporting that it didn't get
            //       this packet on time.
            if(cycles_until_presentation >= min_cycles_before_presentation)
            {
                // we are not that late and can still try to transmit the packet
                m_tx_dbc += fillDataPacketHeader((quadlet_t *)data, length, m_last_timestamp);
                if (m_tx_dbc > 0xff)
                    m_tx_dbc -= 0x100;
                return eCRV_Packet;
            }
            else   // definitely too late
            {
                return eCRV_XRun;
            }
        }
        else if(cycles_until_transmit <= max_cycles_to_transmit_early)
        {
            // it's time send the packet
            m_tx_dbc += fillDataPacketHeader((quadlet_t *)data, length, m_last_timestamp);
            if (m_tx_dbc > 0xff)
                m_tx_dbc -= 0x100;
            return eCRV_Packet;
        }
        else
        {
            debugOutput ( DEBUG_LEVEL_VERY_VERBOSE,
                        "Too early: CY=%04u, TC=%04u, CUT=%04d, TST=%011llu (%04u), TSP=%011llu (%04u)\n",
                        cycle,
                        transmit_at_cycle, cycles_until_transmit,
                        transmit_at_time, ( unsigned int ) TICKS_TO_CYCLES ( transmit_at_time ),
                        presentation_time, ( unsigned int ) TICKS_TO_CYCLES ( presentation_time ) );
#ifdef DEBUG
            if ( cycles_until_transmit > max_cycles_to_transmit_early + 1 )
            {
                debugOutput ( DEBUG_LEVEL_VERY_VERBOSE,
                            "Way too early: CY=%04u, TC=%04u, CUT=%04d, TST=%011llu (%04u), TSP=%011llu (%04u)\n",
                            cycle,
                            transmit_at_cycle, cycles_until_transmit,
                            transmit_at_time, ( unsigned int ) TICKS_TO_CYCLES ( transmit_at_time ),
                            presentation_time, ( unsigned int ) TICKS_TO_CYCLES ( presentation_time ) );
            }
#endif
            // we are too early, send only an empty packet
            return eCRV_EmptyPacket;
        }
    }
    return eCRV_Invalid;
}

enum StreamProcessor::eChildReturnValue
MotuTransmitStreamProcessor::generatePacketData (
    unsigned char *data, unsigned int *length,
    unsigned char *tag, unsigned char *sy,
    int cycle, unsigned int dropped, unsigned int max_length )
{
    quadlet_t *quadlet = (quadlet_t *)data;
    quadlet += 2; // skip the header
    // Size of a single data frame in quadlets
    unsigned dbs = m_event_size / 4;

    // The number of events per packet expected by the MOTU is solely
    // dependent on the current sample rate.  An 'event' is one sample from
    // all channels plus possibly other midi and control data.
    signed n_events = getNominalFramesPerPacket();

    if (m_data_buffer->readFrames(n_events, (char *)(data + 8))) {
        float ticks_per_frame = m_Parent.getDeviceManager().getStreamProcessorManager().getSyncSource().getActualRate();

#if TESTTONE
        // FIXME: remove this hacked in 1 kHz test signal to
        // analog-1 when testing is complete.
        signed int i, int_tpf = (int)ticks_per_frame;
        unsigned char *sample = data+8+16;
        for (i=0; i<n_events; i++, sample+=m_event_size) {
            static signed int a_cx = 0;
            // Each sample is 3 bytes with MSB in lowest address (ie: 
            // network byte order).  After byte order swap, the 24-bit
            // MSB is in the second byte of val.
            signed int val = htonl((int)(0x7fffff*sin((1000.0*2.0*M_PI/24576000.0)*a_cx)));
            memcpy(sample,((char *)&val)+1,3);
            if ((a_cx+=int_tpf) >= 24576000) {
                a_cx -= 24576000;
            }
        }
#endif

        // Set up each frames's SPH.
        for (int i=0; i < n_events; i++, quadlet += dbs) {
//FIXME: not sure which is best for the MOTU
//            int64_t ts_frame = addTicks(ts, (unsigned int)(i * ticks_per_frame));
            int64_t ts_frame = addTicks(m_last_timestamp, (unsigned int)(i * ticks_per_frame));
            *quadlet = htonl(fullTicksToSph(ts_frame));
        }

        return eCRV_OK;
    }
    else return eCRV_XRun;

}

enum StreamProcessor::eChildReturnValue
MotuTransmitStreamProcessor::generateSilentPacketHeader (
    unsigned char *data, unsigned int *length,
    unsigned char *tag, unsigned char *sy,
    int cycle, unsigned int dropped, unsigned int max_length )
{
    debugOutput ( DEBUG_LEVEL_VERY_VERBOSE, "XMIT NONE: CY=%04u, TSP=%011llu (%04u)\n",
                cycle, m_last_timestamp, ( unsigned int ) TICKS_TO_CYCLES ( m_last_timestamp ) );

    // Do housekeeping expected for all packets sent to the MOTU, even
    // for packets containing no audio data.
    *sy = 0x00;
    *tag = 1;      // All MOTU packets have a CIP-like header
    *length = 8;

    m_tx_dbc += fillNoDataPacketHeader ( (quadlet_t *)data, length );
    return eCRV_OK;
}

enum StreamProcessor::eChildReturnValue
MotuTransmitStreamProcessor::generateSilentPacketData (
    unsigned char *data, unsigned int *length,
    unsigned char *tag, unsigned char *sy,
    int cycle, unsigned int dropped, unsigned int max_length )
{
    return eCRV_OK; // no need to do anything
}

unsigned int MotuTransmitStreamProcessor::fillDataPacketHeader (
    quadlet_t *data, unsigned int* length,
    uint32_t ts )
{
    quadlet_t *quadlet = (quadlet_t *)data;
    // Size of a single data frame in quadlets
    unsigned dbs = m_event_size / 4;

    // The number of events per packet expected by the MOTU is solely
    // dependent on the current sample rate.  An 'event' is one sample from
    // all channels plus possibly other midi and control data.
    signed n_events = getNominalFramesPerPacket();

    // construct the packet CIP-like header.  Even if this is a data-less
    // packet the dbs field is still set as if there were data blocks
    // present.  For data-less packets the dbc is the same as the previously
    // transmitted block.
    *quadlet = htonl(0x00000400 | ((m_Parent.get1394Service().getLocalNodeId()&0x3f)<<24) | m_tx_dbc | (dbs<<16));
    quadlet++;
    *quadlet = htonl(0x8222ffff);
    quadlet++;
    return n_events;
}

unsigned int MotuTransmitStreamProcessor::fillNoDataPacketHeader (
    quadlet_t *data, unsigned int* length )
{
    quadlet_t *quadlet = (quadlet_t *)data;
    // Size of a single data frame in quadlets
    unsigned dbs = m_event_size / 4;
    // construct the packet CIP-like header.  Even if this is a data-less
    // packet the dbs field is still set as if there were data blocks
    // present.  For data-less packets the dbc is the same as the previously
    // transmitted block.
    *quadlet = htonl(0x00000400 | ((m_Parent.get1394Service().getLocalNodeId()&0x3f)<<24) | m_tx_dbc | (dbs<<16));
    quadlet++;
    *quadlet = htonl(0x8222ffff);
    quadlet++;
    *length = 8;
    return 0;
}

bool MotuTransmitStreamProcessor::prepareChild()
{
    debugOutput ( DEBUG_LEVEL_VERBOSE, "Preparing (%p)...\n", this );
    return true;
}

/*
* compose the event streams for the packets from the port buffers
*/
bool MotuTransmitStreamProcessor::processWriteBlock(char *data,
                       unsigned int nevents, unsigned int offset) {
    bool no_problem=true;
    unsigned int i;

    // FIXME: ensure the MIDI and control streams are all zeroed until
    // such time as they are fully implemented.
    for (i=0; i<nevents; i++) {
        memset(data+4+i*m_event_size, 0x00, 6);
    }

    for ( PortVectorIterator it = m_Ports.begin();
      it != m_Ports.end();
      ++it ) {
        // If this port is disabled, don't process it
        if((*it)->isDisabled()) {continue;};

        Port *port=(*it);

        switch(port->getPortType()) {

        case Port::E_Audio:
            if (encodePortToMotuEvents(static_cast<MotuAudioPort *>(*it), (quadlet_t *)data, offset, nevents)) {
                debugWarning("Could not encode port %s to Motu events",(*it)->getName().c_str());
                no_problem=false;
            }
            break;
        case Port::E_Midi:
//             if (encodePortToMotuMidiEvents(static_cast<MotuMidiPort *>(*it), (quadlet_t *)data, offset, nevents)) {
//                 debugWarning("Could not encode port %s to Midi events",(*it)->getName().c_str());
//                 no_problem=false;
//             }
            break;
        default: // ignore
            break;
        }
    }
    return no_problem;
}

bool
MotuTransmitStreamProcessor::transmitSilenceBlock(char *data,
                       unsigned int nevents, unsigned int offset) {
    // This is the same as the non-silence version, except that is
    // doesn't read from the port buffers.
    bool no_problem = true;
    for ( PortVectorIterator it = m_Ports.begin();
      it != m_Ports.end();
      ++it ) {
        Port *port=(*it);

        switch(port->getPortType()) {

        case Port::E_Audio:
            if (encodeSilencePortToMotuEvents(static_cast<MotuAudioPort *>(*it), (quadlet_t *)data, offset, nevents)) {
                debugWarning("Could not encode port %s to MBLA events",(*it)->getName().c_str());
                no_problem = false;
            }
            break;
        case Port::E_Midi:
//             if (encodeSilencePortToMotuMidiEvents(static_cast<MotuMidiPort *>(*it), (quadlet_t *)data, offset, nevents)) {
//                 debugWarning("Could not encode port %s to Midi events",(*it)->getName().c_str());
//                 no_problem = false;
//             }
            break;
        default: // ignore
            break;
        }
    }
    return no_problem;
}

int MotuTransmitStreamProcessor::encodePortToMotuEvents(MotuAudioPort *p, quadlet_t *data,
                       unsigned int offset, unsigned int nevents) {
// Encodes nevents worth of data from the given port into the given buffer.  The
// format of the buffer is precisely that which will be sent to the MOTU.
// The basic idea:
//   iterate over the ports
//     * get port buffer address
//     * loop over events
//         - pick right sample in event based upon PortInfo
//         - convert sample from Port format (E_Int24, E_Float, ..) to MOTU
//           native format
//
// We include the ability to start the transfer from the given offset within
// the port (expressed in frames) so the 'efficient' transfer method can be
// utilised.

    unsigned int j=0;

    // Use char here since the target address won't necessarily be
    // aligned; use of an unaligned quadlet_t may cause issues on certain
    // architectures.  Besides, the target (data going directly to the MOTU)
    // isn't structured in quadlets anyway; it mainly consists of packed
    // 24-bit integers.
    unsigned char *target;
    target = (unsigned char *)data + p->getPosition();

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
                    *target = (*buffer >> 16) & 0xff;
                    *(target+1) = (*buffer >> 8) & 0xff;
                    *(target+2) = (*buffer) & 0xff;

                    buffer++;
                    target+=m_event_size;
                }
            }
            break;
        case StreamProcessorManager::eADT_Float:
            {
                const float multiplier = (float)(0x7FFFFF);
                float *buffer=(float *)(p->getBufferAddress());

                assert(nevents + offset <= p->getBufferSize());

                buffer+=offset;

                for(j = 0; j < nevents; j += 1) { // decode max nsamples
                    unsigned int v = (int)(*buffer * multiplier);
                    *target = (v >> 16) & 0xff;
                    *(target+1) = (v >> 8) & 0xff;
                    *(target+2) = v & 0xff;

                    buffer++;
                    target+=m_event_size;
                }
            }
            break;
    }

    return 0;
}

int MotuTransmitStreamProcessor::encodeSilencePortToMotuEvents(MotuAudioPort *p, quadlet_t *data,
                       unsigned int offset, unsigned int nevents) {
    unsigned int j=0;
    unsigned char *target = (unsigned char *)data + p->getPosition();

    switch (m_StreamProcessorManager.getAudioDataType()) {
    default:
        case StreamProcessorManager::eADT_Int24:
        case StreamProcessorManager::eADT_Float:
        for (j = 0; j < nevents; j++) {
            *target = *(target+1) = *(target+2) = 0;
            target += m_event_size;
        }
        break;
    }

    return 0;
}

} // end of namespace Streaming
