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

#include "StreamProcessorManager.h"
#include "generic/StreamProcessor.h"
#include "generic/Port.h"
#include "libieee1394/cycletimer.h"

#include "devicemanager.h"

#include "libutil/Time.h"

#include <errno.h>
#include <assert.h>
#include <math.h>

namespace Streaming {

IMPL_DEBUG_MODULE( StreamProcessorManager, StreamProcessorManager, DEBUG_LEVEL_VERBOSE );

StreamProcessorManager::StreamProcessorManager(DeviceManager &p)
    : m_is_slave( false )
    , m_SyncSource(NULL)
    , m_parent( p )
    , m_xrun_happened( false )
    , m_activity_wait_timeout_nsec( 0 ) // dynamically set
    , m_nb_buffers( 0 )
    , m_period( 0 )
    , m_audio_datatype( eADT_Float )
    , m_nominal_framerate ( 0 )
    , m_xruns(0)
    , m_shutdown_needed(false)
    , m_nbperiods(0)
    , m_WaitLock( new Util::PosixMutex("SPMWAIT") )
{
    addOption(Util::OptionContainer::Option("slaveMode",false));
    sem_init(&m_activity_semaphore, 0, 0);
}

StreamProcessorManager::StreamProcessorManager(DeviceManager &p, unsigned int period,
                                               unsigned int framerate, unsigned int nb_buffers)
    : m_is_slave( false )
    , m_SyncSource(NULL)
    , m_parent( p )
    , m_xrun_happened( false )
    , m_activity_wait_timeout_nsec( 0 ) // dynamically set
    , m_nb_buffers(nb_buffers)
    , m_period(period)
    , m_audio_datatype( eADT_Float )
    , m_nominal_framerate ( framerate )
    , m_xruns(0)
    , m_shutdown_needed(false)
    , m_nbperiods(0)
    , m_WaitLock( new Util::PosixMutex("SPMWAIT") )
{
    addOption(Util::OptionContainer::Option("slaveMode",false));
    sem_init(&m_activity_semaphore, 0, 0);
}

StreamProcessorManager::~StreamProcessorManager() {
    sem_post(&m_activity_semaphore);
    sem_destroy(&m_activity_semaphore);
    delete m_WaitLock;
}

// void
// StreamProcessorManager::handleBusReset(Ieee1394Service &s)
// {
// //     debugOutput( DEBUG_LEVEL_VERBOSE, "(%p) Handle bus reset on service %p...\n", this, &s);
// // 
// //     bool handled_at_least_one = false;
// //     // note that all receive streams are gone once a device is unplugged
// // 
// //     // synchronize with the wait lock
// //     Util::MutexLockHelper lock(*m_WaitLock);
// // 
// //     debugOutput( DEBUG_LEVEL_VERBOSE, "(%p) got wait lock...\n", this);
// //     // cause all SP's to bail out
// //     for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
// //           it != m_ReceiveProcessors.end();
// //           ++it )
// //     {
// //         if(&s == &((*it)->getParent().get1394Service())) {
// //             debugOutput(DEBUG_LEVEL_NORMAL,
// //                         "issue busreset on receive SPM on channel %d\n",
// //                         (*it)->getChannel());
// //             (*it)->handleBusReset();
// //             handled_at_least_one = true;
// //         } else {
// //             debugOutput(DEBUG_LEVEL_NORMAL,
// //                         "skipping receive SPM on channel %d since not on service %p\n",
// //                         (*it)->getChannel(), &s);
// //         }
// //     }
// //     for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
// //           it != m_TransmitProcessors.end();
// //           ++it )
// //     {
// //         if(&s == &((*it)->getParent().get1394Service())) {
// //             debugOutput(DEBUG_LEVEL_NORMAL,
// //                         "issue busreset on transmit SPM on channel %d\n",
// //                         (*it)->getChannel());
// //             (*it)->handleBusReset();
// //             handled_at_least_one = true;
// //         } else {
// //             debugOutput(DEBUG_LEVEL_NORMAL,
// //                         "skipping transmit SPM on channel %d since not on service %p\n",
// //                         (*it)->getChannel(), &s);
// //         }
// //     }
// // 
// //     // FIXME: we request shutdown for now.
// //     m_shutdown_needed = handled_at_least_one;
// }

void
StreamProcessorManager::signalActivity()
{
    sem_post(&m_activity_semaphore);
    debugOutputExtreme(DEBUG_LEVEL_VERBOSE,"%p activity\n", this);
}

enum StreamProcessorManager::eActivityResult
StreamProcessorManager::waitForActivity()
{
    debugOutputExtreme(DEBUG_LEVEL_VERBOSE,"%p waiting for activity\n", this);
    struct timespec ts;
    int result;

    if (m_activity_wait_timeout_nsec >= 0) {

        if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
            debugError("clock_gettime failed\n");
            return eAR_Error;
        }
        ts.tv_nsec += m_activity_wait_timeout_nsec;
        while(ts.tv_nsec >= 1000000000LL) {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000LL;
        }
    }

    if (m_activity_wait_timeout_nsec >= 0) {
        result = sem_timedwait(&m_activity_semaphore, &ts);
    } else {
        result = sem_wait(&m_activity_semaphore);
    }

    if(result != 0) {
        if (errno == ETIMEDOUT) {
            debugOutput(DEBUG_LEVEL_VERBOSE,
                        "(%p) sem_timedwait() timed out (result=%d)\n",
                        this, result);
            return eAR_Timeout;
        } else if (errno == EINTR) {
            debugOutput(DEBUG_LEVEL_VERBOSE,
                        "(%p) sem_[timed]wait() interrupted by signal (result=%d)\n",
                        this, result);
            return eAR_Interrupted;
        } else if (errno == EINVAL) {
            debugError("(%p) sem_[timed]wait error (result=%d errno=EINVAL)\n", 
                        this, result);
            debugError("(%p) timeout_nsec=%lld ts.sec=%d ts.nsec=%lld\n", 
                       this, m_activity_wait_timeout_nsec, ts.tv_sec, ts.tv_nsec);
            return eAR_Error;
        } else {
            debugError("(%p) sem_[timed]wait error (result=%d errno=%d)\n", 
                        this, result, errno);
            debugError("(%p) timeout_nsec=%lld ts.sec=%d ts.nsec=%lld\n", 
                       this, m_activity_wait_timeout_nsec, ts.tv_sec, ts.tv_nsec);
            return eAR_Error;
        }
    }

    debugOutputExtreme(DEBUG_LEVEL_VERBOSE,"%p got activity\n", this);
    return eAR_Activity;
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
    if (processor->getType() == StreamProcessor::ePT_Receive) {
        processor->setVerboseLevel(getDebugLevel()); // inherit debug level
        m_ReceiveProcessors.push_back(processor);
        Util::Functor* f = new Util::MemberFunctor0< StreamProcessorManager*, void (StreamProcessorManager::*)() >
                    ( this, &StreamProcessorManager::updateShadowLists, false );
        processor->addPortManagerUpdateHandler(f);
        updateShadowLists();
        return true;
    }
    if (processor->getType() == StreamProcessor::ePT_Transmit) {
        processor->setVerboseLevel(getDebugLevel()); // inherit debug level
        m_TransmitProcessors.push_back(processor);
        Util::Functor* f = new Util::MemberFunctor0< StreamProcessorManager*, void (StreamProcessorManager::*)() >
                    ( this, &StreamProcessorManager::updateShadowLists, false );
        processor->addPortManagerUpdateHandler(f);
        updateShadowLists();
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
                if (*it == m_SyncSource) {
                    debugOutput(DEBUG_LEVEL_VERBOSE, "unregistering sync source\n");
                    m_SyncSource = NULL;
                }
                m_ReceiveProcessors.erase(it);
                // remove the functor
                Util::Functor * f = processor->getUpdateHandlerForPtr(this);
                if(f) {
                    processor->remPortManagerUpdateHandler(f);
                    delete f;
                }
                updateShadowLists();
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
                if (*it == m_SyncSource) {
                    debugOutput(DEBUG_LEVEL_VERBOSE, "unregistering sync source\n");
                    m_SyncSource = NULL;
                }
                m_TransmitProcessors.erase(it);
                // remove the functor
                Util::Functor * f = processor->getUpdateHandlerForPtr(this);
                if(f) {
                    processor->remPortManagerUpdateHandler(f);
                    delete f;
                }
                updateShadowLists();
                return true;
            }
        }
    }

    debugFatal("Processor (%p) not found!\n",processor);
    return false; //not found
}

bool StreamProcessorManager::setSyncSource(StreamProcessor *s) {
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting sync source to (%p)\n", s);
    m_SyncSource = s;
    return true;
}

bool StreamProcessorManager::prepare() {

    debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing...\n");
    m_is_slave=false;
    if(!getOption("slaveMode", m_is_slave)) {
        debugWarning("Could not retrieve slaveMode parameter, defaulting to false\n");
    }

    m_shutdown_needed=false;

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

    // set the activity timeout value to two periods worth of usecs.
    // since we can expect activity once every period, but we might have some
    // offset, the safe value is two periods.
    int timeout_usec = 2*1000LL * 1000LL * m_period / m_nominal_framerate;
    debugOutput(DEBUG_LEVEL_VERBOSE, "setting activity timeout to %d\n", timeout_usec);
    setActivityWaitTimeoutUsec(timeout_usec);

    updateShadowLists();

    return true;
}

bool
StreamProcessorManager::startDryRunning()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Putting StreamProcessor streams into dry-running state...\n");
    debugOutput( DEBUG_LEVEL_VERBOSE, " Schedule start dry-running...\n");
    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
            it != m_TransmitProcessors.end();
            ++it ) {
        if ((*it)->inError()) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "SP %p in error state\n", *it);
            return false;
        }
        if (!(*it)->isDryRunning()) {
            if(!(*it)->scheduleStartDryRunning(-1)) {
                debugError("Could not put SP %p into the dry-running state\n", *it);
                return false;
            }
        } else {
            debugOutput( DEBUG_LEVEL_VERBOSE, " SP %p already dry-running...\n", *it);
        }
    }
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
            it != m_ReceiveProcessors.end();
            ++it ) {
        if ((*it)->inError()) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "SP %p in error state\n", *it);
            return false;
        }
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
    int cnt = STREAMPROCESSORMANAGER_CYCLES_FOR_DRYRUN; // by then it should have started
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

        SleepRelativeUsec(125);
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
    if(m_SyncSource == NULL) return false;

    // get the options
    int signal_delay_ticks = STREAMPROCESSORMANAGER_SIGNAL_DELAY_TICKS;
    int sync_wait_time_msec = STREAMPROCESSORMANAGER_SYNC_WAIT_TIME_MSEC;
    int cycles_for_startup = STREAMPROCESSORMANAGER_CYCLES_FOR_STARTUP;
    int prestart_cycles_for_xmit = STREAMPROCESSORMANAGER_PRESTART_CYCLES_FOR_XMIT;
    int prestart_cycles_for_recv = STREAMPROCESSORMANAGER_PRESTART_CYCLES_FOR_RECV;
    Util::Configuration &config = m_parent.getConfiguration();
    config.getValueForSetting("streaming.spm.signal_delay_ticks", signal_delay_ticks);
    config.getValueForSetting("streaming.spm.sync_wait_time_msec", sync_wait_time_msec);
    config.getValueForSetting("streaming.spm.cycles_for_startup", cycles_for_startup);
    config.getValueForSetting("streaming.spm.prestart_cycles_for_xmit", prestart_cycles_for_xmit);
    config.getValueForSetting("streaming.spm.prestart_cycles_for_recv", prestart_cycles_for_recv);

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
    max_of_min_delay += signal_delay_ticks;

    m_SyncSource->setSyncDelay(max_of_min_delay);
    unsigned int syncdelay = m_SyncSource->getSyncDelay();
    debugOutput( DEBUG_LEVEL_VERBOSE, " sync delay = %d => %d ticks (%03us %04uc %04ut)...\n", 
        max_of_min_delay, syncdelay,
        (unsigned int)TICKS_TO_SECS(syncdelay),
        (unsigned int)TICKS_TO_CYCLES(syncdelay),
        (unsigned int)TICKS_TO_OFFSET(syncdelay));

    //STEP X: when we implement such a function, we can wait for a signal from the devices that they
    //        have aquired lock
    //debugOutput( DEBUG_LEVEL_VERBOSE, "Waiting for device(s) to indicate clock sync lock...\n");
    //sleep(2); // FIXME: be smarter here

    // make sure that we are dry-running long enough for the
    // DLL to have a decent sync (FIXME: does the DLL get updated when dry-running)?
    debugOutput( DEBUG_LEVEL_VERBOSE, "Waiting for sync...\n");

    unsigned int nb_sync_runs = (sync_wait_time_msec * getNominalRate());
    nb_sync_runs /= 1000;
    nb_sync_runs /= getPeriodSize();

    int64_t time_till_next_period;
    while(nb_sync_runs--) { // or while not sync-ed?
        // check if we were woken up too soon
        time_till_next_period = m_SyncSource->getTimeUntilNextPeriodSignalUsecs();
        debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "waiting for %d usecs...\n", time_till_next_period);
        if(time_till_next_period > 0) {
            // wait for the period
            SleepRelativeUsec(time_till_next_period);
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

    // start wet-running in STREAMPROCESSORMANAGER_CYCLES_FOR_STARTUP cycles
    // this is the time window we have to setup all SP's such that they 
    // can start wet-running correctly.
    time_of_first_sample = addTicks(time_of_first_sample,
                                    cycles_for_startup * TICKS_PER_CYCLE);

    debugOutput( DEBUG_LEVEL_VERBOSE, "  => first sample at TS=%011llu (%03us %04uc %04ut)...\n", 
        time_of_first_sample,
        (unsigned int)TICKS_TO_SECS(time_of_first_sample),
        (unsigned int)TICKS_TO_CYCLES(time_of_first_sample),
        (unsigned int)TICKS_TO_OFFSET(time_of_first_sample));

    // we should start wet-running the transmit SP's some cycles in advance
    // such that we know it is wet-running when it should output its first sample
    uint64_t time_to_start_xmit = substractTicks(time_of_first_sample, 
                                                 prestart_cycles_for_xmit * TICKS_PER_CYCLE);

    uint64_t time_to_start_recv = substractTicks(time_of_first_sample,
                                                 prestart_cycles_for_recv * TICKS_PER_CYCLE);
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

    // switch syncsource to running state
    uint64_t time_to_start_sync;
    // FIXME: this is most likely not going to work for transmit sync sources
    // but those are unsupported in this version
    if(m_SyncSource->getType() == StreamProcessor::ePT_Receive ) {
        time_to_start_sync = time_to_start_recv;
    } else { 
        time_to_start_sync = time_to_start_xmit;
    }
    if(!m_SyncSource->scheduleStartRunning(time_to_start_sync)) {
        debugError("m_SyncSource->scheduleStartRunning(%11llu) failed\n", time_to_start_sync);
        return false;
    }

    // STEP X: switch all non-syncsource SP's over to the running state
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
          it != m_ReceiveProcessors.end();
          ++it ) {
        if(*it != m_SyncSource) {
            if(!(*it)->scheduleStartRunning(time_to_start_recv)) {
                debugError("%p->scheduleStartRunning(%11llu) failed\n", *it, time_to_start_recv);
                return false;
            }
        }
    }
    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
          it != m_TransmitProcessors.end();
          ++it ) {
        if(*it != m_SyncSource) {
            if(!(*it)->scheduleStartRunning(time_to_start_xmit)) {
                debugError("%p->scheduleStartRunning(%11llu) failed\n", *it, time_to_start_xmit);
                return false;
            }
        }
    }
    // wait for the syncsource to start running.
    // that will block the waitForPeriod call until everyone has started (theoretically)
    // note: the SP's are scheduled to start in STREAMPROCESSORMANAGER_CYCLES_FOR_STARTUP cycles,
    // so a 20 times this value should be a good timeout
    int cnt = cycles_for_startup * 20; // by then it should have started
    while (!m_SyncSource->isRunning() && cnt) {
        SleepRelativeUsec(125);
        cnt--;
    }
    if(cnt==0) {
        debugOutput(DEBUG_LEVEL_VERBOSE, " Timeout waiting for the SyncSource to get started\n");
        return false;
    }

    // the sync source is running, we can now read a decent received timestamp from it
    m_time_of_transfer = m_SyncSource->getTimeAtPeriod();

    // and a (still very rough) approximation of the rate
    float rate = m_SyncSource->getTicksPerFrame();
    int64_t delay_in_ticks=(int64_t)(((float)((m_nb_buffers-1) * m_period)) * rate);
    // also add the sync delay
    delay_in_ticks += m_SyncSource->getSyncDelay();
    debugOutput( DEBUG_LEVEL_VERBOSE, "  initial time of transfer %010lld, rate %f...\n",
                m_time_of_transfer, rate);

    // then use this information to initialize the xmit handlers

    //  we now set the buffer tail timestamp of the transmit buffer
    //  to the period transfer time instant plus what's nb_buffers - 1
    //  in ticks. This due to the fact that we (should) have received one period
    //  worth of ticks at t=m_time_of_transfer
    //  hence one period of frames should also have been transmitted, which means
    //  that there should be (nb_buffers - 1) * periodsize of frames in the xmit buffer
    //  that allows us to calculate the tail timestamp for the buffer.

    int64_t transmit_tail_timestamp = addTicks(m_time_of_transfer, delay_in_ticks);

    debugOutput( DEBUG_LEVEL_VERBOSE, "  preset transmit tail TS %010lld, rate %f...\n",
                transmit_tail_timestamp, rate);

    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
        it != m_TransmitProcessors.end();
        ++it ) {
        (*it)->setBufferTailTimestamp(transmit_tail_timestamp);
        (*it)->setTicksPerFrame(rate);
    }

    // align the received streams to be phase aligned
    if(!alignReceivedStreams()) {
        debugError("Could not align streams...\n");
        return false;
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, " StreamProcessor streams running...\n");
    return true;
}

bool
StreamProcessorManager::alignReceivedStreams()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Aligning received streams...\n");
    unsigned int nb_sync_runs;
    unsigned int nb_rcv_sp = m_ReceiveProcessors.size();
    int64_t diff_between_streams[nb_rcv_sp];
    int64_t diff;

    unsigned int i;

    int cnt = STREAMPROCESSORMANAGER_NB_ALIGN_TRIES;
    int align_average_time_msec = STREAMPROCESSORMANAGER_ALIGN_AVERAGE_TIME_MSEC;
    Util::Configuration &config = m_parent.getConfiguration();
    config.getValueForSetting("streaming.spm.align_tries", cnt);
    config.getValueForSetting("streaming.spm.align_average_time_msec", align_average_time_msec);

    unsigned int periods_per_align_try = (align_average_time_msec * getNominalRate());
    periods_per_align_try /= 1000;
    periods_per_align_try /= getPeriodSize();
    debugOutput( DEBUG_LEVEL_VERBOSE, " averaging over %u periods...\n", periods_per_align_try);

    bool aligned = false;
    while (!aligned && cnt--) {
        nb_sync_runs = periods_per_align_try;
        while(nb_sync_runs) {
            debugOutput( DEBUG_LEVEL_VERY_VERBOSE, " check (%d)...\n", nb_sync_runs);
            if(!waitForPeriod()) {
                debugWarning("xrun while aligning streams...\n");
                return false;
            }

            // before we do anything else, transfer
            if(!transferSilence()) {
                debugError("Could not transfer silence\n");
                return false;
            }

            // now calculate the stream offset
            i = 0;
            for ( i = 0; i < nb_rcv_sp; i++) {
                StreamProcessor *s = m_ReceiveProcessors.at(i);
                diff = diffTicks(m_SyncSource->getTimeAtPeriod(), s->getTimeAtPeriod());
                debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "  offset between SyncSP %p and SP %p is %lld ticks...\n", 
                    m_SyncSource, s, diff);
                if ( nb_sync_runs == periods_per_align_try ) {
                    diff_between_streams[i] = diff;
                } else {
                    diff_between_streams[i] += diff;
                }
            }

            nb_sync_runs--;
        }
        // calculate the average offsets
        debugOutput( DEBUG_LEVEL_VERBOSE, " Average offsets:\n");
        int diff_between_streams_frames[nb_rcv_sp];
        aligned = true;
        for ( i = 0; i < nb_rcv_sp; i++) {
            StreamProcessor *s = m_ReceiveProcessors.at(i);

            diff_between_streams[i] /= periods_per_align_try;
            diff_between_streams_frames[i] = (int)roundf(diff_between_streams[i] / s->getTicksPerFrame());
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

    // start all SP's synchonized
    bool start_result = false;
    for (int ntries=0; ntries < STREAMPROCESSORMANAGER_SYNCSTART_TRIES; ntries++) {
        // put all SP's into dry-running state
        if (!startDryRunning()) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Could not put SP's in dry-running state (try %d)\n", ntries);
            start_result = false;
            continue;
        }

        start_result = syncStartAll();
        if(start_result) {
            break;
        } else {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Sync start try %d failed...\n", ntries);
            if(m_shutdown_needed) {
                debugOutput(DEBUG_LEVEL_VERBOSE, "Some fatal error occurred, stop trying.\n");
                return false;
            }
        }
    }
    if (!start_result) {
        debugFatal("Could not syncStartAll...\n");
        return false;
    }
    debugOutput( DEBUG_LEVEL_VERBOSE, " Started...\n");
    return true;
}

bool StreamProcessorManager::stop() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "Stopping...\n");

    debugOutput( DEBUG_LEVEL_VERBOSE, " scheduling stop for all SP's...\n");
    // switch SP's over to the dry-running state
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
          it != m_ReceiveProcessors.end();
          ++it ) {
        if((*it)->isRunning()) {
            if(!(*it)->scheduleStopRunning(-1)) {
                debugError("%p->scheduleStopRunning(-1) failed\n", *it);
                return false;
            }
        }
    }
    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
          it != m_TransmitProcessors.end();
          ++it ) {
        if((*it)->isRunning()) {
            if(!(*it)->scheduleStopRunning(-1)) {
                debugError("%p->scheduleStopRunning(-1) failed\n", *it);
                return false;
            }
        }
    }
    // wait for the SP's to get into the dry-running/stopped state
    int cnt = 8000;
    bool ready = false;
    while (!ready && cnt) {
        ready = true;
        for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
            it != m_ReceiveProcessors.end();
            ++it ) {
            ready &= ((*it)->isDryRunning() || (*it)->isStopped() || (*it)->isWaitingForStream() || (*it)->inError());
        }
        for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
            it != m_TransmitProcessors.end();
            ++it ) {
            ready &= ((*it)->isDryRunning() || (*it)->isStopped() || (*it)->isWaitingForStream() || (*it)->inError());
        }
        SleepRelativeUsec(125);
        cnt--;
    }
    if(cnt==0) {
        debugWarning(" Timeout waiting for the SP's to start dry-running\n");
        for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
            it != m_ReceiveProcessors.end();
            ++it ) {
            (*it)->dumpInfo();
        }
        for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
            it != m_TransmitProcessors.end();
            ++it ) {
            (*it)->dumpInfo();
        }
        return false;
    }

    // switch SP's over to the stopped state
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
          it != m_ReceiveProcessors.end();
          ++it ) {
        if ((*it)->inError()) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "SP %p in error state\n", *it);
        } else if(!(*it)->scheduleStopDryRunning(-1)) {
            debugError("%p->scheduleStopDryRunning(-1) failed\n", *it);
            return false;
        }
    }
    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
          it != m_TransmitProcessors.end();
          ++it ) {
        if ((*it)->inError()) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "SP %p in error state\n", *it);
        } else if(!(*it)->scheduleStopDryRunning(-1)) {
            debugError("%p->scheduleStopDryRunning(-1) failed\n", *it);
            return false;
        }
    }
    // wait for the SP's to get into the stopped state
    cnt = 8000;
    ready = false;
    while (!ready && cnt) {
        ready = true;
        for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
            it != m_ReceiveProcessors.end();
            ++it ) {
            ready &= ((*it)->isStopped() || (*it)->inError());
        }
        for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
            it != m_TransmitProcessors.end();
            ++it ) {
            ready &= ((*it)->isStopped() || (*it)->inError());
        }
        SleepRelativeUsec(125);
        cnt--;
    }
    if(cnt==0) {
        debugWarning(" Timeout waiting for the SP's to stop\n");
        for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
            it != m_ReceiveProcessors.end();
            ++it ) {
            (*it)->dumpInfo();
        }
        for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
            it != m_TransmitProcessors.end();
            ++it ) {
            (*it)->dumpInfo();
        }
        return false;
    }
    debugOutput( DEBUG_LEVEL_VERBOSE, " Stopped...\n");
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

    debugOutput( DEBUG_LEVEL_VERBOSE, "Restarting StreamProcessors...\n");
    // start all SP's synchonized
    bool start_result = false;
    for (int ntries=0; ntries < STREAMPROCESSORMANAGER_SYNCSTART_TRIES; ntries++) {
        if(m_shutdown_needed) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Shutdown requested...\n");
            return true;
        }
        // put all SP's into dry-running state
        if (!startDryRunning()) {
            debugShowBackLog();
            debugOutput(DEBUG_LEVEL_VERBOSE, "Could not put SP's in dry-running state (try %d)\n", ntries);
            start_result = false;
            continue;
        }

        start_result = syncStartAll();
        if(start_result) {
            break;
        } else {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Sync start try %d failed...\n", ntries);
        }
    }
    if (!start_result) {
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
 * @return true if the period is ready, false if not
 */
bool StreamProcessorManager::waitForPeriod() {
    if(m_SyncSource == NULL) return false;
    if(m_shutdown_needed) return false;
    bool xrun_occurred = false;
    bool in_error = false;

    // grab the wait lock
    // this ensures that bus reset handling doesn't interfere
    Util::MutexLockHelper lock(*m_WaitLock);
    debugOutputExtreme(DEBUG_LEVEL_VERBOSE,
                        "waiting for period (%d frames in buffer)...\n",
                        m_SyncSource->getBufferFill());
    uint64_t ticks_at_period = m_SyncSource->getTimeAtPeriod();
    uint64_t ticks_at_period_margin = ticks_at_period + m_SyncSource->getSyncDelay();
    uint64_t pred_system_time_at_xfer = m_SyncSource->getParent().get1394Service().getSystemTimeForCycleTimerTicks(ticks_at_period_margin);

    #ifdef DEBUG
    int64_t now = Util::SystemTimeSource::getCurrentTime();
    debugOutputExtreme(DEBUG_LEVEL_VERBOSE, "pred: %lld, now: %lld, wait: %lld\n", pred_system_time_at_xfer, now, pred_system_time_at_xfer-now );
    #endif

    // wait until it's time to transfer
    Util::SystemTimeSource::SleepUsecAbsolute(pred_system_time_at_xfer);

    #ifdef DEBUG
    now = Util::SystemTimeSource::getCurrentTime();
    debugOutputExtreme(DEBUG_LEVEL_VERBOSE, "pred: %lld now: %lld, excess: %lld\n", pred_system_time_at_xfer, now, now-pred_system_time_at_xfer );
    #endif

    // the period should be ready now

    #if STREAMPROCESSORMANAGER_ALLOW_DELAYED_PERIOD_SIGNAL
    // HACK: we force wait until every SP is ready. this is needed
    // since the raw1394 interface provides no control over interrupts
    // resulting in very bad predictability on when the data is present.
    bool period_not_ready = true;
    while(period_not_ready) {
        period_not_ready = false;
        for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
            it != m_ReceiveProcessors.end();
            ++it ) {
            bool this_sp_period_ready = (*it)->canConsumePeriod();
            if (!this_sp_period_ready) {
                period_not_ready = true;
            }
        }
        for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
            it != m_TransmitProcessors.end();
            ++it ) {
            bool this_sp_period_ready = (*it)->canProducePeriod();
            if (!this_sp_period_ready) {
                period_not_ready = true;
            }
        }

        if (period_not_ready) {
            debugOutput(DEBUG_LEVEL_VERBOSE, " wait extended since period not ready...\n");
            Util::SystemTimeSource::SleepUsecRelative(125); // one cycle
        }

        // check for underruns/errors on the ISO side,
        // those should make us bail out of the wait loop
        for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
            it != m_ReceiveProcessors.end();
            ++it ) {
            // a xrun has occurred on the Iso side
            xrun_occurred |= (*it)->xrunOccurred();
            in_error |= (*it)->inError();
        }
        for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
            it != m_TransmitProcessors.end();
            ++it ) {
            // a xrun has occurred on the Iso side
            xrun_occurred |= (*it)->xrunOccurred();
            in_error |= (*it)->inError();
        }
        if(xrun_occurred | in_error | m_shutdown_needed) break;
    }
    #else
    // check for underruns/errors on the ISO side,
    // those should make us bail out of the wait loop
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
        it != m_ReceiveProcessors.end();
        ++it ) {
        // xrun on data buffer side
        if (!(*it)->canConsumePeriod()) {
            xrun_occurred = true;
        }
        // a xrun has occurred on the Iso side
        xrun_occurred |= (*it)->xrunOccurred();
        in_error |= (*it)->inError();
    }
    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
        it != m_TransmitProcessors.end();
        ++it ) {
        // xrun on data buffer side
        if (!(*it)->canProducePeriod()) {
            xrun_occurred = true;
        }
        // a xrun has occurred on the Iso side
        xrun_occurred |= (*it)->xrunOccurred();
        in_error |= (*it)->inError();
    }
    #endif

    if(xrun_occurred) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "exit due to xrun...\n");
    }
    if(in_error) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "exit due to error...\n");
        m_shutdown_needed = true;
    }

    // we save the 'ideal' time of the transfer at this point,
    // because we can have interleaved read - process - write
    // cycles making that we modify a receiving stream's buffer
    // before we get to writing.
    // NOTE: before waitForPeriod() is called again, both the transmit
    //       and the receive processors should have done their transfer.
    m_time_of_transfer = m_SyncSource->getTimeAtPeriod();
    
    #ifdef DEBUG
    static uint64_t m_time_of_transfer2 = m_time_of_transfer;
    
    int ticks_per_period = (int)(m_SyncSource->getTicksPerFrame() * m_period);
    int diff=diffTicks(m_time_of_transfer, m_time_of_transfer2);
    // display message if the difference between two successive tick
    // values is more than 50 ticks. 1 sample at 48k is 512 ticks
    // so 50 ticks = 10%, which is a rather large jitter value.
    if(diff-ticks_per_period > 50 || diff-ticks_per_period < -50) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "rather large TSP difference TS=%011llu => TS=%011llu (%d, nom %d)\n",
                                            m_time_of_transfer2, m_time_of_transfer, diff, ticks_per_period);
    }
    m_time_of_transfer2 = m_time_of_transfer;
    #endif

    debugOutputExtreme(DEBUG_LEVEL_VERBOSE,
                        "transfer period %d at %llu ticks...\n",
                        m_nbperiods, m_time_of_transfer);

    // this is to notify the client of the delay that we introduced by waiting
    m_delayed_usecs = - m_SyncSource->getTimeUntilNextPeriodSignalUsecs();
    debugOutputExtreme(DEBUG_LEVEL_VERBOSE,
                        "delayed for %d usecs...\n",
                        m_delayed_usecs);

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
    debugOutputExtreme( DEBUG_LEVEL_VERY_VERBOSE, 
                        "XF at %011llu ticks, RBF=%d, XBF=%d, SUM=%d...\n",
                        m_time_of_transfer, rcv_bf, xmt_bf, rcv_bf+xmt_bf);

    // check if xruns occurred on the Iso side.
    // also check if xruns will occur should we transfer() now
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
          it != m_ReceiveProcessors.end();
          ++it ) {

        if ((*it)->xrunOccurred()) {
            debugOutput(DEBUG_LEVEL_NORMAL,
                        "Xrun on RECV SP %p due to ISO side xrun\n", *it);
            (*it)->dumpInfo();
        }
        if (!((*it)->canClientTransferFrames(m_period))) {
            debugOutput(DEBUG_LEVEL_NORMAL,
                        "Xrun on RECV SP %p due to buffer side xrun\n", *it);
            (*it)->dumpInfo();
        }
    }
    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
          it != m_TransmitProcessors.end();
          ++it ) {
        if ((*it)->xrunOccurred()) {
            debugOutput(DEBUG_LEVEL_NORMAL,
                        "Xrun on XMIT SP %p due to ISO side xrun\n", *it);
        }
        if (!((*it)->canClientTransferFrames(m_period))) {
            debugOutput(DEBUG_LEVEL_NORMAL,
                        "Xrun on XMIT SP %p due to buffer side xrun\n", *it);
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
    debugOutputExtreme( DEBUG_LEVEL_VERY_VERBOSE, "Transferring period...\n");
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
    if(m_SyncSource == NULL) return false;
    debugOutputExtreme( DEBUG_LEVEL_VERY_VERBOSE,
        "transfer(%d) at TS=%011llu (%03us %04uc %04ut)...\n", 
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
        int64_t one_ringbuffer_in_ticks=(int64_t)(((float)((m_nb_buffers * m_period))) * rate);

        // the data we are putting into the buffer is intended to be transmitted
        // one ringbuffer size after it has been received

        // we also add one syncdelay as a safety margin, since that's the amount of time we can get
        // postponed.
        int syncdelay = m_SyncSource->getSyncDelay();
        int64_t transmit_timestamp = addTicks(m_time_of_transfer, one_ringbuffer_in_ticks + syncdelay);

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
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "Transferring silent period...\n");
    bool retval=true;
    // NOTE: the order here is opposite from the order in
    // normal operation (transmit is before receive), because
    // we can do that here (data=silence=available) and 
    // it increases reliability (esp. on startup)
    retval &= transferSilence(StreamProcessor::ePT_Transmit);
    retval &= transferSilence(StreamProcessor::ePT_Receive);
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
    if(m_SyncSource == NULL) return false;
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE,
        "transferSilence(%d) at TS=%011llu (%03us %04uc %04ut)...\n", 
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
        // we also add one syncdelay as a safety margin, since that's the amount of time we can get
        // postponed.
        int syncdelay = m_SyncSource->getSyncDelay();
        int64_t transmit_timestamp = addTicks(m_time_of_transfer, one_ringbuffer_in_ticks + syncdelay);

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
    debugOutputShort( DEBUG_LEVEL_NORMAL, "Data type: %s\n", (m_audio_datatype==eADT_Float?"float":"int24"));

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

    debugOutputShort( DEBUG_LEVEL_NORMAL, "----------------------------------------------------\n");

    // list port info in verbose mode
    debugOutputShort( DEBUG_LEVEL_VERBOSE, "Port Information\n");
    int nb_ports;
    
    debugOutputShort( DEBUG_LEVEL_VERBOSE, " Playback\n");
    nb_ports = getPortCount(Port::E_Playback);
    for(int i=0; i < nb_ports; i++) {
        Port *p = getPortByIndex(i, Port::E_Playback);
        debugOutputShort( DEBUG_LEVEL_VERBOSE, "  %3d (%p): ", i, p);
        if (p) {
            bool disabled = p->isDisabled();
            debugOutputShort( DEBUG_LEVEL_VERBOSE, "[%p] [%3s] ", &p->getManager(), (disabled?"off":"on"));
            debugOutputShort( DEBUG_LEVEL_VERBOSE, "[%7s] ", p->getPortTypeName().c_str());
            debugOutputShort( DEBUG_LEVEL_VERBOSE, "%3s ", p->getName().c_str());
        } else {
            debugOutputShort( DEBUG_LEVEL_VERBOSE, "invalid ");
        }
        debugOutputShort( DEBUG_LEVEL_VERBOSE, "\n");
    }
    debugOutputShort( DEBUG_LEVEL_VERBOSE, " Capture\n");
    nb_ports = getPortCount(Port::E_Capture);
    for(int i=0; i < nb_ports; i++) {
        Port *p = getPortByIndex(i, Port::E_Capture);
        debugOutputShort( DEBUG_LEVEL_VERBOSE, "  %3d (%p): ", i, p);
        if (p) {
            bool disabled = p->isDisabled();
            debugOutputShort( DEBUG_LEVEL_VERBOSE, "[%p] [%3s] ", &p->getManager(), (disabled?"off":"on"));
            debugOutputShort( DEBUG_LEVEL_VERBOSE, "[%7s] ", p->getPortTypeName().c_str());
            debugOutputShort( DEBUG_LEVEL_VERBOSE, " %3s ", p->getName().c_str());
        } else {
            debugOutputShort( DEBUG_LEVEL_VERBOSE, " invalid ");
        }
        debugOutputShort( DEBUG_LEVEL_VERBOSE, "\n");
    }

    debugOutputShort( DEBUG_LEVEL_VERBOSE, "----------------------------------------------------\n");

}

void StreamProcessorManager::setVerboseLevel(int l) {
    if(m_WaitLock) m_WaitLock->setVerboseLevel(l);

    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
        it != m_ReceiveProcessors.end();
        ++it ) {
        (*it)->setVerboseLevel(l);
    }
    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
        it != m_TransmitProcessors.end();
        ++it ) {
        (*it)->setVerboseLevel(l);
    }
    setDebugLevel(l);
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting verbose level to %d...\n", l );
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

void
StreamProcessorManager::updateShadowLists()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Updating port shadow lists...\n");
    m_CapturePorts_shadow.clear();
    m_PlaybackPorts_shadow.clear();

    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
        it != m_ReceiveProcessors.end();
        ++it ) {
        PortManager *pm = *it;
        for (int i=0; i < pm->getPortCount(); i++) {
            Port *p = pm->getPortAtIdx(i);
            if (!p) {
                debugError("getPortAtIdx(%d) returned NULL\n", i);
                continue;
            }
            if(p->getDirection() != Port::E_Capture) {
                debugError("port at idx %d for receive SP is not a capture port!\n", i);
                continue;
            }
            m_CapturePorts_shadow.push_back(p);
        }
    }
    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
        it != m_TransmitProcessors.end();
        ++it ) {
        PortManager *pm = *it;
        for (int i=0; i < pm->getPortCount(); i++) {
            Port *p = pm->getPortAtIdx(i);
            if (!p) {
                debugError("getPortAtIdx(%d) returned NULL\n", i);
                continue;
            }
            if(p->getDirection() != Port::E_Playback) {
                debugError("port at idx %d for transmit SP is not a playback port!\n", i);
                continue;
            }
            m_PlaybackPorts_shadow.push_back(p);
        }
    }
}

Port* StreamProcessorManager::getPortByIndex(int idx, enum Port::E_Direction direction) {
    debugOutputExtreme( DEBUG_LEVEL_ULTRA_VERBOSE, "getPortByIndex(%d, %d)...\n", idx, direction);
    if (direction == Port::E_Capture) {
        #ifdef DEBUG
        if(idx >= (int)m_CapturePorts_shadow.size()) {
            debugError("Capture port %d out of range (%d)\n", idx, m_CapturePorts_shadow.size());
            return NULL;
        }
        #endif
        return m_CapturePorts_shadow.at(idx);
    } else {
        #ifdef DEBUG
        if(idx >= (int)m_PlaybackPorts_shadow.size()) {
            debugError("Playback port %d out of range (%d)\n", idx, m_PlaybackPorts_shadow.size());
            return NULL;
        }
        #endif
        return m_PlaybackPorts_shadow.at(idx);
    }
    return NULL;
}

bool StreamProcessorManager::setThreadParameters(bool rt, int priority) {
    m_thread_realtime=rt;
    m_thread_priority=priority;
    return true;
}


} // end of namespace
