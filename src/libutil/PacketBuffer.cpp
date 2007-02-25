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

#include "PacketBuffer.h"
#include <assert.h>


namespace Streaming {

IMPL_DEBUG_MODULE( PacketBuffer, PacketBuffer, DEBUG_LEVEL_VERBOSE );


PacketBuffer::~PacketBuffer() {
	if(payload_buffer) freebob_ringbuffer_free(payload_buffer);
	if(header_buffer) freebob_ringbuffer_free(header_buffer);
	if(len_buffer) freebob_ringbuffer_free(len_buffer);
}

int PacketBuffer::initialize() {
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

	if(payload_buffer) freebob_ringbuffer_free(payload_buffer);
	if(header_buffer) freebob_ringbuffer_free(header_buffer);
	if(len_buffer) freebob_ringbuffer_free(len_buffer);

	payload_buffer=freebob_ringbuffer_create(m_buffersize * m_max_packetsize * sizeof(quadlet_t));
	if(!payload_buffer) {
		debugFatal("Could not allocate payload buffer\n");
		return -1;
	}

	header_buffer=freebob_ringbuffer_create(m_buffersize * (m_headersize) * sizeof(quadlet_t));
	if(!header_buffer) {
		debugFatal("Could not allocate header buffer\n");
		return -1;
	}

	len_buffer=freebob_ringbuffer_create(m_buffersize * sizeof(unsigned int));
	if(!len_buffer) {
		debugFatal("Could not allocate len buffer\n");
		return -1;
	}
	debugOutput( DEBUG_LEVEL_VERBOSE, "exit...\n");

	return 0;
}

void PacketBuffer::flush() {
	if(header_buffer) {
		freebob_ringbuffer_reset(header_buffer);
	}
	if(payload_buffer) {
		freebob_ringbuffer_reset(payload_buffer);
	}
	if(len_buffer) {
		freebob_ringbuffer_reset(len_buffer);
	}
}

int PacketBuffer::addPacket(quadlet_t *packet, int packet_len) {

	unsigned int payload_bytes=sizeof(quadlet_t)*(packet_len-m_headersize);
	unsigned int header_bytes=sizeof(quadlet_t)*(m_headersize);

	debugOutput( DEBUG_LEVEL_VERY_VERBOSE,
	             "add packet: length=%d\n",
	             packet_len);

	if ((freebob_ringbuffer_write_space(payload_buffer) > payload_bytes) 
	    && (freebob_ringbuffer_write_space(header_buffer) > header_bytes)) {

		freebob_ringbuffer_write(payload_buffer,(char *)(packet)+header_bytes, payload_bytes);
		freebob_ringbuffer_write(len_buffer,(char *)(&payload_bytes),sizeof(unsigned int));
		freebob_ringbuffer_write(header_buffer,(char *)(packet), header_bytes);

	} else return -1;
	return 0;

}

int PacketBuffer::getNextPacket(quadlet_t *packet, int max_packet_len) {

	unsigned int bytes=sizeof(quadlet_t)*(m_headersize);
	quadlet_t *ptr=packet+m_headersize;

	debugOutput( DEBUG_LEVEL_VERY_VERBOSE,
	             "getNextPacket\n");

	if(max_packet_len<m_headersize) return -2;

	if (freebob_ringbuffer_read(header_buffer,(char *)packet,bytes) < bytes) {
		return -1;
	}

	if (freebob_ringbuffer_read(len_buffer,(char *)&bytes, sizeof(unsigned int)) < sizeof(unsigned int)) {
		return -3;
	}

	if(bytes>(max_packet_len-m_headersize)*sizeof(quadlet_t)) return -2;

	if(freebob_ringbuffer_read(payload_buffer,(char *)(ptr),bytes) < bytes) {
		return -3;
	}

	return bytes/sizeof(quadlet_t)+m_headersize;
}

int PacketBuffer::getBufferFillPackets() {

	return freebob_ringbuffer_read_space(len_buffer)/sizeof(unsigned int);
}

// in quadlets
int PacketBuffer::getBufferFillPayload() {

	return freebob_ringbuffer_read_space(payload_buffer)/sizeof(quadlet_t);
}

}
