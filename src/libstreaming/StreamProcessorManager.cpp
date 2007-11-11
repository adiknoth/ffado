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

namespace Streaming {

IMPL_DEBUG_MODULE( StreamProcessorManager, StreamProcessorManager, DEBUG_LEVEL_VERBOSE );

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

    m_isoManager=new IsoHandlerManager(m_thread_realtime, m_thread_priority + 1);

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

        usleep(1000);
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "Running check: %d\n", notRunning);
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

    debugOutput( DEBUG_LEVEL_VERBOSE, " StreamProcessor streams running...\n");

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

    debugOutput( DEBUG_LEVEL_VERBOSE, "  %d ticks (%03us %04uc %04ut)...\n", 
        max_of_min_delay,
        (unsigned int)TICKS_TO_SECS(max_of_min_delay),
        (unsigned int)TICKS_TO_CYCLES(max_of_min_delay),
        (unsigned int)TICKS_TO_OFFSET(max_of_min_delay));
    m_SyncSource->setSyncDelay(max_of_min_delay);

    debugOutput( DEBUG_LEVEL_VERBOSE, "Waiting for device to indicate clock sync lock...\n");
    //sleep(2); // FIXME: be smarter here
    
    debugOutput( DEBUG_LEVEL_VERBOSE, "Resetting StreamProcessors...\n");
    // now we reset the frame counters
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
            it != m_ReceiveProcessors.end();
            ++it ) {
        // get the receive SP's going at receiving data
        (*it)->m_data_buffer->setTransparent(false);
        (*it)->reset();
    }

    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
            it != m_TransmitProcessors.end();
            ++it ) {
        // make sure the SP retains it's prefilled data
        (*it)->m_data_buffer->setTransparent(false);
        (*it)->reset();
    }
    
    dumpInfo();
    // All buffers are prefilled and set-up, the only thing 
    // that remains a mistery is the timestamp information.
//     m_SyncSource->m_data_buffer->setTransparent(false);
//     debugShowBackLog();

//     m_SyncSource->setVerboseLevel(DEBUG_LEVEL_ULTRA_VERBOSE);
    
    debugOutput( DEBUG_LEVEL_VERBOSE, "Waiting for sync...\n");
    // in order to obtain that, we wait for the first periods to be
    // received.
    int nb_sync_runs=20;
    while(nb_sync_runs--) { // or while not sync-ed?
        waitForPeriod();
        // drop the frames for all receive SP's
        dryRun(StreamProcessor::E_Receive);
        
        // we don't have to dryrun for the xmit SP's since they
        // are not sending data yet.
        
        // sync the xmit SP's buffer head timestamps
        for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
                it != m_TransmitProcessors.end();
                ++it ) {
            // FIXME: encapsulate
            (*it)->m_data_buffer->setBufferHeadTimestamp(m_time_of_transfer);
        }
    }
//     m_SyncSource->setVerboseLevel(DEBUG_LEVEL_VERBOSE);

    debugOutput( DEBUG_LEVEL_VERBOSE, " sync at TS=%011llu (%03us %04uc %04ut)...\n", 
        m_time_of_transfer,
        (unsigned int)TICKS_TO_SECS(m_time_of_transfer),
        (unsigned int)TICKS_TO_CYCLES(m_time_of_transfer),
        (unsigned int)TICKS_TO_OFFSET(m_time_of_transfer));
    // FIXME: xruns can screw up the framecounter accounting. do something more sane here
    resetXrunCounters();
    // lock the isohandlermanager such that things freeze
//     debugShowBackLog();

    // we now should have decent sync info
    // the buffers of the receive streams should be (approx) empty
    // the buffers of the xmit streams should be full
    
    // note what should the timestamp of the first sample be?
    
    // at this point the buffer head timestamp of the transmit buffers can be
    // set properly since we know the sync source's timestamp of the last
    // buffer transfer. we also know the rate.
    
    debugOutput( DEBUG_LEVEL_VERBOSE, " propagate sync info...\n");
    // FIXME: in the SPM it would be nice to have system time instead of
    //        1394 time
//     float rate=m_SyncSource->getTicksPerFrame();
//     int64_t one_ringbuffer_in_ticks=(int64_t)(((float)(m_nb_buffers*m_period))*rate);
//     // the data at the front of the buffer is intended to be transmitted
//     // nb_periods*period_size after the last received period
//     int64_t transmit_timestamp = addTicks(m_time_of_transfer, one_ringbuffer_in_ticks);

    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
            it != m_TransmitProcessors.end();
            ++it ) {
        // FIXME: encapsulate
        (*it)->m_data_buffer->setBufferHeadTimestamp(m_time_of_transfer);
        //(*it)->m_data_buffer->setNominalRate(rate); //CHECK!!!
    }
    
    dumpInfo();
    
    // this is the place were we should be syncing the received streams too
    // check how much they differ
    
    
    debugOutput( DEBUG_LEVEL_VERBOSE, "Enabling StreamProcessors...\n");

    // FIXME: this should not be in cycles, but in 'time'
    // FIXME: remove the timestamp
    if (!enableStreamProcessors(0)) {
        debugFatal("Could not enable StreamProcessors...\n");
        return false;
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, "Running dry for a while...\n");
    #define MAX_DRYRUN_CYCLES               40
    #define MIN_SUCCESSFUL_DRYRUN_CYCLES    4
    // run some cycles 'dry' such that everything can stabilize
    int nb_dryrun_cycles_left = MAX_DRYRUN_CYCLES;
    int nb_succesful_cycles = 0;
    while(nb_dryrun_cycles_left > 0 &&
          nb_succesful_cycles < MIN_SUCCESSFUL_DRYRUN_CYCLES ) {

        waitForPeriod();

        if (dryRun()) {
            nb_succesful_cycles++;
        } else {
            debugOutput( DEBUG_LEVEL_VERBOSE, " This dry-run was not xrun free...\n" );
            resetXrunCounters();
            // reset the transmit SP's such that there is no issue with accumulating buffers
            // FIXME: what about receive SP's
            for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
                    it != m_TransmitProcessors.end();
                    ++it ) {
                // FIXME: encapsulate
                (*it)->reset(); //CHECK!!!
                (*it)->m_data_buffer->setBufferHeadTimestamp(m_time_of_transfer);
            }
            
            nb_succesful_cycles = 0;
            // FIXME: xruns can screw up the framecounter accounting. do something more sane here
        }
        nb_dryrun_cycles_left--;
    }

    if(nb_dryrun_cycles_left == 0) {
        debugOutput( DEBUG_LEVEL_VERBOSE, " max # dry-run cycles achieved without steady-state...\n" );
        return false;
    }
    debugOutput( DEBUG_LEVEL_VERBOSE, " dry-run resulted in steady-state...\n" );

    // now we should clear the xrun flags
    resetXrunCounters();

/*    debugOutput( DEBUG_LEVEL_VERBOSE, "Aligning streams...\n");
    // run some cycles 'dry' such that everything can stabilize
    nb_dryrun_cycles_left = MAX_DRYRUN_CYCLES;
    nb_succesful_cycles = 0;
    while(nb_dryrun_cycles_left > 0 &&
          nb_succesful_cycles < MIN_SUCCESSFUL_DRYRUN_CYCLES ) {

        waitForPeriod();

        // align the received streams
        int64_t sp_lag;
        for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
                it != m_ReceiveProcessors.end();
                ++it ) {
            uint64_t ts_sp=(*it)->getTimeAtPeriod();
            uint64_t ts_sync=m_SyncSource->getTimeAtPeriod();

            sp_lag = diffTicks(ts_sp, ts_sync);
            debugOutput( DEBUG_LEVEL_VERBOSE, "  SP(%p) TS=%011llu - TS=%011llu = %04lld\n", 
                (*it), ts_sp, ts_sync, sp_lag);
            // sync the other receive SP's to the sync source
//             if((*it) != m_SyncSource) {
//                 if(!(*it)->m_data_buffer->syncCorrectLag(sp_lag)) {
//                         debugOutput(DEBUG_LEVEL_VERBOSE,"could not syncCorrectLag(%11lld) for stream processor (%p)\n",
//                                 sp_lag, *it);
//                 }
//             }
        }


        if (dryRun()) {
            nb_succesful_cycles++;
        } else {
            debugOutput( DEBUG_LEVEL_VERBOSE, " This dry-run was not xrun free...\n" );
            resetXrunCounters();
            nb_succesful_cycles = 0;
            // FIXME: xruns can screw up the framecounter accounting. do something more sane here
        }
        nb_dryrun_cycles_left--;
    }

    if(nb_dryrun_cycles_left == 0) {
        debugOutput( DEBUG_LEVEL_VERBOSE, " max # dry-run cycles achieved without aligned steady-state...\n" );
        return false;
    }
    debugOutput( DEBUG_LEVEL_VERBOSE, " dry-run resulted in aligned steady-state...\n" );*/
    
    // now we should clear the xrun flags
    resetXrunCounters();
    // and off we go
    return true;
}

void StreamProcessorManager::resetXrunCounters(){
    debugOutput( DEBUG_LEVEL_VERBOSE, "Resetting xrun flags...\n");
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
        it != m_ReceiveProcessors.end();
        ++it )
    {
        (*it)->resetXrunCounter();
    }

    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
        it != m_TransmitProcessors.end();
        ++it ) 
    {
        (*it)->resetXrunCounter();
    }
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

    debugOutput( DEBUG_LEVEL_VERBOSE, "Disabling StreamProcessors...\n");
        if (!disableStreamProcessors()) {
        debugFatal("Could not disable StreamProcessors...\n");
        return false;
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
//     debugOutput( DEBUG_LEVEL_VERBOSE, "Waiting for all StreamProcessors to be enabled...\n");
//     // we have to wait until all streamprocessors indicate that they are running
//     // i.e. that there is actually some data stream flowing
//     int wait_cycles=ENABLE_TIMEOUT_MSEC; // two seconds
//     bool notEnabled=true;
//     while (notEnabled && wait_cycles) {
//         wait_cycles--;
//         notEnabled=false;
// 
//         for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
//                 it != m_ReceiveProcessors.end();
//                 ++it ) {
//             if(!(*it)->isEnabled()) notEnabled=true;
//         }
// 
//         for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
//                 it != m_TransmitProcessors.end();
//                 ++it ) {
//             if(!(*it)->isEnabled()) notEnabled=true;
//         }
//         usleep(1000); // one cycle
//     }
// 
//     if(!wait_cycles) { // timout has occurred
//         debugFatal("One or more streams couldn't be enabled (timeout):\n");
// 
//         for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
//                 it != m_ReceiveProcessors.end();
//                 ++it ) {
//             if(!(*it)->isEnabled()) {
//                     debugFatal(" receive stream %p not enabled\n",*it);
//             } else {
//                     debugFatal(" receive stream %p enabled\n",*it);
//             }
//         }
// 
//         for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
//                 it != m_TransmitProcessors.end();
//                 ++it ) {
//             if(!(*it)->isEnabled()) {
//                     debugFatal(" transmit stream %p not enabled\n",*it);
//             } else {
//                     debugFatal(" transmit stream %p enabled\n",*it);
//             }
//         }
//         return false;
//     }

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
        (*it)->m_data_buffer->setTransparent(true);
    }

    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
            it != m_TransmitProcessors.end();
            ++it ) {
        (*it)->disable();
        (*it)->m_data_buffer->setTransparent(true);
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
    m_delayed_usecs=time_till_next_period;

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
        xrun_occurred |= !((*it)->canClientTransferFrames(m_period)) && (*it)->isEnabled();

#ifdef DEBUG
        if ((*it)->xrunOccurred()) {
            debugWarning("Xrun on RECV SP %p due to ISO xrun\n",*it);
            (*it)->dumpInfo();
        }
        if (!((*it)->canClientTransferFrames(m_period)) && (*it)->isEnabled()) {
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
        xrun_occurred |= !((*it)->canClientTransferFrames(m_period)) && (*it)->isEnabled();

#ifdef DEBUG
        if ((*it)->xrunOccurred()) {
            debugWarning("Xrun on XMIT SP %p due to ISO xrun\n",*it);
        }
        if (!((*it)->canClientTransferFrames(m_period)) && (*it)->isEnabled()) {
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
    bool retval=true;
    retval &= dryRun(StreamProcessor::E_Receive);
    retval &= dryRun(StreamProcessor::E_Transmit);
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

bool StreamProcessorManager::transfer(enum StreamProcessor::EProcessorType t) {
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Transferring period...\n");
    bool retval = true;
    // a static cast could make sure that there is no performance
    // penalty for the virtual functions (to be checked)
    if (t==StreamProcessor::E_Receive) {
        // determine the time at which we want reception to start
        float rate=m_SyncSource->getTicksPerFrame();
        int64_t one_frame_in_ticks=(int64_t)(((float)m_period)*rate);
        
        int64_t receive_timestamp = substractTicks(m_time_of_transfer, one_frame_in_ticks);
        
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
                retval &= false; // buffer underrun
            }
        }
    } else {
        for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
                it != m_TransmitProcessors.end();
                ++it ) {
            // FIXME: in the SPM it would be nice to have system time instead of
            //        1394 time
            float rate=m_SyncSource->getTicksPerFrame();
            int64_t one_ringbuffer_in_ticks=(int64_t)(((float)(m_nb_buffers*m_period))*rate);

            // the data we are putting into the buffer is intended to be transmitted
            // one ringbuffer size after it has been received
            int64_t transmit_timestamp = addTicks(m_time_of_transfer, one_ringbuffer_in_ticks);

            if(!(*it)->putFrames(m_period, transmit_timestamp)) {
                debugOutput(DEBUG_LEVEL_VERBOSE, "could not putFrames(%u,%llu) to stream processor (%p)\n",
                        m_period, transmit_timestamp, *it);
                retval &= false; // buffer underrun
            }

        }
    }
    return retval;
}

/**
 * @brief Dry run one period for both receive and transmit StreamProcessors
 *
 * Process one period of frames for all streamprocessors, without touching the
 * client buffers. This only removes an incoming period from the ISO receive buffer and
 * puts one period of silence into the transmit buffers.
 *
 * @return true if successful, false otherwise (indicates xrun).
 */
bool StreamProcessorManager::dryRun() {
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Dry-running period...\n");
    bool retval=true;
    retval &= dryRun(StreamProcessor::E_Receive);
    retval &= dryRun(StreamProcessor::E_Transmit);
    return retval;
}

/**
 * @brief Dry run one period for either the receive or transmit StreamProcessors
 *
 * see dryRun()
 *
 * @param t The processor type to dryRun for (receive or transmit)
 * @return true if successful, false otherwise (indicates xrun).
 */

bool StreamProcessorManager::dryRun(enum StreamProcessor::EProcessorType t) {
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Dry-running period...\n");
    bool retval = true;
    // a static cast could make sure that there is no performance
    // penalty for the virtual functions (to be checked)
    if (t==StreamProcessor::E_Receive) {
        // determine the time at which we want reception to start
        float rate=m_SyncSource->getTicksPerFrame();
        int64_t one_frame_in_ticks=(int64_t)(((float)m_period)*rate);
        
        int64_t receive_timestamp = substractTicks(m_time_of_transfer, one_frame_in_ticks);
        
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

            if(!(*it)->getFramesDry(m_period, receive_timestamp)) {
                    debugOutput(DEBUG_LEVEL_VERBOSE,"could not getFrames(%u, %11llu) from stream processor (%p)\n",
                            m_period, m_time_of_transfer,*it);
                retval &= false; // buffer underrun
            }

        }
    } else {
        for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
                it != m_TransmitProcessors.end();
                ++it ) {
            // FIXME: in the SPM it would be nice to have system time instead of
            //        1394 time
            float rate=m_SyncSource->getTicksPerFrame();
            int64_t one_ringbuffer_in_ticks=(int64_t)(((float)(m_nb_buffers*m_period))*rate);

            // the data we are putting into the buffer is intended to be transmitted
            // one ringbuffer size after it has been received
            int64_t transmit_timestamp = addTicks(m_time_of_transfer, one_ringbuffer_in_ticks);

            if(!(*it)->putFramesDry(m_period, transmit_timestamp)) {
                debugOutput(DEBUG_LEVEL_VERBOSE, "could not putFrames(%u,%llu) to stream processor (%p)\n",
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
