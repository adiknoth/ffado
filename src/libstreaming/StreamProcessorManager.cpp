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
#include <math.h>

#define RUNNING_TIMEOUT_MSEC 4000
#define PREPARE_TIMEOUT_MSEC 4000
#define ENABLE_TIMEOUT_MSEC 4000

// allows to add some processing margin. This shifts the time
// at which the buffer is transfer()'ed, making things somewhat
// more robust. It should be noted though that shifting the transfer
// time to a later time instant also causes the xmit buffer fill to be
// lower on average.
#define FFADO_SIGNAL_DELAY_TICKS 3072*4

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
    
    // try to queue up 75% of the frames in the transmit buffer
    unsigned int nb_frames = (getNbBuffers() - 1) * getPeriodSize() * 1000 / 2000;
    m_isoManager->setTransmitBufferNbFrames(nb_frames);

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
    debugOutput( DEBUG_LEVEL_VERBOSE, "Putting StreamProcessor streams into dry-running state...\n");
    debugOutput( DEBUG_LEVEL_VERBOSE, " Schedule start dry-running...\n");
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
            it != m_ReceiveProcessors.end();
            ++it ) {
        if (!(*it)->isDryRunning()) {
            if(!(*it)->scheduleStartDryRunning(-1)) {
                debugError("Could not put SP %p into the dry-running state\n", *it);
                return false;
            }
        } else {
            debugOutput( DEBUG_LEVEL_VERBOSE, " SP %p already dry-running...\n", *it);
        }
    }
    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
            it != m_TransmitProcessors.end();
            ++it ) {
        if (!(*it)->isDryRunning()) {
            if(!(*it)->scheduleStartDryRunning(-1)) {
                debugError("Could not put SP %p into the dry-running state\n", *it);
                return false;
            }
        } else {
            debugOutput( DEBUG_LEVEL_VERBOSE, " SP %p already dry-running...\n", *it);
        }
    }
    debugOutput( DEBUG_LEVEL_VERBOSE, " Waiting for all SP's to be dry-running...\n");
    // wait for the syncsource to start running.
    // that will block the waitForPeriod call until everyone has started (theoretically)
    #define CYCLES_FOR_DRYRUN 40000
    int cnt = CYCLES_FOR_DRYRUN; // by then it should have started
    bool all_dry_running = false;
    while (!all_dry_running && cnt) {
        all_dry_running = true;
        for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
                it != m_ReceiveProcessors.end();
                ++it ) {
            all_dry_running &= (*it)->isDryRunning();
        }
        for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
                it != m_TransmitProcessors.end();
                ++it ) {
            all_dry_running &= (*it)->isDryRunning();
        }

        usleep(125);
        cnt--;
    }
    if(cnt==0) {
        debugOutput(DEBUG_LEVEL_VERBOSE, " Timeout waiting for the SP's to start dry-running\n");
        for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
                it != m_ReceiveProcessors.end();
                ++it ) {
            debugOutput( DEBUG_LEVEL_VERBOSE, " %s SP %p has state %s\n",
                (*it)->getTypeString(), *it, (*it)->getStateString());
        }
        for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
                it != m_TransmitProcessors.end();
                ++it ) {
            debugOutput( DEBUG_LEVEL_VERBOSE, " %s SP %p has state %s\n",
                (*it)->getTypeString(), *it, (*it)->getStateString());
        }
        return false;
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
    int max_of_min_delay = 0;
    int min_delay = 0;
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
            it != m_ReceiveProcessors.end();
            ++it ) {
        min_delay = (*it)->getMaxFrameLatency();
        if(min_delay > max_of_min_delay) max_of_min_delay = min_delay;
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
        // check if we were woken up too soon
        time_till_next_period = m_SyncSource->getTimeUntilNextPeriodSignalUsecs();
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

    #define CYCLES_FOR_STARTUP 2000
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

    // now align the received streams
    if(!alignReceivedStreams()) {
        debugError("Could not align streams\n");
        return false;
    }
    debugOutput( DEBUG_LEVEL_VERBOSE, " StreamProcessor streams running...\n");
    return true;
}

bool
StreamProcessorManager::alignReceivedStreams()
{
    #define NB_PERIODS_FOR_ALIGN_AVERAGE 20
    #define NB_ALIGN_TRIES 20
    debugOutput( DEBUG_LEVEL_VERBOSE, "Aligning received streams...\n");
    unsigned int nb_sync_runs;
    unsigned int nb_rcv_sp = m_ReceiveProcessors.size();
    int64_t diff_between_streams[nb_rcv_sp];
    int64_t diff;

    unsigned int i;

    bool aligned = false;
    int cnt = NB_ALIGN_TRIES;
    while (!aligned && cnt--) {
        nb_sync_runs = NB_PERIODS_FOR_ALIGN_AVERAGE;
        while(nb_sync_runs) {
            debugOutput( DEBUG_LEVEL_VERY_VERBOSE, " check (%d)...\n", nb_sync_runs);
            waitForPeriod();

            i = 0;
            for ( i = 0; i < nb_rcv_sp; i++) {
                StreamProcessor *s = m_ReceiveProcessors.at(i);
                diff = diffTicks(m_SyncSource->getTimeAtPeriod(), s->getTimeAtPeriod());
                debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "  offset between SyncSP %p and SP %p is %lld ticks...\n", 
                    m_SyncSource, s, diff);
                if ( nb_sync_runs == NB_PERIODS_FOR_ALIGN_AVERAGE ) {
                    diff_between_streams[i] = diff;
                } else {
                    diff_between_streams[i] += diff;
                }
            }
            if(!transferSilence()) {
                debugError("Could not transfer silence\n");
                return false;
            }
            nb_sync_runs--;
        }
        // calculate the average offsets
        debugOutput( DEBUG_LEVEL_VERBOSE, " Average offsets:\n");
        int diff_between_streams_frames[nb_rcv_sp];
        aligned = true;
        for ( i = 0; i < nb_rcv_sp; i++) {
            StreamProcessor *s = m_ReceiveProcessors.at(i);

            diff_between_streams[i] /= NB_PERIODS_FOR_ALIGN_AVERAGE;
            diff_between_streams_frames[i] = roundf(diff_between_streams[i] / s->getTicksPerFrame());
            debugOutput( DEBUG_LEVEL_VERBOSE, "   avg offset between SyncSP %p and SP %p is %lld ticks, %d frames...\n", 
                m_SyncSource, s, diff_between_streams[i], diff_between_streams_frames[i]);

            aligned &= (diff_between_streams_frames[i] == 0);

            // reposition the stream
            if(!s->shiftStream(diff_between_streams_frames[i])) {
                debugError("Could not shift SP %p %d frames\n", s, diff_between_streams_frames[i]);
                return false;
            }
        }
        if (!aligned) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Streams not aligned, doing new round...\n");
        }
    }
    if (cnt == 0) {
        debugError("Align failed\n");
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

    debugOutput( DEBUG_LEVEL_VERBOSE, " scheduling stop for all SP's...\n");

    // switch SP's over to the dry-running state
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
          it != m_ReceiveProcessors.end();
          ++it ) {
        if(!(*it)->scheduleStopRunning(-1)) {
            debugError("%p->scheduleStopRunning(-1) failed\n", *it);
            return false;
        }
    }
    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
          it != m_TransmitProcessors.end();
          ++it ) {
        if(!(*it)->scheduleStopRunning(-1)) {
            debugError("%p->scheduleStopRunning(-1) failed\n", *it);
            return false;
        }
    }
    // wait for the SP's to get into the dry-running state
    int cnt = 200;
    bool ready = false;
    while (!ready && cnt) {
        ready = true;
        for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
            it != m_ReceiveProcessors.end();
            ++it ) {
            ready &= ((*it)->isDryRunning() || (*it)->isStopped());
        }
        for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
            it != m_TransmitProcessors.end();
            ++it ) {
            ready &= ((*it)->isDryRunning() || (*it)->isStopped());
        }
        usleep(125);
        cnt--;
    }
    if(cnt==0) {
        debugOutput(DEBUG_LEVEL_VERBOSE, " Timeout waiting for the SP's to start dry-running\n");
        return false;
    }

    // switch SP's over to the stopped state
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
          it != m_ReceiveProcessors.end();
          ++it ) {
        if(!(*it)->scheduleStopDryRunning(-1)) {
            debugError("%p->scheduleStopDryRunning(-1) failed\n", *it);
            return false;
        }
    }
    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
          it != m_TransmitProcessors.end();
          ++it ) {
        if(!(*it)->scheduleStopDryRunning(-1)) {
            debugError("%p->scheduleStopDryRunning(-1) failed\n", *it);
            return false;
        }
    }
    // wait for the SP's to get into the running state
    cnt = 200;
    ready = false;
    while (!ready && cnt) {
        ready = true;
        for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
            it != m_ReceiveProcessors.end();
            ++it ) {
            ready &= (*it)->isStopped();
        }
        for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
            it != m_TransmitProcessors.end();
            ++it ) {
            ready &= (*it)->isStopped();
        }
        usleep(125);
        cnt--;
    }
    if(cnt==0) {
        debugOutput(DEBUG_LEVEL_VERBOSE, " Timeout waiting for the SP's to stop\n");
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
    bool xrun_occurred = false;

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
        time_till_next_period = m_SyncSource->getTimeUntilNextPeriodSignalUsecs();
    }

    // we save the 'ideal' time of the transfer at this point,
    // because we can have interleaved read - process - write
    // cycles making that we modify a receiving stream's buffer
    // before we get to writing.
    // NOTE: before waitForPeriod() is called again, both the transmit
    //       and the receive processors should have done their transfer.
    m_time_of_transfer = m_SyncSource->getTimeAtPeriod();
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "transfer at %llu ticks...\n",
        m_time_of_transfer);

    // normally we can transfer frames at this time, but in some cases this is not true
    // e.g. when there are not enough frames in the receive buffer.
    // however this doesn't have to be a problem, since we can wait some more until we
    // have enough frames. There is only a problem once the ISO xmit doesn't have packets
    // to transmit, or if the receive buffer overflows. These conditions are signaled by
    // the iso threads
    // check if xruns occurred on the Iso side.
    // also check if xruns will occur should we transfer() now
    #ifdef DEBUG
    int waited = 0;
    #endif
    bool ready_for_transfer = false;
    xrun_occurred = false;
    while (!ready_for_transfer && !xrun_occurred) {
        ready_for_transfer = true;
        for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
            it != m_ReceiveProcessors.end();
            ++it ) {
            ready_for_transfer &= ((*it)->canClientTransferFrames(m_period));
            xrun_occurred |= (*it)->xrunOccurred();
        }
        for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
            it != m_TransmitProcessors.end();
            ++it ) {
            ready_for_transfer &= ((*it)->canClientTransferFrames(m_period));
            xrun_occurred |= (*it)->xrunOccurred();
        }
        if (!ready_for_transfer) {
            usleep(125); // MAGIC: one cycle sleep...

            // in order to avoid this in the future, we increase the sync delay of the sync source SP
            int d = m_SyncSource->getSyncDelay() + TICKS_PER_CYCLE;
            m_SyncSource->setSyncDelay(d);

            #ifdef DEBUG
            waited++;
            #endif
        }
    } // we are either ready or an xrun occurred

    #ifdef DEBUG
    if(waited > 0) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Waited %d x 125us due to SP not ready for transfer\n", waited);
    }
    #endif

    // this is to notify the client of the delay that we introduced by waiting
    m_delayed_usecs = - m_SyncSource->getTimeUntilNextPeriodSignalUsecs();
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "delayed for %d usecs...\n", m_delayed_usecs);

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

    // check if xruns occurred on the Iso side.
    // also check if xruns will occur should we transfer() now
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
          it != m_ReceiveProcessors.end();
          ++it ) {

        if ((*it)->xrunOccurred()) {
            debugWarning("Xrun on RECV SP %p due to ISO side xrun\n",*it);
            (*it)->dumpInfo();
        }
        if (!((*it)->canClientTransferFrames(m_period))) {
            debugWarning("Xrun on RECV SP %p due to buffer side xrun\n",*it);
            (*it)->dumpInfo();
        }
    }
    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
          it != m_TransmitProcessors.end();
          ++it ) {
        if ((*it)->xrunOccurred()) {
            debugWarning("Xrun on XMIT SP %p due to ISO side xrun\n",*it);
        }
        if (!((*it)->canClientTransferFrames(m_period))) {
            debugWarning("Xrun on XMIT SP %p due to buffer side xrun\n",*it);
        }
    }
#endif

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

/**
 * @brief Transfer one period of silence for both receive and transmit StreamProcessors
 *
 * Transfers one period of silence to the Iso side for transmit SP's
 * or dump one period of frames for receive SP's
 *
 * @return true if successful, false otherwise (indicates xrun).
 */
bool StreamProcessorManager::transferSilence() {
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Transferring silent period...\n");
    bool retval=true;
    retval &= transferSilence(StreamProcessor::ePT_Receive);
    retval &= transferSilence(StreamProcessor::ePT_Transmit);
    return retval;
}

/**
 * @brief Transfer one period of silence for either the receive or transmit StreamProcessors
 *
 * Transfers one period of silence to the Iso side for transmit SP's
 * or dump one period of frames for receive SP's
 *
 * @param t The processor type to tranfer for (receive or transmit)
 * @return true if successful, false otherwise (indicates xrun).
 */
bool StreamProcessorManager::transferSilence(enum StreamProcessor::eProcessorType t) {
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "transferSilence(%d) at TS=%011llu (%03us %04uc %04ut)...\n", 
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
            if(!(*it)->dropFrames(m_period, m_time_of_transfer)) {
                    debugWarning("could not dropFrames(%u, %11llu) from stream processor (%p)\n",
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
            if(!(*it)->putSilenceFrames(m_period, transmit_timestamp)) {
                debugWarning("could not putSilenceFrames(%u,%llu) to stream processor (%p)\n",
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
