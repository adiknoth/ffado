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
#ifndef __FREEBOB_ISOSTREAM__
#define __FREEBOB_ISOSTREAM__

#include <libraw1394/raw1394.h>
#include "../debugmodule/debugmodule.h"

namespace FreebobStreaming
{

/*!
\brief The Base Class for ISO streams
*/

class IsoStream
{

    public:

		enum EStreamType {
			EST_Receive,
			EST_Transmit
		};


        IsoStream(int channel) : m_Channel(channel), m_packetcount(0)
        {};
        virtual ~IsoStream()
        {};

		void setVerboseLevel(int l) { setDebugLevel( l ); };

		int getChannel() {return m_Channel;};
		int getPacketCount() {return m_packetcount;};
		void resetPacketCount() {m_packetcount=0;};
	
	protected:
		int m_Channel;

		int m_packetcount;

		DECLARE_DEBUG_MODULE;

};

class IsoRecvStream : public IsoStream
{
	public:
        IsoRecvStream(int);
        virtual ~IsoRecvStream();

		virtual enum EStreamType getType() { return EST_Receive;};

		int 
			putPacket(unsigned char *data, unsigned int length, 
		              unsigned char channel, unsigned char tag, unsigned char sy, 
			          unsigned int cycle, unsigned int dropped);

	private:

};

class IsoXmitStream : public IsoStream
{
	
	public:
        IsoXmitStream(int);
        virtual ~IsoXmitStream();

		virtual enum EStreamType getType() { return EST_Transmit;};
		int 
			getPacket(unsigned char *data, unsigned int *length,
		              unsigned char *tag, unsigned char *sy,
		              int cycle, unsigned int dropped);

	private:

};

}

#endif /* __FREEBOB_ISOSTREAM__ */



