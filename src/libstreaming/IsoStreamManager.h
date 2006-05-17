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
#ifndef __FREEBOB_ISOSTREAMMANAGER__
#define __FREEBOB_ISOSTREAMMANAGER__

#include <vector>
namespace FreebobStreaming
{

/*!
\brief ISO stream management class
*/
class IsoStream;
class IsoXmitStream;
class IsoRecvStream;

typedef std::vector<IsoXmitStream *> IsoXmitStreamVector;
typedef std::vector<IsoXmitStream *>::iterator IsoXmitStreamVectorIterator;
typedef std::vector<IsoRecvStream *> IsoRecvStreamVector;
typedef std::vector<IsoRecvStream *>::iterator IsoRecvStreamVectorIterator;

class IsoStreamManager
{
    public:

        IsoStreamManager();
        virtual ~IsoStreamManager();

		int RegisterStream(IsoStream *);
		int UnregisterStream(IsoStream *);

	private:
		IsoRecvStreamVector m_IsoRecvStreams;
		IsoXmitStreamVector m_IsoXmitStreams;

};

}

#endif /* __FREEBOB_ISOSTREAMMANAGER__ */



