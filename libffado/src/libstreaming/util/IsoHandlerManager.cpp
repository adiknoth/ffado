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

#include "IsoHandlerManager.h"
#include "IsoHandler.h"
#include "../generic/IsoStream.h"

#include "libutil/PosixThread.h"

#include <assert.h>

#define MINIMUM_INTERRUPTS_PER_PERIOD  4U
#define PACKETS_PER_INTERRUPT          4U

namespace Streaming
{

IMPL_DEBUG_MODULE( IsoHandlerManager, IsoHandlerManager, DEBUG_LEVEL_NORMAL );

IsoHandlerManager::IsoHandlerManager() :
   m_State(E_Created),
   m_poll_timeout(100), m_poll_fds(0), m_poll_nfds(0),
   m_realtime(false), m_priority(0), m_xmit_nb_frames( 20 )
{}

IsoHandlerManager::IsoHandlerManager(bool run_rt, unsigned int rt_prio) :
   m_State(E_Created),
   m_poll_timeout(100), m_poll_fds(0), m_poll_nfds(0),
   m_realtime(run_rt), m_priority(rt_prio), m_xmit_nb_frames( 20 )
{}

bool IsoHandlerManager::init()
{
    // the tread that performs the actual packet transfer
    // needs high priority
    unsigned int prio=m_priority+6;

    if (prio>98) prio=98;

    m_isoManagerThread=new Util::PosixThread(
        this,
        m_realtime, prio,
        PTHREAD_CANCEL_DEFERRED);

    if(!m_isoManagerThread) {
        debugFatal("Could not create iso manager thread\n");
        return false;
    }

    // propagate the debug level
    m_isoManagerThread->setVerboseLevel(getDebugLevel());

    return true;
}

bool IsoHandlerManager::Init()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
    pthread_mutex_init(&m_debug_lock, NULL);

    return true;
}

/**
 * the IsoHandlerManager thread execute function iterates the handlers.
 *
 * This means that once the thread is running, streams are
 * transmitted and received (if present on the bus). Make sure
 * that the clients are registered & ready before starting the
 * thread!
 *
 * The register and unregister functions are thread unsafe, so
 * should not be used when the thread is running.
 *
 * @return false if the handlers could not be iterated.
 */
bool IsoHandlerManager::Execute()
{
//     updateCycleTimers();

    pthread_mutex_lock(&m_debug_lock);

    if(!iterate()) {
        debugFatal("Could not iterate the isoManager\n");
        pthread_mutex_unlock(&m_debug_lock);
        return false;
    }

    pthread_mutex_unlock(&m_debug_lock);

    return true;
}

/**
 * Poll the handlers managed by this manager, and iterate them
 * when ready
 *
 * @return true when successful
 */
bool IsoHandlerManager::iterate()
{
    int err;
    int i=0;
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "poll %d fd's, timeout = %dms...\n", m_poll_nfds, m_poll_timeout);

    err = poll (m_poll_fds, m_poll_nfds, m_poll_timeout);

    if (err == -1) {
        if (errno == EINTR) {
            return true;
        }
        debugFatal("poll error: %s\n", strerror (errno));
        return false;
    }

//     #ifdef DEBUG
//     for (i = 0; i < m_poll_nfds; i++) {
//         IsoHandler *s = m_IsoHandlers.at(i);
//         assert(s);
//         debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "(%d) handler %p: iterate? %d, revents: %08X\n", 
//             i, s, (m_poll_fds[i].revents & (POLLIN) == 1), m_poll_fds[i].revents);
//     }
//     #endif

    for (i = 0; i < m_poll_nfds; i++) {
        if (m_poll_fds[i].revents & POLLERR) {
            debugWarning("error on fd for %d\n",i);
        }

        if (m_poll_fds[i].revents & POLLHUP) {
            debugWarning("hangup on fd for %d\n",i);
        }

        if(m_poll_fds[i].revents & (POLLIN)) {
            IsoHandler *s = m_IsoHandlers.at(i);
            assert(s);
            s->iterate();
        }
    }

    return true;

}

bool IsoHandlerManager::registerHandler(IsoHandler *handler)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
    assert(handler);

    m_IsoHandlers.push_back(handler);

    handler->setVerboseLevel(getDebugLevel());

    // rebuild the fd map for poll()'ing.
    return rebuildFdMap();

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
            // erase the iso handler from the list
            m_IsoHandlers.erase(it);
            // rebuild the fd map for poll()'ing.
            return rebuildFdMap();
        }
    }
    debugFatal("Could not find handler (%p)\n", handler);

    return false; //not found

}

bool IsoHandlerManager::rebuildFdMap() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
    int i=0;

    m_poll_nfds=0;
    if(m_poll_fds) free(m_poll_fds);

    // count the number of handlers
    m_poll_nfds=m_IsoHandlers.size();

    // allocate the fd array
    m_poll_fds   = (struct pollfd *) calloc (m_poll_nfds, sizeof (struct pollfd));
    if(!m_poll_fds) {
        debugFatal("Could not allocate memory for poll FD array\n");
        return false;
    }

    // fill the fd map
    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
      it != m_IsoHandlers.end();
      ++it )
    {
        m_poll_fds[i].fd=(*it)->getFileDescriptor();
        m_poll_fds[i].events = POLLIN;
        i++;
    }

    return true;
}

void IsoHandlerManager::disablePolling(IsoStream *stream) {
    int i=0;

    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "Disable polling on stream %p\n",stream);

    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
        it != m_IsoHandlers.end();
        ++it )
    {
        if ((*it)->isStreamRegistered(stream)) {
            m_poll_fds[i].events = 0;
            m_poll_fds[i].revents = 0;
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "polling disabled\n");
        }

        i++;
    }
}

void IsoHandlerManager::enablePolling(IsoStream *stream) {
    int i=0;

    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "Enable polling on stream %p\n",stream);

    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
        it != m_IsoHandlers.end();
        ++it )
    {
        if ((*it)->isStreamRegistered(stream)) {
            m_poll_fds[i].events = POLLIN;
            m_poll_fds[i].revents = 0;
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "polling enabled\n");
        }

        i++;
    }
}


/**
 * Registers an IsoStream with the IsoHandlerManager.
 *
 * If nescessary, an IsoHandler is created to handle this stream.
 * Once an IsoStream is registered to the handler, it will be included
 * in the ISO streaming cycle (i.e. receive/transmit of it will occur).
 *
 * @param stream the stream to register
 * @return true if registration succeeds
 *
 * \todo : currently there is a one-to-one mapping
 *        between streams and handlers, this is not ok for
 *        multichannel receive
 */
bool IsoHandlerManager::registerStream(IsoStream *stream)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Registering stream %p\n",stream);
    assert(stream);

    // make sure the stream isn't already attached to a handler
    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
      it != m_IsoHandlers.end();
      ++it )
    {
        if((*it)->isStreamRegistered(stream)) {
            debugWarning( "stream already registered!\n");
            (*it)->unregisterStream(stream);

        }
    }

    // clean up all handlers that aren't used
    pruneHandlers();

    // allocate a handler for this stream
    if (stream->getStreamType()==IsoStream::eST_Receive) {
        // setup the optimal parameters for the raw1394 ISO buffering
        unsigned int packets_per_period = stream->getPacketsPerPeriod();

#if 1
        // hardware interrupts occur when one DMA block is full, and the size of one DMA
        // block = PAGE_SIZE. Setting the max_packet_size makes sure that the HW irq
        // occurs at a period boundary (optimal CPU use)

        // NOTE: try and use MINIMUM_INTERRUPTS_PER_PERIOD hardware interrupts
        //       per period for better latency.
        unsigned int max_packet_size=(MINIMUM_INTERRUPTS_PER_PERIOD * getpagesize()) / packets_per_period;
        if (max_packet_size < stream->getMaxPacketSize()) {
            debugWarning("calculated max packet size (%u) < stream max packet size (%u)\n",
                         max_packet_size ,(unsigned int)stream->getMaxPacketSize());
            max_packet_size = stream->getMaxPacketSize();
        }

        // Ensure we don't request a packet size bigger than the
        // kernel-enforced maximum which is currently 1 page.
        if (max_packet_size > (unsigned int)getpagesize()) {
            debugWarning("max packet size (%u) > page size (%u)\n", max_packet_size ,(unsigned int)getpagesize());
            max_packet_size = getpagesize();
        }

        unsigned int irq_interval = packets_per_period / MINIMUM_INTERRUPTS_PER_PERIOD;
        if(irq_interval <= 0) irq_interval=1;
        // FIXME: test
        //irq_interval=1;

#else
        // hardware interrupts occur when one DMA block is full, and the size of one DMA
        // block = PAGE_SIZE. Setting the max_packet_size enables control over the IRQ
        // frequency, as the controller uses max_packet_size, and not the effective size
        // when writing to the DMA buffer.

        // configure it such that we have an irq for every PACKETS_PER_INTERRUPT packets
        unsigned int irq_interval=PACKETS_PER_INTERRUPT;

        // unless the period size doesn't allow this
        if ((packets_per_period/MINIMUM_INTERRUPTS_PER_PERIOD) < irq_interval) {
            irq_interval=1;
        }

        // FIXME: test
        irq_interval=1;
#warning Using fixed irq_interval

        unsigned int max_packet_size=getpagesize() / irq_interval;

        if (max_packet_size < stream->getMaxPacketSize()) {
            max_packet_size=stream->getMaxPacketSize();
        }

        // Ensure we don't request a packet size bigger than the
        // kernel-enforced maximum which is currently 1 page.
        if (max_packet_size > (unsigned int)getpagesize())
                    max_packet_size = getpagesize();

#endif
        /* the receive buffer size doesn't matter for the latency,
           but it has a minimal value in order for libraw to operate correctly (300) */
        int buffers=400;

        // create the actual handler
        IsoRecvHandler *h = new IsoRecvHandler(stream->getPort(), buffers,
                                               max_packet_size, irq_interval);

        debugOutput( DEBUG_LEVEL_VERBOSE, " registering IsoRecvHandler\n");

        if(!h) {
            debugFatal("Could not create IsoRecvHandler\n");
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
        debugOutput( DEBUG_LEVEL_VERBOSE, " registered stream (%p) with handler (%p)\n",stream,h);
    }

    if (stream->getStreamType()==IsoStream::eST_Transmit) {
        // setup the optimal parameters for the raw1394 ISO buffering
        unsigned int packets_per_period = stream->getPacketsPerPeriod();

#if 1
        // hardware interrupts occur when one DMA block is full, and the size of one DMA
        // block = PAGE_SIZE. Setting the max_packet_size makes sure that the HW irq
        // occurs at a period boundary (optimal CPU use)
        // NOTE: try and use MINIMUM_INTERRUPTS_PER_PERIOD interrupts per period
        //       for better latency.
        unsigned int max_packet_size=MINIMUM_INTERRUPTS_PER_PERIOD * getpagesize() / packets_per_period;
        if (max_packet_size < stream->getMaxPacketSize()) {
            max_packet_size = stream->getMaxPacketSize();
        }

        // Ensure we don't request a packet size bigger than the
        // kernel-enforced maximum which is currently 1 page.
        if (max_packet_size > (unsigned int)getpagesize())
                    max_packet_size = getpagesize();

         unsigned int irq_interval = packets_per_period / MINIMUM_INTERRUPTS_PER_PERIOD;
         if(irq_interval <= 0) irq_interval=1;
#else
        // hardware interrupts occur when one DMA block is full, and the size of one DMA
        // block = PAGE_SIZE. Setting the max_packet_size enables control over the IRQ
        // frequency, as the controller uses max_packet_size, and not the effective size
        // when writing to the DMA buffer.

        // configure it such that we have an irq for every PACKETS_PER_INTERRUPT packets
        unsigned int irq_interval = PACKETS_PER_INTERRUPT;

        // unless the period size doesn't allow this
        if ((packets_per_period/MINIMUM_INTERRUPTS_PER_PERIOD) < irq_interval) {
            irq_interval = 1;
        }

        // FIXME: test
        irq_interval = 1;
#warning Using fixed irq_interval

        unsigned int max_packet_size = getpagesize() / irq_interval;

        if (max_packet_size < stream->getMaxPacketSize()) {
            max_packet_size = stream->getMaxPacketSize();
        }

        // Ensure we don't request a packet size bigger than the
        // kernel-enforced maximum which is currently 1 page.
        if (max_packet_size > (unsigned int)getpagesize())
                    max_packet_size = getpagesize();
#endif
        // the transmit buffer size should be as low as possible for latency.
        // note however that the raw1394 subsystem tries to keep this buffer
        // full, so we have to make sure that we have enough events in our
        // event buffers

        // FIXME: latency spoiler
        // every irq_interval packets an interrupt will occur. that is when
        // buffers get transfered, meaning that we should have at least some
        // margin here
//         int buffers=irq_interval * 2;

        // the SPM specifies how many packets to buffer
        int buffers = stream->getNominalPacketsNeeded(m_xmit_nb_frames);

        // create the actual handler
        IsoXmitHandler *h = new IsoXmitHandler(stream->getPort(), buffers,
                                               max_packet_size, irq_interval);

        debugOutput( DEBUG_LEVEL_VERBOSE, " registering IsoXmitHandler\n");

        if(!h) {
            debugFatal("Could not create IsoXmitHandler\n");
            return false;
        }

        h->setVerboseLevel(getDebugLevel());

        // init the handler
        if(!h->init()) {
            debugFatal("Could not initialize transmit handler\n");
            return false;
        }

        // register the stream with the handler
        if(!h->registerStream(stream)) {
            debugFatal("Could not register transmit stream with handler\n");
            return false;
        }

        // register the handler with the manager
        if(!registerHandler(h)) {
            debugFatal("Could not register transmit handler with manager\n");
            return false;
        }
        debugOutput( DEBUG_LEVEL_VERBOSE, " registered stream (%p) with handler (%p)\n",stream,h);
    }

    m_IsoStreams.push_back(stream);
    debugOutput( DEBUG_LEVEL_VERBOSE, " %d streams, %d handlers registered\n",
                                      m_IsoStreams.size(), m_IsoHandlers.size());

    return true;
}

bool IsoHandlerManager::unregisterStream(IsoStream *stream)
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
    for ( IsoStreamVectorIterator it = m_IsoStreams.begin();
      it != m_IsoStreams.end();
      ++it )
    {
        if ( *it == stream ) {
            m_IsoStreams.erase(it);

            debugOutput( DEBUG_LEVEL_VERBOSE, " deleted stream (%p) from list...\n", *it);
            return true;
        }
    }

    return false; //not found

}

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


bool IsoHandlerManager::prepare()
{
    bool retval=true;

    debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

    // check state
    if(m_State != E_Created) {
        debugError("Incorrect state, expected E_Created, got %d\n",(int)m_State);
        return false;
    }

    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
          it != m_IsoHandlers.end();
          ++it )
    {
        if(!(*it)->prepare()) {
            debugFatal("Could not prepare handlers\n");
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

bool IsoHandlerManager::startHandlers() {
    return startHandlers(-1);
}

bool IsoHandlerManager::startHandlers(int cycle) {
    bool retval=true;

    debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

    // check state
    if(m_State != E_Prepared) {
        debugError("Incorrect state, expected E_Prepared, got %d\n",(int)m_State);
        return false;
    }

    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
        it != m_IsoHandlers.end();
        ++it )
    {
        debugOutput( DEBUG_LEVEL_VERBOSE, " starting handler (%p)\n",*it);
        if(!(*it)->start(cycle)) {
            debugOutput( DEBUG_LEVEL_VERBOSE, " could not start handler (%p)\n",*it);
            retval=false;
        }
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, "Starting ISO iterator thread...\n");

    // note: libraw1394 doesn't like it if you poll() and/or iterate() before
    //       starting the streams.
    // start the iso runner thread
    m_isoManagerThread->Start();

    if (retval) {
        m_State=E_Running;
    } else {
        m_State=E_Error;
    }

    return retval;
}

bool IsoHandlerManager::stopHandlers() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

    // check state
    if(m_State != E_Running) {
        debugError("Incorrect state, expected E_Running, got %d\n",(int)m_State);
        return false;
    }

    bool retval=true;

    debugOutput( DEBUG_LEVEL_VERBOSE, "Stopping ISO iterator thread...\n");
    m_isoManagerThread->Stop();

    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
        it != m_IsoHandlers.end();
        ++it )
    {
        debugOutput( DEBUG_LEVEL_VERBOSE, "Stopping handler (%p)\n",*it);
        if(!(*it)->stop()){
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
    if(m_isoManagerThread) {
        m_isoManagerThread->setVerboseLevel(getDebugLevel());
    }

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

} // end of namespace Streaming

