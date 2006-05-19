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

class PacketBuffer;

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

        IsoStream(enum EStreamType type, int channel) : m_type(type), m_Channel(channel)
        {};
        virtual ~IsoStream()
        {};

		void setVerboseLevel(int l) { setDebugLevel( l ); };

		int getChannel() {return m_Channel;};

		enum EStreamType getType() { return m_type;};

		int initialize() {return 0;};

		int 
			putPacket(unsigned char *data, unsigned int length, 
		              unsigned char channel, unsigned char tag, unsigned char sy, 
			          unsigned int cycle, unsigned int dropped);
		int 
			getPacket(unsigned char *data, unsigned int *length,
		              unsigned char *tag, unsigned char *sy,
		              int cycle, unsigned int dropped, unsigned int max_length);
	
	protected:
		enum EStreamType m_type;
		int m_Channel;

		DECLARE_DEBUG_MODULE;

};

class IsoStreamBuffered : public IsoStream
{

    public:

        IsoStreamBuffered(int headersize,int buffersize, int max_packetsize, enum EStreamType type, int channel) 
		   : IsoStream(type,channel), m_headersize(headersize), m_buffersize(buffersize), m_max_packetsize(max_packetsize), buffer(0)
        {};
        virtual ~IsoStreamBuffered();

		void setVerboseLevel(int l);

		int initialize();

		int 
			putPacket(unsigned char *data, unsigned int length, 
		              unsigned char channel, unsigned char tag, unsigned char sy, 
			          unsigned int cycle, unsigned int dropped);
		int 
			getPacket(unsigned char *data, unsigned int *length,
		              unsigned char *tag, unsigned char *sy,
		              int cycle, unsigned int dropped, unsigned int max_length);
	int getBufferFillPackets();
	int getBufferFillPayload();

	protected:
		int m_headersize;
		int m_buffersize;
		int m_max_packetsize;

		PacketBuffer *buffer;

		DECLARE_DEBUG_MODULE;

};

}

#endif /* __FREEBOB_ISOSTREAM__ */



