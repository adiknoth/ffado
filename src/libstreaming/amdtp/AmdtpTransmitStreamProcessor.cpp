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

#include "config.h"
#include "AmdtpTransmitStreamProcessor.h"
#include "AmdtpPort.h"
#include "../StreamProcessorManager.h"
#include "devicemanager.h"

#include "libutil/Time.h"

#include "libieee1394/ieee1394service.h"
#include "libieee1394/IsoHandlerManager.h"
#include "libieee1394/cycletimer.h"

#include <netinet/in.h>
#include <assert.h>

namespace Streaming
{

/* transmit */
AmdtpTransmitStreamProcessor::AmdtpTransmitStreamProcessor(FFADODevice &parent, int dimension)
        : StreamProcessor(parent, ePT_Transmit)
        , m_dimension( dimension )
        , m_dbc( 0 )
{}

enum StreamProcessor::eChildReturnValue
AmdtpTransmitStreamProcessor::generatePacketHeader (
    unsigned char *data, unsigned int *length,
    unsigned char *tag, unsigned char *sy,
    int cycle, unsigned int dropped, unsigned int max_length )
{
    struct iec61883_packet *packet = ( struct iec61883_packet * ) data;
    /* Our node ID can change after a bus reset, so it is best to fetch
    * our node ID for each packet. */
    packet->sid = m_1394service.getLocalNodeId() & 0x3f;

    packet->dbs = m_dimension;
    packet->fn = 0;
    packet->qpc = 0;
    packet->sph = 0;
    packet->reserved = 0;
    packet->dbc = m_dbc;
    packet->eoh1 = 2;
    packet->fmt = IEC61883_FMT_AMDTP;

    *tag = IEC61883_TAG_WITH_CIP;
    *sy = 0;

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
    // given by AMDTP_TRANSMIT_TRANSFER_DELAY (in ticks), but in case we
    // are too late for that, this constant defines how late we can
    // be.
    const int min_cycles_before_presentation = 1;
    // FIXME: should become a define
    // the absolute maximum number of cycles we want to transmit
    // a packet ahead of the ideal transmit time. The nominal time
    // the packet is transmitted ahead of the presentation time is
    // given by AMDTP_TRANSMIT_TRANSFER_DELAY (in ticks), but we can send
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
    transmit_at_time = substractTicks ( presentation_time, AMDTP_TRANSMIT_TRANSFER_DELAY );

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
    if ( fc < ( signed int ) m_syt_interval )
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
            unsigned int now_cycle = ( unsigned int ) ( TICKS_TO_CYCLES ( m_1394service.getCycleTimerTicks() ) );

            debugOutput ( DEBUG_LEVEL_VERBOSE,
                        "Insufficient frames (NP): N=%02d, CY=%04u, TC=%04u, CUT=%04d, NOW=%04d\n",
                        fc, cycle, transmit_at_cycle, cycles_until_transmit, now_cycle );
            debugWarning("Insufficient frames (NP): N=%02d, CY=%04u, TC=%04u, CUT=%04d, NOW=%04d\n",
                         fc, cycle, transmit_at_cycle, cycles_until_transmit, now_cycle );

            // there is still time left to send the packet
            // we want the system to give this packet another go at a later time instant
            return eCRV_Again; // note that the raw1394 again system doesn't work as expected

            // we could wait here for a certain time before trying again. However, this
            // is not going to work since we then block the iterator thread, hence also 
            // the receiving code, meaning that we are not processing received packets,
            // and hence there is no progression in the number of frames available.

            // for example:
            // SleepRelativeUsec(125); // one cycle
            // goto try_block_of_frames;

            // or more advanced, calculate how many cycles we are ahead of 'now' and
            // base the sleep on that.

            // note that this requires that there is one thread for each IsoHandler,
            // otherwise we're in the deadlock described above.
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
            debugShowBackLogLines(200);
            flushDebugOutput();
            // however, if we can send this sufficiently before the presentation
            // time, it could be harmless.
            // NOTE: dangerous since the device has no way of reporting that it didn't get
            //       this packet on time.
            if(cycles_until_presentation >= min_cycles_before_presentation)
            {
                // we are not that late and can still try to transmit the packet
                m_dbc += fillDataPacketHeader(packet, length, m_last_timestamp);
                return (fc < (signed)(2*m_syt_interval) ? eCRV_Defer : eCRV_Packet);
            }
            else   // definitely too late
            {
                return eCRV_XRun;
            }
        }
        else if(cycles_until_transmit <= max_cycles_to_transmit_early)
        {
            // it's time send the packet
            m_dbc += fillDataPacketHeader(packet, length, m_last_timestamp);
            return (fc < (signed)(2*m_syt_interval) ? eCRV_Defer : eCRV_Packet);
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
AmdtpTransmitStreamProcessor::generatePacketData (
    unsigned char *data, unsigned int *length,
    unsigned char *tag, unsigned char *sy,
    int cycle, unsigned int dropped, unsigned int max_length )
{
    struct iec61883_packet *packet = ( struct iec61883_packet * ) data;
    if ( m_data_buffer->readFrames ( m_syt_interval, ( char * ) ( data + 8 ) ) )
    {
        // process all ports that should be handled on a per-packet base
        // this is MIDI for AMDTP (due to the need of DBC)
        if ( !encodePacketPorts ( ( quadlet_t * ) ( data+8 ), m_syt_interval, packet->dbc ) )
        {
            debugWarning ( "Problem encoding Packet Ports\n" );
        }
        debugOutput ( DEBUG_LEVEL_ULTRA_VERBOSE, "XMIT DATA (cy %04d): TSP=%011llu (%04u)\n",
                    cycle, m_last_timestamp, ( unsigned int ) TICKS_TO_CYCLES ( m_last_timestamp ) );
        return eCRV_OK;
    }
    else return eCRV_XRun;

}

enum StreamProcessor::eChildReturnValue
AmdtpTransmitStreamProcessor::generateSilentPacketHeader (
    unsigned char *data, unsigned int *length,
    unsigned char *tag, unsigned char *sy,
    int cycle, unsigned int dropped, unsigned int max_length )
{
    struct iec61883_packet *packet = ( struct iec61883_packet * ) data;
    debugOutput ( DEBUG_LEVEL_ULTRA_VERBOSE, "XMIT NONE (cy %04d): CY=%04u, TSP=%011llu (%04u)\n",
                cycle, m_last_timestamp, ( unsigned int ) TICKS_TO_CYCLES ( m_last_timestamp ) );

    /* Our node ID can change after a bus reset, so it is best to fetch
    * our node ID for each packet. */
    packet->sid = m_1394service.getLocalNodeId() & 0x3f;

    packet->dbs = m_dimension;
    packet->fn = 0;
    packet->qpc = 0;
    packet->sph = 0;
    packet->reserved = 0;
    packet->dbc = m_dbc;
    packet->eoh1 = 2;
    packet->fmt = IEC61883_FMT_AMDTP;

    *tag = IEC61883_TAG_WITH_CIP;
    *sy = 0;

    m_dbc += fillNoDataPacketHeader ( packet, length );
    return eCRV_OK;
}

enum StreamProcessor::eChildReturnValue
AmdtpTransmitStreamProcessor::generateSilentPacketData (
    unsigned char *data, unsigned int *length,
    unsigned char *tag, unsigned char *sy,
    int cycle, unsigned int dropped, unsigned int max_length )
{
    return eCRV_OK; // no need to do anything
}

unsigned int AmdtpTransmitStreamProcessor::fillDataPacketHeader (
    struct iec61883_packet *packet, unsigned int* length,
    uint32_t ts )
{

    packet->fdf = m_fdf;

    // convert the timestamp to SYT format
    uint16_t timestamp_SYT = TICKS_TO_SYT ( ts );
    packet->syt = ntohs ( timestamp_SYT );

    *length = m_syt_interval*sizeof ( quadlet_t ) *m_dimension + 8;

    return m_syt_interval;
}

unsigned int AmdtpTransmitStreamProcessor::fillNoDataPacketHeader (
    struct iec61883_packet *packet, unsigned int* length )
{

    // no-data packets have syt=0xFFFF
    // and have the usual amount of events as dummy data (?)
    packet->fdf = IEC61883_FDF_NODATA;
    packet->syt = 0xffff;

    // FIXME: either make this a setting or choose
    bool send_payload=true;
    if ( send_payload )
    {
        // this means no-data packets with payload (DICE doesn't like that)
        *length = 2*sizeof ( quadlet_t ) + m_syt_interval * m_dimension * sizeof ( quadlet_t );
        return m_syt_interval;
    }
    else
    {
        // dbc is not incremented
        // this means no-data packets without payload
        *length = 2*sizeof ( quadlet_t );
        return 0;
    }
}

unsigned int
AmdtpTransmitStreamProcessor::getSytInterval() {
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
unsigned int
AmdtpTransmitStreamProcessor::getFDF() {
    switch (m_StreamProcessorManager.getNominalRate()) {
        case 32000: return IEC61883_FDF_SFC_32KHZ;
        case 44100: return IEC61883_FDF_SFC_44K1HZ;
        case 48000: return IEC61883_FDF_SFC_48KHZ;
        case 88200: return IEC61883_FDF_SFC_88K2HZ;
        case 96000: return IEC61883_FDF_SFC_96KHZ;
        case 176400: return IEC61883_FDF_SFC_176K4HZ;
        case 192000: return IEC61883_FDF_SFC_192KHZ;
        default:
            debugError("Unsupported rate: %d\n", m_StreamProcessorManager.getNominalRate());
            return 0;
    }
}

bool AmdtpTransmitStreamProcessor::prepareChild()
{
    debugOutput ( DEBUG_LEVEL_VERBOSE, "Preparing (%p)...\n", this );
    m_syt_interval = getSytInterval();
    m_fdf = getFDF();

    iec61883_cip_init (
        &m_cip_status,
        IEC61883_FMT_AMDTP,
        m_fdf,
        m_StreamProcessorManager.getNominalRate(),
        m_dimension,
        m_syt_interval );

    for ( PortVectorIterator it = m_Ports.begin();
            it != m_Ports.end();
            ++it )
    {
        if ( ( *it )->getPortType() == Port::E_Midi )
        {
            // we use a timing unit of 10ns
            // this makes sure that for the max syt interval
            // we don't have rounding, and keeps the numbers low
            // we have 1 slot every 8 events
            // we have syt_interval events per packet
            // => syt_interval/8 slots per packet
            // packet rate is 8000pkt/sec => interval=125us
            // so the slot interval is (1/8000)/(syt_interval/8)
            // or: 1/(1000 * syt_interval) sec
            // which is 1e9/(1000*syt_interval) nsec
            // or 100000/syt_interval 'units'
            // the event interval is fixed to 320us = 32000 'units'
            if ( ! ( *it )->useRateControl ( true, ( 100000/m_syt_interval ),32000, false ) )
            {
                debugFatal ( "Could not set signal type to PeriodSignalling" );
                return false;
            }
            break;
        }
    }
    return true;
}

/*
* compose the event streams for the packets from the port buffers
*/
bool AmdtpTransmitStreamProcessor::processWriteBlock ( char *data,
        unsigned int nevents, unsigned int offset )
{
    bool no_problem = true;

    for ( PortVectorIterator it = m_PeriodPorts.begin();
          it != m_PeriodPorts.end();
          ++it )
    {
        if ( (*it)->isDisabled() ) { continue; };

        //FIXME: make this into a static_cast when not DEBUG?
        AmdtpPortInfo *pinfo = dynamic_cast<AmdtpPortInfo *> ( *it );
        assert ( pinfo ); // this should not fail!!

        switch( pinfo->getFormat() )
        {
            case AmdtpPortInfo::E_MBLA:
                if( encodePortToMBLAEvents(static_cast<AmdtpAudioPort *>(*it), (quadlet_t *)data, offset, nevents) )
                {
                    debugWarning ( "Could not encode port %s to MBLA events", (*it)->getName().c_str() );
                    no_problem = false;
                }
                break;
            case AmdtpPortInfo::E_SPDIF: // still unimplemented
                break;
            default: // ignore
                break;
        }
    }
    return no_problem;
}

bool
AmdtpTransmitStreamProcessor::transmitSilenceBlock(
    char *data, unsigned int nevents, unsigned int offset)
{
    bool no_problem = true;
    for(PortVectorIterator it = m_PeriodPorts.begin();
        it != m_PeriodPorts.end();
        ++it )
    {
        //FIXME: make this into a static_cast when not DEBUG?
        AmdtpPortInfo *pinfo=dynamic_cast<AmdtpPortInfo *>(*it);
        assert(pinfo); // this should not fail!!

        switch( pinfo->getFormat() )
        {
            case AmdtpPortInfo::E_MBLA:
                if ( encodeSilencePortToMBLAEvents(static_cast<AmdtpAudioPort *>(*it), (quadlet_t *)data, offset, nevents) )
                {
                    debugWarning("Could not encode port %s to MBLA events", (*it)->getName().c_str());
                    no_problem = false;
                }
                break;
            case AmdtpPortInfo::E_SPDIF: // still unimplemented
                break;
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
bool AmdtpTransmitStreamProcessor::encodePacketPorts ( quadlet_t *data, unsigned int nevents, unsigned int dbc )
{
    bool ok=true;
    quadlet_t byte;

    quadlet_t *target_event=NULL;
    unsigned int j;

    for ( PortVectorIterator it = m_PacketPorts.begin();
            it != m_PacketPorts.end();
            ++it )
    {

#ifdef DEBUG
        AmdtpPortInfo *pinfo=dynamic_cast<AmdtpPortInfo *> ( *it );
        assert ( pinfo ); // this should not fail!!

        // the only packet type of events for AMDTP is MIDI in mbla
        assert ( pinfo->getFormat() ==AmdtpPortInfo::E_Midi );
#endif

        AmdtpMidiPort *mp=static_cast<AmdtpMidiPort *> ( *it );

        // we encode this directly (no function call) due to the high frequency
        /* idea:
        spec says: current_midi_port=(dbc+j)%8;
        => if we start at (dbc+stream->location-1)%8,
        we'll start at the right event for the midi port.
        => if we increment j with 8, we stay at the right event.
        */
        // FIXME: as we know in advance how big a packet is (syt_interval) we can
        //        predict how much loops will be present here
        // first prefill the buffer with NO_DATA's on all time muxed channels

        for ( j = ( dbc & 0x07 ) +mp->getLocation(); j < nevents; j += 8 )
        {

            quadlet_t tmpval;

            target_event= ( quadlet_t * ) ( data + ( ( j * m_dimension ) + mp->getPosition() ) );

            if ( mp->canRead() )   // we can send a byte
            {
                mp->readEvent ( &byte );
                byte &= 0xFF;
                tmpval=htonl (
                        IEC61883_AM824_SET_LABEL ( ( byte ) <<16,
                                                    IEC61883_AM824_LABEL_MIDI_1X ) );

                debugOutput ( DEBUG_LEVEL_ULTRA_VERBOSE, "MIDI port %s, pos=%d, loc=%d, dbc=%d, nevents=%d, dim=%d\n",
                            mp->getName().c_str(), mp->getPosition(), mp->getLocation(), dbc, nevents, m_dimension );
                debugOutput ( DEBUG_LEVEL_ULTRA_VERBOSE, "base=%p, target=%p, value=%08X\n",
                            data, target_event, tmpval );

            }
            else
            {
                // can't send a byte, either because there is no byte,
                // or because this would exceed the maximum rate
                tmpval=htonl (
                        IEC61883_AM824_SET_LABEL ( 0,IEC61883_AM824_LABEL_MIDI_NO_DATA ) );
            }

            *target_event=tmpval;
        }

    }
    return ok;
}

#if USE_SSE
typedef float v4sf __attribute__ ((vector_size (16)));
typedef int v4si __attribute__ ((vector_size (16)));
typedef int v2si __attribute__ ((vector_size (8)));

int AmdtpTransmitStreamProcessor::encodePortToMBLAEvents ( AmdtpAudioPort *p, quadlet_t *data,
        unsigned int offset, unsigned int nevents )
{
    static const float sse_multiplier[4] __attribute__((aligned(16))) = {
        (float)(0x7FFFFF00),
        (float)(0x7FFFFF00),
        (float)(0x7FFFFF00),
        (float)(0x7FFFFF00)
    };

    static const int sse_mask[4] __attribute__((aligned(16))) = {
        0x40000000,  0x40000000,  0x40000000,  0x40000000
    };

    unsigned int out[4];

    unsigned int j=0;
    unsigned int read=0;

    quadlet_t *target_event;

    target_event= ( quadlet_t * ) ( data + p->getPosition() );

    switch ( p->getDataType() )
    {
        default:
        case Port::E_Int24:
        {
            quadlet_t *buffer= ( quadlet_t * ) ( p->getBufferAddress() );

            assert ( nevents + offset <= p->getBufferSize() );

            buffer+=offset;

            for ( j = 0; j < nevents; j += 1 )   // decode max nsamples
            {
                *target_event = htonl ( ( * ( buffer ) & 0x00FFFFFF ) | 0x40000000 );
                buffer++;
                target_event += m_dimension;
            }
        }
        break;
        case Port::E_Float:
        {
            const float multiplier = ( float ) ( 0x7FFFFF00 );
            float *buffer= ( float * ) ( p->getBufferAddress() );

            assert ( nevents + offset <= p->getBufferSize() );

            buffer+=offset;

            j=0;
            if(read>3) {
                for (j = 0; j < read-3; j += 4) {
                    asm("movups %[floatbuff], %%xmm0\n\t"
                            "mulps %[ssemult], %%xmm0\n\t"
                            "cvttps2pi %%xmm0, %[out1]\n\t"
                            "movhlps %%xmm0, %%xmm0\n\t"
                            "psrld $8, %[out1]\n\t"
                            "cvttps2pi %%xmm0, %[out2]\n\t"
                            "por %[mmxmask], %[out1]\n\t"
                            "psrld $8, %[out2]\n\t"
                            "por %[mmxmask], %[out2]\n\t"
                        : [out1] "=&y" (*(v2si*)&out[0]),
                    [out2] "=&y" (*(v2si*)&out[2])
                        : [floatbuff] "m" (*(v4sf*)buffer),
                    [ssemult] "x" (*(v4sf*)sse_multiplier),
                    [mmxmask] "y" (*(v2si*)sse_mask)
                        : "xmm0");
                    buffer += 4;
                    *target_event = htonl(out[0]);
                    target_event += m_dimension;
                    *target_event = htonl(out[1]);
                    target_event += m_dimension;
                    *target_event = htonl(out[2]);
                    target_event += m_dimension;
                    *target_event = htonl(out[3]);
                    target_event += m_dimension;
                }
            }
            for(; j < read; ++j) {
            // don't care for overflow
                float v = *buffer * multiplier;  // v: -231 .. 231
                unsigned int tmp = (int)v;
                *target_event = htonl((tmp >> 8) | 0x40000000);
    
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

#else

int AmdtpTransmitStreamProcessor::encodePortToMBLAEvents ( AmdtpAudioPort *p, quadlet_t *data,
        unsigned int offset, unsigned int nevents )
{
    unsigned int j=0;

    quadlet_t *target_event;

    target_event= ( quadlet_t * ) ( data + p->getPosition() );

    switch ( p->getDataType() )
    {
        default:
        case Port::E_Int24:
        {
            quadlet_t *buffer= ( quadlet_t * ) ( p->getBufferAddress() );

            assert ( nevents + offset <= p->getBufferSize() );

            buffer+=offset;

            for ( j = 0; j < nevents; j += 1 )   // decode max nsamples
            {
                *target_event = htonl ( ( * ( buffer ) & 0x00FFFFFF ) | 0x40000000 );
                buffer++;
                target_event += m_dimension;
            }
        }
        break;
        case Port::E_Float:
        {
            const float multiplier = ( float ) ( 0x7FFFFF00 );
            float *buffer= ( float * ) ( p->getBufferAddress() );

            assert ( nevents + offset <= p->getBufferSize() );

            buffer+=offset;

            for ( j = 0; j < nevents; j += 1 )   // decode max nsamples
            {

                // don't care for overflow
                float v = *buffer * multiplier;  // v: -231 .. 231
                unsigned int tmp = ( ( int ) v );
                *target_event = htonl ( ( tmp >> 8 ) | 0x40000000 );

                buffer++;
                target_event += m_dimension;
            }
        }
        break;
    }

    return 0;
}
#endif

int AmdtpTransmitStreamProcessor::encodeSilencePortToMBLAEvents ( AmdtpAudioPort *p, quadlet_t *data,
        unsigned int offset, unsigned int nevents )
{
    unsigned int j=0;

    quadlet_t *target_event;

    target_event= ( quadlet_t * ) ( data + p->getPosition() );

    switch ( p->getDataType() )
    {
        default:
        case Port::E_Int24:
        case Port::E_Float:
        {
            for ( j = 0; j < nevents; j += 1 )   // decode max nsamples
            {
                *target_event = htonl ( 0x40000000 );
                target_event += m_dimension;
            }
        }
        break;
    }

    return 0;
}

} // end of namespace Streaming
