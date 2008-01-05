/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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
#include "IsoHandler.h"
#include "libstreaming/generic/StreamProcessor.h"

#include "libutil/Atomic.h"

#include "libutil/PosixThread.h"

#include <assert.h>

IMPL_DEBUG_MODULE( IsoHandlerManager, IsoHandlerManager, DEBUG_LEVEL_NORMAL );

using namespace Streaming;

IsoHandlerManager::IsoHandlerManager(Ieee1394Service& service)
   : m_State(E_Created)
   , m_service( service )
   , m_realtime(false), m_priority(0)
   , m_Thread ( NULL )
{}

IsoHandlerManager::IsoHandlerManager(Ieee1394Service& service, bool run_rt, int rt_prio)
   : m_State(E_Created)
   , m_service( service )
   , m_realtime(run_rt), m_priority(rt_prio)
   , m_Thread ( NULL )
{}

IsoHandlerManager::~IsoHandlerManager()
{
    stopHandlers();
    pruneHandlers();
    if(m_IsoHandlers.size() > 0) {
        debugError("Still some handlers in use\n");
    }
    if (m_Thread) {
        m_Thread->Stop();
        delete m_Thread;
    }
}

bool
IsoHandlerManager::setThreadParameters(bool rt, int priority) {
    debugOutput( DEBUG_LEVEL_VERBOSE, "(%p) switch to: (rt=%d, prio=%d)...\n", this, rt, priority);
    if (priority > THREAD_MAX_RTPRIO) priority = THREAD_MAX_RTPRIO; // cap the priority
    m_realtime = rt;
    m_priority = priority;
    bool result = true;
    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
        it != m_IsoHandlers.end();
        ++it )
    {
        result &= (*it)->setThreadParameters(m_realtime, m_priority);
    }

    if (m_Thread) {
        if (m_realtime) {
            m_Thread->AcquireRealTime(m_priority);
        } else {
            m_Thread->DropRealTime();
        }
    }

    return result;
}

/**
 * Update the shadow variables. Should only be called from
 * the iso handler iteration thread
 */
void
IsoHandlerManager::updateShadowVars()
{
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "updating shadow vars...\n");
    unsigned int i;
    m_poll_nfds_shadow = m_IsoHandlers.size();
    if(m_poll_nfds_shadow > ISOHANDLERMANAGER_MAX_ISO_HANDLERS_PER_PORT) {
        debugWarning("Too much ISO Handlers in manager...\n");
        m_poll_nfds_shadow = ISOHANDLERMANAGER_MAX_ISO_HANDLERS_PER_PORT;
    }
    for (i = 0; i < m_poll_nfds_shadow; i++) {
        IsoHandler *h = m_IsoHandlers.at(i);
        assert(h);
        m_IsoHandler_map_shadow[i] = h;

        m_poll_fds_shadow[i].fd = h->getFileDescriptor();
        m_poll_fds_shadow[i].revents = 0;
        if (h->isEnabled()) {
            m_poll_fds_shadow[i].events = POLLIN;
        } else {
            m_poll_fds_shadow[i].events = 0;
        }
    }
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, " updated shadow vars...\n");
}

bool
IsoHandlerManager::Init() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "%p: Init thread...\n", this);
    bool result = true;
    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
        it != m_IsoHandlers.end();
        ++it )
    {
        result &= (*it)->Init();
    }
    return result;
}

bool
IsoHandlerManager::Execute() {
    int err;
    unsigned int i;

    unsigned int m_poll_timeout = 100;

    updateShadowVars();
    // bypass if no handlers are registered
    if (m_poll_nfds_shadow == 0) {
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "bypass iterate since no handlers registered\n");
        usleep(m_poll_timeout * 1000);
        return true;
    }

    // Use a shadow map of the fd's such that the poll call is not in a critical section
    uint64_t poll_enter = m_service.getCurrentTimeAsUsecs();
    err = poll (m_poll_fds_shadow, m_poll_nfds_shadow, m_poll_timeout);
    uint64_t poll_exit = m_service.getCurrentTimeAsUsecs();

    if (err == -1) {
        if (errno == EINTR) {
            return true;
        }
        debugFatal("poll error: %s\n", strerror (errno));
        return false;
    }

    int nb_rcv = 0;
    int nb_xmit = 0;
    uint64_t iter_enter = m_service.getCurrentTimeAsUsecs();
    for (i = 0; i < m_poll_nfds_shadow; i++) {
        if(m_poll_fds_shadow[i].revents) {
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "received events: %08X for (%p)\n",
                m_poll_fds_shadow[i].revents, m_IsoHandler_map_shadow[i]);
        }
        if (m_poll_fds_shadow[i].revents & POLLERR) {
            debugWarning("error on fd for %d\n",i);
        }

        if (m_poll_fds_shadow[i].revents & POLLHUP) {
            debugWarning("hangup on fd for %d\n",i);
        }

        if(m_poll_fds_shadow[i].revents & (POLLIN)) {
            if (m_IsoHandler_map_shadow[i]->getType() == IsoHandler::eHT_Receive) {
                m_IsoHandler_map_shadow[i]->iterate();
                nb_rcv++;
            } else {
                // only iterate the xmit handler if it makes sense
                if(m_IsoHandler_map_shadow[i]->tryWaitForClient()) {
                    m_IsoHandler_map_shadow[i]->iterate();
                    nb_xmit++;
                }
            }
        }
    }
    uint64_t iter_exit = m_service.getCurrentTimeAsUsecs();

    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, " poll took %6lldus, iterate took %6lldus, iterated (R: %2d, X: %2d) handlers\n",
                poll_exit-poll_enter, iter_exit-iter_enter,
                nb_rcv, nb_xmit);

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

#if ISOHANDLER_PER_HANDLER_THREAD
    // the IsoHandlers will create their own thread.
#else
    // create a thread to iterate our handlers
    debugOutput( DEBUG_LEVEL_VERBOSE, "Start thread for %p...\n", this);
    m_Thread = new Util::PosixThread(this, m_realtime, m_priority, 
                                     PTHREAD_CANCEL_DEFERRED);
    if(!m_Thread) {
        debugFatal("No thread\n");
        return false;
    }
    if (m_Thread->Start() != 0) {
        debugFatal("Could not start update thread\n");
        return false;
    }
#endif

    m_State=E_Running;
    return true;
}

bool
IsoHandlerManager::disable(IsoHandler *h) {
    bool result;
    int i=0;
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "Disable on IsoHandler %p\n", h);
    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
        it != m_IsoHandlers.end();
        ++it )
    {
        if ((*it) == h) {
            result = h->disable();
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
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "Enable on IsoHandler %p\n", h);
    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
        it != m_IsoHandlers.end();
        ++it )
    {
        if ((*it) == h) {
            result = h->enable();
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE, " enabled\n");
            return result;
        }
        i++;
    }
    debugError("Handler not found\n");
    return false;
}

bool IsoHandlerManager::registerHandler(IsoHandler *handler)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
    assert(handler);
    handler->setVerboseLevel(getDebugLevel());
    m_IsoHandlers.push_back(handler);
    updateShadowVars();
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
            updateShadowVars();
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
        unsigned int packets_per_period = stream->getPacketsPerPeriod();
        unsigned int max_packet_size = stream->getMaxPacketSize();
        unsigned int page_size = getpagesize();

        // Ensure we don't request a packet size bigger than the
        // kernel-enforced maximum which is currently 1 page.
        if (max_packet_size > page_size) {
            debugError("max packet size (%u) > page size (%u)\n", max_packet_size, page_size);
            return false;
        }

        max_packet_size = page_size;
        unsigned int irq_interval = packets_per_period / MINIMUM_INTERRUPTS_PER_PERIOD;
        if(irq_interval <= 0) irq_interval=1;

        // the SP specifies how many packets to ISO-buffer
        int buffers = stream->getNbPacketsIsoXmitBuffer();

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

    // set the handler's thread parameters
    // receive handlers have lower priority than the client thread
    // since they have ISO side buffering
    // xmit handlers have higher priority since we want client side
    // frames to be put into the ISO buffers ASAP
    int thread_prio;
    if (stream->getType()==StreamProcessor::ePT_Receive) {
        thread_prio = m_priority - 1;
        if (thread_prio < THREAD_MIN_RTPRIO) thread_prio = THREAD_MIN_RTPRIO;
    } else {
        thread_prio = m_priority + 1;
        if (thread_prio > THREAD_MAX_RTPRIO) thread_prio = THREAD_MAX_RTPRIO;
    }

    if(!h->setThreadParameters(m_realtime, thread_prio)) {
        debugFatal("Could not set handler thread parameters\n");
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
            bool result;
            debugOutput( DEBUG_LEVEL_VERBOSE, " stopping handler %p for stream %p\n", *it, stream);
            result = (*it)->disable();
            if(!result) {
                debugOutput( DEBUG_LEVEL_VERBOSE, " could not disable handler (%p)\n",*it);
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
            bool result;
            debugOutput( DEBUG_LEVEL_VERBOSE, " starting handler %p for stream %p\n", *it, stream);
            result = (*it)->enable(cycle);
            if(!result) {
                debugOutput( DEBUG_LEVEL_VERBOSE, " could not enable handler (%p)\n",*it);
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
}

void IsoHandlerManager::dumpInfo() {
    int i=0;
    debugOutputShort( DEBUG_LEVEL_NORMAL, "Dumping IsoHandlerManager Stream handler information...\n");
    debugOutputShort( DEBUG_LEVEL_NORMAL, " State: %d\n",(int)m_State);

    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
          it != m_IsoHandlers.end();
          ++it )
    {
        debugOutputShort( DEBUG_LEVEL_NORMAL, " IsoHandler %d (%p)\n",i++,*it);
        (*it)->dumpInfo();
    }
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
