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


#include "AmdtpOxfordReceiveStreamProcessor.h"
#include "../StreamProcessorManager.h"
#include "devicemanager.h"

#include "libieee1394/cycletimer.h"

#define DLL_PI        (3.141592653589793238)
#define DLL_SQRT2     (1.414213562373095049)
#define DLL_2PI       (2.0 * DLL_PI)

// DLL bandwidth in Hz
#define DLL_BANDWIDTH_HZ  1.0

namespace Streaming {

/* --------------------- RECEIVE ----------------------- */

/* The issues with the FCA202 (and possibly other oxford FW92x devices) are:
 * - they transmit in non-blocking mode
 * - the timestamps are not correct
 *
 * This requires some workarounds.
 *
 * The approach is to 'convert' non-blocking into blocking so
 * that we can use the existing streaming system.
 *
 * The streamprocessor has a "pseudo-packet" buffer that will collect
 * all frames present. Once one SYT_INTERVAL of frames is present, we indicate
 * that a packet has been received.
 *
 * To overcome the timestamping issue we use the time-of-arrival as a timestamp.
 *
 */

AmdtpOxfordReceiveStreamProcessor::AmdtpOxfordReceiveStreamProcessor(FFADODevice &parent, int dimension)
    : AmdtpReceiveStreamProcessor(parent, dimension)
    , m_next_packet_timestamp ( 0xFFFFFFFF )
    , m_temp_buffer( NULL )
    , m_packet_size_bytes( 0 )
    , m_payload_buffer( NULL )
    , m_dll_e2 ( 0 )
    , m_dll_b ( 0 )
    , m_dll_c ( 0 )
    , m_expected_time_of_receive ( 0xFFFFFFFF )
    , m_nominal_ticks_per_frame( 0 )
{}

AmdtpOxfordReceiveStreamProcessor::~AmdtpOxfordReceiveStreamProcessor()
{
    if(m_temp_buffer) ffado_ringbuffer_free(m_temp_buffer);
    if(m_payload_buffer) free(m_payload_buffer);
}

bool
AmdtpOxfordReceiveStreamProcessor::prepareChild()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing (%p)...\n", this);

    int packet_payload_size_events = m_dimension * getSytInterval();

    // allocate space for four packets payload. The issue is that it can be
    // that we receive a bit too much payload such that the total is more
    // than one packet
    FFADO_ASSERT( m_temp_buffer == NULL );
    if( !(m_temp_buffer = ffado_ringbuffer_create(
            packet_payload_size_events * 4 * 4))) {
        debugFatal("Could not allocate memory event ringbuffer\n");
        return false;
    }

    m_next_packet_timestamp = 0xFFFFFFFF;

    m_packet_size_bytes = getSytInterval() * m_dimension * sizeof(quadlet_t);
    m_payload_buffer = (char *)malloc(m_packet_size_bytes);

    if(m_payload_buffer == NULL) {
        debugFatal("could not allocate memory for payload buffer\n");
        return false;
    }

    // init the DLL
    unsigned int nominal_frames_per_second = m_StreamProcessorManager.getNominalRate();
    m_nominal_ticks_per_frame = (double)TICKS_PER_SECOND / (double)nominal_frames_per_second;
    m_dll_e2 = m_nominal_ticks_per_frame * (double)getSytInterval();

    double tupdate = m_nominal_ticks_per_frame * (double)getSytInterval();
    double bw_rel = DLL_BANDWIDTH_HZ / (double)TICKS_PER_SECOND * tupdate;
    if(bw_rel >= 0.5) {
        debugError("Requested bandwidth out of range: %f > %f\n", DLL_BANDWIDTH_HZ / (double)TICKS_PER_SECOND, 0.5 / tupdate);
        return false;
    }
    m_dll_b = bw_rel * (DLL_SQRT2 * DLL_2PI);
    m_dll_c = bw_rel * bw_rel * DLL_2PI * DLL_2PI;

    return AmdtpReceiveStreamProcessor::prepareChild();
}

/**
 * Processes packet header to extract timestamps and so on
 * 
 * this will be abused to copy the payload into the temporary buffer
 * once the buffer is full, we trigger a data-read operation
 * 
 * @param data 
 * @param length 
 * @param channel 
 * @param tag 
 * @param sy 
 * @param pkt_ctr CTR value when packet was received
 * @return 
 */
enum StreamProcessor::eChildReturnValue
AmdtpOxfordReceiveStreamProcessor::processPacketHeader(unsigned char *data, unsigned int length,
                                                       unsigned char tag, unsigned char sy,
                                                       uint32_t pkt_ctr)
{
    struct iec61883_packet *packet = (struct iec61883_packet *) data;
    FFADO_ASSERT(packet);
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "Packet at %03lu %04lu %04lu\n",
                                        CYCLE_TIMER_GET_SECS(pkt_ctr), CYCLE_TIMER_GET_CYCLES(pkt_ctr), CYCLE_TIMER_GET_OFFSET(pkt_ctr));

    bool ok = (packet->fdf != 0xFF) &&
              (packet->fmt == 0x10) &&
              (packet->dbs > 0) &&
              (length >= 2*sizeof(quadlet_t));
    if(ok) { // there is payload
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "Packet with payload\n");
        unsigned int frames_in_tempbuffer = ffado_ringbuffer_read_space(m_temp_buffer) / sizeof (quadlet_t) / m_dimension;

        // if the next packet tsp isn't present, generate one now
        if (m_next_packet_timestamp == 0xFFFFFFFF) {
            uint64_t toa = CYCLE_TIMER_TO_TICKS(pkt_ctr);

            // add some extra time, to ensure causality
            // in a normal system, the SYT's are max 2 cycles apart
            toa = addTicks(toa, 2*TICKS_PER_CYCLE);

            // correct for the frames already in the buffer
            toa = substractTicks(toa, (uint64_t)(m_nominal_ticks_per_frame * frames_in_tempbuffer));
            
            // toa now contains the CTR in ticks of the first frame in the buffer
            // if we calculate it from the time-of-arrival of the current packet
            
            // init if required
            if (m_expected_time_of_receive >= 0xFFFFFFFE) {
                m_expected_time_of_receive = substractTicks(toa, (uint64_t)m_dll_e2);
            }

            // time-of-arrival as timestamp is very jittery, filter this a bit
            double err = diffTicks(toa, m_expected_time_of_receive);
            
            // if err is too large, reinitialize as this is most likely a discontinuity in
            // the streams (e.g. packet lost, xrun)
            if (err > m_dll_e2 * 2.0 || err < -m_dll_e2 * 2.0 ) {
                err = 0.0;
                m_expected_time_of_receive = toa;
            }

            m_next_packet_timestamp = m_expected_time_of_receive;
            double corr = (m_dll_b * err + m_dll_e2);
            if (corr > 0) {
                m_expected_time_of_receive = addTicks(m_expected_time_of_receive, (uint64_t)corr);
            } else {
                m_expected_time_of_receive = substractTicks(m_expected_time_of_receive, (uint64_t)(-corr));
            }
            m_dll_e2 += m_dll_c * err;

            debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
                        "Generated TSP: %16"PRIu64" %"PRId64" %d %d\n", 
                        m_next_packet_timestamp,
                        m_next_packet_timestamp-m_last_timestamp,
                        (int)frames_in_tempbuffer,
                        (int)(((length / sizeof (quadlet_t)) - 2) / m_dimension));
        }

        // add the payload to the temporary ringbuffer
        quadlet_t *payload_start = (quadlet_t *)(data + 8);
        FFADO_ASSERT(m_dimension == packet->dbs);
        unsigned int nevents = ((length / sizeof (quadlet_t)) - 2) / m_dimension;
        unsigned int write_size = nevents * sizeof(quadlet_t) * m_dimension;
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "Payload: %u events, going to write %u bytes\n", nevents, write_size);
        unsigned int written = 0;
        if ((written = ffado_ringbuffer_write(m_temp_buffer, (char *)payload_start, write_size)) < write_size)
        {
            debugFatal("Temporary ringbuffer full (wrote %u bytes of %u)\n", written, write_size);
//             return eCRV_Error;
        }

        // now figure out if we have sufficient frames in the tempbuffer to construct a packet
        unsigned int quadlets_in_tempbuffer = frames_in_tempbuffer * sizeof (quadlet_t);
        if (quadlets_in_tempbuffer >= m_syt_interval * m_dimension) { // yes
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "Sufficient frames in buffer: %u (need %u)\n", quadlets_in_tempbuffer/m_dimension, m_syt_interval);
            m_last_timestamp = m_next_packet_timestamp;
            m_next_packet_timestamp = 0xFFFFFFFF; // next cycle should generate a new timestamp

            // read the 'packet' into the packet payload buffer
            ffado_ringbuffer_read(m_temp_buffer, m_payload_buffer, m_packet_size_bytes);

            // signal a packet reception
            return eCRV_OK;
        } else { // no
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "Insufficient frames in buffer: %u (need %u)\n", quadlets_in_tempbuffer/m_dimension, m_syt_interval);
            return eCRV_Invalid;
        }

    } else {
        return eCRV_Invalid;
    }
}

/**
 * extract the data from the packet
 * 
 * instead of using the data from the packet, we use the data from our 'internal' packet
 * 
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
AmdtpOxfordReceiveStreamProcessor::processPacketData(unsigned char *data, unsigned int length) {
    struct iec61883_packet *packet = (struct iec61883_packet *) data;
    assert(packet);

    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "Processing data\n");

    // put whatever is in the payload buffer at the moment into the timestamped buffer
    if(m_data_buffer->writeFrames(m_syt_interval, m_payload_buffer, m_last_timestamp)) {
        return eCRV_OK;
    } else {
        return eCRV_XRun;
    }
}

} // end of namespace Streaming
