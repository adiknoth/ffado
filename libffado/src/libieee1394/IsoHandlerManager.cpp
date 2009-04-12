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
#include <assert.h>

IMPL_DEBUG_MODULE( IsoHandlerManager, IsoHandlerManager, DEBUG_LEVEL_NORMAL );
IMPL_DEBUG_MODULE( IsoTask, IsoTask, DEBUG_LEVEL_NORMAL );

using namespace Streaming;

// --- ISO Thread --- //

IsoTask::IsoTask(IsoHandlerManager& manager, enum IsoHandler::EHandlerType t)
    : m_manager( manager )
    , m_SyncIsoHandler ( NULL )
    , m_handlerType( t )
    , m_running( false )
    , m_in_busreset( false )
    , m_activity_wait_timeout_nsec (ISOHANDLERMANAGER_ISO_TASK_WAIT_TIMEOUT_USECS * 1000LL)
{
}

IsoTask::~IsoTask()
{
    sem_destroy(&m_activity_semaphore);
}

bool
IsoTask::Init()
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
IsoTask::requestShadowMapUpdate()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "(%p) enter\n", this);
    INC_ATOMIC(&request_update);

    // get the thread going again
    signalActivity();
    debugOutput(DEBUG_LEVEL_VERBOSE, "(%p) exit\n", this);
}

bool
IsoTask::handleBusReset()
{
    bool retval = true;
    m_in_busreset = true;
    requestShadowMapUpdate();
    if(request_update) {
        debugError("shadow map update request not honored\n");
        return false;
    }

    unsigned int i, max;
    max = m_manager.m_IsoHandlers.size();
    for (i = 0; i < max; i++) {
        IsoHandler *h = m_manager.m_IsoHandlers.at(i);
        assert(h);

        // skip the handlers not intended for us
        if(h->getType() != m_handlerType) continue;

        if (!h->handleBusReset()) {
            debugWarning("Failed to handle busreset on %p\n");
            retval = false;
        }
    }

    // re-enable processing
    m_in_busreset = false;
    requestShadowMapUpdate();
    if(request_update) {
        debugError("shadow map update request not honored\n");
        return false;
    }
    return retval;
}

// updates the internal stream map
// note that this should be executed with the guarantee that
// nobody will modify the parent data structures
void
IsoTask::updateShadowMapHelper()
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
IsoTask::Execute()
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
                // if we are going to poll() it, let's ensure
                // it can run until someone wants it to exit
                h->allowIterateLoop();
            }
            m_poll_fds_shadow[i].events = events;
        }

        if(no_one_to_poll) {
            debugOutput(DEBUG_LEVEL_ULTRA_VERBOSE,
                        "(%p, %s) No one to poll, waiting for something to happen\n",
                        this, (m_handlerType == IsoHandler::eHT_Transmit? "Transmit": "Receive"));
            // wait for something to happen
            switch(waitForActivity()) {
                case IsoTask::eAR_Error:
                    debugError("Error while waiting for activity\n");
                    return false;
                case IsoTask::eAR_Interrupted:
                    // FIXME: what to do here?
                    debugWarning("Interrupted while waiting for activity\n");
                    break;
                case IsoTask::eAR_Timeout:
                    // FIXME: what to do here?
                    debugWarning("Timeout while waiting for activity\n");
                    no_one_to_poll = false; // exit the loop to be able to detect failing handlers
                    break;
                case IsoTask::eAR_Activity:
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
                           "(%p, %s) check handler %d: diff = %lld, max = %lld, now: %08lX, last: %08lX\n",
                           this, (m_handlerType == IsoHandler::eHT_Transmit? "Transmit": "Receive"), 
                           i, measured_diff_ticks, max_diff_ticks, ctr_at_poll_return, last_packet_seen);
        if(measured_diff_ticks > max_diff_ticks) {
            debugFatal("(%p, %s) Handler died: now: %08lX, last: %08lX, diff: %lld (max: %lld)\n",
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

enum IsoTask::eActivityResult
IsoTask::waitForActivity()
{
    debugOutputExtreme(DEBUG_LEVEL_VERBOSE,
                       "(%p, %s) waiting for activity\n",
                       this, (m_handlerType == IsoHandler::eHT_Transmit? "Transmit": "Receive"));
    struct timespec ts;
    int result;

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
            debugError("(%p) timeout_nsec=%lld ts.sec=%d ts.nsec=%lld\n", 
                       this, m_activity_wait_timeout_nsec, ts.tv_sec, ts.tv_nsec);
            return eAR_Error;
        } else {
            debugError("(%p) sem_timedwait error (result=%d errno=%d)\n", 
                        this, result, errno);
            debugError("(%p) timeout_nsec=%lld ts.sec=%d ts.nsec=%lld\n", 
                       this, m_activity_wait_timeout_nsec, ts.tv_sec, ts.tv_nsec);
            return eAR_Error;
        }
    }

    debugOutput(DEBUG_LEVEL_ULTRA_VERBOSE,
                "(%p, %s) got activity\n",
                this, (m_handlerType == IsoHandler::eHT_Transmit? "Transmit": "Receive"));
    return eAR_Activity;
}

void
IsoTask::signalActivity()
{
    // signal the activity cond var
    sem_post(&m_activity_semaphore);
    debugOutput(DEBUG_LEVEL_ULTRA_VERBOSE,
                "(%p, %s) activity\n",
                this, (m_handlerType == IsoHandler::eHT_Transmit? "Transmit": "Receive"));
}

void IsoTask::setVerboseLevel(int i) {
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

        enum raw1394_iso_dma_recv_mode receive_mode;
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
    debugOutput( DEBUG_LEVEL_VERBOSE, " %d streams, %d handlers registered\n",
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

IsoHandler *
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

    if (retval) {
        m_State=E_Prepared;
    } else {
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
        case E_Prepared: return "Prepared";
        case E_Running: return "Running";
        case E_Error: return "Error";
    }
}
