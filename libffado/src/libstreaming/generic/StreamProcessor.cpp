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

#define SIGNAL_ACTIVITY_SPM { \
    m_StreamProcessorManager.signalActivity(); \
}
#define SIGNAL_ACTIVITY_ISO_XMIT { \
    m_IsoHandlerManager.signalActivityTransmit(); \
}
#define SIGNAL_ACTIVITY_ISO_RECV { \
    m_IsoHandlerManager.signalActivityReceive(); \
}
#define SIGNAL_ACTIVITY_ALL { \
    m_StreamProcessorManager.signalActivity(); \
    m_IsoHandlerManager.signalActivityTransmit(); \
    m_IsoHandlerManager.signalActivityReceive(); \
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
    , m_last_timestamp( 0 )
    , m_last_timestamp2( 0 )
    , m_correct_last_timestamp( false )
    , m_scratch_buffer( NULL )
    , m_scratch_buffer_size_bytes( 0 )
    , m_ticks_per_frame( 0 )
    , m_dll_bandwidth_hz ( STREAMPROCESSOR_DLL_BW_HZ )
    , m_extra_buffer_frames( 0 )
    , m_max_fs_diff_norm ( 0.01 )
    , m_max_diff_ticks ( 50 )
    , m_in_xrun( false )
{
    // create the timestamped buffer and register ourselves as its client
    m_data_buffer = new Util::TimestampedBuffer(this);
}

StreamProcessor::~StreamProcessor() {
    m_StreamProcessorManager.unregisterProcessor(this);
    if(!m_IsoHandlerManager.unregisterStream(this)) {
        debugOutput(DEBUG_LEVEL_VERBOSE,"Could not unregister stream processor with the Iso manager\n");
    }

    if (m_data_buffer) delete m_data_buffer;
    if (m_scratch_buffer) delete[] m_scratch_buffer;
}

bool
StreamProcessor::periodSizeChanged(unsigned int new_periodsize) {
    // This is called by the StreamProcessorManager whenever the period size
    // is changed via setPeriodSize().  If the stream processor needs to do
    // anything in response it can be done in this method.  Buffer size
    // changes should only ever be made when streaming is not active.
    //
    // Return false if there was a problem dealing with the resize.
    if (m_state!=ePS_Stopped && m_state!=ePS_Created) {
        debugOutput(DEBUG_LEVEL_WARNING, "(%p) period change should only be done with streaming stopped\n", this);
        return false;
    }

    // make the scratch buffer one period of frames long
    m_scratch_buffer_size_bytes = new_periodsize * getEventsPerFrame() * getEventSize();
    debugOutput( DEBUG_LEVEL_VERBOSE, " Allocate scratch buffer of %zd quadlets\n", m_scratch_buffer_size_bytes);
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

    if (!setupDataBuffer()) {
        debugFatal("Could not setup data buffer\n");
        return false;
    }

    return updateState();
}

bool
StreamProcessor::handleBusResetDo()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "(%p) handling busreset\n", this);
    m_state = ePS_Error;
    // this will result in the SPM dying
    m_in_xrun = true;
    SIGNAL_ACTIVITY_ALL;
    return true;
}

bool
StreamProcessor::handleBusReset()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "(%p) handling busreset\n", this);

    // we are sure that we're not iterated since this is called from within the ISO manager thread

    // lock the wait loop of the SPM, such that the client leaves us alone
    m_StreamProcessorManager.lockWaitLoop();

    // pass on to the implementing classes
    bool retval = handleBusResetDo();

    // resume wait loop
    m_StreamProcessorManager.unlockWaitLoop();

    return retval;
}

void StreamProcessor::handlerDied()
{
    debugWarning("Handler died for %p\n", this);
    m_state = ePS_Stopped;
    m_in_xrun = true;
    SIGNAL_ACTIVITY_ALL;
}

int StreamProcessor::getMaxFrameLatency() {
    return (int)(m_IsoHandlerManager.getPacketLatencyForStream( this ) * TICKS_PER_CYCLE);
}

unsigned int
StreamProcessor::getNominalPacketsNeeded(unsigned int nframes)
{
    unsigned int nominal_frames_per_second
                    = m_StreamProcessorManager.getNominalRate();
    uint64_t nominal_ticks_per_frame = TICKS_PER_SECOND / nominal_frames_per_second;
    uint64_t nominal_ticks = nominal_ticks_per_frame * nframes;
    // ensure proper ceiling
    uint64_t nominal_packets = (nominal_ticks+TICKS_PER_CYCLE-1) / TICKS_PER_CYCLE;
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
bool
StreamProcessor::setupDataBuffer() {
    assert(m_data_buffer);

    unsigned int ringbuffer_size_frames = m_StreamProcessorManager.getNbBuffers() * m_StreamProcessorManager.getPeriodSize();
    ringbuffer_size_frames += m_extra_buffer_frames;
    ringbuffer_size_frames += 1; // to ensure that we can fit it all in there

    bool result = true;

    m_correct_last_timestamp = false;
        
    // initialize internal buffer
    result &= m_data_buffer->setBufferSize(ringbuffer_size_frames);

    result &= m_data_buffer->setEventSize( getEventSize() );
    result &= m_data_buffer->setEventsPerFrame( getEventsPerFrame() );
    if(getType() == ePT_Receive) {
        result &= m_data_buffer->setUpdatePeriod( getNominalFramesPerPacket() );
    } else {
        result &= m_data_buffer->setUpdatePeriod( m_StreamProcessorManager.getPeriodSize() );
    }
    // Completing the buffer's setup and calling the prepare() method is not
    // applicable unless the nominal rate (m_ticks_per_frame) has been
    // configured.  This may not be the case when setupDataBuffer() is
    // called from prepare() via periodSizeChanged(), but will be when called
    // from doStop() and from periodSizeChanged() via the stream processor's
    // setPeriodSize() method.
    if (m_ticks_per_frame > 0) {
        result &= m_data_buffer->setNominalRate(m_ticks_per_frame);
        result &= m_data_buffer->setWrapValue(128L * TICKS_PER_SECOND);
        result &= m_data_buffer->setBandwidth(STREAMPROCESSOR_DLL_FAST_BW_HZ / (double)TICKS_PER_SECOND);

        result &= m_data_buffer->prepare(); // FIXME: the name

        debugOutput(DEBUG_LEVEL_VERBOSE, "DLL info: nominal tpf: %f, update period: %d, bandwidth: %e 1/ticks (%e Hz)\n", 
                    m_data_buffer->getNominalRate(), m_data_buffer->getUpdatePeriod(), m_data_buffer->getBandwidth(), m_data_buffer->getBandwidth() * TICKS_PER_SECOND);
    }

    return result;
}

void
StreamProcessor::getBufferHeadTimestamp(ffado_timestamp_t *ts, signed int *fc)
{
    m_data_buffer->getBufferHeadTimestamp(ts, fc);
}

void
StreamProcessor::getBufferTailTimestamp(ffado_timestamp_t *ts, signed int *fc)
{
    m_data_buffer->getBufferTailTimestamp(ts, fc);
}

void StreamProcessor::setBufferTailTimestamp(ffado_timestamp_t new_timestamp)
{
    m_data_buffer->setBufferTailTimestamp(new_timestamp);
}

void
StreamProcessor::setBufferHeadTimestamp(ffado_timestamp_t new_timestamp)
{
    m_data_buffer->setBufferHeadTimestamp(new_timestamp);
}

int StreamProcessor::getBufferFill() {
    return m_data_buffer->getBufferFill();
}

void
StreamProcessor::setExtraBufferFrames(unsigned int frames) {
    debugOutput(DEBUG_LEVEL_VERBOSE, "Setting extra buffer to %d frames\n", frames);
    m_extra_buffer_frames = frames;
}

unsigned int
StreamProcessor::getExtraBufferFrames() {
    return m_extra_buffer_frames;
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

void
StreamProcessor::setTicksPerFrame(float tpf)
{
    assert(m_data_buffer != NULL);
    m_data_buffer->setRate(tpf);
}

bool
StreamProcessor::setDllBandwidth(float bw)
{
    m_dll_bandwidth_hz = bw;
    return true;
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
                           uint32_t pkt_ctr,
                           unsigned int dropped_cycles) {
    // bypass based upon state
#ifdef DEBUG
    if (m_state == ePS_Invalid) {
        debugError("Should not have state %s\n", ePSToString(m_state) );
        return RAW1394_ISO_ERROR;
    }
#endif
    // FIXME: isn't this also an error condition?
    if (m_state == ePS_Created) {
        return RAW1394_ISO_DEFER;
    }

    if (m_state == ePS_Error) {
        debugOutputExtreme(DEBUG_LEVEL_VERBOSE, "skip due to error state\n");
        return RAW1394_ISO_OK;
    }

    // store the previous timestamp
    m_last_timestamp2 = m_last_timestamp;

    // NOTE: synchronized switching is restricted to a 0.5 sec span (4000 cycles)
    //       it happens on the first 'good' cycle for the wait condition
    //       or on the first received cycle that is received afterwards (might be a problem)

    // check whether we are waiting for a stream to be disabled
    if(m_state == ePS_WaitingForStreamDisable) {
        // we then check whether we have to switch on this cycle
        if (diffCycles(CYCLE_TIMER_GET_CYCLES(pkt_ctr), m_cycle_to_switch_state) >= 0) {
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
    else if(m_state == ePS_WaitingForStreamEnable
            && m_next_state == ePS_WaitingForStreamEnable) {
        // we then check whether we have to switch on this cycle
        if (diffCycles(CYCLE_TIMER_GET_CYCLES(pkt_ctr), m_cycle_to_switch_state) >= 0) {
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
    enum eChildReturnValue result = processPacketHeader(data, length, tag, sy, pkt_ctr);

    // handle dropped cycles
    if(dropped_cycles) {
        // make sure the last_timestamp is corrected
        m_correct_last_timestamp = true;
        if (m_state == ePS_Running) {
            // this is an xrun situation
            m_in_xrun = true;
            debugOutput(DEBUG_LEVEL_NORMAL, "Should update state to WaitingForStreamDisable due to dropped packet xrun\n");
            m_cycle_to_switch_state = CYCLE_TIMER_GET_CYCLES(pkt_ctr) + 1; // switch in the next cycle
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
        if (m_last_timestamp > 0 && m_last_timestamp2 > 0) {
            int64_t tsp_diff = diffTicks(m_last_timestamp, m_last_timestamp2);
            debugOutputExtreme(DEBUG_LEVEL_VERBOSE, "TSP diff: %"PRId64"\n", tsp_diff);
            double tsp_diff_d = tsp_diff;
            double fs_syt = 1.0/tsp_diff_d;
            fs_syt *= (double)getNominalFramesPerPacket() * (double)TICKS_PER_USEC * 1e6;
            double fs_nom = (double)m_StreamProcessorManager.getNominalRate();
            double fs_diff = fs_nom - fs_syt;
            double fs_diff_norm = fs_diff/fs_nom;
            debugOutputExtreme(DEBUG_LEVEL_VERBOSE, "Nom fs: %12f, Instantanous fs: %12f, diff: %12f (%12f)\n",
                        fs_nom, fs_syt, fs_diff, fs_diff_norm);
            if (fs_diff_norm > m_max_fs_diff_norm || fs_diff_norm < -m_max_fs_diff_norm) {
                debugWarning( "Instantanous samplerate more than %0.0f%% off nominal. [Nom fs: %12f, Instantanous fs: %12f, diff: %12f (%12f)]\n",
                        m_max_fs_diff_norm*100,
                        fs_nom, fs_syt, fs_diff, fs_diff_norm);
            }

            int ticks_per_packet = (int)(getTicksPerFrame() * getNominalFramesPerPacket());
            int diff = diffTicks(m_last_timestamp, m_last_timestamp2);
                // display message if the difference between two successive tick
                // values is more than 50 ticks. 1 sample at 48k is 512 ticks
                // so 50 ticks = 10%, which is a rather large jitter value.
                if(diff-ticks_per_packet > m_max_diff_ticks || diff-ticks_per_packet < -m_max_diff_ticks) {
                    debugOutput(DEBUG_LEVEL_VERBOSE,
                                "cy %04d rather large TSP difference TS=%011"PRIu64" => TS=%011"PRIu64" (%d, nom %d)\n",
                                (int)CYCLE_TIMER_GET_CYCLES(pkt_ctr), m_last_timestamp2,
                                m_last_timestamp, diff, ticks_per_packet);
                    // !!!HACK!!! FIXME: this is the result of a failure in wrapping/unwrapping somewhere
                    // it's definitely a bug.
                    // try to fix up the timestamp
                    int64_t last_timestamp_fixed;
                    // first try to add one second
                    last_timestamp_fixed = addTicks(m_last_timestamp, TICKS_PER_SECOND);
                    diff = diffTicks(last_timestamp_fixed, m_last_timestamp2);
                    if(diff-ticks_per_packet < 50 && diff-ticks_per_packet > -50) {
                        debugWarning("cy %04d rather large TSP difference TS=%011"PRIu64" => TS=%011"PRIu64" (%d, nom %d)\n",
                                    (int)CYCLE_TIMER_GET_CYCLES(pkt_ctr), m_last_timestamp2,
                                    m_last_timestamp, diff, ticks_per_packet);
                        debugWarning("HACK: fixed by adding one second of ticks. This is a bug being run-time fixed.\n");
                        m_last_timestamp = last_timestamp_fixed;
                    } else {
                        // if that didn't work, try to subtract one second
                        last_timestamp_fixed = substractTicks(m_last_timestamp, TICKS_PER_SECOND);
                        diff = diffTicks(last_timestamp_fixed, m_last_timestamp2);
                        if(diff-ticks_per_packet < 50 && diff-ticks_per_packet > -50) {
                            debugWarning("cy %04d rather large TSP difference TS=%011"PRIu64" => TS=%011"PRIu64" (%d, nom %d)\n",
                                        (int)CYCLE_TIMER_GET_CYCLES(pkt_ctr), m_last_timestamp2,
                                        m_last_timestamp, diff, ticks_per_packet);
                            debugWarning("HACK: fixed by subtracing one second of ticks. This is a bug being run-time fixed.\n");
                            m_last_timestamp = last_timestamp_fixed;
                        }
                    }
                }
                debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                                "%04u %011"PRIu64" %011"PRIu64" %d %d\n",
                                (int)CYCLE_TIMER_GET_CYCLES(pkt_ctr),
                                m_last_timestamp2, m_last_timestamp, 
                                diff, ticks_per_packet);
        }
        #endif

        debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                          "RECV: CY=%04u TS=%011"PRIu64"\n",
                          (int)CYCLE_TIMER_GET_CYCLES(pkt_ctr),
                          m_last_timestamp);

        if(m_correct_last_timestamp) {
            // they represent a discontinuity in the timestamps, and hence are
            // to be dealt with
            debugOutput(DEBUG_LEVEL_NORMAL, "(%p) Correcting timestamp for dropped cycles, discarding packet...\n", this);
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
            if (diffCycles(CYCLE_TIMER_GET_CYCLES(pkt_ctr), m_cycle_to_switch_state) >= 0) {
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

            // don't process the data when waiting for a stream
            if(m_state == ePS_WaitingForStream) {
                return RAW1394_ISO_OK;
            }
        }

        // for all states that reach this we are allowed to
        // do protocol specific data reception
        enum eChildReturnValue result2 = processPacketData(data, length);

        // if an xrun occured, switch to the dryRunning state and
        // allow for the xrun to be picked up
        if (result2 == eCRV_XRun) {
            debugOutput(DEBUG_LEVEL_NORMAL, "processPacketData xrun\n");
            m_in_xrun = true;
            debugOutput(DEBUG_LEVEL_VERBOSE, "Should update state to WaitingForStreamDisable due to data xrun\n");
            m_cycle_to_switch_state = CYCLE_TIMER_GET_CYCLES(pkt_ctr)+1; // switch in the next cycle
            m_next_state = ePS_WaitingForStreamDisable;
            // execute the requested change
            if (!updateState()) { // we are allowed to change the state directly
                debugError("Could not update state!\n");
                return RAW1394_ISO_ERROR;
            }
            return RAW1394_ISO_DEFER;
        } else if(result2 == eCRV_OK) {
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
                           uint32_t pkt_ctr, unsigned int dropped_cycles,
                           unsigned int skipped, unsigned int max_length) {
    if (pkt_ctr == 0xFFFFFFFF) {
        *tag = 0;
        *sy = 0;
        *length = 0;
        return RAW1394_ISO_OK;
    }

    if (m_state == ePS_Error) {
        debugOutputExtreme(DEBUG_LEVEL_VERBOSE, "skip due to error state\n");
        return RAW1394_ISO_OK;
    }

    uint64_t prev_timestamp;
    // note that we can ignore skipped cycles since
    // the protocol will take care of that
    if (dropped_cycles > 0) {
        // HACK: this should not be necessary, since the header generation functions should trigger the xrun.
        //       but apparently there are some issues with the 1394 stack
        m_in_xrun = true;
        if(m_state == ePS_Running) {
            debugShowBackLogLines(200);
            debugOutput(DEBUG_LEVEL_NORMAL, "dropped packets xrun (%u)\n", dropped_cycles);
            debugOutput(DEBUG_LEVEL_VERBOSE, "Should update state to WaitingForStreamDisable due to dropped packets xrun\n");
            m_cycle_to_switch_state = CYCLE_TIMER_GET_CYCLES(pkt_ctr) + 1;
            m_next_state = ePS_WaitingForStreamDisable;
            // execute the requested change
            if (!updateState()) { // we are allowed to change the state directly
                debugError("Could not update state!\n");
                return RAW1394_ISO_ERROR;
            }
            goto send_empty_packet;
        }
    }

#ifdef DEBUG
    // bypass based upon state
    if (m_state == ePS_Invalid) {
        debugError("Should not have state %s\n", ePSToString(m_state) );
        return RAW1394_ISO_ERROR;
    }
#endif

    // FIXME: can this happen?
    if (m_state == ePS_Created) {
        *tag = 0;
        *sy = 0;
        *length = 0;
        return RAW1394_ISO_DEFER;
    }

    // normal processing

    // store the previous timestamp
    // keep the old value here, update m_last_timestamp2 only when
    // a valid packet will be sent
    prev_timestamp = m_last_timestamp;

    // NOTE: synchronized switching is restricted to a 0.5 sec span (4000 cycles)
    //       it happens on the first 'good' cycle for the wait condition
    //       or on the first received cycle that is received afterwards (might be a problem)

    // check whether we are waiting for a stream to be disabled
    if(m_state == ePS_WaitingForStreamDisable) {
        // we then check whether we have to switch on this cycle
        if (diffCycles(CYCLE_TIMER_GET_CYCLES(pkt_ctr), m_cycle_to_switch_state) >= 0) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Should update state to DryRunning\n");
            m_next_state = ePS_DryRunning;
            if (!updateState()) { // we are allowed to change the state directly
                debugError("Could not update state!\n");
                return RAW1394_ISO_ERROR;
            }
        }
        // generate the silent packet header
        enum eChildReturnValue result = generateSilentPacketHeader(data, length, tag, sy, pkt_ctr);
        if (result == eCRV_Packet) {
            debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                               "XMIT SILENT: CY=%04u TS=%011"PRIu64"\n",
                               (int)CYCLE_TIMER_GET_CYCLES(pkt_ctr), m_last_timestamp);

            // assumed not to xrun
            generateSilentPacketData(data, length);
            return RAW1394_ISO_OK;
        // FIXME: PP: I think this [empty packet] should also be a possibility
        // JMW: yes, it should.  MOTU needs it for a clean shutdown.
        } else if (result == eCRV_EmptyPacket) {
            goto send_empty_packet;
        } else {
            debugError("Invalid return value: %d\n", result);
            return RAW1394_ISO_ERROR;
        }
    }
    // check whether we are waiting for a stream to be enabled
    else if(m_state == ePS_WaitingForStreamEnable
            && m_next_state == ePS_WaitingForStreamEnable) {
        // we then check whether we have to switch on this cycle
        if (diffCycles(CYCLE_TIMER_GET_CYCLES(pkt_ctr), m_cycle_to_switch_state) >= 0) {
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
        // check whether we have to switch on this cycle
        if ((diffCycles(CYCLE_TIMER_GET_CYCLES(pkt_ctr), m_cycle_to_switch_state) >= 0)) {
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
        enum eChildReturnValue result = generatePacketHeader(data, length, tag, sy, pkt_ctr);
        if (result == eCRV_Packet || result == eCRV_Defer) {
            debugOutputExtreme(DEBUG_LEVEL_VERBOSE,
                               "XMIT: CY=%04u TS=%011"PRIu64"\n",
                               (int)CYCLE_TIMER_GET_CYCLES(pkt_ctr), m_last_timestamp);

            // valid packet timestamp
            m_last_timestamp2 = prev_timestamp;

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

            enum eChildReturnValue result2 = generatePacketData(data, length);
            // if an xrun occured, switch to the dryRunning state and
            // allow for the xrun to be picked up
            if (result2 == eCRV_XRun) {
                debugOutput(DEBUG_LEVEL_NORMAL, "generatePacketData xrun\n");
                m_in_xrun = true;
                debugOutput(DEBUG_LEVEL_VERBOSE, "Should update state to WaitingForStreamDisable due to data xrun\n");
                m_cycle_to_switch_state = CYCLE_TIMER_GET_CYCLES(pkt_ctr) + 1; // switch in the next cycle
                m_next_state = ePS_WaitingForStreamDisable;
                // execute the requested change
                if (!updateState()) { // we are allowed to change the state directly
                    debugError("Could not update state!\n");
                    return RAW1394_ISO_ERROR;
                }
                goto send_empty_packet;
            }
            #ifdef DEBUG
            if (m_last_timestamp > 0 && m_last_timestamp2 > 0) {
                int64_t tsp_diff = diffTicks(m_last_timestamp, m_last_timestamp2);
                debugOutputExtreme(DEBUG_LEVEL_VERBOSE, "TSP diff: %"PRId64"\n", tsp_diff);
                double tsp_diff_d = tsp_diff;
                double fs_syt = 1.0/tsp_diff_d;
                fs_syt *= (double)getNominalFramesPerPacket() * (double)TICKS_PER_USEC * 1e6;
                double fs_nom = (double)m_StreamProcessorManager.getNominalRate();
                double fs_diff = fs_nom - fs_syt;
                double fs_diff_norm = fs_diff/fs_nom;
                debugOutputExtreme(DEBUG_LEVEL_VERBOSE, "Nom fs: %12f, Instantanous fs: %12f, diff: %12f (%12f)\n",
                           fs_nom, fs_syt, fs_diff, fs_diff_norm);
//                 debugOutput(DEBUG_LEVEL_VERBOSE, "Diff fs: %12f, m_last_timestamp: %011"PRIu64", m_last_timestamp2: %011"PRIu64"\n",
//                            fs_diff, m_last_timestamp, m_last_timestamp2);
                if (fs_diff_norm > m_max_fs_diff_norm || fs_diff_norm < -m_max_fs_diff_norm) {
                    debugWarning( "Instantanous samplerate more than %0.0f%% off nominal. [Nom fs: %12f, Instantanous fs: %12f, diff: %12f (%12f)]\n",
                           m_max_fs_diff_norm*100,
                           fs_nom, fs_syt, fs_diff, fs_diff_norm);
                }
                int ticks_per_packet = (int)(getTicksPerFrame() * getNominalFramesPerPacket());
                int diff = diffTicks(m_last_timestamp, m_last_timestamp2);
                // display message if the difference between two successive tick
                // values is more than 50 ticks. 1 sample at 48k is 512 ticks
                // so 50 ticks = 10%, which is a rather large jitter value.
                if(diff-ticks_per_packet > m_max_diff_ticks || diff-ticks_per_packet < -m_max_diff_ticks) {
                    debugOutput(DEBUG_LEVEL_VERBOSE,
                                "cy %04d, rather large TSP difference TS=%011"PRIu64" => TS=%011"PRIu64" (%d, nom %d)\n",
                                (int)CYCLE_TIMER_GET_CYCLES(pkt_ctr), m_last_timestamp2,
                                m_last_timestamp, diff, ticks_per_packet);
                }
                debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                                "%04d %011"PRIu64" %011"PRIu64" %d %d\n",
                                (int)CYCLE_TIMER_GET_CYCLES(pkt_ctr), m_last_timestamp2,
                                m_last_timestamp, diff, ticks_per_packet);
            }
            #endif

            // skip queueing packets if we detect that there are not enough frames
            // available
            if(result2 == eCRV_Defer || result == eCRV_Defer) {
                return RAW1394_ISO_DEFER;
            } else {
                return RAW1394_ISO_OK;
            }
        } else if (result == eCRV_XRun) { // pick up the possible xruns
            debugOutput(DEBUG_LEVEL_NORMAL, "generatePacketHeader xrun\n");
            m_in_xrun = true;
            debugOutput(DEBUG_LEVEL_VERBOSE, "Should update state to WaitingForStreamDisable due to header xrun\n");
            m_cycle_to_switch_state = CYCLE_TIMER_GET_CYCLES(pkt_ctr) + 1; // switch in the next cycle
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
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "have to retry cycle %d\n", (int)CYCLE_TIMER_GET_CYCLES(pkt_ctr));
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
            generateEmptyPacketHeader(data, length, tag, sy, pkt_ctr);
            generateEmptyPacketData(data, length);
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
        debugOutput(DEBUG_LEVEL_VERBOSE, "Should update '%s' state from %s to %s\n",
                                        getTypeString(), ePSToString(m_state), ePSToString(m_next_state));
        // execute the requested change
        if (!updateState()) { // we are allowed to change the state directly
            debugError("Could not update state!\n");
            return RAW1394_ISO_ERROR;
        }
    }
    debugOutputExtreme(DEBUG_LEVEL_VERBOSE,
                       "XMIT EMPTY: CY=%04u\n",
                       (int)CYCLE_TIMER_GET_CYCLES(pkt_ctr));

    generateEmptyPacketHeader(data, length, tag, sy, pkt_ctr);
    generateEmptyPacketData(data, length);
    return RAW1394_ISO_OK;
}

void StreamProcessor::packetsStopped() {
    m_state = ePS_Stopped;
    m_next_state = ePS_Stopped;
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
    debugOutputExtreme( DEBUG_LEVEL_VERBOSE,
                        "(%p, %s) getFrames(%d, %11"PRIu64")\n",
                        this, getTypeString(), nbframes, ts);
    assert( getType() == ePT_Receive );
    if(isDryRunning()) result = getFramesDry(nbframes, ts);
    else result = getFramesWet(nbframes, ts);
    SIGNAL_ACTIVITY_ISO_RECV;
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
                       "stream (%p): drifts %6d ticks = %10.5f frames (rate=%10.5f), %"PRId64", %"PRIu64", %d\n",
                       this, lag_ticks, lag_frames, srate, ts, ts_expected, fc);
    if (lag_frames >= 1.0) {
        // the stream lags
        debugOutput(DEBUG_LEVEL_VERBOSE, "stream (%p): lags  with %6d ticks = %10.5f frames (rate=%10.5f), %"PRId64", %"PRIu64", %d\n",
                      this, lag_ticks, lag_frames, srate, ts, ts_expected, fc);
    } else if (lag_frames <= -1.0) {
        // the stream leads
        debugOutput(DEBUG_LEVEL_VERBOSE, "stream (%p): leads with %6d ticks = %10.5f frames (rate=%10.5f), %"PRId64", %"PRIu64", %d\n",
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
                       "stream (%p): dry run %d frames (@ ts=%"PRId64")\n",
                       this, nbframes, ts);
    // dry run on this side means that we put silence in all enabled ports
    // since there is do data put into the ringbuffer in the dry-running state
    return provideSilenceBlock(nbframes, 0);
}

bool
StreamProcessor::dropFrames(unsigned int nbframes, int64_t ts)
{
    bool result;
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "StreamProcessor::dropFrames(%d, %"PRId64")\n", nbframes, ts);
    result = m_data_buffer->dropFrames(nbframes);
    SIGNAL_ACTIVITY_ISO_RECV;
    return result;
}

bool StreamProcessor::putFrames(unsigned int nbframes, int64_t ts)
{
    bool result;
    debugOutputExtreme( DEBUG_LEVEL_VERBOSE,
                        "(%p, %s) putFrames(%d, %11"PRIu64")\n",
                        this, getTypeString(), nbframes, ts);
    assert( getType() == ePT_Transmit );
    if(isDryRunning()) result = putFramesDry(nbframes, ts);
    else result = putFramesWet(nbframes, ts);
    SIGNAL_ACTIVITY_ISO_XMIT;
    return result;
}

bool
StreamProcessor::putFramesWet(unsigned int nbframes, int64_t ts)
{
    debugOutputExtreme(DEBUG_LEVEL_ULTRA_VERBOSE,
                       "StreamProcessor::putFramesWet(%d, %"PRIu64")\n",
                       nbframes, ts);
    // transfer the data
    m_data_buffer->blockProcessWriteFrames(nbframes, ts);
    debugOutputExtreme(DEBUG_LEVEL_ULTRA_VERBOSE,
                       " New timestamp: %"PRIu64"\n", ts);
    return true; // FIXME: what about failure?
}

bool
StreamProcessor::putFramesDry(unsigned int nbframes, int64_t ts)
{
    debugOutputExtreme(DEBUG_LEVEL_ULTRA_VERBOSE,
                       "StreamProcessor::putFramesDry(%d, %"PRIu64")\n",
                       nbframes, ts);
    // do nothing
    return true;
}

bool
StreamProcessor::putSilenceFrames(unsigned int nbframes, int64_t ts)
{
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
                       "StreamProcessor::putSilenceFrames(%d, %"PRIu64")\n",
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

    SIGNAL_ACTIVITY_ISO_XMIT;
    return true;
}

bool
StreamProcessor::shiftStream(int nbframes)
{
    // FIXME: this is not a good idea since the ISO thread is also writing the buffer
    // resulting in multiple writers (not supported)
    bool result;
    if(nbframes == 0) return true;
    if(nbframes > 0) {
        debugOutput(DEBUG_LEVEL_VERBOSE,
                    "(%p) dropping %d frames\n",
                    this, nbframes);
        result = m_data_buffer->dropFrames(nbframes);
    } else {
        result = false;
        return result;
        debugOutput(DEBUG_LEVEL_VERBOSE,
                    "(%p) adding %d frames\n",
                    this, nbframes);
        while(nbframes++) {
            result &= m_data_buffer->writeDummyFrame();
        }
    }
    SIGNAL_ACTIVITY_ALL;
    return result;
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

    if (periodSizeChanged(m_StreamProcessorManager.getPeriodSize()) == false)
        return false;

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
    debugOutput( DEBUG_LEVEL_VERBOSE, " Samplerate: %d  [DLL Bandwidth: %f Hz]\n",
             m_StreamProcessorManager.getNominalRate(), m_dll_bandwidth_hz);
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
    SIGNAL_ACTIVITY_ALL;
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
    debugOutput(DEBUG_LEVEL_VERBOSE,"  Now                   : %011"PRIu64" (%03us %04uc %04ut)\n",
                          now,
                          (unsigned int)TICKS_TO_SECS(now),
                          (unsigned int)TICKS_TO_CYCLES(now),
                          (unsigned int)TICKS_TO_OFFSET(now));
    debugOutput(DEBUG_LEVEL_VERBOSE,"  Start at              : %011"PRIu64" (%03us %04uc %04ut)\n",
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
    } else if (m_state == ePS_WaitingForStreamEnable) {
        debugOutput(DEBUG_LEVEL_VERBOSE, " %p still waiting to switch to Running state\n", this);
        // this will happen immediately
        return scheduleStateTransition(ePS_DryRunning, tx);
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
    debugOutput(DEBUG_LEVEL_VERBOSE,"  Now                   : %011"PRIu64" (%03us %04uc %04ut)\n",
                          now,
                          (unsigned int)TICKS_TO_SECS(now),
                          (unsigned int)TICKS_TO_CYCLES(now),
                          (unsigned int)TICKS_TO_OFFSET(now));
    debugOutput(DEBUG_LEVEL_VERBOSE,"  Start at              : %011"PRIu64" (%03us %04uc %04ut)\n",
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
    debugOutput(DEBUG_LEVEL_VERBOSE,"  Now                   : %011"PRIu64" (%03us %04uc %04ut)\n",
                          now,
                          (unsigned int)TICKS_TO_SECS(now),
                          (unsigned int)TICKS_TO_CYCLES(now),
                          (unsigned int)TICKS_TO_OFFSET(now));
    debugOutput(DEBUG_LEVEL_VERBOSE,"  Stop at              : %011"PRIu64" (%03us %04uc %04ut)\n",
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
    debugOutput(DEBUG_LEVEL_VERBOSE,"  Now                   : %011"PRIu64" (%03us %04uc %04ut)\n",
                          now,
                          (unsigned int)TICKS_TO_SECS(now),
                          (unsigned int)TICKS_TO_CYCLES(now),
                          (unsigned int)TICKS_TO_OFFSET(now));
    debugOutput(DEBUG_LEVEL_VERBOSE,"  Stop at              : %011"PRIu64" (%03us %04uc %04ut)\n",
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
    assert(m_data_buffer);

    float ticks_per_frame;

    debugOutput(DEBUG_LEVEL_VERBOSE, "Enter from state: %s\n", ePSToString(m_state));
    bool result = true;

    switch(m_state) {
        case ePS_Created:
            // prepare the framerate estimate
            ticks_per_frame = (TICKS_PER_SECOND*1.0) / ((float)m_StreamProcessorManager.getNominalRate());
            m_ticks_per_frame = ticks_per_frame;
            m_local_node_id= m_1394service.getLocalNodeId() & 0x3f;
            m_correct_last_timestamp = false;
        
            debugOutput(DEBUG_LEVEL_VERBOSE, "Initializing remote ticks/frame to %f\n", ticks_per_frame);
        
            result &= setupDataBuffer();
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

    // clear all data
    result &= m_data_buffer->clearBuffer();
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
    SIGNAL_ACTIVITY_ALL;
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
    SIGNAL_ACTIVITY_ALL;
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
            debugOutput(DEBUG_LEVEL_VERBOSE,
                        "StreamProcessor %p started dry-running\n",
                        this);
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
            // If the stream has been running, clear the previous timestamps
            // as they won't be relevant after a restart
            m_last_timestamp = m_last_timestamp2 = 0;
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
    SIGNAL_ACTIVITY_ALL;
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

    unsigned int ringbuffer_size_frames = m_StreamProcessorManager.getNbBuffers() * m_StreamProcessorManager.getPeriodSize();
    ringbuffer_size_frames += m_extra_buffer_frames;
    ringbuffer_size_frames += 1; // to ensure that we can fit it all in there

    switch(m_state) {
        case ePS_DryRunning:
            // we have to start waiting for an incoming stream
            // this basically means nothing, the state change will
            // be picked up by the packet iterator

            // clear the buffer / resize it to the most recent
            // size setting
            if(!m_data_buffer->resizeBuffer(ringbuffer_size_frames)) {
                debugError("Could not resize data buffer\n");
                return false;
            }

            if (getType() == ePT_Transmit) {
                ringbuffer_size_frames = m_StreamProcessorManager.getNbBuffers() * m_StreamProcessorManager.getPeriodSize();
                ringbuffer_size_frames += m_extra_buffer_frames;

                debugOutput(DEBUG_LEVEL_VERBOSE, "Prefill transmit SP %p with %u frames (xmit prebuffer = %d)\n",
                            this, ringbuffer_size_frames, m_extra_buffer_frames);
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
    SIGNAL_ACTIVITY_ALL;
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
            debugOutput(DEBUG_LEVEL_VERBOSE, "StreamProcessor %p started running\n", 
                                             this);
            m_in_xrun = false;
            m_local_node_id = m_1394service.getLocalNodeId() & 0x3f;
            // reduce the DLL bandwidth to what we require
            result &= m_data_buffer->setBandwidth(m_dll_bandwidth_hz / (double)TICKS_PER_SECOND);
            // enable the data buffer
            m_data_buffer->setTransparent(false);
            m_last_timestamp2 = 0; // NOTE: no use in checking if we just started running
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
    SIGNAL_ACTIVITY_ALL;
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
    SIGNAL_ACTIVITY_ALL;
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
        if (next_state == ePS_DryRunning) {
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
    SIGNAL_ACTIVITY_ALL;
    return false;
updateState_exit_change_failed:
    debugError("State transition failed: %s => %s\n",
        ePSToString(m_state), ePSToString(next_state));
    SIGNAL_ACTIVITY_ALL;
    return false;
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
        // can we put a certain amount of frames into the buffer?
        unsigned int bufferspace = m_data_buffer->getBufferSpace();
        if(bufferspace >= nframes) {
            return true;
        } else return false;
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
// FIXME: I think this can be removed and replaced by putSilenceFrames
bool
StreamProcessor::transferSilence(unsigned int nframes)
{
    bool retval;

    #ifdef DEBUG
    signed int fc;
    ffado_timestamp_t ts_tail_tmp;
    m_data_buffer->getBufferTailTimestamp(&ts_tail_tmp, &fc);
    if (fc != 0) {
        debugWarning("Prefilling a buffer that already contains %d frames\n", fc);
    }
    #endif

    // prepare a buffer of silence
    char *dummybuffer = (char *)calloc(getEventSize(), nframes * getEventsPerFrame());
    transmitSilenceBlock(dummybuffer, nframes, 0);

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
        case ePS_Error: return "ePS_Error";
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
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Port, Channel    : %d, %d\n", m_1394service.getPort(), m_channel);
    m_IsoHandlerManager.dumpInfoForStream(this);
    uint64_t now = m_1394service.getCycleTimerTicks();
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Now                   : %011"PRIu64" (%03us %04uc %04ut)\n",
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
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting verbose level to %d...\n", l );
}

} // end of namespace
