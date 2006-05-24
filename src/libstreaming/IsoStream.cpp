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
#include "PacketBuffer.h"
#include <assert.h>


namespace FreebobStreaming
{

IMPL_DEBUG_MODULE( IsoStream, IsoStream, DEBUG_LEVEL_NORMAL );
IMPL_DEBUG_MODULE( IsoStreamBuffered, IsoStreamBuffered, DEBUG_LEVEL_NORMAL );

int IsoStream::putPacket(unsigned char *data, unsigned int length, 
		              unsigned char channel, unsigned char tag, unsigned char sy, 
			          unsigned int cycle, unsigned int dropped) {

	debugOutput( DEBUG_LEVEL_VERY_VERBOSE,
	             "received packet: length=%d, channel=%d, cycle=%d\n",
	             length, channel, cycle );

	return 0;
}

int IsoStream::getPacket(unsigned char *data, unsigned int *length,
		              unsigned char *tag, unsigned char *sy,
		              int cycle, unsigned int dropped, unsigned int max_length) {
	debugOutput( DEBUG_LEVEL_VERY_VERBOSE,
	             "sending packet: length=%d, cycle=%d\n",
	             *length, cycle );

	memcpy(data,&cycle,sizeof(cycle));
	*length=sizeof(cycle);
	*tag = 1;
	*sy = 0;


	return 0;
}

int IsoStream::getNodeId() {
	if (m_handler) {
		return m_handler->getLocalNodeId();
	}
	return -1;
}


void IsoStream::dumpInfo()
{

	debugOutputShort( DEBUG_LEVEL_NORMAL, "  Stream type    : %s\n",
	     (this->getType()==EST_Receive ? "Receive" : "Transmit"));
	debugOutputShort( DEBUG_LEVEL_NORMAL, "  Port, Channel  : %d, %d\n",
	     m_port, m_channel);

};

int IsoStream::setChannel(int c) {
	m_channel=c;
}


void IsoStream::reset() {

}

void IsoStream::prepare() {
}

int IsoStream::init() {
	return 0;

}

/* buffered variant of the ISO stream */
IsoStreamBuffered::~IsoStreamBuffered() {
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
	if(buffer) delete buffer;

}

int IsoStreamBuffered::init() {
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
	buffer=new PacketBuffer(m_headersize, m_buffersize, m_max_packetsize);

	if(buffer==NULL) return -1;

	return buffer->initialize();

}

int IsoStreamBuffered::putPacket(unsigned char *data, unsigned int length, 
		              unsigned char channel, unsigned char tag, unsigned char sy, 
			          unsigned int cycle, unsigned int dropped) {
	int retval;

	debugOutput( DEBUG_LEVEL_VERY_VERBOSE,
	             "received packet: length=%d, channel=%d, cycle=%d\n",
	             length, channel, cycle );
	if(buffer) {
		if((retval=buffer->addPacket((quadlet_t *)data,length))) {
			debugWarning("Receive buffer overflow\n");
		}
	}

	return 0;
}

int IsoStreamBuffered::getPacket(unsigned char *data, unsigned int *length,
		              unsigned char *tag, unsigned char *sy,
		              int cycle, unsigned int dropped, unsigned int max_length) {
	debugOutput( DEBUG_LEVEL_VERY_VERBOSE,
	             "sending packet: length=%d, cycle=%d\n",
	             *length, cycle );

	int retval;
	*length=sizeof(cycle);
	*tag = 1;
	*sy = 0;

	if(buffer) {
		if((retval=buffer->getNextPacket((quadlet_t *)data,max_length))<0) {
			debugWarning("Transmit buffer underflow: %d\n",retval);
			return -1;
		}
		*length=retval;
	}

	return 0;
}

int IsoStreamBuffered::getBufferFillPackets() {
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

	if(buffer) return buffer->getBufferFillPackets();
	return -1;

}
int IsoStreamBuffered::getBufferFillPayload() {
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

	if(buffer) return buffer->getBufferFillPayload();
	return -1;

}

void IsoStreamBuffered::setVerboseLevel(int l) { 
	setDebugLevel( l ); 
	if(buffer) {
		buffer->setVerboseLevel(l);
	}
}

}
