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


bool IsoHandlerManager::Execute()
{
	int err;
	int i=0;
	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "enter...\n");

	err = poll (m_poll_fds, m_poll_nfds, m_poll_timeout);
	
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
			s->iterate();
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



int IsoHandlerManager::registerHandler(IsoHandler *handler)
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
	assert(handler);
	m_IsoHandlers.push_back(handler);
	handler->setVerboseLevel(getDebugLevel());

	// rebuild the fd map for poll()'ing.
	return rebuildFdMap();	

}

int IsoHandlerManager::unregisterHandler(IsoHandler *handler)
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

	return -1; //not found

}

int IsoHandlerManager::rebuildFdMap() {
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
		return -1;
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

	return 0;
}


// FIXME: currently there is a one-to-one mapping
//        between streams and handlers, this is not ok for 
//        multichannel receive
int IsoHandlerManager::registerStream(IsoStream *stream)
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
	assert(stream);

	// make sure the stream isn't already attached to a handler
    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
          it != m_IsoHandlers.end();
          ++it )
    {
		if((*it)->isStreamRegistered(stream)) {
			debugOutput( DEBUG_LEVEL_VERBOSE, "stream already registered!\n");
			(*it)->unregisterStream(stream);
		}
    }

	// clean up all handlers that aren't used
	pruneHandlers();

	// allocate a handler for this stream
	if (stream->getType()==IsoStream::EST_Receive) {
		IsoRecvHandler *h = new IsoRecvHandler(stream->getPort());
		debugOutput( DEBUG_LEVEL_VERBOSE, " registering IsoRecvHandler\n");

		if(!h) {
			debugFatal("Could not create IsoRecvHandler\n");
			return -1;
		}

		h->setVerboseLevel(getDebugLevel());

		// init the handler
		if(!h->initialize()) {
			debugFatal("Could not initialize receive handler\n");
			return -1;
		}

		// register the stream with the handler
		if(h->registerStream(stream)) {
			debugFatal("Could not register receive stream with handler\n");
			return -1;
		}

		// register the handler with the manager
		if(this->registerHandler(h)) {
			debugFatal("Could not register receive handler with manager\n");
			return -1;
		}
		debugOutput( DEBUG_LEVEL_VERBOSE, " registered stream (%p) with handler (%p)\n",stream,h);
	}
	
	if (stream->getType()==IsoStream::EST_Transmit) {
	
		// setup the optimal parameters for the raw1394 ISO buffering
		unsigned int packets_per_period=stream->getPacketsPerPeriod();
		// hardware interrupts occur when one DMA block is full, and the size of one DMA
		// block = PAGE_SIZE. Setting the max_packet_size makes sure that the HW irq is 
		// occurs at a period boundary (optimal CPU use)
		// NOTE: try and use 2 interrupts per period for better latency.
		unsigned int max_packet_size=getpagesize() / packets_per_period * 2;
		int irq_interval=packets_per_period / 2;

		if (max_packet_size < stream->getMaxPacketSize()) {
			max_packet_size=stream->getMaxPacketSize();
		}

		/* the transmit buffer size should be as low as possible for latency. 
		*/
		int buffers=packets_per_period;
		if (buffers<10) buffers=10;	
		
		// create the actual handler
		IsoXmitHandler *h = new IsoXmitHandler(stream->getPort(), buffers,
		                                       max_packet_size, irq_interval);

		debugOutput( DEBUG_LEVEL_VERBOSE, " registering IsoXmitHandler\n");

		if(!h) {
			debugFatal("Could not create IsoXmitHandler\n");
			return -1;
		}

		h->setVerboseLevel(getDebugLevel());

		// init the handler
		if(!h->initialize()) {
			debugFatal("Could not initialize transmit handler\n");
			return -1;
		}

		// register the stream with the handler
		if(h->registerStream(stream)) {
			debugFatal("Could not register transmit stream with handler\n");
			return -1;
		}

		// register the handler with the manager
		if(this->registerHandler(h)) {
			debugFatal("Could not register transmit handler with manager\n");
			return -1;
		}
		debugOutput( DEBUG_LEVEL_VERBOSE, " registered stream (%p) with handler (%p)\n",stream,h);

	}

	m_IsoStreams.push_back(stream);
	debugOutput( DEBUG_LEVEL_VERBOSE, " %d streams, %d handlers registered\n",
	                                  m_IsoStreams.size(), m_IsoHandlers.size());

	return 0;
}

int IsoHandlerManager::unregisterStream(IsoStream *stream)
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
	assert(stream);

	// make sure the stream isn't attached to a handler anymore
    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
          it != m_IsoHandlers.end();
          ++it )
    {
		if((*it)->isStreamRegistered(stream)) {
			(*it)->unregisterStream(stream);
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
			return 0;
        }
    }

	return -1; //not found

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

int IsoHandlerManager::startHandlers() {
	return startHandlers(-1);
}

int IsoHandlerManager::startHandlers(int cycle) {
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
          it != m_IsoHandlers.end();
          ++it )
    {
		debugOutput( DEBUG_LEVEL_VERBOSE, " starting handler (%p)\n",*it);
		(*it)->start(cycle);
    }
	return 0;
}

void IsoHandlerManager::stopHandlers() {
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

    for ( IsoHandlerVectorIterator it = m_IsoHandlers.begin();
          it != m_IsoHandlers.end();
          ++it )
    {
		debugOutput( DEBUG_LEVEL_VERBOSE, " stopping handler (%p)\n",*it);
		(*it)->stop();
    }
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

