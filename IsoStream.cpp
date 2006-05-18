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

#include "IsoStream.h"

namespace FreebobStreaming
{

IMPL_DEBUG_MODULE( IsoStream, IsoStream, DEBUG_LEVEL_NORMAL );

IsoRecvStream::IsoRecvStream(int channel)
		: IsoStream(channel)
{

}

IsoRecvStream::~IsoRecvStream()
{

}

int IsoRecvStream::putPacket(unsigned char *data, unsigned int length, 
		              unsigned char channel, unsigned char tag, unsigned char sy, 
			          unsigned int cycle, unsigned int dropped) {

	debugOutput( DEBUG_LEVEL_VERY_VERBOSE,
	             "received packet: length=%d, channel=%d, cycle=%d\n",
	             length, channel, cycle );

	m_packetcount++;


	return 0;
}

IsoXmitStream::IsoXmitStream(int channel)
		: IsoStream(channel)
{

}

IsoXmitStream::~IsoXmitStream()
{

}

int IsoXmitStream::getPacket(unsigned char *data, unsigned int *length,
		              unsigned char *tag, unsigned char *sy,
		              int cycle, unsigned int dropped) {
	debugOutput( DEBUG_LEVEL_VERY_VERBOSE,
	             "sending packet: length=%d, cycle=%d\n",
	             *length, cycle );

	memcpy(data,&cycle,sizeof(cycle));
	*length=sizeof(cycle);
	*tag = 1;
	*sy = 0;

	m_packetcount++;

	return 0;
}

}
