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

#include "StreamProcessor.h"
#include "../util/cycletimer.h"
#include "../StreamProcessorManager.h"

#include "libutil/Atomic.h"

#include <assert.h>
#include <math.h>

namespace Streaming {

IMPL_DEBUG_MODULE( StreamProcessor, StreamProcessor, DEBUG_LEVEL_VERBOSE );

StreamProcessor::StreamProcessor(enum eProcessorType type, int port)
    : IsoStream((type==ePT_Receive ? IsoStream::eST_Receive : IsoStream::eST_Transmit), port)
    , m_processor_type ( type )
    , m_state( ePS_Created )
    , m_next_state( ePS_Invalid )
    , m_cycle_to_switch_state( 0 )
    , m_scratch_buffer( NULL )
    , m_scratch_buffer_size_bytes( 0 )
    , m_manager( NULL )
    , m_ticks_per_frame( 0 )
    , m_last_cycle( -1 )
    , m_sync_delay( 0 )
    , m_in_xrun( false )
    , m_last_timestamp(0)
    , m_last_timestamp2(0)
    , m_dropped(0)
{
    // create the timestamped buffer and register ourselves as its client
    m_data_buffer = new Util::TimestampedBuffer(this);
}

StreamProcessor::~StreamProcessor() {
    if (m_data_buffer) delete m_data_buffer;
    if (m_scratch_buffer) delete[] m_scratch_buffer;
}

uint64_t StreamProcessor::getTimeNow() {
    return m_handler->getCycleTimerTicks();
}

int StreamProcessor::getMaxFrameLatency() {
    if (getType() == ePT_Receive) {
        return (int)(m_handler->getWakeupInterval() * TICKS_PER_CYCLE);
    } else {
        return (int)(m_handler->getWakeupInterval() * TICKS_PER_CYCLE);
    }
}

unsigned int
StreamProcessor::getNominalPacketsNeeded(unsigned int nframes)
{
    unsigned int nominal_frames_per_second = m_manager->getNominalRate();
    uint64_t nominal_ticks_per_frame = TICKS_PER_SECOND / nominal_frames_per_second;
    uint64_t nominal_ticks = nominal_ticks_per_frame * nframes;
    uint64_t nominal_packets = nominal_ticks / TICKS_PER_CYCLE;
    return nominal_packets;
}

/***********************************************
 * Buffer management and manipulation          *
 ***********************************************/
int StreamProcessor::getBufferFill() {
    return m_data_buffer->getBufferFill();
}

int64_t
StreamProcessor::getTimeUntilNextPeriodSignalUsecs()
{
    uint64_t time_at_period=getTimeAtPeriod();

    // we delay the period signal with the sync delay
    // this makes that the period signals lag a little compared to reality
    // ISO buffering causes the packets to be received at max
    // m_handler->getWakeupInterval() later than the time they were received.
    // hence their payload is available this amount of time later. However, the
    // period boundary is predicted based upon earlier samples, and therefore can
    // pass before these packets are processed. Adding this extra term makes that
    // the period boundary is signalled later
    time_at_period = addTicks(time_at_period, m_manager->getSyncSource().getSyncDelay());

    uint64_t cycle_timer=m_handler->getCycleTimerTicks();

    // calculate the time until the next period
    int32_t until_next=diffTicks(time_at_period,cycle_timer);

    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "=> TAP=%11llu, CTR=%11llu, UTN=%11ld\n",
        time_at_period, cycle_timer, until_next
        );

    // now convert to usecs
    // don't use the mapping function because it only works
    // for absolute times, not the relative time we are
    // using here (which can also be negative).
    return (int64_t)(((float)until_next) / TICKS_PER_USEC);
}

void
StreamProcessor::setSyncDelay(int d) {
    debugOutput(DEBUG_LEVEL_VERBOSE, "Setting SP %p SyncDelay to %d ticks\n", this, d);
    m_sync_delay = d;
}

uint64_t
StreamProcessor::getTimeAtPeriodUsecs()
{
    return (uint64_t)((float)getTimeAtPeriod() * TICKS_PER_USEC);
}

uint64_t
StreamProcessor::getTimeAtPeriod() 
{
    if (getType() == ePT_Receive) {
        ffado_timestamp_t next_period_boundary=m_data_buffer->getTimestampFromHead(m_manager->getPeriodSize());
    
        #ifdef DEBUG
        ffado_timestamp_t ts;
        signed int fc;
        m_data_buffer->getBufferTailTimestamp(&ts,&fc);
    
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "=> NPD="TIMESTAMP_FORMAT_SPEC", LTS="TIMESTAMP_FORMAT_SPEC", FC=%5u, TPF=%f\n",
            next_period_boundary, ts, fc, getTicksPerFrame()
            );
        #endif
        return (uint64_t)next_period_boundary;
    } else {
        ffado_timestamp_t next_period_boundary=m_data_buffer->getTimestampFromTail((m_manager->getNbBuffers()-1) * m_manager->getPeriodSize());
    
        #ifdef DEBUG
        ffado_timestamp_t ts;
        signed int fc;
        m_data_buffer->getBufferTailTimestamp(&ts,&fc);
    
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "=> NPD="TIMESTAMP_FORMAT_SPEC", LTS="TIMESTAMP_FORMAT_SPEC", FC=%5u, TPF=%f\n",
            next_period_boundary, ts, fc, getTicksPerFrame()
            );
        #endif
        return (uint64_t)next_period_boundary;
    }
}

float
StreamProcessor::getTicksPerFrame()
{
    assert(m_data_buffer != NULL);
    return m_data_buffer->getRate();
}

bool
StreamProcessor::canClientTransferFrames(unsigned int nbframes)
{
    bool can_transfer;
    unsigned int fc = m_data_buffer->getFrameCounter();
    if (getType() == ePT_Receive) {
        can_transfer = (fc >= nbframes);
    } else {
        // there has to be enough space to put the frames in
        can_transfer = m_data_buffer->getBufferSize() - fc > nbframes;
        // or the buffer is transparent
        can_transfer |= m_data_buffer->isTransparent();
    }
    
    #ifdef DEBUG
    if (!can_transfer) {
        debugWarning("(%p, %s) cannot transfer since fc == %u, nbframes == %u\n", 
            this, ePTToString(getType()), fc, nbframes);
    }
    #endif
    
    return can_transfer;
}

/***********************************************
 * I/O API                                     *
 ***********************************************/

// Packet transfer API
enum raw1394_iso_disposition
StreamProcessor::putPacket(unsigned char *data, unsigned int length,
                           unsigned char channel, unsigned char tag, unsigned char sy,
                           unsigned int cycle, unsigned int dropped) {
    if(m_last_cycle == -1) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Handler for %s SP %p is alive (cycle = %u)\n", getTypeString(), this, cycle);
    }

    int dropped_cycles = 0;
    if (m_last_cycle != (int)cycle && m_last_cycle != -1) {
        dropped_cycles = diffCycles(cycle, m_last_cycle) - 1;
        if (dropped_cycles < 0) debugWarning("(%p) dropped < 1 (%d)\n", this, dropped_cycles);
        if (dropped_cycles > 0) {
            debugWarning("(%p) dropped %d packets on cycle %u\n", this, dropped_cycles, cycle);
            m_dropped += dropped_cycles;
        }
    }
    m_last_cycle = cycle;

    // bypass based upon state
    if (m_state == ePS_Invalid) {
        debugError("Should not have state %s\n", ePSToString(m_state) );
        return RAW1394_ISO_ERROR;
    }
    if (m_state == ePS_Created) {
        return RAW1394_ISO_DEFER;
    }

    // store the previous timestamp
    m_last_timestamp2 = m_last_timestamp;

    // NOTE: synchronized switching is restricted to a 0.5 sec span (4000 cycles)
    //       it happens on the first 'good' cycle for the wait condition
    //       or on the first received cycle that is received afterwards (might be a problem)

    // check whether we are waiting for a stream to be disabled
    if(m_state == ePS_WaitingForStreamDisable) {
        // we then check whether we have to switch on this cycle
        if (diffCycles(cycle, m_cycle_to_switch_state) >= 0) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Should update state to DryRunning\n");
            m_next_state = ePS_DryRunning;
            if (!updateState()) { // we are allowed to change the state directly
                debugError("Could not update state!\n");
                return RAW1394_ISO_ERROR;
            }
        } else {
            // not time to disable yet
        }
        // the received data can be discarded while waiting for the stream
        // to be disabled
        return RAW1394_ISO_OK;
    }

    // check whether we are waiting for a stream to be enabled
    else if(m_state == ePS_WaitingForStreamEnable) {
        // we then check whether we have to switch on this cycle
        if (diffCycles(cycle, m_cycle_to_switch_state) >= 0) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Should update state to Running\n");
            m_next_state = ePS_Running;
            if (!updateState()) { // we are allowed to change the state directly
                debugError("Could not update state!\n");
                return RAW1394_ISO_ERROR;
            }
        } else {
            // not time to enable yet
        }
        // we are dryRunning hence data should be processed in any case
    }

    // check the packet header
    enum eChildReturnValue result = processPacketHeader(data, length, channel, tag, sy, cycle, dropped_cycles);
    if (result == eCRV_OK) {
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "RECV: CY=%04u TS=%011llu\n",
                cycle, m_last_timestamp);
        // update some accounting
        m_last_good_cycle = cycle;
        m_last_dropped = dropped_cycles;

        // check whether we are waiting for a stream to startup
        // this requires that the packet is good
        if(m_state == ePS_WaitingForStream) {
            // since we have a packet with an OK header,
            // we can indicate that the stream started up

            // we then check whether we have to switch on this cycle
            if (diffCycles(cycle, m_cycle_to_switch_state) >= 0) {
                debugOutput(DEBUG_LEVEL_VERBOSE, "Should update state to DryRunning due to good packet\n");
                // hence go to the dryRunning state
                m_next_state = ePS_DryRunning;
                if (!updateState()) { // we are allowed to change the state directly
                    debugError("Could not update state!\n");
                    return RAW1394_ISO_ERROR;
                }
            } else {
                // not time (yet) to switch state
            }
            // in both cases we don't want to process the data
            return RAW1394_ISO_OK;
        }

        // check whether a state change has been requested
        // note that only the wait state changes are synchronized with the cycles
        else if(m_state != m_next_state) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Should update state from %s to %s\n",
                                             ePSToString(m_state), ePSToString(m_next_state));
            // execute the requested change
            if (!updateState()) { // we are allowed to change the state directly
                debugError("Could not update state!\n");
                return RAW1394_ISO_ERROR;
            }
        }

        // handle dropped cycles
        if(dropped_cycles) {
            // they represent a discontinuity in the timestamps, and hence are
            // to be dealt with
            debugWarning("(%p) Correcting timestamp for dropped cycles, discarding packet...\n", this);
            m_data_buffer->setBufferTailTimestamp(m_last_timestamp);
            if (m_state == ePS_Running) {
                // this is an xrun situation
                m_in_xrun = true;
                debugOutput(DEBUG_LEVEL_VERBOSE, "Should update state to WaitingForStreamDisable due to dropped packet xrun\n");
                m_cycle_to_switch_state = cycle + 1; // switch in the next cycle
                m_next_state = ePS_WaitingForStreamDisable;
                // execute the requested change
                if (!updateState()) { // we are allowed to change the state directly
                    debugError("Could not update state!\n");
                    return RAW1394_ISO_ERROR;
                }
                return RAW1394_ISO_DEFER;
            }
        }

        // for all states that reach this we are allowed to
        // do protocol specific data reception
        enum eChildReturnValue result2 = processPacketData(data, length, channel, tag, sy, cycle, dropped_cycles);

        // if an xrun occured, switch to the dryRunning state and
        // allow for the xrun to be picked up
        if (result2 == eCRV_XRun) {
            m_in_xrun = true;
            debugOutput(DEBUG_LEVEL_VERBOSE, "Should update state to WaitingForStreamDisable due to data xrun\n");
            m_cycle_to_switch_state = cycle+1; // switch in the next cycle
            m_next_state = ePS_WaitingForStreamDisable;
            // execute the requested change
            if (!updateState()) { // we are allowed to change the state directly
                debugError("Could not update state!\n");
                return RAW1394_ISO_ERROR;
            }
            return RAW1394_ISO_DEFER;
        } else if(result2 == eCRV_OK) {
            // no problem here
            return RAW1394_ISO_OK;
        } else {
            debugError("Invalid response\n");
            return RAW1394_ISO_ERROR;
        }
    } else if(result == eCRV_Invalid) {
        // apparently we don't have to do anything when the packets are not valid
        return RAW1394_ISO_OK;
    } else {
        debugError("Invalid response\n");
        return RAW1394_ISO_ERROR;
    }
    debugError("reached the unreachable\n");
    return RAW1394_ISO_ERROR;
}

enum raw1394_iso_disposition
StreamProcessor::getPacket(unsigned char *data, unsigned int *length,
                           unsigned char *tag, unsigned char *sy,
                           int cycle, unsigned int dropped, unsigned int max_length) {
    if (cycle<0) {
        *tag = 0;
        *sy = 0;
        *length = 0;
        return RAW1394_ISO_OK;
    }

    if(m_last_cycle == -1) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Handler for %s SP %p is alive (cycle = %d)\n", getTypeString(), this, cycle);
    }

    int dropped_cycles = 0;
    if (m_last_cycle != cycle && m_last_cycle != -1) {
        dropped_cycles = diffCycles(cycle, m_last_cycle) - 1;
        if (dropped_cycles < 0) debugWarning("(%p) dropped < 1 (%d)\n", this, dropped_cycles);
        if (dropped_cycles > 0) {
            debugWarning("(%p) dropped %d packets on cycle %u (last_cycle=%u, dropped=%d)\n", this, dropped_cycles, cycle, m_last_cycle, dropped);
            m_dropped += dropped_cycles;
        }
    }
    if (cycle >= 0) {
        m_last_cycle = cycle;
    }

    // bypass based upon state
    if (m_state == ePS_Invalid) {
        debugError("Should not have state %s\n", ePSToString(m_state) );
        return RAW1394_ISO_ERROR;
    }
    if (m_state == ePS_Created) {
        *tag = 0;
        *sy = 0;
        *length = 0;
        return RAW1394_ISO_DEFER;
    }

    // normal processing
    // note that we can't use getCycleTimer directly here,
    // because packets are queued in advance. This means that
    // we the packet we are constructing will be sent out
    // on 'cycle', not 'now'.
    unsigned int ctr = m_handler->getCycleTimer();
    int now_cycles = (int)CYCLE_TIMER_GET_CYCLES(ctr);

    // the difference between the cycle this
    // packet is intended for and 'now'
    int cycle_diff = diffCycles(cycle, now_cycles);

    #ifdef DEBUG
    if(cycle_diff < 0) {
        debugWarning("Requesting packet for cycle %04d which is in the past (now=%04dcy)\n",
            cycle, now_cycles);
    }
    #endif

    // store the previous timestamp
    m_last_timestamp2 = m_last_timestamp;

    // NOTE: synchronized switching is restricted to a 0.5 sec span (4000 cycles)
    //       it happens on the first 'good' cycle for the wait condition
    //       or on the first received cycle that is received afterwards (might be a problem)

    // check whether we are waiting for a stream to be disabled
    if(m_state == ePS_WaitingForStreamDisable) {
        // we then check whether we have to switch on this cycle
        if (diffCycles(cycle, m_cycle_to_switch_state) >= 0) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Should update state to DryRunning\n");
            m_next_state = ePS_DryRunning;
            if (!updateState()) { // we are allowed to change the state directly
                debugError("Could not update state!\n");
                return RAW1394_ISO_ERROR;
            }
        } else {
            // not time to disable yet
        }
    }
    // check whether we are waiting for a stream to be enabled
    else if(m_state == ePS_WaitingForStreamEnable) {
        // we then check whether we have to switch on this cycle
        if (diffCycles(cycle, m_cycle_to_switch_state) >= 0) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Should update state to Running\n");
            m_next_state = ePS_Running;
            if (!updateState()) { // we are allowed to change the state directly
                debugError("Could not update state!\n");
                return RAW1394_ISO_ERROR;
            }
        } else {
            // not time to enable yet
        }
        // we are dryRunning hence data should be processed in any case
    }
    // check whether we are waiting for a stream to startup
    else if(m_state == ePS_WaitingForStream) {
        // as long as the cycle parameter is not in sync with
        // the current time, the stream is considered not
        // to be 'running'
        // we then check whether we have to switch on this cycle
        if ((cycle_diff >= 0) && (diffCycles(cycle, m_cycle_to_switch_state) >= 0)) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Should update state to DryRunning due to good packet\n");
            // hence go to the dryRunning state
            m_next_state = ePS_DryRunning;
            if (!updateState()) { // we are allowed to change the state directly
                debugError("Could not update state!\n");
                return RAW1394_ISO_ERROR;
            }
        } else {
            // not time (yet) to switch state
        }
    }
    else if(m_state == ePS_Running) {
        // check the packet header
        enum eChildReturnValue result = generatePacketHeader(data, length, tag, sy, cycle, dropped_cycles, max_length);
        if (result == eCRV_Packet) {
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "XMIT: CY=%04u TS=%011llu\n",
                    cycle, m_last_timestamp);
            // update some accounting
            m_last_good_cycle = cycle;
            m_last_dropped = dropped_cycles;

            // check whether a state change has been requested
            // note that only the wait state changes are synchronized with the cycles
            if(m_state != m_next_state) {
                debugOutput(DEBUG_LEVEL_VERBOSE, "Should update state from %s to %s\n",
                                                ePSToString(m_state), ePSToString(m_next_state));
                // execute the requested change
                if (!updateState()) { // we are allowed to change the state directly
                    debugError("Could not update state!\n");
                    return RAW1394_ISO_ERROR;
                }
            }

            enum eChildReturnValue result2 = generatePacketData(data, length, tag, sy, cycle, dropped_cycles, max_length);
            // if an xrun occured, switch to the dryRunning state and
            // allow for the xrun to be picked up
            if (result2 == eCRV_XRun) {
                debugOutput(DEBUG_LEVEL_VERBOSE, "Should update state to WaitingForStreamDisable due to data xrun\n");
                m_in_xrun = true;
                m_cycle_to_switch_state = cycle+1; // switch in the next cycle
                m_next_state = ePS_WaitingForStreamDisable;
                // execute the requested change
                if (!updateState()) { // we are allowed to change the state directly
                    debugError("Could not update state!\n");
                    return RAW1394_ISO_ERROR;
                }
                goto send_empty_packet;
            }
            return RAW1394_ISO_OK;
        } else if (result == eCRV_XRun) { // pick up the possible xruns
            debugOutput(DEBUG_LEVEL_VERBOSE, "Should update state to WaitingForStreamDisable due to header xrun\n");
            m_in_xrun = true;
            m_cycle_to_switch_state = cycle+1; // switch in the next cycle
            m_next_state = ePS_WaitingForStreamDisable;
            // execute the requested change
            if (!updateState()) { // we are allowed to change the state directly
                debugError("Could not update state!\n");
                return RAW1394_ISO_ERROR;
            }
        } else if ((result == eCRV_EmptyPacket) || (result == eCRV_Again)) {
            if(m_state != m_next_state) {
                debugOutput(DEBUG_LEVEL_VERBOSE, "Should update state from %s to %s\n",
                                                ePSToString(m_state), ePSToString(m_next_state));
                // execute the requested change
                if (!updateState()) { // we are allowed to change the state directly
                    debugError("Could not update state!\n");
                    return RAW1394_ISO_ERROR;
                }
            }
            goto send_empty_packet;
//         } else if (result == eCRV_Again) {
//             debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "have to retry cycle %d\n", cycle);
//             if(m_state != m_next_state) {
//                 debugOutput(DEBUG_LEVEL_VERBOSE, "Should update state from %s to %s\n",
//                                                 ePSToString(m_state), ePSToString(m_next_state));
//                 // execute the requested change
//                 if (!updateState()) { // we are allowed to change the state directly
//                     debugError("Could not update state!\n");
//                     return RAW1394_ISO_ERROR;
//                 }
//             }
            // force some delay
//             usleep(125);
//             return RAW1394_ISO_AGAIN;
        } else {
            debugError("Invalid return value: %d\n", result);
            return RAW1394_ISO_ERROR;
        }
    }
    // we are not running, so send an empty packet
    // we should generate a valid packet any time
send_empty_packet:
    // note that only the wait state changes are synchronized with the cycles
    if(m_state != m_next_state) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Should update state from %s to %s\n",
                                        ePSToString(m_state), ePSToString(m_next_state));
        // execute the requested change
        if (!updateState()) { // we are allowed to change the state directly
            debugError("Could not update state!\n");
            return RAW1394_ISO_ERROR;
        }
    }

    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "XMIT EMPTY: CY=%04u\n", cycle);
    generateSilentPacketHeader(data, length, tag, sy, cycle, dropped_cycles, max_length);
    generateSilentPacketData(data, length, tag, sy, cycle, dropped_cycles, max_length);
    return RAW1394_ISO_DEFER;
}


// Frame Transfer API
/**
 * Transfer a block of frames from the event buffer to the port buffers
 * @param nbframes number of frames to transfer
 * @param ts the timestamp that the LAST frame in the block should have
 * @return 
 */
bool StreamProcessor::getFrames(unsigned int nbframes, int64_t ts) {
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "%p.getFrames(%d, %11llu)", nbframes, ts);
    assert( getType() == ePT_Receive );
    if(isDryRunning()) return getFramesDry(nbframes, ts);
    else return getFramesWet(nbframes, ts);
}

bool StreamProcessor::getFramesWet(unsigned int nbframes, int64_t ts) {
// FIXME: this should be done somewhere else
#ifdef DEBUG
    uint64_t ts_expected;
    signed int fc;
    int32_t lag_ticks;
    float lag_frames;

    // in order to sync up multiple received streams, we should 
    // use the ts parameter. It specifies the time of the block's 
    // last sample.
    float srate = m_manager->getSyncSource().getTicksPerFrame();
    assert(srate != 0.0);
    int64_t this_block_length_in_ticks = (int64_t)(((float)nbframes) * srate);

    ffado_timestamp_t ts_head_tmp;
    m_data_buffer->getBufferHeadTimestamp(&ts_head_tmp, &fc);
    ts_expected = addTicks((uint64_t)ts_head_tmp, this_block_length_in_ticks);

    lag_ticks = diffTicks(ts, ts_expected);
    lag_frames = (((float)lag_ticks) / srate);
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "stream (%p): drifts %6d ticks = %10.5f frames (rate=%10.5f), %lld, %llu, %d\n",
                 this, lag_ticks, lag_frames, srate, ts, ts_expected, fc);
    if (lag_frames >= 1.0) {
        // the stream lags
        debugWarning( "stream (%p): lags  with %6d ticks = %10.5f frames (rate=%10.5f), %lld, %llu, %d\n",
                      this, lag_ticks, lag_frames, srate, ts, ts_expected, fc);
    } else if (lag_frames <= -1.0) {
        // the stream leads
        debugWarning( "stream (%p): leads with %6d ticks = %10.5f frames (rate=%10.5f), %lld, %llu, %d\n",
                      this, lag_ticks, lag_frames, srate, ts, ts_expected, fc);
    }
#endif
    // ask the buffer to process nbframes of frames
    // using it's registered client's processReadBlock(),
    // which should be ours
    m_data_buffer->blockProcessReadFrames(nbframes);
    return true;
}

bool StreamProcessor::getFramesDry(unsigned int nbframes, int64_t ts)
{
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "stream (%p): dry run %d frames (@ ts=%lld)\n",
                 this, nbframes, ts);
    // dry run on this side means that we put silence in all enabled ports
    // since there is do data put into the ringbuffer in the dry-running state
    return provideSilenceBlock(nbframes, 0);
}

bool
StreamProcessor::dropFrames(unsigned int nbframes, int64_t ts)
{
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "StreamProcessor::dropFrames(%d, %lld)\n", nbframes, ts);
    return m_data_buffer->dropFrames(nbframes);
}

bool StreamProcessor::putFrames(unsigned int nbframes, int64_t ts)
{
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "%p.putFrames(%d, %11llu)", nbframes, ts);
    assert( getType() == ePT_Transmit );
    if(isDryRunning()) return putFramesDry(nbframes, ts);
    else return putFramesWet(nbframes, ts);
}

bool
StreamProcessor::putFramesWet(unsigned int nbframes, int64_t ts)
{
    debugOutput(DEBUG_LEVEL_ULTRA_VERBOSE, "StreamProcessor::putFramesWet(%d, %llu)\n", nbframes, ts);
    // transfer the data
    m_data_buffer->blockProcessWriteFrames(nbframes, ts);
    debugOutput(DEBUG_LEVEL_ULTRA_VERBOSE, " New timestamp: %llu\n", ts);
    return true; // FIXME: what about failure?
}

bool
StreamProcessor::putFramesDry(unsigned int nbframes, int64_t ts)
{
    debugOutput(DEBUG_LEVEL_ULTRA_VERBOSE, "StreamProcessor::putFramesDry(%d, %llu)\n", nbframes, ts);
    // do nothing
    return true;
}

bool
StreamProcessor::putSilenceFrames(unsigned int nbframes, int64_t ts)
{
    debugOutput(DEBUG_LEVEL_ULTRA_VERBOSE, "StreamProcessor::putSilenceFrames(%d, %llu)\n", nbframes, ts);

    size_t bytes_per_frame = getEventSize() * getEventsPerFrame();
    unsigned int scratch_buffer_size_frames = m_scratch_buffer_size_bytes / bytes_per_frame;

    if (nbframes > scratch_buffer_size_frames) {
        debugError("nframes (%u) > scratch_buffer_size_frames (%u)\n",
                   nbframes, scratch_buffer_size_frames);
    }

    assert(m_scratch_buffer);
    if(!transmitSilenceBlock((char *)m_scratch_buffer, nbframes, 0)) {
        debugError("Could not prepare silent block\n");
        return false;
    }
    if(!m_data_buffer->writeFrames(nbframes, (char *)m_scratch_buffer, ts)) {
        debugError("Could not write silent block\n");
        return false;
    }
    return true;
}

bool
StreamProcessor::shiftStream(int nbframes)
{
    if(nbframes == 0) return true;
    if(nbframes > 0) {
        return m_data_buffer->dropFrames(nbframes);
    } else {
        bool result = true;
        while(nbframes++) {
            result &= m_data_buffer->writeDummyFrame();
        }
        return result;
    }
}

/**
 * @brief write silence events to the stream ringbuffers.
 */
bool StreamProcessor::provideSilenceBlock(unsigned int nevents, unsigned int offset)
{
    bool no_problem=true;
    for ( PortVectorIterator it = m_PeriodPorts.begin();
          it != m_PeriodPorts.end();
          ++it ) {
        if((*it)->isDisabled()) {continue;};

        //FIXME: make this into a static_cast when not DEBUG?
        Port *port=dynamic_cast<Port *>(*it);

        switch(port->getPortType()) {

        case Port::E_Audio:
            if(provideSilenceToPort(static_cast<AudioPort *>(*it), offset, nevents)) {
                debugWarning("Could not put silence into to port %s",(*it)->getName().c_str());
                no_problem=false;
            }
            break;
        // midi is a packet based port, don't process
        //    case MotuPortInfo::E_Midi:
        //        break;

        default: // ignore
            break;
        }
    }
    return no_problem;
}

int
StreamProcessor::provideSilenceToPort(
                       AudioPort *p, unsigned int offset, unsigned int nevents)
{
    unsigned int j=0;
    switch(p->getDataType()) {
        default:
        case Port::E_Int24:
            {
                quadlet_t *buffer=(quadlet_t *)(p->getBufferAddress());
                assert(nevents + offset <= p->getBufferSize());
                buffer+=offset;

                for(j = 0; j < nevents; j += 1) { // decode max nsamples
                    *(buffer)=0;
                    buffer++;
                }
            }
            break;
        case Port::E_Float:
            {
                float *buffer=(float *)(p->getBufferAddress());
                assert(nevents + offset <= p->getBufferSize());
                buffer+=offset;

                for(j = 0; j < nevents; j += 1) { // decode max nsamples
                    *buffer = 0.0;
                    buffer++;
                }
            }
            break;
    }
    return 0;
}

/***********************************************
 * State related API                           *
 ***********************************************/
bool StreamProcessor::init()
{
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "init...\n");

    // initialization can be done without requesting it
    // from the packet loop
    m_next_state = ePS_Created;
    return true;
}

bool StreamProcessor::prepare()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Prepare SP (%p)...\n", this);
    if(!m_manager) {
        debugFatal("Not attached to a manager!\n");
        return false;
    }

    // make the scratch buffer one period of frames long
    m_scratch_buffer_size_bytes = m_manager->getPeriodSize() * getEventsPerFrame() * getEventSize();
    debugOutput( DEBUG_LEVEL_VERBOSE, " Allocate scratch buffer of %d quadlets\n");
    if(m_scratch_buffer) delete[] m_scratch_buffer;
    m_scratch_buffer = new byte_t[m_scratch_buffer_size_bytes];
    if(m_scratch_buffer == NULL) {
        debugFatal("Could not allocate scratch buffer\n");
        return false;
    }

    if (!prepareChild()) {
        debugFatal("Could not prepare child\n");
        return false;
    }

    // initialization can be done without requesting it
    // from the packet loop
    m_next_state = ePS_Stopped;
    return updateState();
}

bool
StreamProcessor::scheduleStateTransition(enum eProcessorState state, uint64_t time_instant)
{
    // first set the time, since in the packet loop we first check m_state == m_next_state before
    // using the time
    m_cycle_to_switch_state = TICKS_TO_CYCLES(time_instant);
    m_next_state = state;
    return true;
}

bool
StreamProcessor::waitForState(enum eProcessorState state, unsigned int timeout_ms)
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "Waiting for state %s\n", ePSToString(state));
    int cnt = timeout_ms;
    while (m_state != state && cnt) {
        usleep(1000);
        cnt--;
    }
    if(cnt==0) {
        debugOutput(DEBUG_LEVEL_VERBOSE, " Timeout\n");
        return false;
    }
    return true;
}

bool StreamProcessor::scheduleStartDryRunning(int64_t t) {
    uint64_t tx;
    if (t < 0) {
        tx = addTicks(m_handler->getCycleTimerTicks(), 200 * TICKS_PER_CYCLE);
    } else {
        tx = t;
    }
    debugOutput(DEBUG_LEVEL_VERBOSE,"for %s SP (%p)\n", ePTToString(getType()), this);
    uint64_t now = m_handler->getCycleTimerTicks();
    debugOutput(DEBUG_LEVEL_VERBOSE,"  Now                   : %011llu (%03us %04uc %04ut)\n",
                          now,
                          (unsigned int)TICKS_TO_SECS(now),
                          (unsigned int)TICKS_TO_CYCLES(now),
                          (unsigned int)TICKS_TO_OFFSET(now));
    debugOutput(DEBUG_LEVEL_VERBOSE,"  Start at              : %011llu (%03us %04uc %04ut)\n",
                          tx,
                          (unsigned int)TICKS_TO_SECS(tx),
                          (unsigned int)TICKS_TO_CYCLES(tx),
                          (unsigned int)TICKS_TO_OFFSET(tx));
    if (m_state == ePS_Stopped) {
        return scheduleStateTransition(ePS_WaitingForStream, tx);
    } else if (m_state == ePS_Running) {
        return scheduleStateTransition(ePS_WaitingForStreamDisable, tx);
    } else {
        debugError("Cannot switch to ePS_DryRunning from %s\n", ePSToString(m_state));
        return false;
    }
}

bool StreamProcessor::scheduleStartRunning(int64_t t) {
    uint64_t tx;
    if (t < 0) {
        tx = addTicks(m_handler->getCycleTimerTicks(), 200 * TICKS_PER_CYCLE);
    } else {
        tx = t;
    }
    debugOutput(DEBUG_LEVEL_VERBOSE,"for %s SP (%p)\n", ePTToString(getType()), this);
    uint64_t now = m_handler->getCycleTimerTicks();
    debugOutput(DEBUG_LEVEL_VERBOSE,"  Now                   : %011llu (%03us %04uc %04ut)\n",
                          now,
                          (unsigned int)TICKS_TO_SECS(now),
                          (unsigned int)TICKS_TO_CYCLES(now),
                          (unsigned int)TICKS_TO_OFFSET(now));
    debugOutput(DEBUG_LEVEL_VERBOSE,"  Start at              : %011llu (%03us %04uc %04ut)\n",
                          tx,
                          (unsigned int)TICKS_TO_SECS(tx),
                          (unsigned int)TICKS_TO_CYCLES(tx),
                          (unsigned int)TICKS_TO_OFFSET(tx));
    return scheduleStateTransition(ePS_WaitingForStreamEnable, tx);
}

bool StreamProcessor::scheduleStopDryRunning(int64_t t) {
    uint64_t tx;
    if (t < 0) {
        tx = addTicks(m_handler->getCycleTimerTicks(), 200 * TICKS_PER_CYCLE);
    } else {
        tx = t;
    }
    debugOutput(DEBUG_LEVEL_VERBOSE,"for %s SP (%p)\n", ePTToString(getType()), this);
    uint64_t now = m_handler->getCycleTimerTicks();
    debugOutput(DEBUG_LEVEL_VERBOSE,"  Now                   : %011llu (%03us %04uc %04ut)\n",
                          now,
                          (unsigned int)TICKS_TO_SECS(now),
                          (unsigned int)TICKS_TO_CYCLES(now),
                          (unsigned int)TICKS_TO_OFFSET(now));
    debugOutput(DEBUG_LEVEL_VERBOSE,"  Stop at               : %011llu (%03us %04uc %04ut)\n",
                          tx,
                          (unsigned int)TICKS_TO_SECS(tx),
                          (unsigned int)TICKS_TO_CYCLES(tx),
                          (unsigned int)TICKS_TO_OFFSET(tx));
    return scheduleStateTransition(ePS_Stopped, tx);
}

bool StreamProcessor::scheduleStopRunning(int64_t t) {
    uint64_t tx;
    if (t < 0) {
        tx = addTicks(m_handler->getCycleTimerTicks(), 200 * TICKS_PER_CYCLE);
    } else {
        tx = t;
    }
    debugOutput(DEBUG_LEVEL_VERBOSE,"for %s SP (%p)\n", ePTToString(getType()), this);
    uint64_t now = m_handler->getCycleTimerTicks();
    debugOutput(DEBUG_LEVEL_VERBOSE,"  Now                   : %011llu (%03us %04uc %04ut)\n",
                          now,
                          (unsigned int)TICKS_TO_SECS(now),
                          (unsigned int)TICKS_TO_CYCLES(now),
                          (unsigned int)TICKS_TO_OFFSET(now));
    debugOutput(DEBUG_LEVEL_VERBOSE,"  Stop at               : %011llu (%03us %04uc %04ut)\n",
                          tx,
                          (unsigned int)TICKS_TO_SECS(tx),
                          (unsigned int)TICKS_TO_CYCLES(tx),
                          (unsigned int)TICKS_TO_OFFSET(tx));
    return scheduleStateTransition(ePS_WaitingForStreamDisable, tx);
}

bool StreamProcessor::startDryRunning(int64_t t) {
    if(!scheduleStartDryRunning(t)) {
        debugError("Could not schedule transition\n");
        return false;
    }
    if(!waitForState(ePS_DryRunning, 2000)) {
        debugError(" Timeout while waiting for %s\n", ePSToString(ePS_DryRunning));
        return false;
    }
    return true;
}

bool StreamProcessor::startRunning(int64_t t) {
    if(!scheduleStartRunning(t)) {
        debugError("Could not schedule transition\n");
        return false;
    }
    if(!waitForState(ePS_Running, 2000)) {
        debugError(" Timeout while waiting for %s\n", ePSToString(ePS_Running));
        return false;
    }
    return true;
}

bool StreamProcessor::stopDryRunning(int64_t t) {
    if(!scheduleStopDryRunning(t)) {
        debugError("Could not schedule transition\n");
        return false;
    }
    if(!waitForState(ePS_Stopped, 2000)) {
        debugError(" Timeout while waiting for %s\n", ePSToString(ePS_Stopped));
        return false;
    }
    return true;
}

bool StreamProcessor::stopRunning(int64_t t) {
    if(!scheduleStopRunning(t)) {
        debugError("Could not schedule transition\n");
        return false;
    }
    if(!waitForState(ePS_DryRunning, 2000)) {
        debugError(" Timeout while waiting for %s\n", ePSToString(ePS_DryRunning));
        return false;
    }
    return true;
}


// internal state API

/**
 * @brief Enter the ePS_Stopped state
 * @return true if successful, false if not
 *
 * @pre none
 *
 * @post the buffer and the isostream are ready for use.
 * @post all dynamic structures have been allocated successfully
 * @post the buffer is transparent and empty, and all parameters are set
 *       to the correct initial/nominal values.
 *
 */
bool
StreamProcessor::doStop()
{
    float ticks_per_frame;
    unsigned int ringbuffer_size_frames = (m_manager->getNbBuffers() + 1) * m_manager->getPeriodSize();

    debugOutput(DEBUG_LEVEL_VERBOSE, "Enter from state: %s\n", ePSToString(m_state));
    bool result = true;

    switch(m_state) {
        case ePS_Created:
            assert(m_data_buffer);
            // object just created
            result = m_data_buffer->init();

            // prepare the framerate estimate
            ticks_per_frame = (TICKS_PER_SECOND*1.0) / ((float)m_manager->getNominalRate());
            m_ticks_per_frame = ticks_per_frame;
            debugOutput(DEBUG_LEVEL_VERBOSE,"Initializing remote ticks/frame to %f\n", ticks_per_frame);

            // initialize internal buffer
            result &= m_data_buffer->setBufferSize(ringbuffer_size_frames);

            result &= m_data_buffer->setEventSize( getEventSize() );
            result &= m_data_buffer->setEventsPerFrame( getEventsPerFrame() );
            if(getType() == ePT_Receive) {
                result &= m_data_buffer->setUpdatePeriod( getNominalFramesPerPacket() );
            } else {
                result &= m_data_buffer->setUpdatePeriod( m_manager->getPeriodSize() );
            }
            result &= m_data_buffer->setNominalRate(ticks_per_frame);
            result &= m_data_buffer->setWrapValue(128L*TICKS_PER_SECOND);
            result &= m_data_buffer->prepare(); // FIXME: the name

            // set the parameters of ports we can:
            // we want the audio ports to be period buffered,
            // and the midi ports to be packet buffered
            for ( PortVectorIterator it = m_Ports.begin();
                it != m_Ports.end();
                ++it )
            {
                debugOutput(DEBUG_LEVEL_VERBOSE, "Setting up port %s\n",(*it)->getName().c_str());
                if(!(*it)->setBufferSize(m_manager->getPeriodSize())) {
                    debugFatal("Could not set buffer size to %d\n",m_manager->getPeriodSize());
                    return false;
                }
                switch ((*it)->getPortType()) {
                    case Port::E_Audio:
                        if(!(*it)->setSignalType(Port::E_PeriodSignalled)) {
                            debugFatal("Could not set signal type to PeriodSignalling");
                            return false;
                        }
                        // buffertype and datatype are dependant on the API
                        debugWarning("---------------- ! Doing hardcoded dummy setup ! --------------\n");
                        // buffertype and datatype are dependant on the API
                        if(!(*it)->setBufferType(Port::E_PointerBuffer)) {
                            debugFatal("Could not set buffer type");
                            return false;
                        }
                        if(!(*it)->useExternalBuffer(true)) {
                            debugFatal("Could not set external buffer usage");
                            return false;
                        }
                        if(!(*it)->setDataType(Port::E_Float)) {
                            debugFatal("Could not set data type");
                            return false;
                        }
                        break;
                    case Port::E_Midi:
                        if(!(*it)->setSignalType(Port::E_PacketSignalled)) {
                            debugFatal("Could not set signal type to PacketSignalling");
                            return false;
                        }
                        // buffertype and datatype are dependant on the API
                        debugWarning("---------------- ! Doing hardcoded test setup ! --------------\n");
                        // buffertype and datatype are dependant on the API
                        if(!(*it)->setBufferType(Port::E_RingBuffer)) {
                            debugFatal("Could not set buffer type");
                            return false;
                        }
                        if(!(*it)->setDataType(Port::E_MidiEvent)) {
                            debugFatal("Could not set data type");
                            return false;
                        }
                        break;
                    default:
                        debugWarning("Unsupported port type specified\n");
                        break;
                }
            }
            // the API specific settings of the ports should already be set,
            // as this is called from the processorManager->prepare()
            // so we can init the ports
            result &= PortManager::initPorts();

            break;
        case ePS_DryRunning:
            // what to do here?
            break;
        default:
            debugError("Entry from invalid state: %s\n", ePSToString(m_state));
            return false;
    }

    result &= m_data_buffer->clearBuffer(); // FIXME: don't like the reset() name
    // make the buffer transparent
    m_data_buffer->setTransparent(true);

    // reset all ports
    result &= PortManager::preparePorts();

    m_state = ePS_Stopped;
    #ifdef DEBUG
    if (getDebugLevel() >= DEBUG_LEVEL_VERBOSE) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "State switch complete, dumping SP info...\n");
        dumpInfo();
    }
    #endif
    return result;
}

/**
 * @brief Enter the ePS_WaitingForStream state
 * @return true if successful, false if not
 *
 * @pre all dynamic data structures are allocated successfully
 *
 * @post
 *
 */
bool
StreamProcessor::doWaitForRunningStream()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "Enter from state: %s\n", ePSToString(m_state));
    switch(m_state) {
        case ePS_Stopped:
            // we have to start waiting for an incoming stream
            // this basically means nothing, the state change will
            // be picked up by the packet iterator
            break;
        default:
            debugError("Entry from invalid state: %s\n", ePSToString(m_state));
            return false;
    }
    m_state = ePS_WaitingForStream;
    #ifdef DEBUG
    if (getDebugLevel() >= DEBUG_LEVEL_VERBOSE) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "State switch complete, dumping SP info...\n");
        dumpInfo();
    }
    #endif
    return true;
}

/**
 * @brief Enter the ePS_DryRunning state
 * @return true if successful, false if not
 *
 * @pre
 *
 * @post
 *
 */
bool
StreamProcessor::doDryRunning()
{
    bool result = true;
    debugOutput(DEBUG_LEVEL_VERBOSE, "Enter from state: %s\n", ePSToString(m_state));
    switch(m_state) {
        case ePS_WaitingForStream:
            // a running stream has been detected
            debugOutput(DEBUG_LEVEL_VERBOSE, "StreamProcessor %p started dry-running at cycle %d\n", this, m_last_cycle);
            if (getType() == ePT_Receive) {
                // this to ensure that there is no discontinuity when starting to 
                // update the DLL based upon the received packets
                m_data_buffer->setBufferTailTimestamp(m_last_timestamp);
            } else {
                // FIXME: PC=master mode will have to do something here I guess...
            }
            break;
        case ePS_WaitingForStreamDisable:
            result &= m_data_buffer->clearBuffer(); // FIXME: don't like the reset() name
            m_data_buffer->setTransparent(true);
            break;
        default:
            debugError("Entry from invalid state: %s\n", ePSToString(m_state));
            return false;
    }
    m_state = ePS_DryRunning;
    #ifdef DEBUG
    if (getDebugLevel() >= DEBUG_LEVEL_VERBOSE) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "State switch complete, dumping SP info...\n");
        dumpInfo();
    }
    #endif
    return result;
}

/**
 * @brief Enter the ePS_WaitingForStreamEnable state
 * @return true if successful, false if not
 *
 * @pre
 *
 * @post
 *
 */
bool
StreamProcessor::doWaitForStreamEnable()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "Enter from state: %s\n", ePSToString(m_state));
    unsigned int ringbuffer_size_frames;
    switch(m_state) {
        case ePS_DryRunning:
            // we have to start waiting for an incoming stream
            // this basically means nothing, the state change will
            // be picked up by the packet iterator

            if(!m_data_buffer->clearBuffer()) { // FIXME: don't like the reset() name
                debugError("Could not reset data buffer\n");
                return false;
            }
            if (getType() == ePT_Transmit) {
                ringbuffer_size_frames = m_manager->getNbBuffers() * m_manager->getPeriodSize();
                debugOutput(DEBUG_LEVEL_VERBOSE, "Prefill transmit SP %p with %u frames\n", this, ringbuffer_size_frames);
                // prefill the buffer
                if(!transferSilence(ringbuffer_size_frames)) {
                    debugFatal("Could not prefill transmit stream\n");
                    return false;
                }
            }

            break;
        default:
            debugError("Entry from invalid state: %s\n", ePSToString(m_state));
            return false;
    }
    m_state = ePS_WaitingForStreamEnable;
    #ifdef DEBUG
    if (getDebugLevel() >= DEBUG_LEVEL_VERBOSE) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "State switch complete, dumping SP info...\n");
        dumpInfo();
    }
    #endif
    return true;
}

/**
 * @brief Enter the ePS_Running state
 * @return true if successful, false if not
 *
 * @pre
 *
 * @post
 *
 */
bool
StreamProcessor::doRunning()
{
    bool result = true;
    debugOutput(DEBUG_LEVEL_VERBOSE, "Enter from state: %s\n", ePSToString(m_state));
    switch(m_state) {
        case ePS_WaitingForStreamEnable:
            // a running stream has been detected
            debugOutput(DEBUG_LEVEL_VERBOSE, "StreamProcessor %p started running at cycle %d\n", 
                                             this, m_last_cycle);
            m_in_xrun = false;
            m_data_buffer->setTransparent(false);
            break;
        default:
            debugError("Entry from invalid state: %s\n", ePSToString(m_state));
            return false;
    }
    m_state = ePS_Running;
    #ifdef DEBUG
    if (getDebugLevel() >= DEBUG_LEVEL_VERBOSE) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "State switch complete, dumping SP info...\n");
        dumpInfo();
    }
    #endif
    return result;
}

/**
 * @brief Enter the ePS_WaitingForStreamDisable state
 * @return true if successful, false if not
 *
 * @pre
 *
 * @post
 *
 */
bool
StreamProcessor::doWaitForStreamDisable()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "Enter from state: %s\n", ePSToString(m_state));
    switch(m_state) {
        case ePS_Running:
            // the thread will do the transition
            break;
        default:
            debugError("Entry from invalid state: %s\n", ePSToString(m_state));
            return false;
    }
    m_state = ePS_WaitingForStreamDisable;
    #ifdef DEBUG
    if (getDebugLevel() >= DEBUG_LEVEL_VERBOSE) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "State switch complete, dumping SP info...\n");
        dumpInfo();
    }
    #endif
    return true;
}

/**
 * @brief Updates the state machine and calls the necessary transition functions
 * @return true if successful, false if not
 */
bool StreamProcessor::updateState() {
    bool result = false;
    // copy the current state locally since it could change value,
    // and that's something we don't want to happen inbetween tests
    // if m_next_state changes during this routine, we know for sure
    // that the previous state change was at least attempted correctly.
    enum eProcessorState next_state = m_next_state;

    debugOutput(DEBUG_LEVEL_VERBOSE, "Do state transition: %s => %s\n",
        ePSToString(m_state), ePSToString(next_state));

    if (m_state == next_state) {
        debugWarning("ignoring identity state update from/to %s\n", ePSToString(m_state) );
        return true;
    }

    // after creation, only initialization is allowed
    if (m_state == ePS_Created) {
        if(next_state != ePS_Stopped) {
            goto updateState_exit_with_error;
        }
        // do init here 
        result = doStop();
        if (result) return true;
        else goto updateState_exit_change_failed;
    }

    // after initialization, only WaitingForRunningStream is allowed
    if (m_state == ePS_Stopped) {
        if(next_state != ePS_WaitingForStream) {
            goto updateState_exit_with_error;
        }
        result = doWaitForRunningStream();
        if (result) return true;
        else goto updateState_exit_change_failed;
    }

    // after WaitingForStream, only ePS_DryRunning is allowed
    // this means that the stream started running
    if (m_state == ePS_WaitingForStream) {
        if(next_state != ePS_DryRunning) {
            goto updateState_exit_with_error;
        }
        result = doDryRunning();
        if (result) return true;
        else goto updateState_exit_change_failed;
    }

    // from ePS_DryRunning we can go to:
    //   - ePS_Stopped if something went wrong during DryRunning
    //   - ePS_WaitingForStreamEnable if there is a requested to enable
    if (m_state == ePS_DryRunning) {
        if((next_state != ePS_Stopped) &&
           (next_state != ePS_WaitingForStreamEnable)) {
            goto updateState_exit_with_error;
        }
        if (next_state == ePS_Stopped) {
            result = doStop();
        } else {
            result = doWaitForStreamEnable();
        }
        if (result) return true;
        else goto updateState_exit_change_failed;
    }

    // from ePS_WaitingForStreamEnable we can go to:
    //   - ePS_DryRunning if something went wrong while waiting
    //   - ePS_Running if the stream enabled correctly
    if (m_state == ePS_WaitingForStreamEnable) {
        if((next_state != ePS_DryRunning) &&
           (next_state != ePS_Running)) {
            goto updateState_exit_with_error;
        }
        if (next_state == ePS_Stopped) {
            result = doDryRunning();
        } else {
            result = doRunning();
        }
        if (result) return true;
        else goto updateState_exit_change_failed;
    }

    // from ePS_Running we can only start waiting for a disabled stream
    if (m_state == ePS_Running) {
        if(next_state != ePS_WaitingForStreamDisable) {
            goto updateState_exit_with_error;
        }
        result = doWaitForStreamDisable();
        if (result) return true;
        else goto updateState_exit_change_failed;
    }

    // from ePS_WaitingForStreamDisable we can go to DryRunning
    if (m_state == ePS_WaitingForStreamDisable) {
        if(next_state != ePS_DryRunning) {
            goto updateState_exit_with_error;
        }
        result = doDryRunning();
        if (result) return true;
        else goto updateState_exit_change_failed;
    }

    // if we arrive here there is an error
updateState_exit_with_error:
    debugError("Invalid state transition: %s => %s\n",
        ePSToString(m_state), ePSToString(next_state));
    return false;
updateState_exit_change_failed:
    debugError("State transition failed: %s => %s\n",
        ePSToString(m_state), ePSToString(next_state));
    return false;
}

/***********************************************
 * Helper routines                             *
 ***********************************************/
bool
StreamProcessor::transferSilence(unsigned int nframes)
{
    bool retval;
    signed int fc;
    ffado_timestamp_t ts_tail_tmp;

    // prepare a buffer of silence
    char *dummybuffer = (char *)calloc(sizeof(quadlet_t), nframes * getEventsPerFrame());
    transmitSilenceBlock(dummybuffer, nframes, 0);

    m_data_buffer->getBufferTailTimestamp(&ts_tail_tmp, &fc);
    if (fc != 0) {
        debugWarning("Prefilling a buffer that already contains %d frames\n", fc);
    }

    // add the silence data to the ringbuffer
    if(m_data_buffer->preloadFrames(nframes, dummybuffer, true)) {
        retval = true;
    } else {
        debugWarning("Could not write to event buffer\n");
        retval = false;
    }
    free(dummybuffer);
    return retval;
}

/**
 * @brief convert a eProcessorState to a string
 * @param s the state
 * @return a char * describing the state
 */
const char *
StreamProcessor::ePSToString(enum eProcessorState s) {
    switch (s) {
        case ePS_Invalid: return "ePS_Invalid";
        case ePS_Created: return "ePS_Created";
        case ePS_Stopped: return "ePS_Stopped";
        case ePS_WaitingForStream: return "ePS_WaitingForStream";
        case ePS_DryRunning: return "ePS_DryRunning";
        case ePS_WaitingForStreamEnable: return "ePS_WaitingForStreamEnable";
        case ePS_Running: return "ePS_Running";
        case ePS_WaitingForStreamDisable: return "ePS_WaitingForStreamDisable";
        default: return "error: unknown state";
    }
}

/**
 * @brief convert a eProcessorType to a string
 * @param t the type
 * @return a char * describing the state
 */
const char *
StreamProcessor::ePTToString(enum eProcessorType t) {
    switch (t) {
        case ePT_Receive: return "Receive";
        case ePT_Transmit: return "Transmit";
        default: return "error: unknown type";
    }
}

/***********************************************
 * Debug                                       *
 ***********************************************/
void
StreamProcessor::dumpInfo()
{
    debugOutputShort( DEBUG_LEVEL_NORMAL, " StreamProcessor %p information\n", this);
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Iso stream info:\n");

    IsoStream::dumpInfo();
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  StreamProcessor info:\n");
    if (m_handler) {
        uint64_t now = m_handler->getCycleTimerTicks();
        debugOutputShort( DEBUG_LEVEL_NORMAL, "  Now                   : %011llu (%03us %04uc %04ut)\n",
                          now,
                          (unsigned int)TICKS_TO_SECS(now),
                          (unsigned int)TICKS_TO_CYCLES(now),
                          (unsigned int)TICKS_TO_OFFSET(now));
    }
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Xruns                 : %s\n", (m_in_xrun ? "True":"False"));
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  State                 : %s\n", ePSToString(m_state));
    debugOutputShort( DEBUG_LEVEL_NORMAL, "   Next state           : %s\n", ePSToString(m_next_state));
    debugOutputShort( DEBUG_LEVEL_NORMAL, "    transition at       : %u\n", m_cycle_to_switch_state);
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Buffer                : %p\n", m_data_buffer);
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Nominal framerate     : %u\n", m_manager->getNominalRate());
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Device framerate      : Sync: %f, Buffer %f\n",
        24576000.0/m_manager->getSyncSource().m_data_buffer->getRate(),
        24576000.0/m_data_buffer->getRate()
        );

    m_data_buffer->dumpInfo();
}

void
StreamProcessor::setVerboseLevel(int l) {
    setDebugLevel(l);
    IsoStream::setVerboseLevel(l);
    PortManager::setVerboseLevel(l);
    m_data_buffer->setVerboseLevel(l);
}

} // end of namespace
