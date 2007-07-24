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
#include "StreamProcessor.h"
#include "Port.h"
#include <errno.h>
#include <assert.h>

#include "libstreaming/cycletimer.h"

#define CYCLES_TO_SLEEP_AFTER_RUN_SIGNAL 5

#define RUNNING_TIMEOUT_MSEC 4000
#define PREPARE_TIMEOUT_MSEC 4000
#define ENABLE_TIMEOUT_MSEC 4000

//#define ENABLE_DELAY_CYCLES 100
#define ENABLE_DELAY_CYCLES 1000

namespace Streaming {

IMPL_DEBUG_MODULE( StreamProcessorManager, StreamProcessorManager, DEBUG_LEVEL_NORMAL );

StreamProcessorManager::StreamProcessorManager(unsigned int period, unsigned int nb_buffers)
    : m_is_slave( false )
    , m_SyncSource(NULL)
    , m_nb_buffers(nb_buffers)
    , m_period(period)
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

    if (processor->getType()==StreamProcessor::E_Receive) {
        processor->setVerboseLevel(getDebugLevel()); // inherit debug level

        m_ReceiveProcessors.push_back(processor);

        processor->setManager(this);

        return true;
    }

    if (processor->getType()==StreamProcessor::E_Transmit) {
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

    if (processor->getType()==StreamProcessor::E_Receive) {

        for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
            it != m_ReceiveProcessors.end();
            ++it ) {

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

    if (processor->getType()==StreamProcessor::E_Transmit) {
        for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
            it != m_TransmitProcessors.end();
            ++it ) {

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

StreamProcessor *StreamProcessorManager::getSyncSource() {
    return m_SyncSource;
}

bool StreamProcessorManager::init()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

    m_isoManager=new IsoHandlerManager(m_thread_realtime, m_thread_priority);

    if(!m_isoManager) {
        debugFatal("Could not create IsoHandlerManager\n");
        return false;
    }

    // propagate the debug level
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

    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
        it != m_ReceiveProcessors.end();
        ++it ) {
            if(m_SyncSource == NULL) {
                debugWarning(" => Sync Source is %p.\n", *it);
                m_SyncSource = *it;
            }
    }

    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
        it != m_TransmitProcessors.end();
        ++it ) {
            if(m_SyncSource == NULL) {
                debugWarning(" => Sync Source is %p.\n", *it);
                m_SyncSource = *it;
            }
    }

    // now do the actual preparation
    debugOutput( DEBUG_LEVEL_VERBOSE, "Prepare Receive processors...\n");
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
        it != m_ReceiveProcessors.end();
        ++it ) {

        if(!(*it)->setSyncSource(m_SyncSource)) {
            debugFatal(  " could not set sync source (%p)...\n",(*it));
            return false;
        }

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
        if(!(*it)->setSyncSource(m_SyncSource)) {
            debugFatal(  " could not set sync source (%p)...\n",(*it));
            return false;
        }
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

bool StreamProcessorManager::syncStartAll() {

    debugOutput( DEBUG_LEVEL_VERBOSE, "Waiting for StreamProcessor streams to start running...\n");
    // we have to wait until all streamprocessors indicate that they are running
    // i.e. that there is actually some data stream flowing
    int wait_cycles=RUNNING_TIMEOUT_MSEC; // two seconds
    bool notRunning=true;
    while (notRunning && wait_cycles) {
        wait_cycles--;
        notRunning=false;

        for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
                it != m_ReceiveProcessors.end();
                ++it ) {
            if(!(*it)->isRunning()) notRunning=true;
        }

        for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
                it != m_TransmitProcessors.end();
                ++it ) {
            if(!(*it)->isRunning()) notRunning=true;
        }

        // EXPERIMENT:
        // the only stream that should be running is the sync
        // source stream, as this is the one that defines
        // when to signal buffers. Maybe we get an xrun at startup,
        // but that should be handled.

        // the problem is that otherwise a setup with a device
        // that waits for decent input before sending output
        // will not start up (e.g. the bounce device), because
        // all streams are required to be running.

        // other streams still have at least ENABLE_DELAY_CYCLES cycles
        // to start up
//         if(!m_SyncSource->isRunning()) notRunning=true;

        usleep(1000);
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "Running check: %d\n",notRunning);
    }

    if(!wait_cycles) { // timout has occurred
        debugFatal("One or more streams are not starting up (timeout):\n");

        for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
                it != m_ReceiveProcessors.end();
                ++it ) {
            if(!(*it)->isRunning()) {
                debugFatal(" receive stream %p not running\n",*it);
            } else {
                debugFatal(" receive stream %p running\n",*it);
            }
        }

        for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
                it != m_TransmitProcessors.end();
                ++it ) {
            if(!(*it)->isRunning()) {
                debugFatal(" transmit stream %p not running\n",*it);
            } else {
                debugFatal(" transmit stream %p running\n",*it);
            }
        }
        return false;
    }

    // we want to make sure that everything is running well,
    // so wait for a while
    usleep(USECS_PER_CYCLE * CYCLES_TO_SLEEP_AFTER_RUN_SIGNAL);

    debugOutput( DEBUG_LEVEL_VERBOSE, " StreamProcessor streams running...\n");

    debugOutput( DEBUG_LEVEL_VERBOSE, "Finding minimal sync delay...\n");

    int max_of_min_delay=0;
    int min_delay=0;
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
            it != m_ReceiveProcessors.end();
            ++it ) {
        min_delay=(*it)->getMinimalSyncDelay();
        if(min_delay>max_of_min_delay) max_of_min_delay=min_delay;
    }

    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
            it != m_TransmitProcessors.end();
            ++it ) {
        min_delay=(*it)->getMinimalSyncDelay();
        if(min_delay>max_of_min_delay) max_of_min_delay=min_delay;
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, "  %d ticks\n", max_of_min_delay);
    m_SyncSource->setSyncDelay(max_of_min_delay);


    debugOutput( DEBUG_LEVEL_VERBOSE, "Resetting StreamProcessors...\n");
    // now we reset the frame counters
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
            it != m_ReceiveProcessors.end();
            ++it ) {
        (*it)->reset();
    }

    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
            it != m_TransmitProcessors.end();
            ++it ) {
        (*it)->reset();
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, "Enabling StreamProcessors...\n");

    uint64_t now=m_SyncSource->getTimeNow(); // fixme: should be in usecs, not ticks

    // FIXME: this should not be in cycles, but in 'time'
    unsigned int enable_at=TICKS_TO_CYCLES(now)+ENABLE_DELAY_CYCLES;
    if (enable_at > 8000) enable_at -= 8000;

    if (!enableStreamProcessors(enable_at)) {
        debugFatal("Could not enable StreamProcessors...\n");
        return false;
    }

    return true;
}

bool StreamProcessorManager::start() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "Starting Processors...\n");
    assert(m_isoManager);

    debugOutput( DEBUG_LEVEL_VERBOSE, "Creating handlers for the StreamProcessors...\n");
    debugOutput( DEBUG_LEVEL_VERBOSE, " Receive processors...\n");
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
        it != m_ReceiveProcessors.end();
        ++it ) {
            if (!(*it)->prepareForStart()) {
                debugOutput(DEBUG_LEVEL_VERBOSE,"Receive stream processor (%p) failed to prepare for start\n", *it);
                return false;
            }
            if (!m_isoManager->registerStream(*it)) {
                debugOutput(DEBUG_LEVEL_VERBOSE,"Could not register receive stream processor (%p) with the Iso manager\n",*it);
                return false;
            }
        }

    debugOutput( DEBUG_LEVEL_VERBOSE, " Transmit processors...\n");
    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
        it != m_TransmitProcessors.end();
        ++it ) {
            if (!(*it)->prepareForStart()) {
                debugOutput(DEBUG_LEVEL_VERBOSE,"Transmit stream processor (%p) failed to prepare for start\n", *it);
                return false;
            }
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

    debugOutput( DEBUG_LEVEL_VERBOSE, "Disabling StreamProcessors...\n");
        if (!disableStreamProcessors()) {
        debugFatal("Could not disable StreamProcessors...\n");
        return false;
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, "Starting IsoHandlers...\n");
    if (!m_isoManager->startHandlers(-1)) {
        debugFatal("Could not start handlers...\n");
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
    int wait_cycles=PREPARE_TIMEOUT_MSEC; // two seconds ought to be sufficient
    bool allReady = false;
    while (!allReady && wait_cycles) {
        wait_cycles--;
        allReady = true;

        for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
            it != m_ReceiveProcessors.end();
            ++it ) {
            if(!(*it)->prepareForStop()) allReady = false;
        }

        for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
            it != m_TransmitProcessors.end();
            ++it ) {
            if(!(*it)->prepareForStop()) allReady = false;
        }
        usleep(1000);
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
 * Enables the registered StreamProcessors
 * @return true if successful, false otherwise
 */
bool StreamProcessorManager::enableStreamProcessors(uint64_t time_to_enable_at) {
    debugOutput( DEBUG_LEVEL_VERBOSE, "Enabling StreamProcessors at %llu...\n", time_to_enable_at);

    debugOutput( DEBUG_LEVEL_VERBOSE, " Sync Source StreamProcessor (%p)...\n",m_SyncSource);
    debugOutput( DEBUG_LEVEL_VERBOSE, "  Prepare...\n");
    if (!m_SyncSource->prepareForEnable(time_to_enable_at)) {
            debugFatal("Could not prepare Sync Source StreamProcessor for enable()...\n");
        return false;
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, "  Enable...\n");
    m_SyncSource->enable(time_to_enable_at);

    debugOutput( DEBUG_LEVEL_VERBOSE, " Other StreamProcessors...\n");

    // we prepare the streamprocessors for enable
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
            it != m_ReceiveProcessors.end();
            ++it ) {
        if(*it != m_SyncSource) {
            debugOutput( DEBUG_LEVEL_VERBOSE, " Prepare Receive SP (%p)...\n",*it);
            (*it)->prepareForEnable(time_to_enable_at);
        }
    }

    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
            it != m_TransmitProcessors.end();
            ++it ) {
        if(*it != m_SyncSource) {
            debugOutput( DEBUG_LEVEL_VERBOSE, " Prepare Transmit SP (%p)...\n",*it);
            (*it)->prepareForEnable(time_to_enable_at);
        }
    }

    // then we enable the streamprocessors
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
            it != m_ReceiveProcessors.end();
            ++it ) {
        if(*it != m_SyncSource) {
            debugOutput( DEBUG_LEVEL_VERBOSE, " Enable Receive SP (%p)...\n",*it);
            (*it)->enable(time_to_enable_at);
        }
    }

    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
            it != m_TransmitProcessors.end();
            ++it ) {
        if(*it != m_SyncSource) {
            debugOutput( DEBUG_LEVEL_VERBOSE, " Enable Transmit SP (%p)...\n",*it);
            (*it)->enable(time_to_enable_at);
        }
    }

    // now we wait for the SP's to get enabled
    debugOutput( DEBUG_LEVEL_VERBOSE, "Waiting for all StreamProcessors to be enabled...\n");
    // we have to wait until all streamprocessors indicate that they are running
    // i.e. that there is actually some data stream flowing
    int wait_cycles=ENABLE_TIMEOUT_MSEC; // two seconds
    bool notEnabled=true;
    while (notEnabled && wait_cycles) {
        wait_cycles--;
        notEnabled=false;

        for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
                it != m_ReceiveProcessors.end();
                ++it ) {
            if(!(*it)->isEnabled()) notEnabled=true;
        }

        for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
                it != m_TransmitProcessors.end();
                ++it ) {
            if(!(*it)->isEnabled()) notEnabled=true;
        }
        usleep(1000); // one cycle
    }

    if(!wait_cycles) { // timout has occurred
        debugFatal("One or more streams couldn't be enabled (timeout):\n");

        for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
                it != m_ReceiveProcessors.end();
                ++it ) {
            if(!(*it)->isEnabled()) {
                    debugFatal(" receive stream %p not enabled\n",*it);
            } else {
                    debugFatal(" receive stream %p enabled\n",*it);
            }
        }

        for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
                it != m_TransmitProcessors.end();
                ++it ) {
            if(!(*it)->isEnabled()) {
                    debugFatal(" transmit stream %p not enabled\n",*it);
            } else {
                    debugFatal(" transmit stream %p enabled\n",*it);
            }
        }
        return false;
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, " => all StreamProcessors enabled...\n");

    return true;
}

/**
 * Disables the registered StreamProcessors
 * @return true if successful, false otherwise
 */
bool StreamProcessorManager::disableStreamProcessors() {
    // we prepare the streamprocessors for disable
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
            it != m_ReceiveProcessors.end();
            ++it ) {
        (*it)->prepareForDisable();
    }

    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
            it != m_TransmitProcessors.end();
            ++it ) {
        (*it)->prepareForDisable();
    }

    // then we disable the streamprocessors
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
            it != m_ReceiveProcessors.end();
            ++it ) {
        (*it)->disable();
    }

    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
            it != m_TransmitProcessors.end();
            ++it ) {
        (*it)->disable();
    }

    // now we wait for the SP's to get disabled
    debugOutput( DEBUG_LEVEL_VERBOSE, "Waiting for all StreamProcessors to be disabled...\n");
    // we have to wait until all streamprocessors indicate that they are running
    // i.e. that there is actually some data stream flowing
    int wait_cycles=ENABLE_TIMEOUT_MSEC; // two seconds
    bool enabled=true;
    while (enabled && wait_cycles) {
        wait_cycles--;
        enabled=false;

        for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
                it != m_ReceiveProcessors.end();
                ++it ) {
            if((*it)->isEnabled()) enabled=true;
        }

        for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
                it != m_TransmitProcessors.end();
                ++it ) {
            if((*it)->isEnabled()) enabled=true;
        }
        usleep(1000); // one cycle
    }

    if(!wait_cycles) { // timout has occurred
        debugFatal("One or more streams couldn't be disabled (timeout):\n");

        for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
                it != m_ReceiveProcessors.end();
                ++it ) {
            if(!(*it)->isEnabled()) {
                    debugFatal(" receive stream %p not enabled\n",*it);
            } else {
                    debugFatal(" receive stream %p enabled\n",*it);
            }
        }

        for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
                it != m_TransmitProcessors.end();
                ++it ) {
            if(!(*it)->isEnabled()) {
                    debugFatal(" transmit stream %p not enabled\n",*it);
            } else {
                    debugFatal(" transmit stream %p enabled\n",*it);
            }
        }
        return false;
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, " => all StreamProcessors disabled...\n");

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

    /*
     * Reset means:
     * 1) Disabling the SP's, so that they don't process any packets
     *    note: the isomanager does keep on delivering/requesting them
     * 2) Bringing all buffers & streamprocessors into a know state
     *    - Clear all capture buffers
     *    - Put nb_periods*period_size of null frames into the playback buffers
     * 3) Re-enable the SP's
     */
    debugOutput( DEBUG_LEVEL_VERBOSE, "Disabling StreamProcessors...\n");
        if (!disableStreamProcessors()) {
        debugFatal("Could not disable StreamProcessors...\n");
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
        debugOutput( DEBUG_LEVEL_VERBOSE, "waiting for %d usecs...\n", time_till_next_period);

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

        // check if we were waked up too soon
        time_till_next_period=m_SyncSource->getTimeUntilNextPeriodSignalUsecs();
    }

    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "delayed for %d usecs...\n", time_till_next_period);

    // this is to notify the client of the delay
    // that we introduced
    m_delayed_usecs=time_till_next_period;

    // we save the 'ideal' time of the transfer at this point,
    // because we can have interleaved read - process - write
    // cycles making that we modify a receiving stream's buffer
    // before we get to writing.
    // NOTE: before waitForPeriod() is called again, both the transmit
    //       and the receive processors should have done their transfer.
    m_time_of_transfer=m_SyncSource->getTimeAtPeriod();
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
        m_time_of_transfer,rcv_bf,xmt_bf,rcv_bf+xmt_bf);

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

    debugOutput( DEBUG_LEVEL_VERBOSE, "Transferring period...\n");

    if (!transfer(StreamProcessor::E_Receive)) return false;
    if (!transfer(StreamProcessor::E_Transmit)) return false;

    return true;
}

/**
 * @brief Transfer one period of frames for either the receive or transmit StreamProcessors
 *
 * Transfers one period of frames from the client side to the Iso side or vice versa.
 *
 * @param t The processor type to tranfer for (receive or transmit)
 * @return true if successful, false otherwise (indicates xrun).
 */

bool StreamProcessorManager::transfer(enum StreamProcessor::EProcessorType t) {
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Transferring period...\n");

    // a static cast could make sure that there is no performance
    // penalty for the virtual functions (to be checked)
    if (t==StreamProcessor::E_Receive) {
        
        // determine the time at which we want reception to start
        float rate=m_SyncSource->getTicksPerFrame();
        int64_t one_frame_in_ticks=(int64_t)(((float)m_period)*rate);
        
        int64_t receive_timestamp = substractTicks(m_time_of_transfer,one_frame_in_ticks);
        
        if(receive_timestamp<0) {
            debugWarning("receive ts < 0.0 : %lld, m_time_of_transfer= %llu, one_frame_in_ticks=%lld\n",
             receive_timestamp, m_time_of_transfer, one_frame_in_ticks);
        }
        if(receive_timestamp>(128L*TICKS_PER_SECOND)) {
            debugWarning("receive ts > 128L*TICKS_PER_SECOND : %lld, m_time_of_transfer= %llu, one_frame_in_ticks=%lld\n",
             receive_timestamp, m_time_of_transfer, one_frame_in_ticks);
        }
        
        for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
                it != m_ReceiveProcessors.end();
                ++it ) {

            if(!(*it)->getFrames(m_period, receive_timestamp)) {
                    debugOutput(DEBUG_LEVEL_VERBOSE,"could not getFrames(%u, %11llu) from stream processor (%p)\n",
                            m_period, m_time_of_transfer,*it);
                    return false; // buffer underrun
            }

        }
    } else {
        for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
                it != m_TransmitProcessors.end();
                ++it ) {

            if(!(*it)->putFrames(m_period, (int64_t)m_time_of_transfer)) {
                debugOutput(DEBUG_LEVEL_VERBOSE, "could not putFrames(%u,%llu) to stream processor (%p)\n",
                        m_period, m_time_of_transfer, *it);
                return false; // buffer overrun
            }

        }
    }

    return true;
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
