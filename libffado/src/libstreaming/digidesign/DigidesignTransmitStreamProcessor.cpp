/*
 * Copyright (C) 2005-2008, 2011 by Jonathan Woithe
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

#include "libutil/float_cast.h"

#include "DigidesignTransmitStreamProcessor.h"
#include "DigidesignPort.h"
#include "../StreamProcessorManager.h"
#include "devicemanager.h"

#include "libieee1394/ieee1394service.h"
#include "libieee1394/IsoHandlerManager.h"
#include "libieee1394/cycletimer.h"

#include "libutil/ByteSwap.h"

#include <cstring>
#include <assert.h>

namespace Streaming
{

/* transmit */
DigidesignTransmitStreamProcessor::DigidesigTransmitStreamProcessor(FFADODevice &parent, unsigned int event_size )
        : StreamProcessor(parent, ePT_Transmit )
        , m_event_size( event_size )
{
    // Provide any other initialisation code needed.
}

unsigned int
DigidesignTransmitStreamProcessor::getMaxPacketSize() {

    // Return the maximum packet size expected given the current device
    // configuration, in bytes.  Often this will depend on the current
    // sample rate which can be retrieved using something like this:
    //
    //   int framerate = m_Parent.getDeviceManager().getStreamProcessorManager().getNominalRate();

    return 0;
}

unsigned int
DigidesignTransmitStreamProcessor::getNominalFramesPerPacket() {

    // Return the number of frames per packet.  Often this will depend on
    // the device's current sample rate which can be obtained as per the
    // comment in getMaxPacketSize().
    
    return 0;
}

enum StreamProcessor::eChildReturnValue
DigidesignTransmitStreamProcessor::generatePacketHeader (
    unsigned char *data, unsigned int *length,
    unsigned char *tag, unsigned char *sy,
    uint32_t pkt_ctr )
{

    // Construct the header portion of a packet to send to the Digidesign
    // hardware.  This normally requires the "length", "tag" and "sy" fields
    // to be set as needed.  While "length" should be set here, "data"
    // probably ought to be left alone (there's another method for dealing
    // with the packet data).  "pkt_ctr" gives the iso cycle timer
    // corresponding to the time that the packet will be transmitted.
    //
    // It is this method which determines whether a packet should be sent in
    // the iso cycle indicated by "pkt_ctr".  The code which follows is
    // mostly boiler-plate and lifted straight from the MOTU driver (which
    // in turn came from the AMDTP code from memory).  The basic theory as
    // to when a packet should be sent is very similar for all devices
    // because the timing requirements are effectively abstracted by the
    // concept of the timestamp.
    //
    // The vast majority of the logic in this method came from Pieter
    // Palmers, who wrote the foundational streaming infrastructure.

    unsigned int cycle = CYCLE_TIMER_GET_CYCLES(pkt_ctr);

    signed n_events = getNominalFramesPerPacket();

    // Do housekeeping expected for all packets.  Note that if it is later
    // identified that an empty packet should be sent then "length" will be
    // overriden in generateEmptyPacketHeader().
    //
    // As per the firewire standards, only set "tag" if the Digidesign
    // expects a CIP header in the first two bytes of "data".  Similarly,
    // remove the "+8" from the length calculation if no CIP header is to be
    // included.
    *sy = 0x00;
    *tag = 1;      // Set to 0 if Digidesign don't use CIP headers
    *length = n_events*m_event_size + 8;

    signed int fc;
    uint64_t presentation_time;
    unsigned int presentation_cycle;
    int cycles_until_presentation;

    uint64_t transmit_at_time;
    unsigned int transmit_at_cycle;
    int cycles_until_transmit;

    debugOutput ( DEBUG_LEVEL_ULTRA_VERBOSE, "Try for cycle %d\n", cycle );
    // check whether the packet buffer has packets for us to send.
    // the base timestamp is the one of the next sample in the buffer
    ffado_timestamp_t ts_head_tmp;
    m_data_buffer->getBufferHeadTimestamp ( &ts_head_tmp, &fc ); // thread safe

    // the timestamp gives us the time at which we want the sample block
    // to be output by the device
    presentation_time = ( uint64_t ) ts_head_tmp;

    // now we calculate the time when we have to transmit the sample block
    transmit_at_time = substractTicks ( presentation_time, DIGIDESIGN_TRANSMIT_TRANSFER_DELAY );

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
        if ( cycles_until_presentation <= DIGIDESIGN_MIN_CYCLES_BEFORE_PRESENTATION )
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
                        "Too late: CY=%04u, TC=%04u, CUT=%04d, TSP=%011"PRIu64" (%04u)\n",
                        cycle,
                        transmit_at_cycle, cycles_until_transmit,
                        presentation_time, (unsigned int)TICKS_TO_CYCLES(presentation_time) );

            // however, if we can send this sufficiently before the presentation
            // time, it could be harmless.
            // NOTE: dangerous since the device has no way of reporting that it didn't get
            //       this packet on time.
            if(cycles_until_presentation >= DIGIDESIGN_MIN_CYCLES_BEFORE_PRESENTATION)
            {
                // we are not that late and can still try to transmit the packet
                fillDataPacketHeader((quadlet_t *)data, length, presentation_time);
                m_last_timestamp = presentation_time;
                return eCRV_Packet;
            }
            else   // definitely too late
            {
                return eCRV_XRun;
            }
        }
        else if(cycles_until_transmit <= DIGIDESIGN_MAX_CYCLES_TO_TRANSMIT_EARLY)
        {
            // it's time send the packet
            fillDataPacketHeader((quadlet_t *)data, length, presentation_time);
            m_last_timestamp = presentation_time;
            return eCRV_Packet;
        }
        else
        {
            debugOutput ( DEBUG_LEVEL_VERY_VERBOSE,
                        "Too early: CY=%04u, TC=%04u, CUT=%04d, TST=%011"PRIu64" (%04u), TSP=%011"PRIu64" (%04u)\n",
                        cycle,
                        transmit_at_cycle, cycles_until_transmit,
                        transmit_at_time, ( unsigned int ) TICKS_TO_CYCLES ( transmit_at_time ),
                        presentation_time, ( unsigned int ) TICKS_TO_CYCLES ( presentation_time ) );
#ifdef DEBUG
            if ( cycles_until_transmit > DIGIDESIGN_MAX_CYCLES_TO_TRANSMIT_EARLY + 1 )
            {
                debugOutput ( DEBUG_LEVEL_VERY_VERBOSE,
                            "Way too early: CY=%04u, TC=%04u, CUT=%04d, TST=%011"PRIu64" (%04u), TSP=%011"PRIu64" (%04u)\n",
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
DigidesignTransmitStreamProcessor::generatePacketData (
    unsigned char *data, unsigned int *length)
{

    // This method should fill the "length" bytes of "data" with streaming
    // data from the devices ports.  Similar to the receive side, this
    // method calls the object's data buffer readFrames() method which takes
    // care of calling the encoding functions to facilitate this data copy.

    // Treat the packet data as being in quadlets.
    quadlet_t *quadlet = (quadlet_t *)data;

    // If there's a CIP header included, skip past it.  Otherwise don't
    // do this step.
    quadlet += 2;

    signed n_events = getNominalFramesPerPacket();
    unsigned dbs = m_event_size / 4;

    // Encode data into packet.  If a CIP header is to be placed at the
    // start of "data", the pointer passed to readFrames() should be
    // (data+8) so audio data isn't copied to that location.
    if (m_data_buffer->readFrames(n_events, (char *)(data + 8))) {

        // If audio data was succesfully copied, deal with timestamps
        // embedded in the ISO stream if relevant.  How this is done depends
        // on what the device expects.  Some devices like the MOTUs
        // timestamp each frame, while others have a single timestamp
        // somewhere in the packet which applies to a particular
        // representative frame within the packet.
        //
        // The timestamps are usually in terms of iso cycle timer ticks, and
        // it's therefore often useful to know how many ticks correspond to
        // the interval between two frames (as deduced by the rate at which
        // data is arriving from the device).  This can be accessed from
        // here with something like:
        //
        //   float ticks_per_frame = m_Parent.getDeviceManager().getStreamProcessorManager().getSyncSource().getTicksPerFrame();
        //
        // "quadlet" starts out pointing to the start of the first frame,
        // and it can be advanced to the next frame by adding dbs to it.

        return eCRV_OK;
    }
    else return eCRV_XRun;

}

enum StreamProcessor::eChildReturnValue
DigidesignTransmitStreamProcessor::generateEmptyPacketHeader (
    unsigned char *data, unsigned int *length,
    unsigned char *tag, unsigned char *sy,
    uint32_t pkt_ctr )
{
    debugOutput ( DEBUG_LEVEL_VERY_VERBOSE, "XMIT EMPTY: CY=%04d, TSP=%011"PRIu64" (%04u)\n",
                (int)CYCLE_TIMER_GET_CYCLES(pkt_ctr), m_last_timestamp, 
                ( unsigned int ) TICKS_TO_CYCLES ( m_last_timestamp ) );

    // An "empty" packet is one which contains no audio data.  It is used
    // when it is determined that no audio data needs to be sent to the
    // device in a given iso cycle.  Whether one sends empty packets or just
    // skips the cycle entirely depends on the device's protocol.  Some
    // expect empty packets while others are happy if nothing is sent.
    

    // The following sets up an "empty" packet assuming that a CIP header is
    // included in such a packet.  If this isn't the case, "tag" should be 0
    // and the "length" will be 0.
    *sy = 0x00;
    *tag = 1;
    *length = 8;

    fillNoDataPacketHeader ( (quadlet_t *)data, length );
    return eCRV_OK;
}

enum StreamProcessor::eChildReturnValue
DigidesignTransmitStreamProcessor::generateEmptyPacketData (
    unsigned char *data, unsigned int *length)
{
    // By definition an empty packet doesn't contain any audio data, so
    // normally this method doesn't have to do anything.
    return eCRV_OK;
}

enum StreamProcessor::eChildReturnValue
DigidesignTransmitStreamProcessor::generateSilentPacketHeader (
    unsigned char *data, unsigned int *length,
    unsigned char *tag, unsigned char *sy,
    uint32_t pkt_ctr )
{
    // This method generates a silent packet - that is, one which contains
    // nothing but zeros in the audio data fields.  All other aspects of the
    // packet are the same as a regular data packet so much of the logic
    // from generatePacketHeader() is needed here too.  The main difference
    // between the two methods is the source of audio data - here we just
    // need zeros.
    //
    // Note that not all devices require "silent" packets.  If the
    // Digidesign interfaces don't this function may ultimtely be removed.

    unsigned int cycle = CYCLE_TIMER_GET_CYCLES(pkt_ctr);

    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "XMIT SILENT: CY=%04u, TSP=%011"PRIu64" (%04u)\n",
                 cycle, m_last_timestamp,
                 ( unsigned int ) TICKS_TO_CYCLES ( m_last_timestamp ) );

    signed n_events = getNominalFramesPerPacket();

    // Again, here's the housekeeping.  If there's no CIP header needed, set "tag"
    // to 0 and remove the "+8" from the setting of "length" below.
    *sy = 0x00;
    *tag = 1;

    /* Assume the packet will have audio data.  If it turns out we need an empty packet
     * the length will be overridden by fillNoDataPacketHeader().
     */
    *length = n_events*m_event_size + 8;

    uint64_t presentation_time;
    unsigned int presentation_cycle;
    int cycles_until_presentation;
            
    uint64_t transmit_at_time;
    unsigned int transmit_at_cycle;
    int cycles_until_transmit;

    /* The sample buffer is not necessarily running when silent packets are
     * needed, so use m_last_timestamp (the timestamp of the previously sent
     * data packet) as the basis for the presentation time of the next
     * packet.  Since we're only writing zeros we don't have to deal with
     * buffer xruns.
     */
    float ticks_per_frame = m_Parent.getDeviceManager().getStreamProcessorManager().getSyncSource().getTicksPerFrame();
    presentation_time = addTicks(m_last_timestamp, (unsigned int)lrintf(n_events * ticks_per_frame));

    transmit_at_time = substractTicks(presentation_time, DIGIDESIGN_TRANSMIT_TRANSFER_DELAY);
    presentation_cycle = (unsigned int)(TICKS_TO_CYCLES(presentation_time));
    transmit_at_cycle = (unsigned int)(TICKS_TO_CYCLES(transmit_at_time));
    cycles_until_presentation = diffCycles(presentation_cycle, cycle);
    cycles_until_transmit = diffCycles(transmit_at_cycle, cycle);

    if (cycles_until_transmit < 0)
    {
        if (cycles_until_presentation >= DIGIDESIGN_MIN_CYCLES_BEFORE_PRESENTATION)
        {
            m_last_timestamp = presentation_time;
            fillDataPacketHeader((quadlet_t *)data, length, m_last_timestamp);
            return eCRV_Packet;
        }
        else
        {
            return eCRV_XRun;
        }
    }
    else if (cycles_until_transmit <= DIGIDESIGN_MAX_CYCLES_TO_TRANSMIT_EARLY)
    {
        m_last_timestamp = presentation_time;
        fillDataPacketHeader((quadlet_t *)data, length, m_last_timestamp);
        return eCRV_Packet;
    }
    else
    {
        return eCRV_EmptyPacket;
    }
    return eCRV_Invalid;
}

enum StreamProcessor::eChildReturnValue
DigidesignTransmitStreamProcessor::generateSilentPacketData (
    unsigned char *data, unsigned int *length )
{
    // Simply set all audio data to zero since that's what's meant by
    // a "silent" packet.  Note that for the example code given below
    // m_event_size is in bytes.

    quadlet_t *quadlet = (quadlet_t *)data;
    quadlet += 2; // skip the header - remove if no CIP header is used
    // Size of a single data frame in quadlets
    unsigned dbs = m_event_size / 4;

    signed n_events = getNominalFramesPerPacket();

    memset(quadlet, 0, n_events*m_event_size);
    float ticks_per_frame = m_Parent.getDeviceManager().getStreamProcessorManager().getSyncSource().getTicksPerFrame();

    // If there are per-frame timestamps to set up (or other things), it's done here.  "quadlet" starts out
    // pointing to the start of the first frame, and it can be advanced to the next frame by adding dbs to it.

    return eCRV_OK;
}

unsigned int DigidesignTransmitStreamProcessor::fillDataPacketHeader (
    quadlet_t *data, unsigned int* length,
    uint32_t ts )
{
    // If there's a CIP header (or a similar per-packet header distinct from
    // the standard iso header) this method is used to construct it.  The return
    // value is the number of events (aka frames) to be included in the packet.

    quadlet_t *quadlet = (quadlet_t *)data;
    // Size of a single data frame in quadlets.
    unsigned dbs = m_event_size / 4;

    signed n_events = getNominalFramesPerPacket();

    // Depending on the device this might have to be set to something sensible.
    unsigned int tx_dbc = 0;

    // The following shows how a CIP header can be constructed.  This is
    // taken directly from the MOTU driver and therefore contains some
    // hard-coded fields as they are used by the MOTU devices.  Most
    // importantly, the MOTUs don't always follow the ieee1394 standard when
    // it comes to fields in the CIP header, so this code is at best a guide
    // as to how things might be done.
    //
    // The entire thing can be omitted if CIP headers aren't used by the
    // digidesign devices.
    *quadlet = CondSwapToBus32(0x00000400 | ((m_Parent.get1394Service().getLocalNodeId()&0x3f)<<24) | tx_dbc | (dbs<<16));
    quadlet++;
    *quadlet = CondSwapToBus32(0x8222ffff);
    quadlet++;

    return n_events;
}

unsigned int DigidesignTransmitStreamProcessor::fillNoDataPacketHeader (
    quadlet_t *data, unsigned int* length )
{
    // This constructs any per-packet headers required in packets containing
    // no audio data.  As for fillDataPacketHeader(), this is an example
    // lifted from the MOTU code to show how it might be done. 
    // fillNoDataPacketHeader() should return the number of frames to be
    // transmitted in the packet, which is 0 by definition.

    quadlet_t *quadlet = (quadlet_t *)data;
    // Size of a single data frame in quadlets.  See comment in
    // fillDataPacketHeader() regarding the Ultralite.
    unsigned dbs = m_event_size / 4;
    // construct the packet CIP-like header.  Even if this is a data-less
    // packet the dbs field is still set as if there were data blocks
    // present.  For data-less packets the dbc is the same as the previously
    // transmitted block.

    // Depending on the device this might have to be set to something sensible.
    unsigned dbs = m_event_size / 4;

    *quadlet = CondSwapToBus32(0x00000400 | ((m_Parent.get1394Service().getLocalNodeId()&0x3f)<<24) | tx_dbc | (dbs<<16));
    quadlet++;
    *quadlet = CondSwapToBus32(0x8222ffff);
    quadlet++;
    *length = 8;
    return 0;
}

bool DigidesignTransmitStreamProcessor::prepareChild()
{
    debugOutput ( DEBUG_LEVEL_VERBOSE, "Preparing (%p)...\n", this );

    // Additional setup can be done here if nececssary.  Normally this
    // method doesn't do anything but it's provided in case it proves useful
    // for some device.

    return true;
}

/*
* Compose the event streams for the packets from the port buffers
*/
bool DigidesignTransmitStreamProcessor::processWriteBlock(char *data,
                       unsigned int nevents, unsigned int offset) {
    bool no_problem=true;
    unsigned int i;

    // This function is the transmit equivalent of
    // DigidesignReceiveStreamProcessor::processReadBlock().  It iterates
    // over the ports registered with the device and calls the applicable
    // encoding methods to transfer data from the port buffers into the
    // packet data which is pointed to by "data".  "nevents" is the number
    // of events (aka frames) to transfer and "offset" is the position
    // within the port ring buffers to take data from.

    for ( PortVectorIterator it = m_Ports.begin();
      it != m_Ports.end();
      ++it ) {
        // If this port is disabled, unconditionally send it silence.
        if((*it)->isDisabled()) {
          if (encodeSilencePortToDigidesignEvents(static_cast<DigidesignAudioPort *>(*it), (quadlet_t *)data, offset, nevents)) {
            debugWarning("Could not encode silence for disabled port %s to Digidesign events\n",(*it)->getName().c_str());
            // Don't treat this as a fatal error at this point
          }
          continue;
        }

        Port *port=(*it);

        switch(port->getPortType()) {

        case Port::E_Audio:
            if (encodePortToDigidesignEvents(static_cast<DigidesignAudioPort *>(*it), (quadlet_t *)data, offset, nevents)) {
                debugWarning("Could not encode port %s to Digidesign events\n",(*it)->getName().c_str());
                no_problem=false;
            }
            break;
        case Port::E_Midi:
             if (encodePortToDigidesignMidiEvents(static_cast<DigidesignMidiPort *>(*it), (quadlet_t *)data, offset, nevents)) {
                 debugWarning("Could not encode port %s to Midi events\n",(*it)->getName().c_str());
                 no_problem=false;
             }
            break;
        default: // ignore
            break;
        }
    }
    return no_problem;
}

bool
DigidesignTransmitStreamProcessor::transmitSilenceBlock(char *data,
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
            if (encodeSilencePortToDigidesignEvents(static_cast<DigidesignAudioPort *>(*it), (quadlet_t *)data, offset, nevents)) {
                debugWarning("Could not encode port %s to Digidesign events\n",(*it)->getName().c_str());
                no_problem = false;
            }
            break;
        case Port::E_Midi:
            if (encodeSilencePortToDigidesignMidiEvents(static_cast<DigidesignMidiPort *>(*it), (quadlet_t *)data, offset, nevents)) {
                debugWarning("Could not encode port %s to Midi events\n",(*it)->getName().c_str());
                no_problem = false;
            }
            break;
        default: // ignore
            break;
        }
    }
    return no_problem;
}

int DigidesignTransmitStreamProcessor::encodePortToDigidesignEvents(DigidesignAudioPort *p, quadlet_t *data,
                       unsigned int offset, unsigned int nevents) {
// Encodes nevents worth of data from the given port into the given buffer.  The
// format of the buffer is precisely that which will be sent to the Digidesign interface.
// The basic idea:
//   iterate over the ports
//     * get port buffer address
//     * loop over events
//         - pick right sample in event based upon PortInfo
//         - convert sample from Port format (E_Int24, E_Float, ..) to Digidesign
//           native format
//
// We include the ability to start the transfer from the given offset within
// the port (expressed in frames) so the 'efficient' transfer method can be
// utilised.

// This code assumes that the Digidesign expects its data to be packed
// 24-bit integers.  If this is not the case changes will be required.

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
                    float in = *buffer;

#if DIGIDESIGN_CLIP_FLOATS
                    if (unlikely(in > 1.0)) in = 1.0;
                    if (unlikely(in < -1.0)) in = -1.0;
#endif
                    unsigned int v = lrintf(in * multiplier);
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

int DigidesignTransmitStreamProcessor::encodeSilencePortToDigidesignEvents(DigidesignAudioPort *p, quadlet_t *data,
                       unsigned int offset, unsigned int nevents) {

    // Encodes silence to the digidesign channel corresponding to the given
    // audio port.  As for encodePortToDigidesignEvents() above, this
    // assumes that each audio data sample is a packed signed 24-bit
    // integer.  Changes will be necessary if Digidesign uses a different
    // format in the packets.

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

int DigidesignTransmitStreamProcessor::encodePortToDigidesignMidiEvents(
                       DigidesignMidiPort *p, quadlet_t *data,
                       unsigned int offset, unsigned int nevents) {

    // Encode MIDI data into the packet to be sent to the Digidesign
    // hardware.  Depending on the way MIDI data is formatted in the packet,
    // this function may take a similar form to
    // encodePortToDigidesignEvents(), or it may be completely different. 
    // For example, the MOTU driver structures it quite differently due to
    // the way MIDI is carried in the packet.
    // 
    // The return value is zero.

    return 0;
}

int DigidesignTransmitStreamProcessor::encodeSilencePortToDigidesignMidiEvents(
                       DigidesignMidiPort *p, quadlet_t *data,
                       unsigned int offset, unsigned int nevents) {

    // Write zeros to a MIDI port region of the transmit packet.

    unsigned int j;
    unsigned char *target = (unsigned char *)data + p->getPosition();

    // For now, a "silent" MIDI event contains nothing but zeroes.  This
    // may have to change if we find this isn't for some reason appropriate.
    for (j=0; j<nevents; j++, target+=m_event_size) {
       memset(target, 0, 3);
    }

    return 0;
}

} // end of namespace Streaming
