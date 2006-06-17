/* $Id$ */

/*
 *   FreeBob Streaming API
 *   FreeBob = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2006 Pieter Palmers <pieterpalmers@users.sourceforge.net>
 *
 *   This program is free software {} you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation {} either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY {} without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program {} if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * 
 *
 */

#include "IsoHandlerManager.h"
#include "IsoHandler.h"
#include "IsoStream.h"
#include <assert.h>


namespace FreebobStreaming
{

IMPL_DEBUG_MODULE( IsoHandlerManager, IsoHandlerManager, DEBUG_LEVEL_NORMAL );

IsoHandlerManager::IsoHandlerManager() :
   m_poll_timeout(100), m_poll_fds(0), m_poll_nfds(0)
{

}


IsoHandlerManager::~IsoHandlerManager()
{

}

bool IsoHandlerManager::Init()
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

	return true;
}

// Intel recommends that a serializing instruction 
// should be called before and after rdtsc. 
// CPUID is a serializing instruction. 
#define read_rdtsc(time) \
	__asm__ __volatile__( \
	"pushl %%ebx\n\t" \
	"cpuid\n\t" \
 	"rdtsc\n\t" \
 	"mov %%eax,(%0)\n\t" \
 	"cpuid\n\t" \
	"popl %%ebx\n\t" \
 	: /* no output */ \
 	: "S"(&time) \
 	: "eax", "ecx", "edx", "memory")

static inline unsigned long debugGetCurrentUTime() {
    unsigned retval;
    read_rdtsc(retval);
    return retval;
}

bool IsoHandlerManager::Execute()
{
	int err;
	int i=0;
	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "enter...\n");
	
	unsigned long tstamp=debugGetCurrentUTime();

	err = poll (m_poll_fds, m_poll_nfds, m_poll_timeout);
	
// 	debugOutput(DEBUG_LEVEL_VERBOSE, "Poll took: %6d\n", debugGetCurrentUTime()-tstamp);
	
	if (err == -1) {
		if (errno == EINTR) {
			return true;
		}
		debugFatal("poll error: %s\n", strerror (errno));
		return false;
	}

	for (i = 0; i < m_poll_nfds; i++) {
		if (m_poll_fds[i].revents & POLLERR) {
			debugWarning("error on fd for %d\n",i);
		}

		if (m_poll_fds[i].revents & POLLHUP) {
			debugWarning("hangup on fd for %d\n",i);
		}
		
		if(m_poll_fds[i].revents & (POLLIN)) {
			IsoHandler *s=m_IsoHandlers.at(i);
			assert(s);
			
			unsigned int packetcount_prev=s->getPacketCount();
			
			tstamp=debugGetCurrentUTime();
			
			s->iterate();
/*			debugOutput(DEBUG_LEVEL_VERBOSE, "Iterate %p: time: %6d | packets: %3d\n", 
			     s, debugGetCurrentUTime()-tstamp, s->getPacketCount()-packetcount_prev
			     );*/
		}
	}
	return true;

}

bool IsoHandlerManager::prepare()
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
          it != m_IsoHandlers.end();
          ++it )
    {
        if(!(*it)->prepare()) {
			debugFatal("Could not prepare handlers\n");
			return false;
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
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "Disable polling on stream %p\n",stream);
	int i=0;
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
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "Enable polling on stream %p\n",stream);
	int i=0;
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
	if (stream->getType()==IsoStream::EST_Receive) {
		// setup the optimal parameters for the raw1394 ISO buffering
		unsigned int packets_per_period=stream->getPacketsPerPeriod();
		
		// hardware interrupts occur when one DMA block is full, and the size of one DMA
		// block = PAGE_SIZE. Setting the max_packet_size makes sure that the HW irq is 
		// occurs at a period boundary (optimal CPU use)
		
		// NOTE: try and use 4 hardware interrupts per period for better latency.
		unsigned int max_packet_size=4 * getpagesize() / packets_per_period;
		if (max_packet_size < stream->getMaxPacketSize()) {
			max_packet_size=stream->getMaxPacketSize();
		}
		
		int irq_interval=packets_per_period / 4;
        if(irq_interval <= 0) irq_interval=1;

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
	
	if (stream->getType()==IsoStream::EST_Transmit) {
	
		// setup the optimal parameters for the raw1394 ISO buffering
		unsigned int packets_per_period=stream->getPacketsPerPeriod();
		// hardware interrupts occur when one DMA block is full, and the size of one DMA
		// block = PAGE_SIZE. Setting the max_packet_size makes sure that the HW irq  
		// occurs at a period boundary (optimal CPU use)
		// NOTE: try and use 4 interrupts per period for better latency.
		unsigned int max_packet_size=4 * getpagesize() / packets_per_period;
		if (max_packet_size < stream->getMaxPacketSize()) {
			max_packet_size=stream->getMaxPacketSize();
		}
		
 		int irq_interval=packets_per_period / 4;
 		if(irq_interval <= 0) irq_interval=1;
        
		// the transmit buffer size should be as low as possible for latency. 
		// note however that the raw1394 subsystem tries to keep this buffer
		// full, so we have to make sure that we have enough events in our
		// event buffers
		int buffers=packets_per_period;
		
		// NOTE: this is dangerous: what if there is not enough prefill?
// 		if (buffers<10) buffers=10;	
		
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
    }

}

bool IsoHandlerManager::startHandlers() {
	return startHandlers(-1);
}

bool IsoHandlerManager::startHandlers(int cycle) {
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

	for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
	  it != m_IsoHandlers.end();
	  ++it )
	{
		debugOutput( DEBUG_LEVEL_VERBOSE, " starting handler (%p)\n",*it);
		if(!(*it)->start(cycle)) {
			debugOutput( DEBUG_LEVEL_VERBOSE, " could not start handler (%p)\n",*it);
			return false;
		}
	}
	
	return true;
}

bool IsoHandlerManager::stopHandlers() {
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

	for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
	  it != m_IsoHandlers.end();
	  ++it )
	{
		debugOutput( DEBUG_LEVEL_VERBOSE, " stopping handler (%p)\n",*it);
		if(!(*it)->stop()){
			debugOutput( DEBUG_LEVEL_VERBOSE, " could not stop handler (%p)\n",*it);
			return false;
		}
	}
	return true;
}

void IsoHandlerManager::setVerboseLevel(int i) {
	setDebugLevel(i);

    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
          it != m_IsoHandlers.end();
          ++it )
    {
		(*it)->setVerboseLevel(i);
    }
}

void IsoHandlerManager::dumpInfo() {
	debugOutputShort( DEBUG_LEVEL_NORMAL, "Dumping IsoHandlerManager Stream handler information...\n");
	int i=0;

    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
          it != m_IsoHandlers.end();
          ++it )
    {
		debugOutputShort( DEBUG_LEVEL_NORMAL, " Stream %d (%p)\n",i++,*it);

		(*it)->dumpInfo();
    }

}

} // end of namespace FreebobStreaming

