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

namespace FreebobStreaming
{

IMPL_DEBUG_MODULE( IsoHandlerManager, IsoHandlerManager, DEBUG_LEVEL_NORMAL );

IsoHandlerManager::IsoHandlerManager() :
   m_poll_timeout(-1)
{

}


IsoHandlerManager::~IsoHandlerManager()
{

}

bool IsoHandlerManager::Init()
{
	// prepare the fd map

	return true;
}


bool IsoHandlerManager::Execute()
{
	int err;
	int i=0;

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

		}
	}
	return true;

}

int IsoHandlerManager::registerHandler(IsoHandler *handler)
{
	assert(handler);
	IsoRecvHandler *hrx;
	IsoXmitHandler *htx;

	hrx=dynamic_cast<IsoRecvHandler *>(handler);

	if (hrx) {
		m_IsoRecvHandlers.push_back(hrx);
		return 0;
	}
	
	htx=dynamic_cast<IsoXmitHandler *>(handler);

	if (htx) {
		m_IsoXmitHandlers.push_back(htx);
		return 0;
	}

	return -1;

}

int IsoHandlerManager::unregisterHandler(IsoHandler *handler)
{
	assert(handler);

    for ( IsoRecvHandlerVectorIterator it = m_IsoRecvHandlers.begin();
          it != m_IsoRecvHandlers.end();
          ++it )
    {
		// FIXME: how do I compare these two pointers?
        IsoHandler* s = dynamic_cast<IsoHandler *>(*it);
        if ( s == handler ) { 
            m_IsoRecvHandlers.erase(it);
			return 0;
        }
    }

    for ( IsoXmitHandlerVectorIterator it = m_IsoXmitHandlers.begin();
          it != m_IsoXmitHandlers.end();
          ++it )
    {
		// FIXME: how do I compare these two pointers?
        IsoHandler* s = dynamic_cast<IsoHandler *>(*it);
        if ( s == handler ) { 
            m_IsoXmitHandlers.erase(it);
			return 0;
        }
    }

	return -1; //not found

}


}
