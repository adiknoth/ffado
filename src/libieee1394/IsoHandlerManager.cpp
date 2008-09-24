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

bool
IsoTask::requestShadowMapUpdate()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "(%p) enter\n", this);
    INC_ATOMIC(&request_update);

    // get the thread going again
    signalActivity();

    if (m_running) {
        int timeout = 1000;
        while(request_update && timeout--) {
            Util::SystemTimeSource::SleepUsecRelative(1000);
        }
        if(timeout == 0) {
            debugError("timeout waiting for shadow map update\n");
        }
    }
    debugOutput(DEBUG_LEVEL_VERBOSE, "(%p) exit\n", this);
    return true;
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
    long long int timeout_nsec = ISOHANDLERMANAGER_ISO_TASK_WAIT_TIMEOUT_USECS * 1000LL;

    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        debugError("clock_gettime failed\n");
        return eAR_Error;
    }

    ts.tv_nsec += timeout_nsec;
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
                       this, timeout_nsec, ts.tv_sec, ts.tv_nsec);
            return eAR_Error;
        } else {
            debugError("(%p) sem_timedwait error (result=%d errno=%d)\n", 
                        this, result, errno);
            debugError("(%p) timeout_nsec=%lld ts.sec=%d ts.nsec=%lld\n", 
                       this, timeout_nsec, ts.tv_sec, ts.tv_nsec);
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
    m_realtime = rt;
    m_priority = priority;

    if (m_IsoThreadTransmit) {
        if (m_realtime) {
            m_IsoThreadTransmit->AcquireRealTime(m_priority
                                                 + ISOHANDLERMANAGER_ISO_PRIO_INCREASE
                                                 + ISOHANDLERMANAGER_ISO_PRIO_INCREASE_XMIT);
        } else {
            m_IsoThreadTransmit->DropRealTime();
        }
    }
    if (m_IsoThreadReceive) {
        if (m_realtime) {
            m_IsoThreadReceive->AcquireRealTime(m_priority 
                                                + ISOHANDLERMANAGER_ISO_PRIO_INCREASE
                                                + ISOHANDLERMANAGER_ISO_PRIO_INCREASE_RECV);
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

    // create threads to iterate our ISO handlers
    debugOutput( DEBUG_LEVEL_VERBOSE, "Create iso thread for %p transmit...\n", this);
    m_IsoTaskTransmit = new IsoTask( *this, IsoHandler::eHT_Transmit );
    if(!m_IsoTaskTransmit) {
        debugFatal("No task\n");
        return false;
    }
    m_IsoTaskTransmit->setVerboseLevel(getDebugLevel());
    m_IsoThreadTransmit = new Util::PosixThread(m_IsoTaskTransmit, "ISOXMT", m_realtime,
                                                m_priority + ISOHANDLERMANAGER_ISO_PRIO_INCREASE
                                                + ISOHANDLERMANAGER_ISO_PRIO_INCREASE_XMIT,
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
                                               m_priority + ISOHANDLERMANAGER_ISO_PRIO_INCREASE
                                               + ISOHANDLERMANAGER_ISO_PRIO_INCREASE_RECV,
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

bool
IsoHandlerManager::disable(IsoHandler *h) {
    bool result;
    int i=0;
    debugOutput(DEBUG_LEVEL_VERBOSE, "Disable on IsoHandler %p\n", h);
    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
        it != m_IsoHandlers.end();
        ++it )
    {
        if ((*it) == h) {
            result = h->disable();
            if(h->getType() == IsoHandler::eHT_Transmit) {
                result &= m_IsoTaskTransmit->requestShadowMapUpdate();
            } else {
                result &= m_IsoTaskReceive->requestShadowMapUpdate();
            }
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE, " disabled\n");
            return result;
        }
        i++;
    }
    debugError("Handler not found\n");
    return false;
}

bool
IsoHandlerManager::enable(IsoHandler *h) {
    bool result;
    int i=0;
    debugOutput(DEBUG_LEVEL_VERBOSE, "Enable on IsoHandler %p\n", h);
    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
        it != m_IsoHandlers.end();
        ++it )
    {
        if ((*it) == h) {
            result = h->enable();
            if(h->getType() == IsoHandler::eHT_Transmit) {
                result &= m_IsoTaskTransmit->requestShadowMapUpdate();
            } else {
                result &= m_IsoTaskReceive->requestShadowMapUpdate();
            }
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE, " enabled\n");
            return result;
        }
        i++;
    }
    debugError("Handler not found\n");
    return false;
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
        // setup the optimal parameters for the raw1394 ISO buffering
        unsigned int packets_per_period = stream->getPacketsPerPeriod();
        unsigned int max_packet_size = stream->getMaxPacketSize() + 8; // bufferfill takes another 8 bytes for headers
        unsigned int page_size = getpagesize();

        // Ensure we don't request a packet size bigger than the
        // kernel-enforced maximum which is currently 1 page.
        // NOTE: PP: this is not really true AFAICT
        if (max_packet_size > page_size) {
            debugError("max packet size (%u) > page size (%u)\n", max_packet_size, page_size);
            return false;
        }

        // the interrupt/wakeup interval prediction of raw1394 is a mess...
        int wanted_irq_interval = (packets_per_period-1) / MINIMUM_INTERRUPTS_PER_PERIOD;
        if(wanted_irq_interval <= 0) wanted_irq_interval=1;

        // mimic kernel initialization
        unsigned int kern_buff_stride = RAW1394_RCV_MIN_BUF_STRIDE;
        for (; kern_buff_stride < max_packet_size; kern_buff_stride *= 2);
        if (kern_buff_stride > page_size) kern_buff_stride = page_size;
        // kern_buff_stride is the minimal granularity of interrupts
        // therefore the following will result in on-average correct interrupt timing
        int irq_interval = (wanted_irq_interval * max_packet_size) / kern_buff_stride;

        // the receive buffer size doesn't matter for the latency,
        // but it has a minimal value in order for libraw to operate correctly (300)
        int buffers=400;

        // create the actual handler
        h = new IsoHandler(*this, IsoHandler::eHT_Receive,
                           buffers, max_packet_size, irq_interval);

        debugOutput( DEBUG_LEVEL_VERBOSE, " creating IsoRecvHandler\n");

        if(!h) {
            debugFatal("Could not create IsoRecvHandler\n");
            return false;
        }

    } else if (stream->getType()==StreamProcessor::ePT_Transmit) {
        // setup the optimal parameters for the raw1394 ISO buffering
//        unsigned int packets_per_period = stream->getPacketsPerPeriod();
        unsigned int max_packet_size = stream->getMaxPacketSize();
//         unsigned int page_size = getpagesize();

        // Ensure we don't request a packet size bigger than the
        // kernel-enforced maximum which is currently 1 page.
//         if (max_packet_size > page_size) {
//             debugError("max packet size (%u) > page size (%u)\n", max_packet_size, page_size);
//             return false;
//         }
        if (max_packet_size > MAX_XMIT_PACKET_SIZE) {
            debugError("max packet size (%u) > MAX_XMIT_PACKET_SIZE (%u)\n",
                       max_packet_size, MAX_XMIT_PACKET_SIZE);
            return false;
        }

        // the SP specifies how many packets to ISO-buffer
        int buffers = stream->getNbPacketsIsoXmitBuffer();
        if (buffers > MAX_XMIT_NB_BUFFERS) {
            debugOutput(DEBUG_LEVEL_VERBOSE,
                        "nb buffers (%u) > MAX_XMIT_NB_BUFFERS (%u)\n",
                        buffers, MAX_XMIT_NB_BUFFERS);
            buffers = MAX_XMIT_NB_BUFFERS;
        }
        unsigned int irq_interval = buffers / MINIMUM_INTERRUPTS_PER_PERIOD;
        if(irq_interval <= 0) irq_interval=1;

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

    // init the handler
    if(!h->init()) {
        debugFatal("Could not initialize receive handler\n");
        return false;
    }

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
            if(!(*it)->disable()) {
                debugOutput( DEBUG_LEVEL_VERBOSE, " could not disable handler (%p)\n",*it);
                return false;
            }
            bool result;
            if((*it)->getType() == IsoHandler::eHT_Transmit) {
                result = m_IsoTaskTransmit->requestShadowMapUpdate();
            } else {
                result = m_IsoTaskReceive->requestShadowMapUpdate();
            }
            if(!result) {
                debugOutput( DEBUG_LEVEL_VERBOSE, " could not update shadow map for handler (%p)\n",*it);
                return false;
            }
            return true;
        }
    }
    debugError("Stream %p has no attached handler\n", stream);
    return false;
}

int
IsoHandlerManager::getPacketLatencyForStream(Streaming::StreamProcessor *stream) {
    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
      it != m_IsoHandlers.end();
      ++it )
    {
        if((*it)->isStreamRegistered(stream)) {
            unsigned int page_size = getpagesize();
            unsigned int max_packet_size = stream->getMaxPacketSize() + 8;
            int average_packet_size_bytes = stream->getAveragePacketSize();
            int irq_interval = (*it)->getIrqInterval();

            // mimic kernel initialization
            unsigned int kern_buff_stride = RAW1394_RCV_MIN_BUF_STRIDE;
            for (; kern_buff_stride < max_packet_size; kern_buff_stride *= 2);
            if (kern_buff_stride > page_size) kern_buff_stride = page_size;
            
            // we can only have one interrupt every kern_buff_stride bytes
            int packets_per_block = kern_buff_stride / average_packet_size_bytes;
            int blocks_per_interrupt = irq_interval / packets_per_block + 1;
            return blocks_per_interrupt * packets_per_block;
        }
    }
    debugError("Stream %p has no attached handler\n", stream);
    return 0;
}

void
IsoHandlerManager::flushHandlerForStream(Streaming::StreamProcessor *stream) {
    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
      it != m_IsoHandlers.end();
      ++it )
    {
        if((*it)->isStreamRegistered(stream)) {
            (*it)->flush();
        }
    }
    debugError("Stream %p has no attached handler\n", stream);
    return;
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
            if(!(*it)->enable(cycle)) {
                debugOutput( DEBUG_LEVEL_VERBOSE, " could not enable handler (%p)\n",*it);
                return false;
            }
            bool result;
            if((*it)->getType() == IsoHandler::eHT_Transmit) {
                result = m_IsoTaskTransmit->requestShadowMapUpdate();
            } else {
                result = m_IsoTaskReceive->requestShadowMapUpdate();
            }
            if(!result) {
                debugOutput( DEBUG_LEVEL_VERBOSE, " could not update shadow map for handler (%p)\n",*it);
                return false;
            }
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
        if(!(*it)->disable()){
            debugOutput( DEBUG_LEVEL_VERBOSE, " could not stop handler (%p)\n",*it);
            retval=false;
        }
        bool result;
        if((*it)->getType() == IsoHandler::eHT_Transmit) {
            result = m_IsoTaskTransmit->requestShadowMapUpdate();
        } else {
            result = m_IsoTaskReceive->requestShadowMapUpdate();
        }
        if(!result) {
            debugOutput( DEBUG_LEVEL_VERBOSE, " could not update shadow map for handler (%p)\n",*it);
            return false;
        }
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
