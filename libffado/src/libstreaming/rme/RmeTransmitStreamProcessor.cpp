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

#include "config.h"

#include "libutil/float_cast.h"

#include "RmeTransmitStreamProcessor.h"
#include "RmePort.h"
#include "../StreamProcessorManager.h"
#include "devicemanager.h"

#include "libieee1394/ieee1394service.h"
#include "libieee1394/IsoHandlerManager.h"
#include "libieee1394/cycletimer.h"

#include "libutil/ByteSwap.h"

#include "../../rme/rme_avdevice.h"

#include <cstring>
#include <assert.h>

// Set to 1 to enable the generation of a 1 kHz test tone in analog output 1.  Even with
// this defined to 1 the test tone will now only be produced if run with a non-zero 
// debug level.
#define TESTTONE 1

#if TESTTONE
#include <math.h>
#endif

/* Provide more intuitive access to GCC's branch predition built-ins */
#define likely(x)   __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)

namespace Streaming
{

// A macro to extract specific bits from a native endian quadlet
#define get_bits(_d,_start,_len) (((_d)>>((_start)-(_len)+1)) & ((1<<(_len))-1))

/* transmit */
RmeTransmitStreamProcessor::RmeTransmitStreamProcessor(FFADODevice &parent, 
  unsigned int model, unsigned int event_size )
        : StreamProcessor(parent, ePT_Transmit )
        , m_rme_model( model)
        , m_event_size( event_size )
        , m_tx_dbc( 0 )
        , mb_head( 0 )
        , mb_tail( 0 )
        , midi_lock( 0 )
        , streaming_has_run ( 0 )
        , streaming_has_dryrun ( 0 )
        , streaming_start_count ( 0 )
{
  int srate = m_Parent.getDeviceManager().getStreamProcessorManager().getNominalRate();
  /* Work out how many audio samples should be left between MIDI data bytes
   * in order to stay under the MIDI hardware baud rate of 31250.  MIDI data
   * is transmitted using 10 bits per byte (including the start/stop bit) so
   * this gives us 3125 bytes per second.
   */
  midi_tx_period = lrintf(ceil((float)srate / 3125));
}

unsigned int
RmeTransmitStreamProcessor::getMaxPacketSize() {
    Rme::Device *dev = static_cast<Rme::Device *>(&m_Parent);
    // Each channel comprises a single 32-bit quadlet.  Note that the return 
    // value is in bytes.
    // FIXME: getFramesPerPacket() is fixed by the sample rate class.  It
    // needs to be confirmed that the values in use are still ok even
    // when the DDS is set to run 4% fast (something that's not presently
    // implemented by FFADO, but should be at some point).
    return dev->getFramesPerPacket() * dev->getNumChannels() * 4;
}

unsigned int
RmeTransmitStreamProcessor::getNominalFramesPerPacket() {
    return static_cast<Rme::Device *>(&m_Parent)->getFramesPerPacket();
}

bool
RmeTransmitStreamProcessor::resetForStreaming()
{
    streaming_has_run = 0;
    streaming_has_dryrun = 0;
    streaming_start_count = 0;
    return true;
}

enum StreamProcessor::eChildReturnValue
RmeTransmitStreamProcessor::generatePacketHeader (
    unsigned char *data, unsigned int *length,
    unsigned char *tag, unsigned char *sy,
    uint32_t pkt_ctr )
{
    unsigned int cycle = CYCLE_TIMER_GET_CYCLES(pkt_ctr);
    signed n_events = getNominalFramesPerPacket();

    // Called once per packet.  Need to work out whether data should be sent
    // in this cycle or not and then act accordingly.  Need to deal with 
    // the condition where the buffers don't contain sufficient data to fill
    // the packet.  FIXME: this function is incomplete.

    // Do housekeeping expected for all packets, irrespective of whether
    // they will contain data.
    *sy = 0x00;
    *length = 0;

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
    transmit_at_time = substractTicks ( presentation_time, RME_TRANSMIT_TRANSFER_DELAY );

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
        if ( cycles_until_presentation <= RME_MIN_CYCLES_BEFORE_PRESENTATION )
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
        // There are enough frames, so check the time they are intended for.
        // All frames have a certain 'time window' in which they can be sent
        // this corresponds to the range of the timestamp mechanism: we can
        // send a packet 15 cycles in advance of the 'presentation time' in
        // theory we can send the packet up till one cycle before the
        // presentation time, however this is not very smart.

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

            // however, if we can send this sufficiently before the
            // presentation time, it could be harmless.
            // NOTE: dangerous since the device has no way of reporting that
            //       it didn't get this packet on time.
            if(cycles_until_presentation >= RME_MIN_CYCLES_BEFORE_PRESENTATION)
            {
                // we are not that late and can still try to transmit the
                // packet
                *length = n_events*m_event_size;
                m_tx_dbc += fillDataPacketHeader((quadlet_t *)data, length, presentation_time);
                m_last_timestamp = presentation_time;
                if (m_tx_dbc > 0xff)
                    m_tx_dbc -= 0x100;
                return eCRV_Packet;
            }
            else   // definitely too late
            {
                return eCRV_XRun;
            }
        }
        else if(cycles_until_transmit <= RME_MAX_CYCLES_TO_TRANSMIT_EARLY)
        {
            // it's time send the packet
            *length = n_events*m_event_size;
            m_tx_dbc += fillDataPacketHeader((quadlet_t *)data, length, presentation_time);
            m_last_timestamp = presentation_time;
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
            if ( cycles_until_transmit > RME_MAX_CYCLES_TO_TRANSMIT_EARLY + 1 )
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
RmeTransmitStreamProcessor::generatePacketData (
    unsigned char *data, unsigned int *length)
{
    // Size of a single data frame in quadlets
//    unsigned dbs = m_event_size / 4;

    // The number of events per packet expected by the RME is solely
    // dependent on the current sample rate.  An 'event' is one sample from
    // all channels plus possibly other midi and control data.
    signed n_events = getNominalFramesPerPacket();

    if (m_data_buffer->readFrames(n_events, (char *)(data))) {
//        float ticks_per_frame = m_Parent.getDeviceManager().getStreamProcessorManager().getSyncSource().getTicksPerFrame();

//        for (int i=0; i < n_events; i++, quadlet += dbs) {
//            int64_t ts_frame = addTicks(m_last_timestamp, (unsigned int)lrintf(i * ticks_per_frame));
//            *quadlet = CondSwapToBus32(fullTicksToSph(ts_frame));
//        }
        // FIXME: temporary
//        if (*length > 0) {
//            memset(data, *length, 0);
//        }
//
// 1 kHz tone into ch7 (phones L) for testing
{
static signed int dpy = 0;
float ticks_per_frame = m_Parent.getDeviceManager().getStreamProcessorManager().getSyncSource().getTicksPerFrame();
  signed int i, int_tpf = lrintf(ticks_per_frame);
//signed int j;
//  quadlet_t *sample = (quadlet_t *)data;
  quadlet_t *sample = (quadlet_t *)data + 6;
if (dpy==0) {
  debugOutput(DEBUG_LEVEL_NORMAL, "ticks per frame: %d %d %d (len=%d)\n", int_tpf, n_events, m_event_size, *length);
}
if (++dpy == 8000)
dpy=0;
#if TESTTONE
        if (getDebugLevel() > 0) {
            for (i=0; i<n_events; i++, sample+=m_event_size/4) {
                static signed int a_cx = 0;
                signed int val = lrintf(0x7fffff*sin((1000.0*2.0*M_PI/24576000.0)*a_cx));
//for (j=0; j<18;j++)
//*(sample+j) = val << 8;
                *sample = val << 8;
                if ((a_cx+=int_tpf) >= 24576000) {
                    a_cx -= 24576000;
                }
            }
        }
}
#endif

        return eCRV_OK;
    }
    else
    {
        // FIXME: debugOutput() for initial testing only
        debugOutput(DEBUG_LEVEL_VERBOSE, "readFrames() failure\n");
        return eCRV_XRun;
    }
}

enum StreamProcessor::eChildReturnValue
RmeTransmitStreamProcessor::generateEmptyPacketHeader (
    unsigned char *data, unsigned int *length,
    unsigned char *tag, unsigned char *sy,
    uint32_t pkt_ctr )
{
    debugOutput ( DEBUG_LEVEL_VERY_VERBOSE, "XMIT EMPTY: CY=%04lu, TSP=%011llu (%04u)\n",
                CYCLE_TIMER_GET_CYCLES(pkt_ctr), m_last_timestamp, 
                ( unsigned int ) TICKS_TO_CYCLES ( m_last_timestamp ) );

    // Do housekeeping expected for all packets, even for packets containing
    // no audio data.
    *sy = 0x00;
    *length = 0;
    *tag = 0;

    // During the dryRunning state used during startup FFADO will request
    // "empty" packets.  However, the fireface will only "start" (ie:
    // sending data to the DACs and sending data from the ADCs) 
    // packets once it has received a certain amount of data from the PC.
    // Since the receive stream processor won't register as dry running 
    // until data is received, we therefore need to send some data during
    // the dry running state in order to kick the process into gear.
    //
    // Non-empty "empty" packets are not desired during the dry-running
    // state encountered during closedown, however, so take steps to ensure
    // they are only sent during the startup sequence.  This logic is
    // presently a quick and dirty hack and will be revisited in due course
    // once a final control method has been established.
    if (streaming_has_run==0 && isDryRunning()) {
        signed n_events = getNominalFramesPerPacket();
//        unsigned int cycle = CYCLE_TIMER_GET_CYCLES(pkt_ctr);

        streaming_has_dryrun = 1;
//        if (streaming_start_count < (1)*n_events) {
{
            streaming_start_count += n_events;
            *length = n_events * m_event_size;
//            generateSilentPacketData(data, length);
        }
    }

    if (!isDryRunning() && streaming_has_dryrun==1)
        streaming_has_run=1;

//    m_tx_dbc += fillNoDataPacketHeader ( (quadlet_t *)data, length );

    return eCRV_OK;
}

enum StreamProcessor::eChildReturnValue
RmeTransmitStreamProcessor::generateEmptyPacketData (
    unsigned char *data, unsigned int *length)
{
    /* If dry-running data is being sent, zero the data */
    if (*length > 0) {
        memset(data, 0, *length);
    }
    return eCRV_OK; // no need to do anything
}

enum StreamProcessor::eChildReturnValue
RmeTransmitStreamProcessor::generateSilentPacketHeader (
    unsigned char *data, unsigned int *length,
    unsigned char *tag, unsigned char *sy,
    uint32_t pkt_ctr )
{
    unsigned int cycle = CYCLE_TIMER_GET_CYCLES(pkt_ctr);

    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "XMIT SILENT: CY=%04u, TSP=%011llu (%04u)\n",
                 cycle, m_last_timestamp,
                 ( unsigned int ) TICKS_TO_CYCLES ( m_last_timestamp ) );

    // A "silent" packet is identical to a regular data packet except all
    // audio data is set to zero.  The requirements of the silent packet
    // for RME devices is still being confirmed.

    // The number of events per packet expected by the RME is solely
    // dependent on the current sample rate.  An 'event' is one sample from
    // all channels plus possibly other midi and control data.
    signed n_events = getNominalFramesPerPacket();

    // Do housekeeping expected for all packets, even for packets containing
    // no audio data.
    *sy = 0x00;

    /* Assume the packet will be empty unless proven otherwise */
    *length = 0;

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

    transmit_at_time = substractTicks(presentation_time, RME_TRANSMIT_TRANSFER_DELAY);
    presentation_cycle = (unsigned int)(TICKS_TO_CYCLES(presentation_time));
    transmit_at_cycle = (unsigned int)(TICKS_TO_CYCLES(transmit_at_time));
    cycles_until_presentation = diffCycles(presentation_cycle, cycle);
    cycles_until_transmit = diffCycles(transmit_at_cycle, cycle);

    if (cycles_until_transmit < 0)
    {
        if (cycles_until_presentation >= RME_MIN_CYCLES_BEFORE_PRESENTATION)
        {
            m_last_timestamp = presentation_time;
            m_tx_dbc += fillDataPacketHeader((quadlet_t *)data, length, m_last_timestamp);
            if (m_tx_dbc > 0xff)
                m_tx_dbc -= 0x100;
            return eCRV_Packet;
        }
        else
        {
            return eCRV_XRun;
        }
    }
    else if (cycles_until_transmit <= RME_MAX_CYCLES_TO_TRANSMIT_EARLY)
    {
        m_last_timestamp = presentation_time;
        m_tx_dbc += fillDataPacketHeader((quadlet_t *)data, length, m_last_timestamp);
        if (m_tx_dbc > 0xff)
            m_tx_dbc -= 0x100;
        return eCRV_Packet;
    }
    else
    {
        return eCRV_EmptyPacket;
    }
    return eCRV_Invalid;
}

enum StreamProcessor::eChildReturnValue
RmeTransmitStreamProcessor::generateSilentPacketData (
    unsigned char *data, unsigned int *length )
{
    // Simply set all audio data to zero since that's what's meant by
    // a "silent" packet.
    memset(data, 0, *length);

    return eCRV_OK;
}

unsigned int RmeTransmitStreamProcessor::fillDataPacketHeader (
    quadlet_t *data, unsigned int* length,
    uint32_t ts )
{
//    quadlet_t *quadlet = (quadlet_t *)data;
    // Size of a single data frame in quadlets.
//    unsigned dbs = m_event_size / 4;

    // The number of events per packet expected by the RME is solely
    // dependent on the current sample rate.  An 'event' is one sample from
    // all channels plus possibly other midi and control data.
    signed n_events = getNominalFramesPerPacket();
    *length = n_events*m_event_size;

    return n_events;
}

unsigned int RmeTransmitStreamProcessor::fillNoDataPacketHeader (
    quadlet_t *data, unsigned int* length )
{
    *length = 0;
    return 0;
}

bool RmeTransmitStreamProcessor::prepareChild()
{
    debugOutput ( DEBUG_LEVEL_VERBOSE, "Preparing (%p)...\n", this );
    m_max_fs_diff_norm = 10.0;
    m_max_diff_ticks = 30720;

// Unsure whether this helps yet.  Testing continues.
m_dll_bandwidth_hz = 1.0; // 0.1;
    return true;
}

/*
* compose the event streams for the packets from the port buffers
*/
bool RmeTransmitStreamProcessor::processWriteBlock(char *data,
                       unsigned int nevents, unsigned int offset) {
    bool no_problem=true;

    for ( PortVectorIterator it = m_Ports.begin();
      it != m_Ports.end();
      ++it ) {
        // If this port is disabled, unconditionally send it silence.
        if((*it)->isDisabled()) {
          if (encodeSilencePortToRmeEvents(static_cast<RmeAudioPort *>(*it), (quadlet_t *)data, offset, nevents)) {
            debugWarning("Could not encode silence for disabled port %s to Rme events\n",(*it)->getName().c_str());
            // Don't treat this as a fatal error at this point
          }
          continue;
        }

        Port *port=(*it);

        switch(port->getPortType()) {

        case Port::E_Audio:
            if (encodePortToRmeEvents(static_cast<RmeAudioPort *>(*it), (quadlet_t *)data, offset, nevents)) {
                debugWarning("Could not encode port %s to Rme events\n",(*it)->getName().c_str());
                no_problem=false;
            }
            break;
        case Port::E_Midi:
             if (encodePortToRmeMidiEvents(static_cast<RmeMidiPort *>(*it), (quadlet_t *)data, offset, nevents)) {
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
RmeTransmitStreamProcessor::transmitSilenceBlock(char *data,
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
            if (encodeSilencePortToRmeEvents(static_cast<RmeAudioPort *>(*it), (quadlet_t *)data, offset, nevents)) {
                debugWarning("Could not encode port %s to MBLA events\n",(*it)->getName().c_str());
                no_problem = false;
            }
            break;
        case Port::E_Midi:
            if (encodeSilencePortToRmeMidiEvents(static_cast<RmeMidiPort *>(*it), (quadlet_t *)data, offset, nevents)) {
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

int RmeTransmitStreamProcessor::encodePortToRmeEvents(RmeAudioPort *p, quadlet_t *data,
                       unsigned int offset, unsigned int nevents) {
// Encodes nevents worth of data from the given port into the given buffer.  The
// format of the buffer is precisely that which will be sent to the RME.
// The basic idea:
//   iterate over the ports
//     * get port buffer address
//     * loop over events
//         - pick right sample in event based upon PortInfo
//         - convert sample from Port format (E_Int24, E_Float, ..) to RME
//           native format
//
// We include the ability to start the transfer from the given offset within
// the port (expressed in frames) so the 'efficient' transfer method can be
// utilised.

    unsigned int j=0;

    quadlet_t *target;
    target = data + p->getPosition()/4;

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
                    *target = (*buffer & 0x00ffffff) << 8;
                    buffer++;
                    target+=m_event_size/4;
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
#if RME_CLIP_FLOATS
                    if (unlikely(in > 1.0)) in = 1.0;
                    if (unlikely(in < -1.0)) in = -1.0;
#endif
                    unsigned int v = lrintf(in * multiplier);
                    *target = (v << 8);
                    buffer++;
                    target+=m_event_size/4;
                }
            }
            break;
    }

    return 0;
}

int RmeTransmitStreamProcessor::encodeSilencePortToRmeEvents(RmeAudioPort *p, quadlet_t *data,
                       unsigned int offset, unsigned int nevents) {
    unsigned int j=0;
    quadlet_t *target = data + p->getPosition()/4;

    switch (m_StreamProcessorManager.getAudioDataType()) {
    default:
        case StreamProcessorManager::eADT_Int24:
        case StreamProcessorManager::eADT_Float:
        for (j = 0; j < nevents; j++) {
            *target = 0;
            target += m_event_size/4;
        }
        break;
    }

    return 0;
}

int RmeTransmitStreamProcessor::encodePortToRmeMidiEvents(
                       RmeMidiPort *p, quadlet_t *data,
                       unsigned int offset, unsigned int nevents) {

    unsigned int j;
    quadlet_t *src = (quadlet_t *)p->getBufferAddress();
    src += offset;
    unsigned char *target = (unsigned char *)data + p->getPosition();

    // Send a MIDI byte if there is one to send.  RME MIDI data is sent using
    // a 3-byte sequence within a frame starting at the port's position.
    // A non-zero MSB indicates there is MIDI data to send.

    for (j=0; j<nevents; j++, src++, target+=m_event_size) {
        if (midi_lock)
            midi_lock--;

        /* FFADO's MIDI subsystem dictates that at the most there will be one
         * MIDI byte every 8th's sample, making a MIDI byte "unlikely".
         */
        if (unlikely(*src & 0xff000000)) { 
            /* A MIDI byte is ready to send - buffer it */
            midibuffer[mb_head++] = *src;
            mb_head &= MIDIBUFFER_SIZE-1;
            if (unlikely(mb_head == mb_tail)) {
            /* Buffer overflow - dump oldest byte. */
            /* Ideally this would dump an entire MIDI message, but this is only
             * feasible if it's possible to determine the message size easily.
             */
                mb_tail = (mb_tail+1) & (MIDIBUFFER_SIZE-1);
                debugWarning("RME MIDI buffer overflow\n");
            }
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Buffered MIDI byte %d\n", *src & 0xff);
        }

        /* Send the MIDI byte at the tail of the buffer if enough time has elapsed
         * since the last MIDI byte was sent.  For most iterations through the loop
         * this condition will be false.
         */
        if (unlikely(mb_head!=mb_tail && !midi_lock)) {
            *(target) = 0x01;
            *(target+1) = 0x00;
            *(target+2) = midibuffer[mb_tail] & 0xff;
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Sent MIDI byte %d (j=%d)\n", midibuffer[mb_tail], j);
            mb_tail = (mb_tail+1) & (MIDIBUFFER_SIZE-1);
            midi_lock = midi_tx_period;
        }
    }

    return 0;
}

int RmeTransmitStreamProcessor::encodeSilencePortToRmeMidiEvents(
                       RmeMidiPort *p, quadlet_t *data,
                       unsigned int offset, unsigned int nevents) {

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
