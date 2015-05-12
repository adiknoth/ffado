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

#include "IsoHandlerManager.h"
#include "ieee1394service.h"
#include "cycletimer.h"
#include "libstreaming/generic/StreamProcessor.h"

#include "libutil/Atomic.h"
#include "libutil/PosixThread.h"
#include "libutil/SystemTimeSource.h"
#include "libutil/Watchdog.h"
#include "libutil/Configuration.h"

#include <cstring>
#include <unistd.h>
#include <assert.h>

IMPL_DEBUG_MODULE( IsoHandlerManager, IsoHandlerManager, DEBUG_LEVEL_NORMAL );
IMPL_DEBUG_MODULE( IsoHandlerManager::IsoTask, IsoTask, DEBUG_LEVEL_NORMAL );
IMPL_DEBUG_MODULE( IsoHandlerManager::IsoHandler, IsoHandler, DEBUG_LEVEL_NORMAL );

using namespace Streaming;

// --- ISO Thread --- //

IsoHandlerManager::IsoTask::IsoTask(IsoHandlerManager& manager, enum IsoHandler::EHandlerType t)
    : m_manager( manager )
    , m_SyncIsoHandler ( NULL )
    , m_handlerType( t )
    , m_running( false )
    , m_in_busreset( false )
    , m_activity_wait_timeout_nsec (ISOHANDLERMANAGER_ISO_TASK_WAIT_TIMEOUT_USECS * 1000LL)
{
}

IsoHandlerManager::IsoTask::~IsoTask()
{
    sem_destroy(&m_activity_semaphore);
}

bool
IsoHandlerManager::IsoTask::Init()
{
    request_update = 0;

    int i;
    for (i=0; i < ISOHANDLERMANAGER_MAX_ISO_HANDLERS_PER_PORT; i++) {
        m_IsoHandler_map_shadow[i] = NULL;
        m_poll_fds_shadow[i].events = 0;
    }
    m_poll_nfds_shadow = 0;

    #ifdef DEBUG
    m_last_loop_entry = 0;
    m_successive_short_loops = 0;
    #endif

    sem_init(&m_activity_semaphore, 0, 0);
    m_running = true;
    return true;
}

void
IsoHandlerManager::IsoTask::requestShadowMapUpdate()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "(%p) enter\n", this);
    INC_ATOMIC(&request_update);

    // get the thread going again
    signalActivity();
    debugOutput(DEBUG_LEVEL_VERBOSE, "(%p) exit\n", this);
}

bool
IsoHandlerManager::IsoTask::handleBusReset()
{
    bool retval = true;
    if(!m_running) {
        // nothing to do here
        return true;
    }
    m_in_busreset = true;
    requestShadowMapUpdate();

    unsigned int i, max;
    max = m_manager.m_IsoHandlers.size();
    for (i = 0; i < max; i++) {
        IsoHandler *h = m_manager.m_IsoHandlers.at(i);
        assert(h);

        // skip the handlers not intended for us
        if(h->getType() != m_handlerType) continue;

        if (!h->handleBusReset()) {
            debugWarning("Failed to handle busreset on %p\n", h);
            retval = false;
        }
    }

    // re-enable processing
    m_in_busreset = false;
    requestShadowMapUpdate();
    return retval;
}

// updates the internal stream map
// note that this should be executed with the guarantee that
// nobody will modify the parent data structures
void
IsoHandlerManager::IsoTask::updateShadowMapHelper()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "(%p) updating shadow vars...\n", this);
    // we are handling a busreset
    if(m_in_busreset) {
        m_poll_nfds_shadow = 0;
        return;
    }
    unsigned int i, cnt, max;
    max = m_manager.m_IsoHandlers.size();
    m_SyncIsoHandler = NULL;
    for (i = 0, cnt = 0; i < max; i++) {

        // FIXME: This is a very crude guard against some other thread
        // deleting handlers while this function is running.  While this
        // didn't tend to happen with the old kernel firewire stack, delays
        // in shutdown experienced in the new stack mean it can happen that
        // a handler disappears during the running of this function.  This
        // test should prevent "out of range" exceptions in most cases. 
        // However, it is racy: if the deletion happens between this
        // conditional and the following at() call, an out of range
        // condition can still happen.
        if (i>=m_manager.m_IsoHandlers.size())
            continue;

        IsoHandler *h = m_manager.m_IsoHandlers.at(i);
        assert(h);

        // skip the handlers not intended for us
        if(h->getType() != m_handlerType) continue;

        // update the state of the handler
        // FIXME: maybe this is not the best place to do this
        // it might be better to eliminate the 'requestShadowMapUpdate'
        // entirely and replace it with a mechanism that implements all
        // actions on the m_manager.m_IsoHandlers in the loop
        h->updateState();

        // rebuild the map
        if (h->isEnabled()) {
            m_IsoHandler_map_shadow[cnt] = h;
            m_poll_fds_shadow[cnt].fd = h->getFileDescriptor();
            m_poll_fds_shadow[cnt].revents = 0;
            m_poll_fds_shadow[cnt].events = POLLIN;
            cnt++;
            // FIXME: need a more generic approach here
            if(   m_SyncIsoHandler == NULL
               && h->getType() == IsoHandler::eHT_Transmit) {
                m_SyncIsoHandler = h;
            }

            debugOutput( DEBUG_LEVEL_VERBOSE, "(%p) %s handler %p added\n",
                                              this, h->getTypeString(), h);
        } else {
            debugOutput( DEBUG_LEVEL_VERBOSE, "(%p) %s handler %p skipped (disabled)\n",
                                              this, h->getTypeString(), h);
        }
        if(cnt > ISOHANDLERMANAGER_MAX_ISO_HANDLERS_PER_PORT) {
            debugWarning("Too much ISO Handlers in thread...\n");
            break;
        }
    }

    // FIXME: need a more generic approach here
    // if there are no active transmit handlers,
    // use the first receive handler
    if(   m_SyncIsoHandler == NULL
       && m_poll_nfds_shadow) {
        m_SyncIsoHandler = m_IsoHandler_map_shadow[0];
    }
    m_poll_nfds_shadow = cnt;
    debugOutput( DEBUG_LEVEL_VERBOSE, "(%p) updated shadow vars...\n", this);
}

bool
IsoHandlerManager::IsoTask::Execute()
{
    debugOutput(DEBUG_LEVEL_ULTRA_VERBOSE,
                "(%p, %s) Execute\n",
                this, (m_handlerType == IsoHandler::eHT_Transmit? "Transmit": "Receive"));
    int err;
    unsigned int i;
    unsigned int m_poll_timeout = 10;

    #ifdef DEBUG
    uint64_t now = Util::SystemTimeSource::getCurrentTimeAsUsecs();
    int diff = now - m_last_loop_entry;
    if(diff < 100) {
        debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                           "(%p, %s) short loop detected (%d usec), cnt: %d\n",
                           this, (m_handlerType == IsoHandler::eHT_Transmit? "Transmit": "Receive"),
                           diff, m_successive_short_loops);
        m_successive_short_loops++;
        if(m_successive_short_loops > 10000) {
            debugError("Shutting down runaway thread\n");
            m_running = false;
            return false;
        }
    } else {
        // reset the counter
        m_successive_short_loops = 0;
    }
    m_last_loop_entry = now;
    #endif

    // if some other thread requested a shadow map update, do it
    if(request_update) {
        updateShadowMapHelper();
        DEC_ATOMIC(&request_update); // ack the update
        assert(request_update >= 0);
    }

    // bypass if no handlers are registered
    if (m_poll_nfds_shadow == 0) {
        debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                           "(%p, %s) bypass iterate since no handlers to poll\n",
                           this, (m_handlerType == IsoHandler::eHT_Transmit? "Transmit": "Receive"));
        usleep(m_poll_timeout * 1000);
        return true;
    }

    // FIXME: what can happen is that poll() returns, but not all clients are
    // ready. there might be some busy waiting behavior that still has to be solved.

    // setup the poll here
    // we should prevent a poll() where no events are specified, since that will only time-out
    bool no_one_to_poll = true;
    while(no_one_to_poll) {
        for (i = 0; i < m_poll_nfds_shadow; i++) {
            short events = 0;
            IsoHandler *h = m_IsoHandler_map_shadow[i];
            // we should only poll on a transmit handler
            // that has a client that is ready to send
            // something. Otherwise it will end up in
            // busy wait looping since the packet function
            // will defer processing (also avoids the
            // AGAIN problem)
            if (h->canIterateClient()) {
                events = POLLIN | POLLPRI;
                no_one_to_poll = false;
            }
            m_poll_fds_shadow[i].events = events;
        }

        if(no_one_to_poll) {
            debugOutputExtreme(DEBUG_LEVEL_VERBOSE,
                        "(%p, %s) No one to poll, waiting for something to happen\n",
                        this, (m_handlerType == IsoHandler::eHT_Transmit? "Transmit": "Receive"));
            // wait for something to happen
            switch(waitForActivity()) {
                case IsoHandlerManager::IsoTask::eAR_Error:
                    debugError("Error while waiting for activity\n");
                    return false;
                case IsoHandlerManager::IsoTask::eAR_Interrupted:
                    // FIXME: what to do here?
                    debugWarning("Interrupted while waiting for activity\n");
                    break;
                case IsoHandlerManager::IsoTask::eAR_Timeout:
                    // FIXME: what to do here?
                    debugWarning("Timeout while waiting for activity\n");
                    no_one_to_poll = false; // exit the loop to be able to detect failing handlers
                    break;
                case IsoHandlerManager::IsoTask::eAR_Activity:
                    // do nothing
                    debugOutputExtreme(DEBUG_LEVEL_VERBOSE,
                                       "(%p, %s) something happened\n",
                                       this, (m_handlerType == IsoHandler::eHT_Transmit? "Transmit": "Receive"));
                    break;
            }
        }
    }

    // Use a shadow map of the fd's such that we don't have to update
    // the fd map everytime we run poll().
    err = poll (m_poll_fds_shadow, m_poll_nfds_shadow, m_poll_timeout);
    uint32_t ctr_at_poll_return = m_manager.get1394Service().getCycleTimer();

    if (err < 0) {
        if (errno == EINTR) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Ignoring poll return due to signal\n");
            return true;
        }
        debugFatal("poll error: %s\n", strerror (errno));
        m_running = false;
        return false;
    }

    // find handlers that have died
    uint64_t ctr_at_poll_return_ticks = CYCLE_TIMER_TO_TICKS(ctr_at_poll_return);
    bool handler_died = false;
    for (i = 0; i < m_poll_nfds_shadow; i++) {
        // figure out if a handler has died

        // this is the time of the last packet we saw in the iterate() handler
        uint32_t last_packet_seen = m_IsoHandler_map_shadow[i]->getLastPacketTime();
        if (last_packet_seen == 0xFFFFFFFF) {
            // this was not iterated yet, so can't be dead
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
                        "(%p, %s) handler %d didn't see any packets yet\n",
                        this, (m_handlerType == IsoHandler::eHT_Transmit? "Transmit": "Receive"), i);
            continue;
        }

        uint64_t last_packet_seen_ticks = CYCLE_TIMER_TO_TICKS(last_packet_seen);
        // we use a relatively large value to distinguish between "death" and xrun
        int64_t max_diff_ticks = TICKS_PER_SECOND * 2;
        int64_t measured_diff_ticks = diffTicks(ctr_at_poll_return_ticks, last_packet_seen_ticks);

        debugOutputExtreme(DEBUG_LEVEL_VERBOSE,
                           "(%p, %s) check handler %d: diff = %"PRId64", max = %"PRId64", now: %08X, last: %08X\n",
                           this, (m_handlerType == IsoHandler::eHT_Transmit? "Transmit": "Receive"), 
                           i, measured_diff_ticks, max_diff_ticks, ctr_at_poll_return, last_packet_seen);
        if(measured_diff_ticks > max_diff_ticks) {
            debugFatal("(%p, %s) Handler died: now: %08X, last: %08X, diff: %"PRId64" (max: %"PRId64")\n",
                       this, (m_handlerType == IsoHandler::eHT_Transmit? "Transmit": "Receive"),
                       ctr_at_poll_return, last_packet_seen, measured_diff_ticks, max_diff_ticks);
            m_IsoHandler_map_shadow[i]->notifyOfDeath();
            handler_died = true;
        }
    }

    if(handler_died) {
        m_running = false;
        return false; // one or more handlers have died
    }

    // iterate the handlers
    for (i = 0; i < m_poll_nfds_shadow; i++) {
        #ifdef DEBUG
        if(m_poll_fds_shadow[i].revents) {
            debugOutputExtreme(DEBUG_LEVEL_VERBOSE,
                        "(%p, %s) received events: %08X for (%d/%d, %p, %s)\n",
                        this, (m_handlerType == IsoHandler::eHT_Transmit? "Transmit": "Receive"),
                        m_poll_fds_shadow[i].revents,
                        i, m_poll_nfds_shadow,
                        m_IsoHandler_map_shadow[i],
                        m_IsoHandler_map_shadow[i]->getTypeString());
        }
        #endif

        // if we get here, it means two things:
        // 1) the kernel can accept or provide packets (poll returned POLLIN)
        // 2) the client can provide or accept packets (since we enabled polling)
        if(m_poll_fds_shadow[i].revents & (POLLIN)) {
            m_IsoHandler_map_shadow[i]->iterate(ctr_at_poll_return);
        } else {
            // there might be some error condition
            if (m_poll_fds_shadow[i].revents & POLLERR) {
                debugWarning("(%p) error on fd for %d\n", this, i);
            }
            if (m_poll_fds_shadow[i].revents & POLLHUP) {
                debugWarning("(%p) hangup on fd for %d\n", this, i);
            }
        }
    }
    return true;
}

enum IsoHandlerManager::IsoTask::eActivityResult
IsoHandlerManager::IsoTask::waitForActivity()
{
    debugOutputExtreme(DEBUG_LEVEL_VERBOSE,
                       "(%p, %s) waiting for activity\n",
                       this, (m_handlerType == IsoHandler::eHT_Transmit? "Transmit": "Receive"));
    struct timespec ts;
    int result;

    // sem_timedwait() cannot be set up to use any clock rather than 
    // CLOCK_REALTIME.  Therefore we get the time from CLOCK_REALTIME here.
    // Doing this rather than Util::SystemTimeSource::clockGettime() doesn't
    // pose a problem here because the resulting time is only used with
    // sem_timedwait() to implement timeout functionality.
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        debugError("clock_gettime failed\n");
        return eAR_Error;
    }

    ts.tv_nsec += m_activity_wait_timeout_nsec;
    while(ts.tv_nsec >= 1000000000LL) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000LL;
    }

    result = sem_timedwait(&m_activity_semaphore, &ts);

    if(result != 0) {
        if (errno == ETIMEDOUT) {
            debugOutput(DEBUG_LEVEL_VERBOSE,
                        "(%p) sem_timedwait() timed out (result=%d)\n",
                        this, result);
            return eAR_Timeout;
        } else if (errno == EINTR) {
            debugOutput(DEBUG_LEVEL_VERBOSE,
                        "(%p) sem_timedwait() interrupted by signal (result=%d)\n",
                        this, result);
            return eAR_Interrupted;
        } else if (errno == EINVAL) {
            debugError("(%p) sem_timedwait error (result=%d errno=EINVAL)\n", 
                        this, result);
            debugError("(%p) timeout_nsec=%lld ts.sec=%"PRId64" ts.nsec=%"PRId64"\n", 
                       this, m_activity_wait_timeout_nsec, 
		       (int64_t)ts.tv_sec, (int64_t)ts.tv_nsec);
            return eAR_Error;
        } else {
            debugError("(%p) sem_timedwait error (result=%d errno=%d)\n", 
                        this, result, errno);
            debugError("(%p) timeout_nsec=%lld ts.sec=%"PRId64" ts.nsec=%"PRId64"\n", 
                       this, m_activity_wait_timeout_nsec,
		       (int64_t)ts.tv_sec, (int64_t)ts.tv_nsec);
            return eAR_Error;
        }
    }

    debugOutput(DEBUG_LEVEL_ULTRA_VERBOSE,
                "(%p, %s) got activity\n",
                this, (m_handlerType == IsoHandler::eHT_Transmit? "Transmit": "Receive"));
    return eAR_Activity;
}

void
IsoHandlerManager::IsoTask::signalActivity()
{
    // signal the activity cond var
    sem_post(&m_activity_semaphore);
    debugOutput(DEBUG_LEVEL_ULTRA_VERBOSE,
                "(%p, %s) activity\n",
                this, (m_handlerType == IsoHandler::eHT_Transmit? "Transmit": "Receive"));
}

void IsoHandlerManager::IsoTask::setVerboseLevel(int i) {
    setDebugLevel(i);
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting verbose level to %d...\n", i );
}

// -- the ISO handler manager -- //
IsoHandlerManager::IsoHandlerManager(Ieee1394Service& service)
   : m_State(E_Created)
   , m_service( service )
   , m_realtime(false), m_priority(0)
   , m_IsoThreadTransmit ( NULL )
   , m_IsoTaskTransmit ( NULL )
   , m_IsoThreadReceive ( NULL )
   , m_IsoTaskReceive ( NULL )
{
}

IsoHandlerManager::IsoHandlerManager(Ieee1394Service& service, bool run_rt, int rt_prio)
   : m_State(E_Created)
   , m_service( service )
   , m_realtime(run_rt), m_priority(rt_prio)
   , m_IsoThreadTransmit ( NULL )
   , m_IsoTaskTransmit ( NULL )
   , m_IsoThreadReceive ( NULL )
   , m_IsoTaskReceive ( NULL )
   , m_MissedCyclesOK ( false )
{
}

IsoHandlerManager::~IsoHandlerManager()
{
    stopHandlers();
    pruneHandlers();
    if(m_IsoHandlers.size() > 0) {
        debugError("Still some handlers in use\n");
    }
    if (m_IsoThreadTransmit) {
        m_IsoThreadTransmit->Stop();
        delete m_IsoThreadTransmit;
    }
    if (m_IsoThreadReceive) {
        m_IsoThreadReceive->Stop();
        delete m_IsoThreadReceive;
    }
    if (m_IsoTaskTransmit) {
        delete m_IsoTaskTransmit;
    }
    if (m_IsoTaskReceive) {
        delete m_IsoTaskReceive;
    }
}

bool
IsoHandlerManager::handleBusReset()
{
    debugOutput( DEBUG_LEVEL_NORMAL, "bus reset...\n");
    // A few things can happen on bus reset:
    // 1) no devices added/removed => streams are still valid, but might have to be restarted
    // 2) a device was removed => some streams become invalid
    // 3) a device was added => same as 1, new device is ignored
    if (!m_IsoTaskTransmit) {
        debugError("No xmit task\n");
        return false;
    }
    if (!m_IsoTaskReceive) {
        debugError("No receive task\n");
        return false;
    }
    if (!m_IsoTaskTransmit->handleBusReset()) {
        debugWarning("could no handle busreset on xmit\n");
    }
    if (!m_IsoTaskReceive->handleBusReset()) {
        debugWarning("could no handle busreset on recv\n");
    }
    return true;
}

void
IsoHandlerManager::requestShadowMapUpdate()
{
    if(m_IsoTaskTransmit) m_IsoTaskTransmit->requestShadowMapUpdate();
    if(m_IsoTaskReceive) m_IsoTaskReceive->requestShadowMapUpdate();
}

bool
IsoHandlerManager::setThreadParameters(bool rt, int priority) {
    debugOutput( DEBUG_LEVEL_VERBOSE, "(%p) switch to: (rt=%d, prio=%d)...\n", this, rt, priority);
    if (priority > THREAD_MAX_RTPRIO) priority = THREAD_MAX_RTPRIO; // cap the priority
    if (priority < THREAD_MIN_RTPRIO) priority = THREAD_MIN_RTPRIO; // cap the priority
    m_realtime = rt;
    m_priority = priority;

    // grab the options from the parent
    Util::Configuration *config = m_service.getConfiguration();
    int ihm_iso_prio_increase = ISOHANDLERMANAGER_ISO_PRIO_INCREASE;
    int ihm_iso_prio_increase_xmit = ISOHANDLERMANAGER_ISO_PRIO_INCREASE_XMIT;
    int ihm_iso_prio_increase_recv = ISOHANDLERMANAGER_ISO_PRIO_INCREASE_RECV;
    if(config) {
        config->getValueForSetting("ieee1394.isomanager.prio_increase", ihm_iso_prio_increase);
        config->getValueForSetting("ieee1394.isomanager.prio_increase_xmit", ihm_iso_prio_increase_xmit);
        config->getValueForSetting("ieee1394.isomanager.prio_increase_recv", ihm_iso_prio_increase_recv);
    }

    if (m_IsoThreadTransmit) {
        if (m_realtime) {
            m_IsoThreadTransmit->AcquireRealTime(m_priority
                                                 + ihm_iso_prio_increase
                                                 + ihm_iso_prio_increase_xmit);
        } else {
            m_IsoThreadTransmit->DropRealTime();
        }
    }
    if (m_IsoThreadReceive) {
        if (m_realtime) {
            m_IsoThreadReceive->AcquireRealTime(m_priority 
                                                + ihm_iso_prio_increase
                                                + ihm_iso_prio_increase_recv);
        } else {
            m_IsoThreadReceive->DropRealTime();
        }
    }

    return true;
}

bool IsoHandlerManager::init()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Initializing ISO manager %p...\n", this);
    // check state
    if(m_State != E_Created) {
        debugError("Manager already initialized...\n");
        return false;
    }

    // grab the options from the parent
    Util::Configuration *config = m_service.getConfiguration();
    int ihm_iso_prio_increase = ISOHANDLERMANAGER_ISO_PRIO_INCREASE;
    int ihm_iso_prio_increase_xmit = ISOHANDLERMANAGER_ISO_PRIO_INCREASE_XMIT;
    int ihm_iso_prio_increase_recv = ISOHANDLERMANAGER_ISO_PRIO_INCREASE_RECV;
    int64_t isotask_activity_timeout_usecs = ISOHANDLERMANAGER_ISO_TASK_WAIT_TIMEOUT_USECS;
    if(config) {
        config->getValueForSetting("ieee1394.isomanager.prio_increase", ihm_iso_prio_increase);
        config->getValueForSetting("ieee1394.isomanager.prio_increase_xmit", ihm_iso_prio_increase_xmit);
        config->getValueForSetting("ieee1394.isomanager.prio_increase_recv", ihm_iso_prio_increase_recv);
        config->getValueForSetting("ieee1394.isomanager.isotask_activity_timeout_usecs", isotask_activity_timeout_usecs);
    }

    // create threads to iterate our ISO handlers
    debugOutput( DEBUG_LEVEL_VERBOSE, "Create iso thread for %p transmit...\n", this);
    m_IsoTaskTransmit = new IsoTask( *this, IsoHandler::eHT_Transmit );
    if(!m_IsoTaskTransmit) {
        debugFatal("No task\n");
        return false;
    }
    m_IsoTaskTransmit->setVerboseLevel(getDebugLevel());
    m_IsoTaskTransmit->m_activity_wait_timeout_nsec = isotask_activity_timeout_usecs * 1000LL;
    m_IsoThreadTransmit = new Util::PosixThread(m_IsoTaskTransmit, "ISOXMT", m_realtime,
                                                m_priority + ihm_iso_prio_increase
                                                + ihm_iso_prio_increase_xmit,
                                                PTHREAD_CANCEL_DEFERRED);

    if(!m_IsoThreadTransmit) {
        debugFatal("No thread\n");
        return false;
    }
    m_IsoThreadTransmit->setVerboseLevel(getDebugLevel());

    debugOutput( DEBUG_LEVEL_VERBOSE, "Create iso thread for %p receive...\n", this);
    m_IsoTaskReceive = new IsoTask( *this, IsoHandler::eHT_Receive );
    if(!m_IsoTaskReceive) {
        debugFatal("No task\n");
        return false;
    }
    m_IsoTaskReceive->setVerboseLevel(getDebugLevel());
    m_IsoThreadReceive = new Util::PosixThread(m_IsoTaskReceive, "ISORCV", m_realtime,
                                               m_priority + ihm_iso_prio_increase
                                               + ihm_iso_prio_increase_recv,
                                               PTHREAD_CANCEL_DEFERRED);

    if(!m_IsoThreadReceive) {
        debugFatal("No thread\n");
        return false;
    }
    m_IsoThreadReceive->setVerboseLevel(getDebugLevel());
    // register the thread with the RT watchdog
    Util::Watchdog *watchdog = m_service.getWatchdog();
    if(watchdog) {
        if(!watchdog->registerThread(m_IsoThreadTransmit)) {
            debugWarning("could not register iso transmit thread with watchdog\n");
        }
        if(!watchdog->registerThread(m_IsoThreadReceive)) {
            debugWarning("could not register iso receive thread with watchdog\n");
        }
    } else {
        debugWarning("could not find valid watchdog\n");
    }

    if (m_IsoThreadTransmit->Start() != 0) {
        debugFatal("Could not start ISO Transmit thread\n");
        return false;
    }
    if (m_IsoThreadReceive->Start() != 0) {
        debugFatal("Could not start ISO Receive thread\n");
        return false;
    }

    m_State=E_Running;
    return true;
}

void
IsoHandlerManager::signalActivityTransmit()
{
    assert(m_IsoTaskTransmit);
    m_IsoTaskTransmit->signalActivity();
}

void
IsoHandlerManager::signalActivityReceive()
{
    assert(m_IsoTaskReceive);
    m_IsoTaskReceive->signalActivity();
}

bool IsoHandlerManager::registerHandler(IsoHandler *handler)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
    assert(handler);
    handler->setVerboseLevel(getDebugLevel());
    m_IsoHandlers.push_back(handler);
    requestShadowMapUpdate();
    return true;
}

bool IsoHandlerManager::unregisterHandler(IsoHandler *handler)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
    assert(handler);

    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
      it != m_IsoHandlers.end();
      ++it )
    {
        if ( *it == handler ) {
            m_IsoHandlers.erase(it);
            requestShadowMapUpdate();
            return true;
        }
    }
    debugFatal("Could not find handler (%p)\n", handler);
    return false; //not found
}

/**
 * Registers an StreamProcessor with the IsoHandlerManager.
 *
 * If nescessary, an IsoHandler is created to handle this stream.
 * Once an StreamProcessor is registered to the handler, it will be included
 * in the ISO streaming cycle (i.e. receive/transmit of it will occur).
 *
 * @param stream the stream to register
 * @return true if registration succeeds
 *
 * \todo : currently there is a one-to-one mapping
 *        between streams and handlers, this is not ok for
 *        multichannel receive
 */
bool IsoHandlerManager::registerStream(StreamProcessor *stream)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Registering %s stream %p\n", stream->getTypeString(), stream);
    assert(stream);

    IsoHandler* h = NULL;

    // make sure the stream isn't already attached to a handler
    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
      it != m_IsoHandlers.end();
      ++it )
    {
        if((*it)->isStreamRegistered(stream)) {
            debugError( "stream already registered!\n");
            return false;
        }
    }

    // clean up all handlers that aren't used
    pruneHandlers();

    // allocate a handler for this stream
    if (stream->getType()==StreamProcessor::ePT_Receive) {
        // grab the options from the parent
        Util::Configuration *config = m_service.getConfiguration();
        int receive_mode_setting = DEFAULT_ISO_RECEIVE_MODE;
        int bufferfill_mode_threshold = BUFFERFILL_MODE_THRESHOLD;
        int min_interrupts_per_period = MINIMUM_INTERRUPTS_PER_PERIOD;
        int max_nb_buffers_recv = MAX_RECV_NB_BUFFERS;
        int min_packetsize_recv = MIN_RECV_PACKET_SIZE;
        if(config) {
            config->getValueForSetting("ieee1394.isomanager.iso_receive_mode", receive_mode_setting);
            config->getValueForSetting("ieee1394.isomanager.bufferfill_mode_threshold", bufferfill_mode_threshold);
            config->getValueForSetting("ieee1394.isomanager.min_interrupts_per_period", min_interrupts_per_period);
            config->getValueForSetting("ieee1394.isomanager.max_nb_buffers_recv", max_nb_buffers_recv);
            config->getValueForSetting("ieee1394.isomanager.min_packetsize_recv", min_packetsize_recv);
        }

        // setup the optimal parameters for the raw1394 ISO buffering
        unsigned int packets_per_period = stream->getPacketsPerPeriod();
        // reserve space for the 1394 header too (might not be necessary)
        unsigned int max_packet_size = stream->getMaxPacketSize() + 8;
        unsigned int page_size = getpagesize();

        enum raw1394_iso_dma_recv_mode receive_mode =
		RAW1394_DMA_PACKET_PER_BUFFER;
        switch(receive_mode_setting) {
            case 0:
                if(packets_per_period < (unsigned)bufferfill_mode_threshold) {
                    debugOutput( DEBUG_LEVEL_VERBOSE, "Using packet-per-buffer mode (auto) [%d, %d]\n",
                                 packets_per_period, bufferfill_mode_threshold);
                    receive_mode = RAW1394_DMA_PACKET_PER_BUFFER;
                } else {
                    debugOutput( DEBUG_LEVEL_VERBOSE, "Using bufferfill mode (auto) [%d, %d]\n",
                                 packets_per_period, bufferfill_mode_threshold);
                    receive_mode = RAW1394_DMA_BUFFERFILL;
                }
                break;
            case 1: 
                debugOutput( DEBUG_LEVEL_VERBOSE, "Using packet-per-buffer mode (config)\n");
                receive_mode = RAW1394_DMA_PACKET_PER_BUFFER;
                break;
            case 2:
                debugOutput( DEBUG_LEVEL_VERBOSE, "Using bufferfill mode (config)\n");
                receive_mode = RAW1394_DMA_BUFFERFILL;
                break;
            default: debugWarning("Bogus receive mode setting in config: %d\n", receive_mode_setting);
        }

        // Ensure we don't request a packet size bigger than the
        // kernel-enforced maximum which is currently 1 page.
        // NOTE: PP: this is not really true AFAICT
        if (max_packet_size > page_size) {
            debugError("max packet size (%u) > page size (%u)\n", max_packet_size, page_size);
            return false;
        }
        if (max_packet_size < (unsigned)min_packetsize_recv) {
            debugError("min packet size (%u) < MIN_RECV_PACKET_SIZE (%u), using min value\n",
                       max_packet_size, min_packetsize_recv);
            max_packet_size = min_packetsize_recv;
        }

        // apparently a too small value causes issues too
        if(max_packet_size < 200) max_packet_size = 200;

        // the interrupt/wakeup interval prediction of raw1394 is a mess...
        int irq_interval = (packets_per_period-1) / min_interrupts_per_period;
        if(irq_interval <= 0) irq_interval=1;

        // the receive buffer size doesn't matter for the latency,
        // it does seem to be confined to a certain region for correct
        // operation. However it is not clear how many.
        int buffers = max_nb_buffers_recv;

        // ensure at least 2 hardware interrupts per ISO buffer wraparound
        if(irq_interval > buffers/2) {
            irq_interval = buffers/2;
        }

        // create the actual handler
        debugOutput( DEBUG_LEVEL_VERBOSE, " creating IsoRecvHandler\n");
        h = new IsoHandler(*this, IsoHandler::eHT_Receive,
                           buffers, max_packet_size, irq_interval);

        if(!h) {
            debugFatal("Could not create IsoRecvHandler\n");
            return false;
        }

        h->setReceiveMode(receive_mode);

    } else if (stream->getType()==StreamProcessor::ePT_Transmit) {
        // grab the options from the parent
        Util::Configuration *config = m_service.getConfiguration();
        int min_interrupts_per_period = MINIMUM_INTERRUPTS_PER_PERIOD;
        int max_nb_buffers_xmit = MAX_XMIT_NB_BUFFERS;
        int max_packetsize_xmit = MAX_XMIT_PACKET_SIZE;
        int min_packetsize_xmit = MIN_XMIT_PACKET_SIZE;
        if(config) {
            config->getValueForSetting("ieee1394.isomanager.min_interrupts_per_period", min_interrupts_per_period);
            config->getValueForSetting("ieee1394.isomanager.max_nb_buffers_xmit", max_nb_buffers_xmit);
            config->getValueForSetting("ieee1394.isomanager.max_packetsize_xmit", max_packetsize_xmit);
            config->getValueForSetting("ieee1394.isomanager.min_packetsize_xmit", min_packetsize_xmit);
        }

        // setup the optimal parameters for the raw1394 ISO buffering
        // reserve space for the 1394 header too (might not be necessary)
        unsigned int max_packet_size = stream->getMaxPacketSize() + 8;

        if (max_packet_size > (unsigned)max_packetsize_xmit) {
            debugError("max packet size (%u) > MAX_XMIT_PACKET_SIZE (%u)\n",
                       max_packet_size, max_packetsize_xmit);
            return false;
        }
        if (max_packet_size < (unsigned)min_packetsize_xmit) {
            debugError("min packet size (%u) < MIN_XMIT_PACKET_SIZE (%u), using min value\n",
                       max_packet_size, min_packetsize_xmit);
            max_packet_size = min_packetsize_xmit;
        }

        int buffers = max_nb_buffers_xmit;
        unsigned int packets_per_period = stream->getPacketsPerPeriod();

        int irq_interval = (packets_per_period-1) / min_interrupts_per_period;
        if(irq_interval <= 0) irq_interval=1;
        // ensure at least 2 hardware interrupts per ISO buffer wraparound
        if(irq_interval > buffers/2) {
            irq_interval = buffers/2;
        }

        debugOutput( DEBUG_LEVEL_VERBOSE, " creating IsoXmitHandler\n");

        // create the actual handler
        h = new IsoHandler(*this, IsoHandler::eHT_Transmit,
                           buffers, max_packet_size, irq_interval);

        if(!h) {
            debugFatal("Could not create IsoXmitHandler\n");
            return false;
        }

    } else {
        debugFatal("Bad stream type\n");
        return false;
    }

    h->setVerboseLevel(getDebugLevel());

    // register the stream with the handler
    if(!h->registerStream(stream)) {
        debugFatal("Could not register receive stream with handler\n");
        return false;
    }

    // register the handler with the manager
    if(!registerHandler(h)) {
        debugFatal("Could not register receive handler with manager\n");
        return false;
    }
    debugOutput( DEBUG_LEVEL_VERBOSE, " registered stream (%p) with handler (%p)\n", stream, h);

    m_StreamProcessors.push_back(stream);
    debugOutput( DEBUG_LEVEL_VERBOSE, " %zd streams, %zd handlers registered\n",
                                      m_StreamProcessors.size(), m_IsoHandlers.size());
    return true;
}

bool IsoHandlerManager::unregisterStream(StreamProcessor *stream)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Unregistering %s stream %p\n", stream->getTypeString(), stream);
    assert(stream);

    // make sure the stream isn't attached to a handler anymore
    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
      it != m_IsoHandlers.end();
      ++it )
    {
        if((*it)->isStreamRegistered(stream)) {
            if(!(*it)->unregisterStream(stream)) {
                debugOutput( DEBUG_LEVEL_VERBOSE, " could not unregister stream (%p) from handler (%p)...\n",stream,*it);
                return false;
            }
            debugOutput( DEBUG_LEVEL_VERBOSE, " unregistered stream (%p) from handler (%p)...\n",stream,*it);
        }
    }

    // clean up all handlers that aren't used
    pruneHandlers();

    // remove the stream from the registered streams list
    for ( StreamProcessorVectorIterator it = m_StreamProcessors.begin();
      it != m_StreamProcessors.end();
      ++it )
    {
        if ( *it == stream ) {
            m_StreamProcessors.erase(it);
            debugOutput( DEBUG_LEVEL_VERBOSE, " deleted stream (%p) from list...\n", *it);
            return true;
        }
    }
    return false; //not found
}

/**
 * @brief unregister a handler from the manager
 * @note called without the lock held.
 */
void IsoHandlerManager::pruneHandlers() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
    IsoHandlerVector toUnregister;

    // find all handlers that are not in use
    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
          it != m_IsoHandlers.end();
          ++it )
    {
        if(!((*it)->inUse())) {
            debugOutput( DEBUG_LEVEL_VERBOSE, " handler (%p) not in use\n",*it);
            toUnregister.push_back(*it);
        }
    }
    // delete them
    for ( IsoHandlerVectorIterator it = toUnregister.begin();
          it != toUnregister.end();
          ++it )
    {
        unregisterHandler(*it);

        debugOutput( DEBUG_LEVEL_VERBOSE, " deleting handler (%p)\n",*it);

        // Now the handler's been unregistered it won't be reused
        // again.  Therefore it really needs to be formally deleted
        // to free up the raw1394 handle.  Otherwise things fall
        // apart after several xrun recoveries as the system runs
        // out of resources to support all the disused but still
        // allocated raw1394 handles.  At least this is the current
        // theory as to why we end up with "memory allocation"
        // failures after several Xrun recoveries.
        delete *it;
    }
}

int
IsoHandlerManager::getPacketLatencyForStream(Streaming::StreamProcessor *stream) {
    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
      it != m_IsoHandlers.end();
      ++it )
    {
        if((*it)->isStreamRegistered(stream)) {
            return (*it)->getIrqInterval();
        }
    }
    debugError("Stream %p has no attached handler\n", stream);
    return 0;
}

IsoHandlerManager::IsoHandler *
IsoHandlerManager::getHandlerForStream(Streaming::StreamProcessor *stream) {
    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
      it != m_IsoHandlers.end();
      ++it )
    {
        if((*it)->isStreamRegistered(stream)) {
            return (*it);
        }
    }
    debugError("Stream %p has no attached handler\n", stream);
    return NULL;
}

void
IsoHandlerManager::dumpInfoForStream(Streaming::StreamProcessor *stream)
{
    IsoHandler *h = getHandlerForStream(stream);
    if (h) {
        #ifdef DEBUG
        debugOutputShort( DEBUG_LEVEL_NORMAL, "  Packets, Dropped, Skipped : %d, %d, %d\n",
                            h->m_packets, h->m_dropped, h->m_skipped);
        #else
        debugOutputShort( DEBUG_LEVEL_NORMAL, "  Packets : %d\n", h->m_packets);
        #endif
    } else {
        debugError("No handler for stream %p??\n", stream);
    }
}

void IsoHandlerManager::setIsoStartCycleForStream(Streaming::StreamProcessor *stream, signed int cycle) {
    // Permit the direct manipulation of the m_switch_on_cycle field from
    // the stream's handler.  This is usually used to set it to -1 so the
    // kernel (at least with the ieee1394 stack) starts the streaming as
    // soon as possible, something that is required for some interfaces (eg:
    // RME).  Note that as of 20 Dec 2010 it seems that ordinarily
    // m_switch_on_cycle remains fixed at 0 (its initialised value) because
    // requestEnable() doesn't set it.  This allows the override configured
    // by this function to take effect.
    IsoHandler *h = getHandlerForStream(stream);
    h->setIsoStartCycle(cycle);
}

bool
IsoHandlerManager::startHandlerForStream(Streaming::StreamProcessor *stream) {
    return startHandlerForStream(stream, -1);
}

bool
IsoHandlerManager::startHandlerForStream(Streaming::StreamProcessor *stream, int cycle) {
    // check state
    if(m_State != E_Running) {
        debugError("Incorrect state, expected E_Running, got %s\n", eHSToString(m_State));
        return false;
    }
    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
      it != m_IsoHandlers.end();
      ++it )
    {
        if((*it)->isStreamRegistered(stream)) {
            debugOutput( DEBUG_LEVEL_VERBOSE, " starting handler %p for stream %p\n", *it, stream);
            if(!(*it)->requestEnable(cycle)) {
                debugOutput( DEBUG_LEVEL_VERBOSE, " could not request enable for handler %p)\n",*it);
                return false;
            }

            if((*it)->getType() == IsoHandler::eHT_Transmit) {
                m_IsoTaskTransmit->requestShadowMapUpdate();
            } else {
                m_IsoTaskReceive->requestShadowMapUpdate();
            }

            debugOutput(DEBUG_LEVEL_VERY_VERBOSE, " requested enable for handler %p\n", *it);
            return true;
        }
    }
    debugError("Stream %p has no attached handler\n", stream);
    return false;
}

bool
IsoHandlerManager::stopHandlerForStream(Streaming::StreamProcessor *stream) {
    // check state
    if(m_State != E_Running) {
        debugError("Incorrect state, expected E_Running, got %s\n", eHSToString(m_State));
        return false;
    }
    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
      it != m_IsoHandlers.end();
      ++it )
    {
        if((*it)->isStreamRegistered(stream)) {
            debugOutput( DEBUG_LEVEL_VERBOSE, " stopping handler %p for stream %p\n", *it, stream);
            if(!(*it)->requestDisable()) {
                debugOutput( DEBUG_LEVEL_VERBOSE, " could not request disable for handler %p\n",*it);
                return false;
            }

            if((*it)->getType() == IsoHandler::eHT_Transmit) {
                m_IsoTaskTransmit->requestShadowMapUpdate();
            } else {
                m_IsoTaskReceive->requestShadowMapUpdate();
            }

            debugOutput(DEBUG_LEVEL_VERBOSE, " requested disable for handler %p\n", *it);
            return true;
        }
    }
    debugError("Stream %p has no attached handler\n", stream);
    return false;
}

bool IsoHandlerManager::stopHandlers() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

    // check state
    if(m_State != E_Running) {
        debugError("Incorrect state, expected E_Running, got %s\n", eHSToString(m_State));
        return false;
    }

    bool retval=true;

    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
        it != m_IsoHandlers.end();
        ++it )
    {
        debugOutput( DEBUG_LEVEL_VERBOSE, "Stopping handler (%p)\n",*it);

        if(!(*it)->requestDisable()) {
            debugOutput( DEBUG_LEVEL_VERBOSE, " could not request disable for handler %p\n",*it);
            return false;
        }

        if((*it)->getType() == IsoHandler::eHT_Transmit) {
            m_IsoTaskTransmit->requestShadowMapUpdate();
        } else {
            m_IsoTaskReceive->requestShadowMapUpdate();
        }

        debugOutput(DEBUG_LEVEL_VERBOSE, " requested disable for handler %p\n", *it);
    }

    if (!retval) {
        m_State=E_Error;
    }
    return retval;
}

bool IsoHandlerManager::reset() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
    // check state
    if(m_State == E_Error) {
        debugFatal("Resetting from error condition not yet supported...\n");
        return false;
    }
    // if not in an error condition, reset means stop the handlers
    return stopHandlers();
}

void IsoHandlerManager::setVerboseLevel(int i) {
    setDebugLevel(i);
    // propagate the debug level
    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
          it != m_IsoHandlers.end();
          ++it )
    {
        (*it)->setVerboseLevel(i);
    }
    if(m_IsoThreadTransmit) m_IsoThreadTransmit->setVerboseLevel(i);
    if(m_IsoTaskTransmit)   m_IsoTaskTransmit->setVerboseLevel(i);
    if(m_IsoThreadReceive)  m_IsoThreadReceive->setVerboseLevel(i);
    if(m_IsoTaskReceive)    m_IsoTaskReceive->setVerboseLevel(i);
    setDebugLevel(i);
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting verbose level to %d...\n", i );
}

void IsoHandlerManager::dumpInfo() {
    #ifdef DEBUG
    unsigned int i=0;
    debugOutputShort( DEBUG_LEVEL_NORMAL, "Dumping IsoHandlerManager Stream handler information...\n");
    debugOutputShort( DEBUG_LEVEL_NORMAL, " State: %d\n",(int)m_State);

    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
          it != m_IsoHandlers.end();
          ++it )
    {
        debugOutputShort( DEBUG_LEVEL_NORMAL, " IsoHandler %d (%p)\n",i++,*it);
        (*it)->dumpInfo();
    }
    #endif
}

const char *
IsoHandlerManager::eHSToString(enum eHandlerStates s) {
    switch (s) {
        default: return "Invalid";
        case E_Created: return "Created";
        case E_Running: return "Running";
        case E_Error: return "Error";
    }
}


// ISOHANDLER

/* the C callbacks */
enum raw1394_iso_disposition
IsoHandlerManager::IsoHandler::iso_transmit_handler(raw1394handle_t handle,
        unsigned char *data, unsigned int *length,
        unsigned char *tag, unsigned char *sy,
        int cycle, unsigned int dropped1) {

    IsoHandler *xmitHandler = static_cast<IsoHandler *>(raw1394_get_userdata(handle));
    assert(xmitHandler);
    unsigned int skipped = (dropped1 & 0xFFFF0000) >> 16;
    unsigned int dropped = dropped1 & 0xFFFF;
    return xmitHandler->getPacket(data, length, tag, sy, cycle, dropped, skipped);
}

enum raw1394_iso_disposition
IsoHandlerManager::IsoHandler::iso_receive_handler(raw1394handle_t handle, unsigned char *data,
                        unsigned int length, unsigned char channel,
                        unsigned char tag, unsigned char sy, unsigned int cycle,
                        unsigned int dropped) {

    IsoHandler *recvHandler = static_cast<IsoHandler *>(raw1394_get_userdata(handle));
    assert(recvHandler);

    return recvHandler->putPacket(data, length, channel, tag, sy, cycle, dropped);
}

IsoHandlerManager::IsoHandler::IsoHandler(IsoHandlerManager& manager, enum EHandlerType t)
   : m_manager( manager )
   , m_type ( t )
   , m_handle( NULL )
   , m_buf_packets( 400 )
   , m_max_packet_size( 1024 )
   , m_irq_interval( -1 )
   , m_last_cycle( -1 )
   , m_last_now( 0xFFFFFFFF )
   , m_last_packet_handled_at( 0xFFFFFFFF )
   , m_receive_mode ( RAW1394_DMA_PACKET_PER_BUFFER )
   , m_Client( 0 )
   , m_speed( RAW1394_ISO_SPEED_400 )
   , m_State( eHS_Stopped )
   , m_NextState( eHS_Stopped )
   , m_switch_on_cycle(0)
#ifdef DEBUG
   , m_packets ( 0 )
   , m_dropped( 0 )
   , m_skipped( 0 )
   , m_min_ahead( 7999 )
#endif
{
    pthread_mutex_init(&m_disable_lock, NULL);
}

IsoHandlerManager::IsoHandler::IsoHandler(IsoHandlerManager& manager, enum EHandlerType t, 
                       unsigned int buf_packets, unsigned int max_packet_size, int irq)
   : m_manager( manager )
   , m_type ( t )
   , m_handle( NULL )
   , m_buf_packets( buf_packets )
   , m_max_packet_size( max_packet_size )
   , m_irq_interval( irq )
   , m_last_cycle( -1 )
   , m_last_now( 0xFFFFFFFF )
   , m_last_packet_handled_at( 0xFFFFFFFF )
   , m_receive_mode ( RAW1394_DMA_PACKET_PER_BUFFER )
   , m_Client( 0 )
   , m_speed( RAW1394_ISO_SPEED_400 )
   , m_State( eHS_Stopped )
   , m_NextState( eHS_Stopped )
   , m_switch_on_cycle(0)
#ifdef DEBUG
   , m_packets ( 0 )
   , m_dropped( 0 )
   , m_skipped( 0 )
   , m_min_ahead( 7999 )
#endif
{
    pthread_mutex_init(&m_disable_lock, NULL);
}

IsoHandlerManager::IsoHandler::IsoHandler(IsoHandlerManager& manager, enum EHandlerType t, unsigned int buf_packets,
                       unsigned int max_packet_size, int irq,
                       enum raw1394_iso_speed speed)
   : m_manager( manager )
   , m_type ( t )
   , m_handle( NULL )
   , m_buf_packets( buf_packets )
   , m_max_packet_size( max_packet_size )
   , m_irq_interval( irq )
   , m_last_cycle( -1 )
   , m_last_now( 0xFFFFFFFF )
   , m_last_packet_handled_at( 0xFFFFFFFF )
   , m_receive_mode ( RAW1394_DMA_PACKET_PER_BUFFER )
   , m_Client( 0 )
   , m_speed( speed )
   , m_State( eHS_Stopped )
   , m_NextState( eHS_Stopped )
   , m_switch_on_cycle(0)
#ifdef DEBUG
   , m_packets( 0 )
   , m_dropped( 0 )
   , m_skipped( 0 )
   , m_min_ahead( 7999 )
#endif
   , m_deferred_cycles( 0 )
{
    pthread_mutex_init(&m_disable_lock, NULL);
}

IsoHandlerManager::IsoHandler::~IsoHandler() {
// Don't call until libraw1394's raw1394_new_handle() function has been
// fixed to correctly initialise the iso_packet_infos field.  Bug is
// confirmed present in libraw1394 1.2.1.  In any case,
// raw1394_destroy_handle() will do any iso system shutdown required.
//     raw1394_iso_shutdown(m_handle);

// Typically, by the time this function is called the IsoTask thread would
// have called disable() on the handler (in the FW_ISORCV/FW_ISOXMT
// threads).  However, the raw1394_destroy_handle() call therein takes
// upwards of 20 milliseconds to complete under the new kernel firewire
// stack, and may not have completed by the time ~IsoHandler() is called by
// the "jackd" thread.  Thus, wait for the lock before testing the state
// of the handle so any in-progress disable() is complete.
    if (pthread_mutex_trylock(&m_disable_lock) == EBUSY) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "waiting for disable lock\n");
        pthread_mutex_lock(&m_disable_lock);
    }
    pthread_mutex_unlock(&m_disable_lock);
    if(m_handle) {
        if (m_State == eHS_Running) {
            debugError("BUG: Handler still running!\n");
            disable();
        }
    }
    pthread_mutex_destroy(&m_disable_lock);
}

bool
IsoHandlerManager::IsoHandler::canIterateClient()
{
    debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE, "checking...\n");
    if(m_Client) {
        bool result;

        if (m_type == eHT_Receive) {
            result = m_Client->canProducePacket();
        } else {
            result = m_Client->canConsumePacket();
        }
        debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE, " returns %d\n", result);
        return result && (m_State != eHS_Error);
    } else {
        debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE, " no client\n");
    }
    return false;
}

bool
IsoHandlerManager::IsoHandler::iterate() {
    return iterate(m_manager.get1394Service().getCycleTimer());
}

bool
IsoHandlerManager::IsoHandler::iterate(uint32_t cycle_timer_now) {
    debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE, "(%p, %s) Iterating ISO handler at %08X...\n",
                       this, getTypeString(), cycle_timer_now);
    m_last_now = cycle_timer_now;
    if(m_State == eHS_Running) {
        assert(m_handle);

        #if ISOHANDLER_FLUSH_BEFORE_ITERATE
        // this flushes all packets received since the poll() returned
        // from kernel to userspace such that they are processed by this
        // iterate. Doing so might result in lower latency capability
        // and/or better reliability
        if(m_type == eHT_Receive) {
            raw1394_iso_recv_flush(m_handle);
        }
        #endif

        if(raw1394_loop_iterate(m_handle)) {
            debugError( "IsoHandler (%p): Failed to iterate handler: %s\n",
                        this, strerror(errno));
            return false;
        }
        debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE, "(%p, %s) done interating ISO handler...\n",
                           this, getTypeString());
        return true;
    } else {
        debugOutput(DEBUG_LEVEL_VERBOSE, "(%p, %s) Not iterating a non-running handler...\n",
                    this, getTypeString());
        return false;
    }
}

/**
 * Bus reset handler
 *
 * @return ?
 */

bool
IsoHandlerManager::IsoHandler::handleBusReset()
{
    debugOutput( DEBUG_LEVEL_NORMAL, "bus reset...\n");
    m_last_packet_handled_at = 0xFFFFFFFF;

    #define CSR_CYCLE_TIME            0x200
    #define CSR_REGISTER_BASE  0xfffff0000000ULL
    // do a simple read on ourself in order to update the internal structures
    // this avoids read failures after a bus reset
    quadlet_t buf=0;
    raw1394_read(m_handle, raw1394_get_local_id(m_handle),
                 CSR_REGISTER_BASE | CSR_CYCLE_TIME, 4, &buf);

    return m_Client->handleBusReset();
}

/**
 * Call this if you find out that this handler has died for some
 * external reason.
 */
void
IsoHandlerManager::IsoHandler::notifyOfDeath()
{
    m_State = eHS_Error;
    m_NextState = eHS_Error;

    // notify the client of the fact that we have died
    m_Client->handlerDied();

    // wake ourselves up
    if(m_handle) raw1394_wake_up(m_handle);
}

void IsoHandlerManager::IsoHandler::dumpInfo()
{
    int channel=-1;
    if (m_Client) channel=m_Client->getChannel();

    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Handler type................: %s\n",
            getTypeString());
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Port, Channel...............: %2d, %2d\n",
            m_manager.get1394Service().getPort(), channel);
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Buffer, MaxPacketSize, IRQ..: %4d, %4d, %4d\n",
            m_buf_packets, m_max_packet_size, m_irq_interval);
    if (this->getType() == eHT_Transmit) {
        debugOutputShort( DEBUG_LEVEL_NORMAL, "  Speed ..................: %2d\n",
                                            m_speed);
        #ifdef DEBUG
        debugOutputShort( DEBUG_LEVEL_NORMAL, "  Min ISOXMT bufferfill : %04d\n", m_min_ahead);
        #endif
    }
    #ifdef DEBUG
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Last cycle, dropped.........: %4d, %4u, %4u\n",
            m_last_cycle, m_dropped, m_skipped);
    #endif

}

void IsoHandlerManager::IsoHandler::setVerboseLevel(int l)
{
    setDebugLevel(l);
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting verbose level to %d...\n", l );
}

bool IsoHandlerManager::IsoHandler::registerStream(StreamProcessor *stream)
{
    assert(stream);
    debugOutput( DEBUG_LEVEL_VERBOSE, "registering stream (%p)\n", stream);

    if (m_Client) {
            debugFatal( "Generic IsoHandlers can have only one client\n");
            return false;
    }
    m_Client=stream;
    return true;
}

bool IsoHandlerManager::IsoHandler::unregisterStream(StreamProcessor *stream)
{
    assert(stream);
    debugOutput( DEBUG_LEVEL_VERBOSE, "unregistering stream (%p)\n", stream);

    if(stream != m_Client) {
            debugFatal( "no client registered\n");
            return false;
    }
    m_Client=0;
    return true;
}

// ISO packet interface
enum raw1394_iso_disposition IsoHandlerManager::IsoHandler::putPacket(
                    unsigned char *data, unsigned int length,
                    unsigned char channel, unsigned char tag, unsigned char sy,
                    unsigned int cycle, unsigned int dropped) {
    // keep track of dropped cycles
    int dropped_cycles = 0;
    if (m_last_cycle != (int)cycle && m_last_cycle != -1 && m_manager.m_MissedCyclesOK == false) {
        dropped_cycles = diffCycles(cycle, m_last_cycle) - 1;
        #ifdef DEBUG
        if (dropped_cycles < 0) {
            debugWarning("(%p) dropped < 1 (%d), cycle: %d, last_cycle: %d, dropped: %d\n", 
                         this, dropped_cycles, cycle, m_last_cycle, dropped);
        }
        if (dropped_cycles > 0) {
            debugOutput(DEBUG_LEVEL_VERBOSE,
                        "(%p) dropped %d packets on cycle %u, 'dropped'=%u, cycle=%d, m_last_cycle=%d\n",
                        this, dropped_cycles, cycle, dropped, cycle, m_last_cycle);
            m_dropped += dropped_cycles;
        }
        #endif
    }
    m_last_cycle = cycle;

    // the m_last_now value is set when the iterate() function is called.
    uint32_t now_cycles = CYCLE_TIMER_GET_CYCLES(m_last_now);

    // two cases can occur:
    // (1) this packet has been received before iterate() was called (normal case).
    // (2) this packet has been received after iterate() was called.
    //     happens when the kernel flushes more packets while we are already processing.
    //
    // In case (1) now_cycles is a small number of cycles larger than cycle. In
    // case (2) now_cycles is a small number of cycles smaller than cycle.
    // hence  abs(diffCycles(now_cycles, cycles)) has to be 'small'

    // we can calculate the time of arrival for this packet as
    // 'now' + diffCycles(cycles, now_cycles) * TICKS_PER_CYCLE
    // in its properly wrapped version
    int64_t diff_cycles = diffCycles(cycle, now_cycles);
    int64_t tmp = CYCLE_TIMER_TO_TICKS(m_last_now);
    tmp += diff_cycles * (int64_t)TICKS_PER_CYCLE;
    uint64_t pkt_ctr_ticks = wrapAtMinMaxTicks(tmp);
    uint32_t pkt_ctr = TICKS_TO_CYCLE_TIMER(pkt_ctr_ticks);
    #ifdef DEBUG
    if( (now_cycles < cycle)
        && diffCycles(now_cycles, cycle) < 0
        // ignore this on dropped cycles, since it's normal
        // that now is ahead on the received packets (as we miss packets)
        && dropped_cycles == 0) 
    {
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "Special non-unwrapping happened\n");
    }
    #endif

    #if ISOHANDLER_CHECK_CTR_RECONSTRUCTION
    // add a seconds field
    uint32_t now = m_manager.get1394Service().getCycleTimer();
    uint32_t now_secs_ref = CYCLE_TIMER_GET_SECS(now);
    // causality results in the fact that 'now' is always after 'cycle'
    // or at best, equal (if this handler was called within 125us after
    // the packet was on the wire).
    if(CYCLE_TIMER_GET_CYCLES(now) < cycle) {
        // the cycle field has wrapped, substract one second
        if(now_secs_ref == 0) {
            now_secs_ref = 127;
        } else  {
            now_secs_ref -= 1;
        }
    }
    uint32_t pkt_ctr_ref = cycle << 12;
    pkt_ctr_ref |= (now_secs_ref & 0x7F) << 25;

    if((pkt_ctr & ~0x0FFFL) != pkt_ctr_ref) {
        debugWarning("reconstructed CTR counter discrepancy\n");
        debugWarning(" ingredients: %X, %X, %X, %X, %X, %d, %ld, %ld, %"PRId64"\n",
                     cycle, pkt_ctr_ref, pkt_ctr, 
		     now, m_last_now, now_secs_ref, 
		     (long int)CYCLE_TIMER_GET_SECS(now),
		     (long int)CYCLE_TIMER_GET_SECS(m_last_now),
		     tmp);
        debugWarning(" diffcy = %"PRId64" \n", diff_cycles);
    }
    #endif
    m_last_packet_handled_at = pkt_ctr;

    // leave the offset field (for now?)

    debugOutputExtreme(DEBUG_LEVEL_ULTRA_VERBOSE,
                "received packet: length=%d, channel=%d, cycle=%d, at %08X\n",
                length, channel, cycle, pkt_ctr);
    m_packets++;
    #ifdef DEBUG
    if (length > m_max_packet_size) {
        debugWarning("(%p, %s) packet too large: len=%u max=%u\n",
                     this, getTypeString(), length, m_max_packet_size);
    }
    if(m_last_cycle == -1) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Handler for %s SP %p is alive (cycle = %u)\n", getTypeString(), this, cycle);
    }
    #endif

    // iterate the client if required
    if(m_Client)
        return m_Client->putPacket(data, length, channel, tag, sy, pkt_ctr, dropped_cycles);

    return RAW1394_ISO_OK;
}

enum raw1394_iso_disposition
IsoHandlerManager::IsoHandler::getPacket(unsigned char *data, unsigned int *length,
                      unsigned char *tag, unsigned char *sy,
                      int cycle, unsigned int dropped, unsigned int skipped) {

    uint32_t pkt_ctr;
    if (cycle < 0) {
        // mark invalid
        pkt_ctr = 0xFFFFFFFF;
    } else {
        // the m_last_now value is set when the iterate() function is called.
        uint32_t now_cycles = CYCLE_TIMER_GET_CYCLES(m_last_now);

        // two cases can occur:
        // (1) this packet has been received before iterate() was called (normal case).
        // (2) this packet has been received after iterate() was called.
        //     happens when the kernel flushes more packets while we are already processing.
        //
        // In case (1) now_cycles is a small number of cycles larger than cycle. In
        // case (2) now_cycles is a small number of cycles smaller than cycle.
        // hence  abs(diffCycles(now_cycles, cycles)) has to be 'small'

        // we can calculate the time of arrival for this packet as
        // 'now' + diffCycles(cycles, now_cycles) * TICKS_PER_CYCLE
        // in its properly wrapped version
        int64_t diff_cycles = diffCycles(cycle, now_cycles);
        int64_t tmp = CYCLE_TIMER_TO_TICKS(m_last_now);
        tmp += diff_cycles * (int64_t)TICKS_PER_CYCLE;
        uint64_t pkt_ctr_ticks = wrapAtMinMaxTicks(tmp);
        pkt_ctr = TICKS_TO_CYCLE_TIMER(pkt_ctr_ticks);

//debugOutput(DEBUG_LEVEL_VERBOSE, "cy=%d, now_cy=%d, diff_cy=%lld, tmp=%lld, pkt_ctr_ticks=%lld, pkt_ctr=%d\n",
//  cycle, now_cycles, diff_cycles, tmp, pkt_ctr_ticks, pkt_ctr);
        #if ISOHANDLER_CHECK_CTR_RECONSTRUCTION
        // add a seconds field
        uint32_t now = m_manager.get1394Service().getCycleTimer();
        uint32_t now_secs_ref = CYCLE_TIMER_GET_SECS(now);
        // causality results in the fact that 'now' is always after 'cycle'
        if(CYCLE_TIMER_GET_CYCLES(now) > (unsigned int)cycle) {
            // the cycle field has wrapped, add one second
            now_secs_ref += 1;
            // no need for this:
            if(now_secs_ref == 128) {
               now_secs_ref = 0;
            }
        }
        uint32_t pkt_ctr_ref = cycle << 12;
        pkt_ctr_ref |= (now_secs_ref & 0x7F) << 25;

        if(((pkt_ctr & ~0x0FFFL) != pkt_ctr_ref) && (m_packets > m_buf_packets)) {
            debugWarning("reconstructed CTR counter discrepancy\n");
            debugWarning(" ingredients: %X, %X, %X, %X, %X, %d, %ld, %ld, %"PRId64"\n",
                        cycle, pkt_ctr_ref, pkt_ctr,
			 now, m_last_now, now_secs_ref, 
			 (long int)CYCLE_TIMER_GET_SECS(now),
			 (long int)CYCLE_TIMER_GET_SECS(m_last_now), 
			 tmp);
            debugWarning(" diffcy = %"PRId64" \n", diff_cycles);
        }
        #endif
    }
    if (m_packets < m_buf_packets) { // these are still prebuffer packets
        m_last_packet_handled_at = 0xFFFFFFFF;
    } else {
        m_last_packet_handled_at = pkt_ctr;
    }
    debugOutputExtreme(DEBUG_LEVEL_ULTRA_VERBOSE,
                "sending packet: length=%d, cycle=%d, at %08X\n",
                *length, cycle, pkt_ctr);

    m_packets++;

    #ifdef DEBUG
    if(m_last_cycle == -1) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Handler for %s SP %p is alive. cycle=%d state=%i\n", getTypeString(), this, cycle, m_State);
    }
    #endif

    if (m_last_cycle == -1)
        m_deferred_cycles = 0;

    // keep track of dropped cycles
    int dropped_cycles = 0;
    if (m_last_cycle != cycle && m_last_cycle != -1) {
        dropped_cycles = diffCycles(cycle, m_last_cycle) - 1;
        // correct for skipped packets
        // since those are not dropped, but only delayed
        dropped_cycles -= skipped;

        // Correct for cycles previously seen but deferred
        if (dropped_cycles == 0)
            m_deferred_cycles = 0;
        else
            dropped_cycles -= m_deferred_cycles;

        #ifdef DEBUG
        if(skipped) {
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
                        "(%p) skipped %d cycles, cycle: %d, last_cycle: %d, dropped: %d\n", 
                        this, skipped, cycle, m_last_cycle, dropped);
            m_skipped += skipped;
        }
        if (dropped_cycles < 0) { 
            debugWarning("(%p) dropped < 1 (%d), cycle: %d, last_cycle: %d, dropped: %d, skipped: %d\n", 
                         this, dropped_cycles, cycle, m_last_cycle, dropped, skipped);
        }
        if (dropped_cycles > 0) {
            debugOutput(DEBUG_LEVEL_VERBOSE,
                        "(%p) dropped %d packets on cycle %u (last_cycle=%u, dropped=%d, skipped: %d)\n",
                        this, dropped_cycles, cycle, m_last_cycle, dropped, skipped);
            m_dropped += dropped_cycles - skipped;
        }
        #endif
    }

    #ifdef DEBUG
//    if (cycle >= 0) {
//        int ahead = diffCycles(cycle, now_cycles);
//        if (ahead < m_min_ahead) m_min_ahead = ahead;
//    }

    if (dropped > 0) {
        debugOutput(DEBUG_LEVEL_VERBOSE,
                    "(%p) OHCI issue on cycle %u (dropped_cycles=%d, last_cycle=%u, dropped=%d, skipped: %d)\n",
                    this, cycle, dropped_cycles, m_last_cycle, dropped, skipped);
    }
    #endif

    if(m_Client) {
        enum raw1394_iso_disposition retval;
        retval = m_Client->getPacket(data, length, tag, sy, pkt_ctr, dropped_cycles, skipped, m_max_packet_size);
        #ifdef DEBUG
        if (*length > m_max_packet_size) {
            debugWarning("(%p, %s) packet too large: len=%u max=%u\n",
                         this, getTypeString(), *length, m_max_packet_size);
        }
        #endif
        if (cycle >= 0) {
            if (retval!=RAW1394_ISO_DEFER && retval!=RAW1394_ISO_AGAIN) {
                m_last_cycle = cycle;
            } else
                m_deferred_cycles++;
        }
        return retval;
    }

    if (cycle >= 0)
        m_last_cycle = cycle;

    *tag = 0;
    *sy = 0;
    *length = 0;
    return RAW1394_ISO_OK;
}

bool
IsoHandlerManager::IsoHandler::enable(int cycle)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "start on cycle %d\n", cycle);

    // check the state
    if(m_State != eHS_Stopped) {
        debugError("Incorrect state, expected eHS_Stopped, got %d\n",(int)m_State);
        return false;
    }

    assert(m_handle == NULL);

    // create a handle for the ISO traffic
    m_handle = raw1394_new_handle_on_port( m_manager.get1394Service().getPort() );
    if ( !m_handle ) {
        if ( !errno ) {
            debugError("libraw1394 not compatible\n");
        } else {
            debugError("Could not get 1394 handle: %s\n", strerror(errno) );
            debugError("Are ieee1394 and raw1394 drivers loaded?\n");
        }
        return false;
    }
    raw1394_set_userdata(m_handle, static_cast<void *>(this));

    // Reset housekeeping data before preparing and starting the handler. 
    // If only done afterwards, the transmit handler could be called before
    // these have been reset, leading to problems in getPacket().
#ifdef DEBUG
    m_min_ahead = 7999;
#endif
    m_packets = 0;
    m_last_cycle = -1;

    // indicate that the first iterate() still has to occur.
    m_last_now = 0xFFFFFFFF;
    m_last_packet_handled_at = 0xFFFFFFFF;

    // prepare the handler, allocate the resources
    debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing iso handler (%p, client=%p)\n", this, m_Client);
    dumpInfo();
    if (getType() == eHT_Receive) {
        if(raw1394_iso_recv_init(m_handle,
                                iso_receive_handler,
                                m_buf_packets,
                                m_max_packet_size,
                                m_Client->getChannel(),
                                m_receive_mode,
                                m_irq_interval)) {
            debugFatal("Could not do receive initialization (PACKET_PER_BUFFER)!\n" );
            debugFatal("  %s\n",strerror(errno));
            return false;
        }

        if(raw1394_iso_recv_start(m_handle, cycle, -1, 0)) {
            debugFatal("Could not start receive handler (%s)\n",strerror(errno));
            dumpInfo();
            return false;
        }
    } else {
        if(raw1394_iso_xmit_init(m_handle,
                                iso_transmit_handler,
                                m_buf_packets,
                                m_max_packet_size,
                                m_Client->getChannel(),
                                m_speed,
                                m_irq_interval)) {
            debugFatal("Could not do xmit initialisation!\n" );
            return false;
        }

        if(raw1394_iso_xmit_start(m_handle, cycle, 0)) {
            debugFatal("Could not start xmit handler (%s)\n", strerror(errno));
            dumpInfo();
            return false;
        }
    }

    m_State = eHS_Running;
    m_NextState = eHS_Running;
    return true;
}

bool
IsoHandlerManager::IsoHandler::disable()
{
    signed int i, have_lock = 0;

    debugOutput( DEBUG_LEVEL_VERBOSE, "(%p, %s) enter...\n", 
                 this, (m_type==eHT_Receive?"Receive":"Transmit"));

    i = pthread_mutex_trylock(&m_disable_lock);
    if (i == 0)
        have_lock = 1;
    else
    if (i == EBUSY) {
        // Some other thread is disabling this handler, a process which can
        // take considerable time when using the new kernel firewire stack. 
        // Wait until it is finished before returning so the present caller
        // can act knowing that the disable has occurred and is complete
        // (which is what normally would be expected).
        debugOutput( DEBUG_LEVEL_VERBOSE, "waiting for disable lock\n");
        pthread_mutex_lock(&m_disable_lock);
        debugOutput( DEBUG_LEVEL_VERBOSE, "now have disable lock\n");
        if (m_State == eHS_Stopped) {
            debugOutput( DEBUG_LEVEL_VERBOSE, "another disable() has completed\n");
            pthread_mutex_unlock(&m_disable_lock);
            return true;
        }
        have_lock = 1;
    }

    // check state
    if(m_State != eHS_Running) {
        debugError("Incorrect state, expected eHS_Running, got %d\n",(int)m_State);
        if (have_lock)
            pthread_mutex_unlock(&m_disable_lock);
        return false;
    }

    assert(m_handle != NULL);

    debugOutput( DEBUG_LEVEL_VERBOSE, "(%p, %s) wake up handle...\n", 
                 this, (m_type==eHT_Receive?"Receive":"Transmit"));

    // wake up any waiting reads/polls
    raw1394_wake_up(m_handle);

    // this is put here to try and avoid the
    // Runaway context problem
    // don't know if it will help though.
/*    if(m_State != eHS_Error) { // if the handler is dead, this might block forever
        raw1394_iso_xmit_sync(m_handle);
    }*/
    debugOutput( DEBUG_LEVEL_VERBOSE, "(%p, %s) stop...\n", 
                 this, (m_type==eHT_Receive?"Receive":"Transmit"));

    // stop iso traffic
    raw1394_iso_stop(m_handle);
    // deallocate resources

    // Don't call until libraw1394's raw1394_new_handle() function has been
    // fixed to correctly initialise the iso_packet_infos field.  Bug is
    // confirmed present in libraw1394 1.2.1.
    raw1394_iso_shutdown(m_handle);

    // When running on the new kernel firewire stack, this call can take of
    // the order of 20 milliseconds to return, in which time other threads
    // may wish to test the state of the handler and call this function
    // themselves.  The m_disable_lock mutex is used to work around this.
    raw1394_destroy_handle(m_handle);
    m_handle = NULL;

    m_State = eHS_Stopped;
    m_NextState = eHS_Stopped;

    if (have_lock)
        pthread_mutex_unlock(&m_disable_lock);
    return true;
}

// functions to request enable or disable at the next opportunity
bool
IsoHandlerManager::IsoHandler::requestEnable(int cycle)
{
    if (m_State == eHS_Running) {
        debugError("Enable requested on enabled stream '%s'\n", getTypeString());
        return false;
    }
    if (m_State != eHS_Stopped) {
        debugError("Enable requested on stream '%s' with state: %d\n", getTypeString(), m_State);
        return false;
    }
    m_NextState = eHS_Running;
    return true;
}

bool
IsoHandlerManager::IsoHandler::requestDisable()
{
    if (m_State == eHS_Stopped) {
        // Don't treat this as an error condition because during a user
        // shutdown the stream would have been disabled by
        // stopHandlerForStream().  Therefore when requestDisable() is
        // subnsequently called by IsoHandlerManager::stopHandlers() in the
        // IsoHandlerManager destructor with the stream disabled the
        // condition is not an error.
        //
        // For now print a warning, but this might be removed in future if
        // the above framework remains in place.
        debugWarning("Disable requested on disabled stream\n");
        return true;
    }
    if (m_State != eHS_Running) {
        debugError("Disable requested on stream with state=%d\n", m_State);
        return false;
    }
    m_NextState = eHS_Stopped;
    return true;
}

// Explicitly preset m_switch_on_cycle since requestEnable doesn't do this
// and thus all enables requested via that route always occur on cycle 0.
void
IsoHandlerManager::IsoHandler::setIsoStartCycle(signed int cycle)
{
  m_switch_on_cycle = cycle;
}

void
IsoHandlerManager::IsoHandler::updateState()
{
    // execute state changes requested
    if(m_State != m_NextState) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "(%p) handler needs state update from %d => %d\n", this, m_State, m_NextState);
        if(m_State == eHS_Stopped && m_NextState == eHS_Running) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "handler has to be enabled\n");
            enable(m_switch_on_cycle);
        } else if(m_State == eHS_Running && m_NextState == eHS_Stopped) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "handler has to be disabled\n");
            disable();
        } else {
            debugError("Unknown state transition\n");
        }
    }
}

/**
 * @brief convert a EHandlerType to a string
 * @param t the type
 * @return a char * describing the state
 */
const char *
IsoHandlerManager::IsoHandler::eHTToString(enum EHandlerType t) {
    switch (t) {
        case eHT_Receive: return "Receive";
        case eHT_Transmit: return "Transmit";
        default: return "error: unknown type";
    }
}
