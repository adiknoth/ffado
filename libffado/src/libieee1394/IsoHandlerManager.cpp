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

#include "IsoHandlerManager.h"
#include "ieee1394service.h" 
#include "IsoHandler.h"
#include "libstreaming/generic/StreamProcessor.h"

#include "libutil/Atomic.h"

#include <assert.h>

#define MINIMUM_INTERRUPTS_PER_PERIOD  8U

IMPL_DEBUG_MODULE( IsoHandlerManager, IsoHandlerManager, DEBUG_LEVEL_NORMAL );

using namespace Streaming;

IsoHandlerManager::IsoHandlerManager(Ieee1394Service& service)
   : m_State(E_Created)
   , m_service( service )
   , m_realtime(false), m_priority(0)
{}

IsoHandlerManager::IsoHandlerManager(Ieee1394Service& service, bool run_rt, int rt_prio)
   : m_State(E_Created)
   , m_service( service )
   , m_realtime(run_rt), m_priority(rt_prio)
{}

IsoHandlerManager::~IsoHandlerManager()
{
    stopHandlers();
}

bool
IsoHandlerManager::setThreadParameters(bool rt, int priority) {
    debugOutput( DEBUG_LEVEL_VERBOSE, "(%p) switch to: (rt=%d, prio=%d)...\n", this, rt, priority);
    if (priority > 98) priority = 98; // cap the priority
    m_realtime = rt;
    m_priority = priority;
    bool result = true;
    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
        it != m_IsoHandlers.end();
        ++it )
    {
        result &= (*it)->setThreadParameters(m_realtime, m_priority);
    }
    return result;
}

bool IsoHandlerManager::init()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Initializing ISO manager %p...\n", this);
    // check state
    if(m_State != E_Created) {
        debugError("Manager already initialized...\n");
        return false;
    }

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
        //max_packet_size = getpagesize(); // HACK
        //irq_interval=2; // HACK
        // create the actual handler
        h = new IsoHandler(*this, IsoHandler::eHT_Receive,
                           buffers, max_packet_size, irq_interval);
        if(!h) {
            debugFatal("Could not create IsoRecvHandler\n");
            return false;
        }
        debugOutput( DEBUG_LEVEL_VERBOSE, " registering IsoRecvHandler\n");

    } else if (stream->getType()==StreamProcessor::ePT_Transmit) {
        // setup the optimal parameters for the raw1394 ISO buffering
        unsigned int packets_per_period = stream->getPacketsPerPeriod();

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
         if(irq_interval <= 0) irq_interval = 1;

        // the transmit buffer size should be as low as possible for latency.
        // note however that the raw1394 subsystem tries to keep this buffer
        // full, so we have to make sure that we have enough events in our
        // event buffers

        // FIXME: latency spoiler
        // every irq_interval packets an interrupt will occur. that is when
        // buffers get transfered, meaning that we should have at least some
        // margin here
        //irq_interval=2;
        //int buffers=30;
        //max_packet_size = getpagesize(); // HACK

        // the SP specifies how many packets to buffer
        int buffers = stream->getNbPacketsIsoXmitBuffer();

        // create the actual handler
        h = new IsoHandler(*this, IsoHandler::eHT_Transmit,
                           buffers, max_packet_size, irq_interval);

        debugOutput( DEBUG_LEVEL_VERBOSE, " registering IsoXmitHandler\n");

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
    if(!h->setThreadParameters(m_realtime, m_priority)) {
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
