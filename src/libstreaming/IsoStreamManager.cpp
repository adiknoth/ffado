/* $Id$ */

/*
 *   FreeBob Streaming API
 *   FreeBob = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2005,2006 Pieter Palmers <pieterpalmers@users.sourceforge.net>
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

#include "IsoStreamManager.h"
#include "IsoStream.h"

#include <assert.h>

namespace FreebobStreaming
{

IsoStreamManager::IsoStreamManager()
{

}

IsoStreamManager::~IsoStreamManager()
{

}

int IsoStreamManager::registerStream(IsoStream *stream)
{
	assert(stream);

	if (stream->getType()==IsoStream::EST_Receive) {
		m_IsoRecvStreams.push_back(stream);
		return 0;
	}
	
	if (stream->getType()==IsoStream::EST_Transmit) {
		m_IsoXmitStreams.push_back(stream);
		return 0;
	}

	return -1;
}

int IsoStreamManager::unregisterStream(IsoStream *stream)
{
	assert(stream);

    for ( IsoStreamVectorIterator it = m_IsoRecvStreams.begin();
          it != m_IsoRecvStreams.end();
          ++it )
    {
        if ( *it == stream ) { 
            m_IsoRecvStreams.erase(it);
			return 0;
        }
    }

    for ( IsoStreamVectorIterator it = m_IsoXmitStreams.begin();
          it != m_IsoXmitStreams.end();
          ++it )
    {
       if ( *it == stream ) { 
            m_IsoXmitStreams.erase(it);
			return 0;
        }
    }

	return -1; //not found

}


}
