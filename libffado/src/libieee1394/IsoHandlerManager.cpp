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
#include "libstreaming/generic/StreamProcessor.h"

#include "libutil/Atomic.h"
#include "libutil/PosixThread.h"
#include "libutil/SystemTimeSource.h"
#include "libutil/Watchdog.h"

#include <assert.h>

IMPL_DEBUG_MODULE( IsoHandlerManager, IsoHandlerManager, DEBUG_LEVEL_NORMAL );
IMPL_DEBUG_MODULE( IsoTask, IsoTask, DEBUG_LEVEL_NORMAL );

using namespace Streaming;

// --- ISO Thread --- //

IsoTask::IsoTask(IsoHandlerManager& manager, enum IsoHandler::EHandlerType t)
    : m_manager( manager )
    , m_SyncIsoHandler ( NULL )
    , m_handlerType( t )
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
    return true;
}

bool
IsoTask::requestShadowMapUpdate()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "(%p) enter\n", this);
    INC_ATOMIC(&request_update);
    return true;
}

// updates the internal stream map
// note that this should be executed with the guarantee that
// nobody will modify the parent data structures
void
IsoTask::updateShadowMapHelper()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "(%p) updating shadow vars...\n", this);
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
    debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
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
            debugOutputExtreme(DEBUG_LEVEL_VERBOSE,
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

    if (err < 0) {
        if (errno == EINTR) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Ignoring poll return due to signal\n");
            return true;
        }
        debugFatal("poll error: %s\n", strerror (errno));
        return false;
    }

    for (i = 0; i < m_poll_nfds_shadow; i++) {
        #ifdef DEBUG
        if(m_poll_fds_shadow[i].revents) {
            debugOutput(DEBUG_LEVEL_ULTRA_VERBOSE,
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
            m_IsoHandler_map_shadow[i]->iterate();
        } else {
            // there might be some error condition
            if (m_poll_fds_shadow[i].revents & POLLERR) {
                debugWarning("(%p) error on fd for %d\n", this, i);
            }
            if (m_poll_fds_shadow[i].revents & POLLHUP) {
                debugWarning("(%p) hangup on fd for %d\n", this, i);
            }
        }

//         #ifdef DEBUG
//         // check if the handler is still alive
//         if(m_IsoHandler_map_shadow[i]->isDead()) {
//             debugError("Iso handler (%p, %s) is dead!\n",
//                        m_IsoHandler_map_shadow[i],
//                        m_IsoHandler_map_shadow[i]->getTypeString());
//             return false; // shutdown the system
//         }
//         #endif

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
    long long int timeout_nsec=0;
    int timeout_sec = 0;

    timeout_nsec = ISOHANDLERMANAGER_ISO_TASK_WAIT_TIMEOUT_USECS * 1000LL;
    timeout_sec = 0;
    while(timeout_nsec >= 1000000000LL) {
        timeout_sec += 1;
        timeout_nsec -= 1000000000LL;
    }
    ts.tv_nsec += timeout_nsec;
    ts.tv_sec += timeout_sec;

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
        } else {
            debugError("(%p) sem_timedwait error (result=%d errno=%d)\n", 
                        this, result, errno);
            debugError("(%p) timeout_sec=%d timeout_nsec=%lld ts.sec=%d ts.nsec=%lld\n", 
                       this, timeout_sec, timeout_nsec, ts.tv_sec, ts.tv_nsec);
            return eAR_Error;
        }
    }

    debugOutputExtreme(DEBUG_LEVEL_VERBOSE,
                       "(%p, %s) got activity\n",
                       this, (m_handlerType == IsoHandler::eHT_Transmit? "Transmit": "Receive"));
    return eAR_Activity;
}

void
IsoTask::signalActivity()
{
    // signal the activity cond var
    sem_post(&m_activity_semaphore);
    debugOutputExtreme(DEBUG_LEVEL_VERBOSE,
                       "(%p, %s) activity\n",
                       this, (m_handlerType == IsoHandler::eHT_Transmit? "Transmit": "Receive"));
}

void IsoTask::setVerboseLevel(int i) {
    setDebugLevel(i);
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
    m_IsoThreadTransmit = new Util::PosixThread(m_IsoTaskTransmit, m_realtime,
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
    m_IsoThreadReceive = new Util::PosixThread(m_IsoTaskReceive, m_realtime,
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
    debugOutput( DEBUG_LEVEL_VERBOSE, "Registering stream %p\n",stream);
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
        unsigned int max_packet_size = stream->getMaxPacketSize();
        unsigned int page_size = getpagesize() - 2; // for one reason or another this is necessary

        // Ensure we don't request a packet size bigger than the
        // kernel-enforced maximum which is currently 1 page.
        if (max_packet_size > page_size) {
            debugError("max packet size (%u) > page size (%u)\n", max_packet_size, page_size);
            return false;
        }

        unsigned int irq_interval = packets_per_period / MINIMUM_INTERRUPTS_PER_PERIOD;
        if(irq_interval <= 0) irq_interval=1;
        
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
    debugOutput( DEBUG_LEVEL_VERBOSE, "Unregistering stream %p\n",stream);
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
            return (*it)->getPacketLatency();
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
            return (*it)->flush();
        }
    }
    debugError("Stream %p has no attached handler\n", stream);
    return;
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
