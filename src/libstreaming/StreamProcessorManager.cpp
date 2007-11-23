/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software Foundation;
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#include "StreamProcessorManager.h"
#include "generic/StreamProcessor.h"
#include "generic/Port.h"
#include "util/cycletimer.h"

#include <errno.h>
#include <assert.h>

#define RUNNING_TIMEOUT_MSEC 4000
#define PREPARE_TIMEOUT_MSEC 4000
#define ENABLE_TIMEOUT_MSEC 4000

// allows to add some processing margin. This shifts the time
// at which the buffer is transfer()'ed, making things somewhat
// more robust. It should be noted though that shifting the transfer
// time to a later time instant also causes the xmit buffer fill to be
// lower on average.
#define FFADO_SIGNAL_DELAY_TICKS 3072

namespace Streaming {

IMPL_DEBUG_MODULE( StreamProcessorManager, StreamProcessorManager, DEBUG_LEVEL_VERBOSE );

StreamProcessorManager::StreamProcessorManager(unsigned int period, unsigned int framerate, unsigned int nb_buffers)
    : m_is_slave( false )
    , m_SyncSource(NULL)
    , m_nb_buffers(nb_buffers)
    , m_period(period)
    , m_nominal_framerate ( framerate )
    , m_xruns(0)
    , m_isoManager(0)
    , m_nbperiods(0)
{
    addOption(Util::OptionContainer::Option("slaveMode",false));
}

StreamProcessorManager::~StreamProcessorManager() {
    if (m_isoManager) delete m_isoManager;
}

/**
 * Registers \ref processor with this manager.
 *
 * also registers it with the isohandlermanager
 *
 * be sure to call isohandlermanager->init() first!
 * and be sure that the processors are also ->init()'ed
 *
 * @param processor
 * @return true if successfull
 */
bool StreamProcessorManager::registerProcessor(StreamProcessor *processor)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Registering processor (%p)\n",processor);
    assert(processor);
    assert(m_isoManager);

    if (processor->getType() == StreamProcessor::ePT_Receive) {
        processor->setVerboseLevel(getDebugLevel()); // inherit debug level

        m_ReceiveProcessors.push_back(processor);
        processor->setManager(this);
        return true;
    }

    if (processor->getType() == StreamProcessor::ePT_Transmit) {
        processor->setVerboseLevel(getDebugLevel()); // inherit debug level

        m_TransmitProcessors.push_back(processor);
        processor->setManager(this);
        return true;
    }

    debugFatal("Unsupported processor type!\n");
    return false;
}

bool StreamProcessorManager::unregisterProcessor(StreamProcessor *processor)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Unregistering processor (%p)\n",processor);
    assert(processor);

    if (processor->getType()==StreamProcessor::ePT_Receive) {

        for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
              it != m_ReceiveProcessors.end();
              ++it )
        {
            if ( *it == processor ) {
                m_ReceiveProcessors.erase(it);
                processor->clearManager();
                if(!m_isoManager->unregisterStream(processor)) {
                    debugOutput(DEBUG_LEVEL_VERBOSE,"Could not unregister receive stream processor from the Iso manager\n");
                    return false;
                }
                return true;
            }
        }
    }

    if (processor->getType()==StreamProcessor::ePT_Transmit) {
        for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
              it != m_TransmitProcessors.end();
              ++it )
        {
            if ( *it == processor ) {
                m_TransmitProcessors.erase(it);
                processor->clearManager();
                if(!m_isoManager->unregisterStream(processor)) {
                    debugOutput(DEBUG_LEVEL_VERBOSE,"Could not unregister transmit stream processor from the Iso manager\n");
                    return false;
                }
                return true;
            }
        }
    }

    debugFatal("Processor (%p) not found!\n",processor);
    return false; //not found
}

bool StreamProcessorManager::setSyncSource(StreamProcessor *s) {
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting sync source to (%p)\n", s);
    m_SyncSource=s;
    return true;
}

bool StreamProcessorManager::init()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
    m_isoManager = new IsoHandlerManager(m_thread_realtime, m_thread_priority + 1);
    if(!m_isoManager) {
        debugFatal("Could not create IsoHandlerManager\n");
        return false;
    }
    m_isoManager->setVerboseLevel(getDebugLevel());
    if(!m_isoManager->init()) {
        debugFatal("Could not initialize IsoHandlerManager\n");
        return false;
    }

    m_xrun_happened=false;
    return true;
}

bool StreamProcessorManager::prepare() {

    debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing...\n");

    m_is_slave=false;
    if(!getOption("slaveMode", m_is_slave)) {
        debugWarning("Could not retrieve slaveMode parameter, defaulting to false\n");
    }

    // if no sync source is set, select one here
    if(m_SyncSource == NULL) {
       debugWarning("Sync Source is not set. Defaulting to first StreamProcessor.\n");
    }

    // FIXME: put into separate method
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
          it != m_ReceiveProcessors.end();
          ++it )
    {
        if(m_SyncSource == NULL) {
            debugWarning(" => Sync Source is %p.\n", *it);
            m_SyncSource = *it;
        }
    }
    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
          it != m_TransmitProcessors.end();
          ++it )
    {
        if(m_SyncSource == NULL) {
            debugWarning(" => Sync Source is %p.\n", *it);
            m_SyncSource = *it;
        }
    }

    // now do the actual preparation of the SP's
    debugOutput( DEBUG_LEVEL_VERBOSE, "Prepare Receive processors...\n");
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
        it != m_ReceiveProcessors.end();
        ++it ) {

        if(!(*it)->setOption("slaveMode", m_is_slave)) {
            debugOutput(DEBUG_LEVEL_VERBOSE, " note: could not set slaveMode option for (%p)...\n",(*it));
        }

        if(!(*it)->prepare()) {
            debugFatal(  " could not prepare (%p)...\n",(*it));
            return false;
        }
    }
    debugOutput( DEBUG_LEVEL_VERBOSE, "Prepare Transmit processors...\n");
    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
        it != m_TransmitProcessors.end();
        ++it ) {
        if(!(*it)->setOption("slaveMode", m_is_slave)) {
            debugOutput(DEBUG_LEVEL_VERBOSE, " note: could not set slaveMode option for (%p)...\n",(*it));
        }
        if(!(*it)->prepare()) {
            debugFatal( " could not prepare (%p)...\n",(*it));
            return false;
        }
    }

    // if there are no stream processors registered,
    // fail
    if (m_ReceiveProcessors.size() + m_TransmitProcessors.size() == 0) {
        debugFatal("No stream processors registered, can't do anything usefull\n");
        return false;
    }
    return true;
}

bool StreamProcessorManager::startDryRunning() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "Waiting for StreamProcessor streams to start dry-running...\n");
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
            it != m_ReceiveProcessors.end();
            ++it ) {
        if(!(*it)->startDryRunning(-1)) {
            debugError("Could not put SP %p into the dry-running state\n", *it);
            return false;
        }
    }
    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
            it != m_TransmitProcessors.end();
            ++it ) {
        if(!(*it)->startDryRunning(-1)) {
            debugError("Could not put SP %p into the dry-running state\n", *it);
            return false;
        }
    }
    debugOutput( DEBUG_LEVEL_VERBOSE, " StreamProcessor streams dry-running...\n");
    return true;
}

bool StreamProcessorManager::syncStartAll() {
    // figure out when to get the SP's running.
    // the xmit SP's should also know the base timestamp
    // streams should be aligned here

    // now find out how long we have to delay the wait operation such that
    // the received frames will all be presented to the SP
    debugOutput( DEBUG_LEVEL_VERBOSE, "Finding minimal sync delay...\n");
    int max_of_min_delay=0;
    int min_delay=0;
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
            it != m_ReceiveProcessors.end();
            ++it ) {
        min_delay=(*it)->getMaxFrameLatency();
        if(min_delay>max_of_min_delay) max_of_min_delay=min_delay;
    }

    // add some processing margin. This only shifts the time
    // at which the buffer is transfer()'ed. This makes things somewhat
    // more robust. It should be noted though that shifting the transfer
    // time to a later time instant also causes the xmit buffer fill to be
    // lower on average.
    max_of_min_delay += FFADO_SIGNAL_DELAY_TICKS;
    debugOutput( DEBUG_LEVEL_VERBOSE, " sync delay = %d ticks (%03us %04uc %04ut)...\n", 
        max_of_min_delay,
        (unsigned int)TICKS_TO_SECS(max_of_min_delay),
        (unsigned int)TICKS_TO_CYCLES(max_of_min_delay),
        (unsigned int)TICKS_TO_OFFSET(max_of_min_delay));
    m_SyncSource->setSyncDelay(max_of_min_delay);

    //STEP X: when we implement such a function, we can wait for a signal from the devices that they
    //        have aquired lock
    //debugOutput( DEBUG_LEVEL_VERBOSE, "Waiting for device(s) to indicate clock sync lock...\n");
    //sleep(2); // FIXME: be smarter here

    // make sure that we are dry-running long enough for the
    // DLL to have a decent sync (FIXME: does the DLL get updated when dry-running)?
    debugOutput( DEBUG_LEVEL_VERBOSE, "Waiting for sync...\n");
    int nb_sync_runs=20;
    int64_t time_till_next_period;
    while(nb_sync_runs--) { // or while not sync-ed?
        // check if we were waked up too soon
        time_till_next_period=m_SyncSource->getTimeUntilNextPeriodSignalUsecs();
        debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "waiting for %d usecs...\n", time_till_next_period);
        if(time_till_next_period > 0) {
            // wait for the period
            usleep(time_till_next_period);
        }
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, "Propagate sync info...\n");
    // FIXME: in the SPM it would be nice to have system time instead of
    //        1394 time

    // we now should have decent sync info on the sync source
    // determine a point in time where the system should start
    // figure out where we are now
    uint64_t time_of_first_sample = m_SyncSource->getTimeAtPeriod();
    debugOutput( DEBUG_LEVEL_VERBOSE, " sync at TS=%011llu (%03us %04uc %04ut)...\n", 
        time_of_first_sample,
        (unsigned int)TICKS_TO_SECS(time_of_first_sample),
        (unsigned int)TICKS_TO_CYCLES(time_of_first_sample),
        (unsigned int)TICKS_TO_OFFSET(time_of_first_sample));

    #define CYCLES_FOR_STARTUP 200
    // start wet-running in CYCLES_FOR_STARTUP cycles
    // this is the time window we have to setup all SP's such that they 
    // can start wet-running correctly.
    time_of_first_sample = addTicks(time_of_first_sample,
                                    CYCLES_FOR_STARTUP * TICKS_PER_CYCLE);

    debugOutput( DEBUG_LEVEL_VERBOSE, "  => first sample at TS=%011llu (%03us %04uc %04ut)...\n", 
        time_of_first_sample,
        (unsigned int)TICKS_TO_SECS(time_of_first_sample),
        (unsigned int)TICKS_TO_CYCLES(time_of_first_sample),
        (unsigned int)TICKS_TO_OFFSET(time_of_first_sample));

    // we should start wet-running the transmit SP's some cycles in advance
    // such that we know it is wet-running when it should output its first sample
    #define PRESTART_CYCLES_FOR_XMIT 20
    uint64_t time_to_start_xmit = substractTicks(time_of_first_sample, 
                                                 PRESTART_CYCLES_FOR_XMIT * TICKS_PER_CYCLE);

    #define PRESTART_CYCLES_FOR_RECV 0
    uint64_t time_to_start_recv = substractTicks(time_of_first_sample,
                                                 PRESTART_CYCLES_FOR_RECV * TICKS_PER_CYCLE);
    debugOutput( DEBUG_LEVEL_VERBOSE, "  => xmit starts at  TS=%011llu (%03us %04uc %04ut)...\n", 
        time_to_start_xmit,
        (unsigned int)TICKS_TO_SECS(time_to_start_xmit),
        (unsigned int)TICKS_TO_CYCLES(time_to_start_xmit),
        (unsigned int)TICKS_TO_OFFSET(time_to_start_xmit));
    debugOutput( DEBUG_LEVEL_VERBOSE, "  => recv starts at  TS=%011llu (%03us %04uc %04ut)...\n", 
        time_to_start_recv,
        (unsigned int)TICKS_TO_SECS(time_to_start_recv),
        (unsigned int)TICKS_TO_CYCLES(time_to_start_recv),
        (unsigned int)TICKS_TO_OFFSET(time_to_start_recv));

    // at this point the buffer head timestamp of the transmit buffers can be set
    // this is the presentation time of the first sample in the buffer
    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
          it != m_TransmitProcessors.end();
          ++it ) {
        (*it)->setBufferHeadTimestamp(time_of_first_sample);
    }

    // STEP X: switch SP's over to the running state
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
          it != m_ReceiveProcessors.end();
          ++it ) {
        if(!(*it)->scheduleStartRunning(time_to_start_recv)) {
            debugError("%p->scheduleStartRunning(%11llu) failed\n", *it, time_to_start_recv);
            return false;
        }
    }
    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
          it != m_TransmitProcessors.end();
          ++it ) {
        if(!(*it)->scheduleStartRunning(time_to_start_xmit)) {
            debugError("%p->scheduleStartRunning(%11llu) failed\n", *it, time_to_start_xmit);
            return false;
        }
    }
    // wait for the syncsource to start running.
    // that will block the waitForPeriod call until everyone has started (theoretically)
    int cnt = CYCLES_FOR_STARTUP * 2; // by then it should have started
    while (!m_SyncSource->isRunning() && cnt) {
        usleep(125);
        cnt--;
    }
    if(cnt==0) {
        debugOutput(DEBUG_LEVEL_VERBOSE, " Timeout waiting for the SyncSource to get started\n");
        return false;
    }
    debugOutput( DEBUG_LEVEL_VERBOSE, " StreamProcessor streams running...\n");
    return true;
}

bool StreamProcessorManager::start() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "Starting Processors...\n");
    assert(m_isoManager);

    debugOutput( DEBUG_LEVEL_VERBOSE, "Creating handlers for the StreamProcessors...\n");
    debugOutput( DEBUG_LEVEL_VERBOSE, " Receive processors...\n");
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
          it != m_ReceiveProcessors.end();
          ++it )
    {
        if (!m_isoManager->registerStream(*it)) {
            debugOutput(DEBUG_LEVEL_VERBOSE,"Could not register receive stream processor (%p) with the Iso manager\n",*it);
            return false;
        }
    }
    debugOutput( DEBUG_LEVEL_VERBOSE, " Transmit processors...\n");
    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
          it != m_TransmitProcessors.end();
          ++it )
    {
        if (!m_isoManager->registerStream(*it)) {
            debugOutput(DEBUG_LEVEL_VERBOSE,"Could not register transmit stream processor (%p) with the Iso manager\n",*it);
            return false;
        }
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing IsoHandlerManager...\n");
    if (!m_isoManager->prepare()) {
        debugFatal("Could not prepare isoManager\n");
        return false;
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, "Starting IsoHandlers...\n");
    if (!m_isoManager->startHandlers(-1)) {
        debugFatal("Could not start handlers...\n");
        return false;
    }

    // put all SP's into dry-running state
    if (!startDryRunning()) {
        debugFatal("Could not put SP's in dry-running state\n");
        return false;
    }

    // start all SP's synchonized
    if (!syncStartAll()) {
        debugFatal("Could not syncStartAll...\n");
        return false;
    }

    // dump the iso stream information when in verbose mode
    if(getDebugLevel()>=DEBUG_LEVEL_VERBOSE) {
        m_isoManager->dumpInfo();
    }

    return true;
}

bool StreamProcessorManager::stop() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "Stopping...\n");
    assert(m_isoManager);

    debugOutput( DEBUG_LEVEL_VERBOSE, "Waiting for all StreamProcessors to prepare to stop...\n");
    // Most stream processors can just stop without special treatment.  However, some
    // (like the MOTU) need to do a few things before it's safe to turn off the iso
    // handling.
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
          it != m_ReceiveProcessors.end();
          ++it ) {
        if(!(*it)->stop()) {
            debugError("Could not stop SP %p", (*it));
        }
    }
    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
          it != m_TransmitProcessors.end();
          ++it ) {
        if(!(*it)->stop()) {
            debugError("Could not stop SP %p", (*it));
        }
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, "Stopping handlers...\n");
    if(!m_isoManager->stopHandlers()) {
       debugFatal("Could not stop ISO handlers\n");
       return false;
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, "Unregistering processors from handlers...\n");
    // now unregister all streams from iso manager
    debugOutput( DEBUG_LEVEL_VERBOSE, " Receive processors...\n");
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
          it != m_ReceiveProcessors.end();
          ++it ) {
        if (!m_isoManager->unregisterStream(*it)) {
            debugOutput(DEBUG_LEVEL_VERBOSE,"Could not unregister receive stream processor (%p) from the Iso manager\n",*it);
            return false;
        }
    }
    debugOutput( DEBUG_LEVEL_VERBOSE, " Transmit processors...\n");
    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
          it != m_TransmitProcessors.end();
          ++it ) {
        if (!m_isoManager->unregisterStream(*it)) {
            debugOutput(DEBUG_LEVEL_VERBOSE,"Could not unregister transmit stream processor (%p) from the Iso manager\n",*it);
            return false;
        }
    }

    return true;
}

/**
 * Called upon Xrun events. This brings all StreamProcessors back
 * into their starting state, and then carries on streaming. This should
 * have the same effect as restarting the whole thing.
 *
 * @return true if successful, false otherwise
 */
bool StreamProcessorManager::handleXrun() {

    debugOutput( DEBUG_LEVEL_VERBOSE, "Handling Xrun ...\n");

    dumpInfo();

    /*
     * Reset means:
     * 1) Disabling the SP's, so that they don't process any packets
     *    note: the isomanager does keep on delivering/requesting them
     * 2) Bringing all buffers & streamprocessors into a know state
     *    - Clear all capture buffers
     *    - Put nb_periods*period_size of null frames into the playback buffers
     * 3) Re-enable the SP's
     */

    // put all SP's back into dry-running state
    if (!startDryRunning()) {
        debugFatal("Could not put SP's in dry-running state\n");
        return false;
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, "Restarting StreamProcessors...\n");
    // start all SP's synchonized
    if (!syncStartAll()) {
        debugFatal("Could not syncStartAll...\n");
        return false;
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, "Xrun handled...\n");

    return true;
}

/**
 * @brief Waits until the next period of samples is ready
 *
 * This function does not return until a full period of samples is (or should be)
 * ready to be transferred.
 *
 * @return true if the period is ready, false if an xrun occurred
 */
bool StreamProcessorManager::waitForPeriod() {
    int time_till_next_period;
    bool xrun_occurred=false;

    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "enter...\n");

    assert(m_SyncSource);

    time_till_next_period=m_SyncSource->getTimeUntilNextPeriodSignalUsecs();

    while(time_till_next_period > 0) {
        debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "waiting for %d usecs...\n", time_till_next_period);

        // wait for the period
        usleep(time_till_next_period);

        // check for underruns on the ISO side,
        // those should make us bail out of the wait loop
        for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
            it != m_ReceiveProcessors.end();
            ++it ) {
            // a xrun has occurred on the Iso side
            xrun_occurred |= (*it)->xrunOccurred();
        }
        for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
            it != m_TransmitProcessors.end();
            ++it ) {
            // a xrun has occurred on the Iso side
            xrun_occurred |= (*it)->xrunOccurred();
        }

        if(xrun_occurred) break;

        // check if we were waked up too soon
        time_till_next_period=m_SyncSource->getTimeUntilNextPeriodSignalUsecs();
    }

    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "delayed for %d usecs...\n", time_till_next_period);

    // this is to notify the client of the delay
    // that we introduced
    m_delayed_usecs = -time_till_next_period;

    // we save the 'ideal' time of the transfer at this point,
    // because we can have interleaved read - process - write
    // cycles making that we modify a receiving stream's buffer
    // before we get to writing.
    // NOTE: before waitForPeriod() is called again, both the transmit
    //       and the receive processors should have done their transfer.
    m_time_of_transfer = m_SyncSource->getTimeAtPeriod();
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "transfer at %llu ticks...\n",
        m_time_of_transfer);

#ifdef DEBUG
    int rcv_bf=0, xmt_bf=0;
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
        it != m_ReceiveProcessors.end();
        ++it ) {
        rcv_bf = (*it)->getBufferFill();
    }
    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
        it != m_TransmitProcessors.end();
        ++it ) {
        xmt_bf = (*it)->getBufferFill();
    }
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "XF at %011llu ticks, RBF=%d, XBF=%d, SUM=%d...\n",
        m_time_of_transfer, rcv_bf, xmt_bf, rcv_bf+xmt_bf);

#endif

    xrun_occurred=false;

    // check if xruns occurred on the Iso side.
    // also check if xruns will occur should we transfer() now

    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
          it != m_ReceiveProcessors.end();
          ++it ) {
        // a xrun has occurred on the Iso side
        xrun_occurred |= (*it)->xrunOccurred();

        // if this is true, a xrun will occur
        xrun_occurred |= !((*it)->canClientTransferFrames(m_period));

#ifdef DEBUG
        if ((*it)->xrunOccurred()) {
            debugWarning("Xrun on RECV SP %p due to ISO xrun\n",*it);
            (*it)->dumpInfo();
        }
        if (!((*it)->canClientTransferFrames(m_period))) {
            debugWarning("Xrun on RECV SP %p due to buffer xrun\n",*it);
            (*it)->dumpInfo();
        }
#endif

    }
    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
          it != m_TransmitProcessors.end();
          ++it ) {
        // a xrun has occurred on the Iso side
        xrun_occurred |= (*it)->xrunOccurred();

        // if this is true, a xrun will occur
        xrun_occurred |= !((*it)->canClientTransferFrames(m_period));

#ifdef DEBUG
        if ((*it)->xrunOccurred()) {
            debugWarning("Xrun on XMIT SP %p due to ISO xrun\n",*it);
        }
        if (!((*it)->canClientTransferFrames(m_period))) {
            debugWarning("Xrun on XMIT SP %p due to buffer xrun\n",*it);
        }
#endif
    }

    m_nbperiods++;

    // now we can signal the client that we are (should be) ready
    return !xrun_occurred;
}

/**
 * @brief Transfer one period of frames for both receive and transmit StreamProcessors
 *
 * Transfers one period of frames from the client side to the Iso side and vice versa.
 *
 * @return true if successful, false otherwise (indicates xrun).
 */
bool StreamProcessorManager::transfer() {

    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Transferring period...\n");
    bool retval=true;
    retval &= transfer(StreamProcessor::ePT_Receive);
    retval &= transfer(StreamProcessor::ePT_Transmit);
    return retval;
}

/**
 * @brief Transfer one period of frames for either the receive or transmit StreamProcessors
 *
 * Transfers one period of frames from the client side to the Iso side or vice versa.
 *
 * @param t The processor type to tranfer for (receive or transmit)
 * @return true if successful, false otherwise (indicates xrun).
 */

bool StreamProcessorManager::transfer(enum StreamProcessor::eProcessorType t) {
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "transfer(%d) at TS=%011llu (%03us %04uc %04ut)...\n", 
        t, m_time_of_transfer,
        (unsigned int)TICKS_TO_SECS(m_time_of_transfer),
        (unsigned int)TICKS_TO_CYCLES(m_time_of_transfer),
        (unsigned int)TICKS_TO_OFFSET(m_time_of_transfer));

    bool retval = true;
    // a static cast could make sure that there is no performance
    // penalty for the virtual functions (to be checked)
    if (t==StreamProcessor::ePT_Receive) {
        for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
                it != m_ReceiveProcessors.end();
                ++it ) {
            if(!(*it)->getFrames(m_period, m_time_of_transfer)) {
                    debugWarning("could not getFrames(%u, %11llu) from stream processor (%p)\n",
                            m_period, m_time_of_transfer,*it);
                retval &= false; // buffer underrun
            }
        }
    } else {
        // FIXME: in the SPM it would be nice to have system time instead of
        //        1394 time
        float rate = m_SyncSource->getTicksPerFrame();
        int64_t one_ringbuffer_in_ticks=(int64_t)(((float)(m_nb_buffers * m_period)) * rate);

        // the data we are putting into the buffer is intended to be transmitted
        // one ringbuffer size after it has been received
        int64_t transmit_timestamp = addTicks(m_time_of_transfer, one_ringbuffer_in_ticks);

        for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
                it != m_TransmitProcessors.end();
                ++it ) {
            // FIXME: in the SPM it would be nice to have system time instead of
            //        1394 time
            if(!(*it)->putFrames(m_period, transmit_timestamp)) {
                debugWarning("could not putFrames(%u,%llu) to stream processor (%p)\n",
                        m_period, transmit_timestamp, *it);
                retval &= false; // buffer underrun
            }
        }
    }
    return retval;
}

void StreamProcessorManager::dumpInfo() {
    debugOutputShort( DEBUG_LEVEL_NORMAL, "----------------------------------------------------\n");
    debugOutputShort( DEBUG_LEVEL_NORMAL, "Dumping StreamProcessorManager information...\n");
    debugOutputShort( DEBUG_LEVEL_NORMAL, "Period count: %6d\n", m_nbperiods);

    debugOutputShort( DEBUG_LEVEL_NORMAL, " Receive processors...\n");
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
        it != m_ReceiveProcessors.end();
        ++it ) {
        (*it)->dumpInfo();
    }

    debugOutputShort( DEBUG_LEVEL_NORMAL, " Transmit processors...\n");
    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
        it != m_TransmitProcessors.end();
        ++it ) {
        (*it)->dumpInfo();
    }

    debugOutputShort( DEBUG_LEVEL_NORMAL, "Iso handler info:\n");
    m_isoManager->dumpInfo();
    debugOutputShort( DEBUG_LEVEL_NORMAL, "----------------------------------------------------\n");

}

void StreamProcessorManager::setVerboseLevel(int l) {
    setDebugLevel(l);

    if (m_isoManager) m_isoManager->setVerboseLevel(l);

    debugOutput( DEBUG_LEVEL_VERBOSE, " Receive processors...\n");
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
        it != m_ReceiveProcessors.end();
        ++it ) {
        (*it)->setVerboseLevel(l);
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, " Transmit processors...\n");
    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
        it != m_TransmitProcessors.end();
        ++it ) {
        (*it)->setVerboseLevel(l);
    }
}


int StreamProcessorManager::getPortCount(enum Port::E_PortType type, enum Port::E_Direction direction) {
    int count=0;

    if (direction == Port::E_Capture) {
        for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
            it != m_ReceiveProcessors.end();
            ++it ) {
            count += (*it)->getPortCount(type);
        }
    } else {
        for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
            it != m_TransmitProcessors.end();
            ++it ) {
            count += (*it)->getPortCount(type);
        }
    }
    return count;
}

int StreamProcessorManager::getPortCount(enum Port::E_Direction direction) {
    int count=0;

    if (direction == Port::E_Capture) {
        for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
            it != m_ReceiveProcessors.end();
            ++it ) {
            count += (*it)->getPortCount();
        }
    } else {
        for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
            it != m_TransmitProcessors.end();
            ++it ) {
            count += (*it)->getPortCount();
        }
    }
    return count;
}

// TODO: implement a port map here, instead of the loop

Port* StreamProcessorManager::getPortByIndex(int idx, enum Port::E_Direction direction) {
    int count=0;
    int prevcount=0;

    if (direction == Port::E_Capture) {
        for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
            it != m_ReceiveProcessors.end();
            ++it ) {
            count += (*it)->getPortCount();
            if (count > idx) {
                return (*it)->getPortAtIdx(idx-prevcount);
            }
            prevcount=count;
        }
    } else {
        for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
            it != m_TransmitProcessors.end();
            ++it ) {
            count += (*it)->getPortCount();
            if (count > idx) {
                return (*it)->getPortAtIdx(idx-prevcount);
            }
            prevcount=count;
        }
    }
    return NULL;
}

bool StreamProcessorManager::setThreadParameters(bool rt, int priority) {
    m_thread_realtime=rt;
    m_thread_priority=priority;
    return true;
}


} // end of namespace
