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

#include "StreamProcessor.h"
#include "../StreamProcessorManager.h"

#include "devicemanager.h"

#include "libieee1394/ieee1394service.h"
#include "libieee1394/IsoHandlerManager.h"
#include "libieee1394/cycletimer.h"

#include "libutil/Time.h"

#include "libutil/Atomic.h"

#include <assert.h>
#include <math.h>

#define SIGNAL_ACTIVITY { \
    pthread_mutex_lock(&m_activity_cond_lock); \
    pthread_cond_broadcast(&m_activity_cond); \
    pthread_mutex_unlock(&m_activity_cond_lock); \
}

namespace Streaming {

IMPL_DEBUG_MODULE( StreamProcessor, StreamProcessor, DEBUG_LEVEL_VERBOSE );

StreamProcessor::StreamProcessor(FFADODevice &parent, enum eProcessorType type)
    : m_processor_type ( type )
    , m_state( ePS_Created )
    , m_next_state( ePS_Invalid )
    , m_cycle_to_switch_state( 0 )
    , m_Parent( parent )
    , m_1394service( parent.get1394Service() ) // local cache
    , m_IsoHandlerManager( parent.get1394Service().getIsoHandlerManager() ) // local cache
    , m_StreamProcessorManager( m_Parent.getDeviceManager().getStreamProcessorManager() ) // local cache
    , m_local_node_id ( 0 ) // local cache
    , m_channel( -1 )
    , m_dropped( 0 )
    , m_last_timestamp( 0 )
    , m_last_timestamp2( 0 )
    , m_correct_last_timestamp( false )
    , m_scratch_buffer( NULL )
    , m_scratch_buffer_size_bytes( 0 )
    , m_ticks_per_frame( 0 )
    , m_last_cycle( -1 )
    , m_sync_delay( 0 )
    , m_in_xrun( false )
{
    // create the timestamped buffer and register ourselves as its client
    m_data_buffer = new Util::TimestampedBuffer(this);
    pthread_mutex_init(&m_activity_cond_lock, NULL);
    pthread_cond_init(&m_activity_cond, NULL);
}

StreamProcessor::~StreamProcessor() {
    m_StreamProcessorManager.unregisterProcessor(this);
    if(!m_IsoHandlerManager.unregisterStream(this)) {
        debugOutput(DEBUG_LEVEL_VERBOSE,"Could not unregister stream processor with the Iso manager\n");
    }

    // lock the condition mutex to keep threads from blocking on
    // the condition var while we destroy it
    pthread_mutex_lock(&m_activity_cond_lock);
    // now signal activity, releasing threads that
    // are already blocking on the condition variable
    pthread_cond_broadcast(&m_activity_cond);
    // then destroy it
    pthread_cond_destroy(&m_activity_cond);
    pthread_mutex_unlock(&m_activity_cond_lock);

    // destroy the mutexes
    pthread_mutex_destroy(&m_activity_cond_lock);
    if (m_data_buffer) delete m_data_buffer;
    if (m_scratch_buffer) delete[] m_scratch_buffer;
}

void
StreamProcessor::handleBusReset()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "(%p) handling busreset\n", this);
    // for now, we try and make sure everything is cleanly shutdown
    if(!stopRunning(-1)) {
        debugError("Failed to stop SP\n");
    }
    SIGNAL_ACTIVITY;
}

void StreamProcessor::handlerDied()
{
    debugWarning("Handler died for %p\n", this);
    m_state = ePS_Stopped;
    m_in_xrun = true;
}

uint64_t StreamProcessor::getTimeNow() {
    return m_1394service.getCycleTimerTicks();
}

int StreamProcessor::getMaxFrameLatency() {
    if (getType() == ePT_Receive) {
        return (int)(m_IsoHandlerManager.getPacketLatencyForStream( this ) * TICKS_PER_CYCLE);
    } else {
        return (int)(m_IsoHandlerManager.getPacketLatencyForStream( this ) * TICKS_PER_CYCLE);
    }
}

unsigned int
StreamProcessor::getNominalPacketsNeeded(unsigned int nframes)
{
    unsigned int nominal_frames_per_second 
                    = m_StreamProcessorManager.getNominalRate();
    uint64_t nominal_ticks_per_frame = TICKS_PER_SECOND / nominal_frames_per_second;
    uint64_t nominal_ticks = nominal_ticks_per_frame * nframes;
    uint64_t nominal_packets = nominal_ticks / TICKS_PER_CYCLE;
    return nominal_packets;
}

unsigned int
StreamProcessor::getPacketsPerPeriod()
{
    return getNominalPacketsNeeded(m_StreamProcessorManager.getPeriodSize());
}

unsigned int
StreamProcessor::getNbPacketsIsoXmitBuffer()
{
    // if we use one thread per packet, we can put every frame directly into the ISO buffer
    // the waitForClient in IsoHandler will take care of the fact that the frames are
    // not present in time
    unsigned int packets_to_prebuffer = (getPacketsPerPeriod() * (m_StreamProcessorManager.getNbBuffers())) + 10;
    debugOutput(DEBUG_LEVEL_VERBOSE, "Nominal prebuffer: %u\n", packets_to_prebuffer);
    return packets_to_prebuffer;
}

/***********************************************
 * Buffer management and manipulation          *
 ***********************************************/
void StreamProcessor::flush() {
    m_IsoHandlerManager.flushHandlerForStream(this);
}

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
    time_at_period = addTicks(time_at_period, m_StreamProcessorManager.getSyncSource().getSyncDelay());

    uint64_t cycle_timer=m_1394service.getCycleTimerTicks();

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
StreamProcessor::setSyncDelay(unsigned int d) {
    #ifdef DEBUG
    unsigned int frames = (unsigned int)((float)d / getTicksPerFrame());
    debugOutput(DEBUG_LEVEL_VERBOSE, "Setting SP %p SyncDelay to %u ticks, %u frames\n", this, d, frames);
    #endif
    m_sync_delay = d; // FIXME: sync delay not necessary anymore
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
        ffado_timestamp_t next_period_boundary = m_data_buffer->getTimestampFromHead(m_StreamProcessorManager.getPeriodSize());
    
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
        ffado_timestamp_t next_period_boundary = m_data_buffer->getTimestampFromTail((m_StreamProcessorManager.getNbBuffers()-1) * m_StreamProcessorManager.getPeriodSize());
    
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
        debugOutput(DEBUG_LEVEL_VERBOSE, "(%p, %s) cannot transfer since fc == %u, nbframes == %u\n", 
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
                           unsigned int cycle, unsigned int dropped,
                           unsigned int skipped) {
#ifdef DEBUG
    if(m_last_cycle == -1) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Handler for %s SP %p is alive (cycle = %u)\n", getTypeString(), this, cycle);
    }
#endif

    int dropped_cycles = 0;
    if (m_last_cycle != (int)cycle && m_last_cycle != -1) {
        dropped_cycles = diffCycles(cycle, m_last_cycle) - 1;
        if (dropped_cycles < 0) {
            debugWarning("(%p) dropped < 1 (%d), cycle: %d, last_cycle: %d, dropped: %d, 'skipped'=%u\n", 
                         this, dropped_cycles, cycle, m_last_cycle, dropped, skipped);
        }
        if (dropped_cycles > 0) {
            debugWarning("(%p) dropped %d packets on cycle %u, 'dropped'=%u, 'skipped'=%u, cycle=%d, m_last_cycle=%d\n",
                this, dropped_cycles, cycle, dropped, skipped, cycle, m_last_cycle);
            m_dropped += dropped_cycles;
            m_last_cycle = cycle;
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
        // similarly for dropped packets
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

    // handle dropped cycles
    if(dropped_cycles) {
        // make sure the last_timestamp is corrected
        m_correct_last_timestamp = true;
        if (m_state == ePS_Running) {
            // this is an xrun situation
            m_in_xrun = true;
            debugWarning("Should update state to WaitingForStreamDisable due to dropped packet xrun\n");
            m_cycle_to_switch_state = cycle + 1; // switch in the next cycle
            m_next_state = ePS_WaitingForStreamDisable;
            // execute the requested change
            if (!updateState()) { // we are allowed to change the state directly
                debugError("Could not update state!\n");
                return RAW1394_ISO_ERROR;
            }
        }
    }

    if (result == eCRV_OK) {
        #ifdef DEBUG
        int ticks_per_packet = getTicksPerFrame() * getNominalFramesPerPacket();
        int diff=diffTicks(m_last_timestamp, m_last_timestamp2);
        // display message if the difference between two successive tick
        // values is more than 50 ticks. 1 sample at 48k is 512 ticks
        // so 50 ticks = 10%, which is a rather large jitter value.
        if(diff-ticks_per_packet > 50 || diff-ticks_per_packet < -50) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "rather large TSP difference TS=%011llu => TS=%011llu (%d, nom %d)\n",
                                             m_last_timestamp2, m_last_timestamp, diff, ticks_per_packet);
        }
        #endif

        debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                          "RECV: CY=%04u TS=%011llu\n",
                          cycle, m_last_timestamp);
        // update some accounting
        m_last_good_cycle = cycle;
        m_last_dropped = dropped_cycles;

        if(m_correct_last_timestamp) {
            // they represent a discontinuity in the timestamps, and hence are
            // to be dealt with
            debugWarning("(%p) Correcting timestamp for dropped cycles, discarding packet...\n", this);
            m_data_buffer->setBufferTailTimestamp(substractTicks(m_last_timestamp,
                                                                 (uint64_t)(getNominalFramesPerPacket()
                                                                            * getTicksPerFrame())));
            m_correct_last_timestamp = false;
        }

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

        // for all states that reach this we are allowed to
        // do protocol specific data reception
        enum eChildReturnValue result2 = processPacketData(data, length, channel, tag, sy, cycle, dropped_cycles);

        // if an xrun occured, switch to the dryRunning state and
        // allow for the xrun to be picked up
        if (result2 == eCRV_XRun) {
            debugWarning("processPacketData xrun\n");
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
            SIGNAL_ACTIVITY;
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
                           int cycle, unsigned int dropped,
                           unsigned int skipped, unsigned int max_length) {
    if (cycle<0) {
        *tag = 0;
        *sy = 0;
        *length = 0;
        return RAW1394_ISO_OK;
    }

    unsigned int ctr;
    int now_cycles;
    int cycle_diff;

#ifdef DEBUG
    if(m_last_cycle == -1) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Handler for %s SP %p is alive (cycle = %d)\n", getTypeString(), this, cycle);
    }
#endif

    int dropped_cycles = 0;
    if (m_last_cycle != cycle && m_last_cycle != -1) {
        dropped_cycles = diffCycles(cycle, m_last_cycle) - 1;
        // correct for skipped packets
        // since those are not dropped, but only delayed
        dropped_cycles =- skipped;
        if(skipped) {
            debugWarning("(%p) skipped %d cycles, cycle: %d, last_cycle: %d, dropped: %d\n", 
                         this, skipped, cycle, m_last_cycle, dropped);
        }
        if (dropped_cycles < 0) { 
            debugWarning("(%p) dropped < 1 (%d), cycle: %d, last_cycle: %d, dropped: %d, skipped: %d\n", 
                         this, dropped_cycles, cycle, m_last_cycle, dropped, skipped);
        }
        if (dropped_cycles > 0) {
            debugWarning("(%p) dropped %d packets on cycle %u (last_cycle=%u, dropped=%d, skipped: %d)\n",
                         this, dropped_cycles, cycle, m_last_cycle, dropped, skipped);
            m_dropped += dropped_cycles;
            // HACK: this should not be necessary, since the header generation functions should trigger the xrun.
            //       but apparently there are some issues with the 1394 stack
            m_in_xrun = true;
            if(m_state == ePS_Running) {
                debugShowBackLogLines(200);
                debugWarning("dropped packets xrun\n");
                debugOutput(DEBUG_LEVEL_VERBOSE, "Should update state to WaitingForStreamDisable due to dropped packets xrun\n");
                m_cycle_to_switch_state = cycle + 1;
                m_next_state = ePS_WaitingForStreamDisable;
                // execute the requested change
                if (!updateState()) { // we are allowed to change the state directly
                    debugError("Could not update state!\n");
                    return RAW1394_ISO_ERROR;
                }
                goto send_empty_packet;
            }
        }
    }
    if (cycle >= 0) {
        m_last_cycle = cycle;
    }

#ifdef DEBUG
    // bypass based upon state
    if (m_state == ePS_Invalid) {
        debugError("Should not have state %s\n", ePSToString(m_state) );
        return RAW1394_ISO_ERROR;
    }
#endif

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
    ctr = m_1394service.getCycleTimer();
    now_cycles = (int)CYCLE_TIMER_GET_CYCLES(ctr);

    // the difference between the cycle this
    // packet is intended for and 'now'
    cycle_diff = diffCycles(cycle, now_cycles);

    if(cycle_diff < 0 && (m_state == ePS_Running || m_state == ePS_DryRunning)) {
        debugWarning("Requesting packet for cycle %04d which is in the past (now=%04dcy)\n",
            cycle, now_cycles);
        if(m_state == ePS_Running) {
            debugShowBackLogLines(200);
//             flushDebugOutput();
//             assert(0);
            debugWarning("generatePacketData xrun\n");
            m_in_xrun = true;
            debugOutput(DEBUG_LEVEL_VERBOSE, "Should update state to WaitingForStreamDisable due to data xrun\n");
            m_cycle_to_switch_state = cycle + 1;
            m_next_state = ePS_WaitingForStreamDisable;
            // execute the requested change
            if (!updateState()) { // we are allowed to change the state directly
                debugError("Could not update state!\n");
                return RAW1394_ISO_ERROR;
            }
            goto send_empty_packet;
        }
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
        }
        // generate the silent packet header
        enum eChildReturnValue result = generateSilentPacketHeader(data, length, tag, sy, cycle, dropped_cycles, max_length);
        if (result == eCRV_Packet) {
            debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                               "XMIT SILENT: CY=%04u TS=%011llu\n",
                               cycle, m_last_timestamp);
            // update some accounting
            m_last_good_cycle = cycle;
            m_last_dropped = dropped_cycles;

            // assumed not to xrun
            generateSilentPacketData(data, length, tag, sy, cycle, dropped_cycles, max_length);
            return RAW1394_ISO_OK;
        } else {
            debugError("Invalid return value: %d\n", result);
            return RAW1394_ISO_ERROR;
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
            debugOutput(DEBUG_LEVEL_VERBOSE, "Should update state to WaitingForStream to DryRunning\n");
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
        if (result == eCRV_Packet || result == eCRV_Defer) {
            debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                               "XMIT: CY=%04u TS=%011llu\n",
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
                debugWarning("generatePacketData xrun\n");
                m_in_xrun = true;
                debugOutput(DEBUG_LEVEL_VERBOSE, "Should update state to WaitingForStreamDisable due to data xrun\n");
                m_cycle_to_switch_state = cycle + 1; // switch in the next cycle
                m_next_state = ePS_WaitingForStreamDisable;
                // execute the requested change
                if (!updateState()) { // we are allowed to change the state directly
                    debugError("Could not update state!\n");
                    return RAW1394_ISO_ERROR;
                }
                goto send_empty_packet;
            }
            // skip queueing packets if we detect that there are not enough frames
            // available
            if(result2 == eCRV_Defer || result == eCRV_Defer)
                return RAW1394_ISO_DEFER;
            else
                return RAW1394_ISO_OK;
        } else if (result == eCRV_XRun) { // pick up the possible xruns
            debugWarning("generatePacketHeader xrun\n");
            m_in_xrun = true;
            debugOutput(DEBUG_LEVEL_VERBOSE, "Should update state to WaitingForStreamDisable due to header xrun\n");
            m_cycle_to_switch_state = cycle + 1; // switch in the next cycle
            m_next_state = ePS_WaitingForStreamDisable;
            // execute the requested change
            if (!updateState()) { // we are allowed to change the state directly
                debugError("Could not update state!\n");
                return RAW1394_ISO_ERROR;
            }
        } else if (result == eCRV_EmptyPacket) {
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
        } else if (result == eCRV_Again) {
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "have to retry cycle %d\n", cycle);
            if(m_state != m_next_state) {
                debugOutput(DEBUG_LEVEL_VERBOSE, "Should update state from %s to %s\n",
                                                ePSToString(m_state), ePSToString(m_next_state));
                // execute the requested change
                if (!updateState()) { // we are allowed to change the state directly
                    debugError("Could not update state!\n");
                    return RAW1394_ISO_ERROR;
                }
            }
//             usleep(125); // only when using thread-per-handler
//             return RAW1394_ISO_AGAIN;
            generateEmptyPacketHeader(data, length, tag, sy, cycle, dropped_cycles, max_length);
            generateEmptyPacketData(data, length, tag, sy, cycle, dropped_cycles, max_length);
            return RAW1394_ISO_DEFER;
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

    debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                       "XMIT EMPTY: CY=%04u\n",
                       cycle);
    generateEmptyPacketHeader(data, length, tag, sy, cycle, dropped_cycles, max_length);
    generateEmptyPacketData(data, length, tag, sy, cycle, dropped_cycles, max_length);
    return RAW1394_ISO_OK;
}


// Frame Transfer API
/**
 * Transfer a block of frames from the event buffer to the port buffers
 * @param nbframes number of frames to transfer
 * @param ts the timestamp that the LAST frame in the block should have
 * @return 
 */
bool StreamProcessor::getFrames(unsigned int nbframes, int64_t ts) {
    bool result;
    debugOutputExtreme( DEBUG_LEVEL_VERY_VERBOSE,
                        "%p.getFrames(%d, %11llu)",
                        nbframes, ts);
    assert( getType() == ePT_Receive );
    if(isDryRunning()) result = getFramesDry(nbframes, ts);
    else result = getFramesWet(nbframes, ts);
    SIGNAL_ACTIVITY;
    return result;
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
    float srate = m_StreamProcessorManager.getSyncSource().getTicksPerFrame();
    assert(srate != 0.0);
    int64_t this_block_length_in_ticks = (int64_t)(((float)nbframes) * srate);

    ffado_timestamp_t ts_head_tmp;
    m_data_buffer->getBufferHeadTimestamp(&ts_head_tmp, &fc);
    ts_expected = addTicks((uint64_t)ts_head_tmp, this_block_length_in_ticks);

    lag_ticks = diffTicks(ts, ts_expected);
    lag_frames = (((float)lag_ticks) / srate);
    debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                       "stream (%p): drifts %6d ticks = %10.5f frames (rate=%10.5f), %lld, %llu, %d\n",
                       this, lag_ticks, lag_frames, srate, ts, ts_expected, fc);
    if (lag_frames >= 1.0) {
        // the stream lags
        debugOutput(DEBUG_LEVEL_VERBOSE, "stream (%p): lags  with %6d ticks = %10.5f frames (rate=%10.5f), %lld, %llu, %d\n",
                      this, lag_ticks, lag_frames, srate, ts, ts_expected, fc);
    } else if (lag_frames <= -1.0) {
        // the stream leads
        debugOutput(DEBUG_LEVEL_VERBOSE, "stream (%p): leads with %6d ticks = %10.5f frames (rate=%10.5f), %lld, %llu, %d\n",
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
    debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                       "stream (%p): dry run %d frames (@ ts=%lld)\n",
                       this, nbframes, ts);
    // dry run on this side means that we put silence in all enabled ports
    // since there is do data put into the ringbuffer in the dry-running state
    return provideSilenceBlock(nbframes, 0);
}

bool
StreamProcessor::dropFrames(unsigned int nbframes, int64_t ts)
{
    bool result;
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "StreamProcessor::dropFrames(%d, %lld)\n", nbframes, ts);
    result = m_data_buffer->dropFrames(nbframes);
    SIGNAL_ACTIVITY;
    return result;
}

bool StreamProcessor::putFrames(unsigned int nbframes, int64_t ts)
{
    bool result;
    debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                       "%p.putFrames(%d, %11llu)",
                       nbframes, ts);
    assert( getType() == ePT_Transmit );
    if(isDryRunning()) result = putFramesDry(nbframes, ts);
    else result = putFramesWet(nbframes, ts);
    SIGNAL_ACTIVITY;
    return result;
}

bool
StreamProcessor::putFramesWet(unsigned int nbframes, int64_t ts)
{
    debugOutputExtreme(DEBUG_LEVEL_ULTRA_VERBOSE,
                       "StreamProcessor::putFramesWet(%d, %llu)\n",
                       nbframes, ts);
    // transfer the data
    m_data_buffer->blockProcessWriteFrames(nbframes, ts);
    debugOutputExtreme(DEBUG_LEVEL_ULTRA_VERBOSE,
                       " New timestamp: %llu\n", ts);
    return true; // FIXME: what about failure?
}

bool
StreamProcessor::putFramesDry(unsigned int nbframes, int64_t ts)
{
    debugOutputExtreme(DEBUG_LEVEL_ULTRA_VERBOSE,
                       "StreamProcessor::putFramesDry(%d, %llu)\n",
                       nbframes, ts);
    // do nothing
    return true;
}

bool
StreamProcessor::putSilenceFrames(unsigned int nbframes, int64_t ts)
{
    debugOutputExtreme(DEBUG_LEVEL_ULTRA_VERBOSE,
                       "StreamProcessor::putSilenceFrames(%d, %llu)\n",
                       nbframes, ts);

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

    SIGNAL_ACTIVITY;
    return true;
}

bool
StreamProcessor::shiftStream(int nbframes)
{
    bool result;
    if(nbframes == 0) return true;
    if(nbframes > 0) {
        result = m_data_buffer->dropFrames(nbframes);
        SIGNAL_ACTIVITY;
        return result;
    } else {
        result = true;
        while(nbframes++) {
            result &= m_data_buffer->writeDummyFrame();
        }
        SIGNAL_ACTIVITY;
        return result;
    }
}

/**
 * @brief write silence events to the stream ringbuffers.
 */
bool StreamProcessor::provideSilenceBlock(unsigned int nevents, unsigned int offset)
{
    bool no_problem=true;
    for ( PortVectorIterator it = m_Ports.begin();
          it != m_Ports.end();
          ++it ) {
        if((*it)->isDisabled()) {continue;};

        if(provideSilenceToPort((*it), offset, nevents)) {
            debugWarning("Could not put silence into to port %s",(*it)->getName().c_str());
            no_problem=false;
        }
    }
    return no_problem;
}

int
StreamProcessor::provideSilenceToPort(Port *p, unsigned int offset, unsigned int nevents)
{
    unsigned int j=0;
    switch(p->getPortType()) {
        default:
            debugError("Invalid port type: %d\n", p->getPortType());
            return -1;
        case Port::E_Midi:
        case Port::E_Control:
            {
                quadlet_t *buffer=(quadlet_t *)(p->getBufferAddress());
                assert(nevents + offset <= p->getBufferSize());
                buffer+=offset;

                for(j = 0; j < nevents; j += 1) {
                    *(buffer)=0;
                    buffer++;
                }
            }
            break;
        case Port::E_Audio:
            switch(m_StreamProcessorManager.getAudioDataType()) {
            case StreamProcessorManager::eADT_Int24:
                {
                    quadlet_t *buffer=(quadlet_t *)(p->getBufferAddress());
                    assert(nevents + offset <= p->getBufferSize());
                    buffer+=offset;
    
                    for(j = 0; j < nevents; j += 1) {
                        *(buffer)=0;
                        buffer++;
                    }
                }
                break;
            case StreamProcessorManager::eADT_Float:
                {
                    float *buffer=(float *)(p->getBufferAddress());
                    assert(nevents + offset <= p->getBufferSize());
                    buffer+=offset;
    
                    for(j = 0; j < nevents; j += 1) {
                        *buffer = 0.0;
                        buffer++;
                    }
                }
                break;
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

    if(!m_IsoHandlerManager.registerStream(this)) {
        debugOutput(DEBUG_LEVEL_VERBOSE,"Could not register stream processor with the Iso manager\n");
        return false;
    }
    if(!m_StreamProcessorManager.registerProcessor(this)) {
        debugOutput(DEBUG_LEVEL_VERBOSE,"Could not register stream processor with the SP manager\n");
        return false;
    }

    // initialization can be done without requesting it
    // from the packet loop
    m_next_state = ePS_Created;
    return true;
}

bool StreamProcessor::prepare()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Prepare SP (%p)...\n", this);

    // make the scratch buffer one period of frames long
    m_scratch_buffer_size_bytes = m_StreamProcessorManager.getPeriodSize() * getEventsPerFrame() * getEventSize();
    debugOutput( DEBUG_LEVEL_VERBOSE, " Allocate scratch buffer of %d quadlets\n");
    if(m_scratch_buffer) delete[] m_scratch_buffer;
    m_scratch_buffer = new byte_t[m_scratch_buffer_size_bytes];
    if(m_scratch_buffer == NULL) {
        debugFatal("Could not allocate scratch buffer\n");
        return false;
    }

    // set the parameters of ports we can:
    // we want the audio ports to be period buffered,
    // and the midi ports to be packet buffered
    for ( PortVectorIterator it = m_Ports.begin();
        it != m_Ports.end();
        ++it )
    {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Setting up port %s\n",(*it)->getName().c_str());
        if(!(*it)->setBufferSize(m_StreamProcessorManager.getPeriodSize())) {
            debugFatal("Could not set buffer size to %d\n",m_StreamProcessorManager.getPeriodSize());
            return false;
        }
    }
    // the API specific settings of the ports should already be set,
    // as this is called from the processorManager->prepare()
    // so we can init the ports
    if(!PortManager::initPorts()) {
        debugFatal("Could not initialize ports\n");
        return false;
    }

    if (!prepareChild()) {
        debugFatal("Could not prepare child\n");
        return false;
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, "Prepared for:\n");
    debugOutput( DEBUG_LEVEL_VERBOSE, " Samplerate: %d\n",
             m_StreamProcessorManager.getNominalRate());
    debugOutput( DEBUG_LEVEL_VERBOSE, " PeriodSize: %d, NbBuffers: %d\n",
             m_StreamProcessorManager.getPeriodSize(), m_StreamProcessorManager.getNbBuffers());
    debugOutput( DEBUG_LEVEL_VERBOSE, " Port: %d, Channel: %d\n",
             m_1394service.getPort(), m_channel);

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
    // wake up any threads that might be waiting on data in the buffers
    // since a state transition can cause data to become available
    SIGNAL_ACTIVITY;
    return true;
}

bool
StreamProcessor::waitForState(enum eProcessorState state, unsigned int timeout_ms)
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "Waiting for state %s\n", ePSToString(state));
    int cnt = timeout_ms;
    while (m_state != state && cnt) {
        SleepRelativeUsec(1000);
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
        tx = addTicks(m_1394service.getCycleTimerTicks(), 200 * TICKS_PER_CYCLE);
    } else {
        tx = t;
    }
    uint64_t start_handler_ticks = substractTicks(tx, 100 * TICKS_PER_CYCLE);

    debugOutput(DEBUG_LEVEL_VERBOSE,"for %s SP (%p)\n", ePTToString(getType()), this);
    #ifdef DEBUG
    uint64_t now = m_1394service.getCycleTimerTicks();
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
    #endif
    if (m_state == ePS_Stopped) {
        if(!m_IsoHandlerManager.startHandlerForStream(
                                        this, TICKS_TO_CYCLES(start_handler_ticks))) {
            debugError("Could not start handler for SP %p\n", this);
            return false;
        }
        return scheduleStateTransition(ePS_WaitingForStream, tx);
    } else if (m_state == ePS_Running) {
        return scheduleStateTransition(ePS_WaitingForStreamDisable, tx);
    } else if (m_state == ePS_DryRunning) {
        debugOutput(DEBUG_LEVEL_VERBOSE, " %p already in DryRunning state\n", this);
        return true;
    } else if (m_state == ePS_WaitingForStreamDisable) {
        debugOutput(DEBUG_LEVEL_VERBOSE, " %p already waiting to switch to DryRunning state\n", this);
        return true;
    } else {
        debugError("Cannot switch to ePS_DryRunning from %s\n", ePSToString(m_state));
        return false;
    }
}

bool StreamProcessor::scheduleStartRunning(int64_t t) {
    uint64_t tx;
    if (t < 0) {
        tx = addTicks(m_1394service.getCycleTimerTicks(), 200 * TICKS_PER_CYCLE);
    } else {
        tx = t;
    }
    debugOutput(DEBUG_LEVEL_VERBOSE,"for %s SP (%p)\n", ePTToString(getType()), this);
    #ifdef DEBUG
    uint64_t now = m_1394service.getCycleTimerTicks();
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
    #endif
    return scheduleStateTransition(ePS_WaitingForStreamEnable, tx);
}

bool StreamProcessor::scheduleStopDryRunning(int64_t t) {
    uint64_t tx;
    if (t < 0) {
        tx = addTicks(m_1394service.getCycleTimerTicks(), 2000 * TICKS_PER_CYCLE);
    } else {
        tx = t;
    }
    debugOutput(DEBUG_LEVEL_VERBOSE,"for %s SP (%p)\n", ePTToString(getType()), this);
    #ifdef DEBUG
    uint64_t now = m_1394service.getCycleTimerTicks();
    debugOutput(DEBUG_LEVEL_VERBOSE,"  Now                   : %011llu (%03us %04uc %04ut)\n",
                          now,
                          (unsigned int)TICKS_TO_SECS(now),
                          (unsigned int)TICKS_TO_CYCLES(now),
                          (unsigned int)TICKS_TO_OFFSET(now));
    debugOutput(DEBUG_LEVEL_VERBOSE,"  Stop at              : %011llu (%03us %04uc %04ut)\n",
                          tx,
                          (unsigned int)TICKS_TO_SECS(tx),
                          (unsigned int)TICKS_TO_CYCLES(tx),
                          (unsigned int)TICKS_TO_OFFSET(tx));
    #endif

    return scheduleStateTransition(ePS_Stopped, tx);
}

bool StreamProcessor::scheduleStopRunning(int64_t t) {
    uint64_t tx;
    if (t < 0) {
        tx = addTicks(m_1394service.getCycleTimerTicks(), 2000 * TICKS_PER_CYCLE);
    } else {
        tx = t;
    }
    debugOutput(DEBUG_LEVEL_VERBOSE,"for %s SP (%p)\n", ePTToString(getType()), this);
    #ifdef DEBUG
    uint64_t now = m_1394service.getCycleTimerTicks();
    debugOutput(DEBUG_LEVEL_VERBOSE,"  Now                   : %011llu (%03us %04uc %04ut)\n",
                          now,
                          (unsigned int)TICKS_TO_SECS(now),
                          (unsigned int)TICKS_TO_CYCLES(now),
                          (unsigned int)TICKS_TO_OFFSET(now));
    debugOutput(DEBUG_LEVEL_VERBOSE,"  Stop at              : %011llu (%03us %04uc %04ut)\n",
                          tx,
                          (unsigned int)TICKS_TO_SECS(tx),
                          (unsigned int)TICKS_TO_CYCLES(tx),
                          (unsigned int)TICKS_TO_OFFSET(tx));
    #endif
    return scheduleStateTransition(ePS_WaitingForStreamDisable, tx);
}

bool StreamProcessor::startDryRunning(int64_t t) {
    if(getState() == ePS_DryRunning) {
        // already in the correct state
        return true;
    }
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
    if(getState() == ePS_Running) {
        // already in the correct state
        return true;
    }
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
    if(getState() == ePS_Stopped) {
        // already in the correct state
        return true;
    }
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
    if(getState() == ePS_DryRunning) {
        // already in the correct state
        return true;
    }
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
    unsigned int ringbuffer_size_frames = (m_StreamProcessorManager.getNbBuffers() + 1) * m_StreamProcessorManager.getPeriodSize();

    debugOutput(DEBUG_LEVEL_VERBOSE, "Enter from state: %s\n", ePSToString(m_state));
    bool result = true;

    switch(m_state) {
        case ePS_Created:
            assert(m_data_buffer);

            // prepare the framerate estimate
            ticks_per_frame = (TICKS_PER_SECOND*1.0) / ((float)m_StreamProcessorManager.getNominalRate());
            m_ticks_per_frame = ticks_per_frame;
            m_local_node_id= m_1394service.getLocalNodeId() & 0x3f;
            m_correct_last_timestamp = false;

            debugOutput(DEBUG_LEVEL_VERBOSE,"Initializing remote ticks/frame to %f\n", ticks_per_frame);

            // initialize internal buffer
            result &= m_data_buffer->setBufferSize(ringbuffer_size_frames);

            result &= m_data_buffer->setEventSize( getEventSize() );
            result &= m_data_buffer->setEventsPerFrame( getEventsPerFrame() );
            if(getType() == ePT_Receive) {
                result &= m_data_buffer->setUpdatePeriod( getNominalFramesPerPacket() );
            } else {
                result &= m_data_buffer->setUpdatePeriod( m_StreamProcessorManager.getPeriodSize() );
            }
            result &= m_data_buffer->setNominalRate(ticks_per_frame);
            result &= m_data_buffer->setWrapValue(128L*TICKS_PER_SECOND);
            result &= m_data_buffer->prepare(); // FIXME: the name

            break;
        case ePS_DryRunning:
            if(!m_IsoHandlerManager.stopHandlerForStream(this)) {
                debugError("Could not stop handler for SP %p\n", this);
                return false;
            }
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
    SIGNAL_ACTIVITY;
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
    SIGNAL_ACTIVITY;
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
            m_local_node_id = m_1394service.getLocalNodeId() & 0x3f;
            if (getType() == ePT_Receive) {
                // this to ensure that there is no discontinuity when starting to 
                // update the DLL based upon the received packets
                m_data_buffer->setBufferTailTimestamp(m_last_timestamp);
            } else {
                // FIXME: PC=master mode will have to do something here I guess...
            }
            break;
        case ePS_WaitingForStreamEnable: // when xrunning at startup
            result &= m_data_buffer->clearBuffer();
            m_data_buffer->setTransparent(true);
            break;
        case ePS_WaitingForStreamDisable:
            result &= m_data_buffer->clearBuffer();
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
    SIGNAL_ACTIVITY;
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

            if(!m_data_buffer->clearBuffer()) {
                debugError("Could not reset data buffer\n");
                return false;
            }
            if (getType() == ePT_Transmit) {
                ringbuffer_size_frames = m_StreamProcessorManager.getNbBuffers() * m_StreamProcessorManager.getPeriodSize();
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
    SIGNAL_ACTIVITY;
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
            m_local_node_id = m_1394service.getLocalNodeId() & 0x3f;
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
    SIGNAL_ACTIVITY;
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
    SIGNAL_ACTIVITY;
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
        if (result) {return true;}
        else goto updateState_exit_change_failed;
    }

    // after initialization, only WaitingForRunningStream is allowed
    if (m_state == ePS_Stopped) {
        if(next_state != ePS_WaitingForStream) {
            goto updateState_exit_with_error;
        }
        result = doWaitForRunningStream();
        if (result) {return true;}
        else goto updateState_exit_change_failed;
    }

    // after WaitingForStream, only ePS_DryRunning is allowed
    // this means that the stream started running
    if (m_state == ePS_WaitingForStream) {
        if(next_state != ePS_DryRunning) {
            goto updateState_exit_with_error;
        }
        result = doDryRunning();
        if (result) {return true;}
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
        if (result) {return true;}
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
        if (result) {return true;}
        else goto updateState_exit_change_failed;
    }

    // from ePS_Running we can only start waiting for a disabled stream
    if (m_state == ePS_Running) {
        if(next_state != ePS_WaitingForStreamDisable) {
            goto updateState_exit_with_error;
        }
        result = doWaitForStreamDisable();
        if (result) {return true;}
        else goto updateState_exit_change_failed;
    }

    // from ePS_WaitingForStreamDisable we can go to DryRunning
    if (m_state == ePS_WaitingForStreamDisable) {
        if(next_state != ePS_DryRunning) {
            goto updateState_exit_with_error;
        }
        result = doDryRunning();
        if (result) {return true;}
        else goto updateState_exit_change_failed;
    }

    // if we arrive here there is an error
updateState_exit_with_error:
    debugError("Invalid state transition: %s => %s\n",
        ePSToString(m_state), ePSToString(next_state));
    SIGNAL_ACTIVITY;
    return false;
updateState_exit_change_failed:
    debugError("State transition failed: %s => %s\n",
        ePSToString(m_state), ePSToString(next_state));
    SIGNAL_ACTIVITY;
    return false;
}

bool StreamProcessor::waitForProducePacket()
{
    return waitForProduce(getNominalFramesPerPacket());
}
bool StreamProcessor::waitForProducePeriod()
{
    return waitForProduce(m_StreamProcessorManager.getPeriodSize());
}
bool StreamProcessor::waitForProduce(unsigned int nframes)
{
    debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                       "(%p, %s) wait ...\n",
                       this, getTypeString());
    struct timespec ts;
    int result;
    int max_runs = 1000;

    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        debugError("clock_gettime failed\n");
        return false;
    }

    // FIXME: hardcoded timeout of 10 sec
//     ts.tv_nsec += 1000 * 1000000LL;
//     while (ts.tv_nsec > 1000000000LL) {
//         ts.tv_sec += 1;
//         ts.tv_nsec -= 1000000000LL;
//     }
    ts.tv_sec += 2;
    
    pthread_mutex_lock(&m_activity_cond_lock);
    while(!canProduce(nframes) && max_runs) {
        result = pthread_cond_timedwait(&m_activity_cond, &m_activity_cond_lock, &ts);
    
        if(result != 0) {
            if (result == ETIMEDOUT) {
                debugOutput(DEBUG_LEVEL_VERBOSE,
                            "(%p, %s) pthread_cond_timedwait() timed out (result=%d)\n",
                            this, getTypeString(), result);
                pthread_mutex_unlock(&m_activity_cond_lock);
                dumpInfo();
                return false;
            } else if (result == EINTR) {
                debugOutput(DEBUG_LEVEL_VERBOSE,
                            "(%p, %s) pthread_cond_timedwait() interrupted by signal (result=%d)\n",
                            this, getTypeString(), result);
                pthread_mutex_unlock(&m_activity_cond_lock);
                dumpInfo();
                return false;
            } else {
                debugError("(%p, %s) pthread_cond_timedwait error (result=%d)\n", 
                            this, getTypeString(), result);
                pthread_mutex_unlock(&m_activity_cond_lock);
                dumpInfo();
                return false;
            }
        }
    }
    pthread_mutex_unlock(&m_activity_cond_lock);
    if(max_runs == 0) {
        debugWarning("(%p) runaway loop\n");
    }
    return true;
}

bool StreamProcessor::waitForConsumePacket()
{
    return waitForConsume(getNominalFramesPerPacket());
}
bool StreamProcessor::waitForConsumePeriod()
{
    return waitForConsume(m_StreamProcessorManager.getPeriodSize());
}
bool StreamProcessor::waitForConsume(unsigned int nframes)
{
    debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                       "(%p, %s) wait ...\n",
                       this, getTypeString());
    struct timespec ts;
    int result;

    int max_runs = 1000;

    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        debugError("clock_gettime failed\n");
        return false;
    }

    // FIXME: hardcoded timeout of 10 sec
//     ts.tv_nsec += 1000 * 1000000LL;
//     while (ts.tv_nsec > 1000000000LL) {
//         ts.tv_sec += 1;
//         ts.tv_nsec -= 1000000000LL;
//     }
    ts.tv_sec += 2;

    pthread_mutex_lock(&m_activity_cond_lock);
    while(!canConsume(nframes) && max_runs) {
        result = pthread_cond_timedwait(&m_activity_cond, &m_activity_cond_lock, &ts);
        if(result != 0) {
            if (result == ETIMEDOUT) {
                debugOutput(DEBUG_LEVEL_VERBOSE,
                            "(%p, %s) pthread_cond_timedwait() timed out (result=%d)\n",
                            this, getTypeString(), result);
                pthread_mutex_unlock(&m_activity_cond_lock);
                dumpInfo();
                return false;
            } else if (result == EINTR) {
                debugOutput(DEBUG_LEVEL_VERBOSE,
                            "(%p, %s) pthread_cond_timedwait() interrupted by signal (result=%d)\n",
                            this, getTypeString(), result);
                pthread_mutex_unlock(&m_activity_cond_lock);
                dumpInfo();
                return false;
            } else {
                debugError("(%p, %s) pthread_cond_timedwait error (result=%d)\n", 
                            this, getTypeString(), result);
                pthread_mutex_unlock(&m_activity_cond_lock);
                dumpInfo();
                return false;
            }
        }
        max_runs--;
    }
    pthread_mutex_unlock(&m_activity_cond_lock);
    
    if(max_runs == 0) {
        debugWarning("(%p) runaway loop\n");
    }
    
    debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                       "(%p, %s) leave ...\n",
                       this, getTypeString());
    return true;
}

bool StreamProcessor::canProducePacket()
{
    return canProduce(getNominalFramesPerPacket());
}
bool StreamProcessor::canProducePeriod()
{
    return canProduce(m_StreamProcessorManager.getPeriodSize());
}
bool StreamProcessor::canProduce(unsigned int nframes)
{
    if(m_in_xrun) return true;
    if(m_state == ePS_Running && m_next_state == ePS_Running) {
        
        if(getType() == ePT_Transmit) {
            // can we put a certain amount of frames into the buffer?
            unsigned int bufferspace = m_data_buffer->getBufferSpace();
            if(bufferspace >= nframes) {
                return true;
            } else return false;
        } else {
            // do we still have to put frames in the buffer?
            unsigned int bufferfill = m_data_buffer->getBufferFill();
            unsigned int periodsize = m_StreamProcessorManager.getPeriodSize();
            if (bufferfill > periodsize) return false;
            else return true;
        }


    } else {
        if(getType() == ePT_Transmit) {
            // if we are an xmit SP, we cannot accept frames 
            // when not running
            return false;
        } else {
            // if we are a receive SP, we can always accept frames
            // when not running
            return true;
        }
    }
}

bool StreamProcessor::canConsumePacket()
{
    return canConsume(getNominalFramesPerPacket());
}
bool StreamProcessor::canConsumePeriod()
{
    return canConsume(m_StreamProcessorManager.getPeriodSize());
}
bool StreamProcessor::canConsume(unsigned int nframes)
{
    if(m_in_xrun) return true;
    if(m_state == ePS_Running && m_next_state == ePS_Running) {
        // check whether we already fullfil the criterion
        unsigned int bufferfill = m_data_buffer->getBufferFill();
        if(bufferfill >= nframes) {
            return true;
        } else return false;
    } else {
        if(getType() == ePT_Transmit) {
            // if we are an xmit SP, and we're not running,
            // we can always provide frames
            return true;
        } else {
            // if we are a receive SP, we can't provide frames
            // when not running
            return false;
        }
    }
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
    char *dummybuffer = (char *)calloc(getEventSize(), nframes * getEventsPerFrame());
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
    #ifdef DEBUG
    debugOutputShort( DEBUG_LEVEL_NORMAL, " StreamProcessor %p, %s:\n", this, ePTToString(m_processor_type));
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Port, Channel  : %d, %d\n", m_1394service.getPort(), m_channel);
    uint64_t now = m_1394service.getCycleTimerTicks();
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Now                   : %011llu (%03us %04uc %04ut)\n",
                        now,
                        (unsigned int)TICKS_TO_SECS(now),
                        (unsigned int)TICKS_TO_CYCLES(now),
                        (unsigned int)TICKS_TO_OFFSET(now));
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Xrun?                 : %s\n", (m_in_xrun ? "True":"False"));
    if (m_state == m_next_state) {
        debugOutputShort( DEBUG_LEVEL_NORMAL, "  State                 : %s\n", 
                                            ePSToString(m_state));
    } else {
        debugOutputShort( DEBUG_LEVEL_NORMAL, "  State                 : %s (Next: %s)\n", 
                                              ePSToString(m_state), ePSToString(m_next_state));
        debugOutputShort( DEBUG_LEVEL_NORMAL, "    transition at       : %u\n", m_cycle_to_switch_state);
    }
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Buffer                : %p\n", m_data_buffer);
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Framerate             : Nominal: %u, Sync: %f, Buffer %f\n",
                                          m_StreamProcessorManager.getNominalRate(),
                                          24576000.0/m_StreamProcessorManager.getSyncSource().m_data_buffer->getRate(),
                                          24576000.0/m_data_buffer->getRate());
    float d = getSyncDelay();
    debugOutputShort(DEBUG_LEVEL_NORMAL, "  Sync delay             : %f ticks (%f frames, %f cy)\n",
                                         d, d/getTicksPerFrame(),
                                         d/((float)TICKS_PER_CYCLE));
    #endif
    m_data_buffer->dumpInfo();
}

void
StreamProcessor::printBufferInfo()
{
    debugOutput(DEBUG_LEVEL_NORMAL,
                "(%p, %8s) fc: %d fill: %u\n",
                this, getTypeString(), m_data_buffer->getFrameCounter(), m_data_buffer->getBufferFill() );
}

void
StreamProcessor::setVerboseLevel(int l) {
    setDebugLevel(l);
    PortManager::setVerboseLevel(l);
    m_data_buffer->setVerboseLevel(l);
}

} // end of namespace
