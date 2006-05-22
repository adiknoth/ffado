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

#include "Port.h"
 #include <stdlib.h>
#include <assert.h>


namespace FreebobStreaming {

IMPL_DEBUG_MODULE( Port, Port, DEBUG_LEVEL_NORMAL );
IMPL_DEBUG_MODULE( AudioPort, AudioPort, DEBUG_LEVEL_NORMAL );
IMPL_DEBUG_MODULE( MidiPort, MidiPort, DEBUG_LEVEL_NORMAL );
IMPL_DEBUG_MODULE( ControlPort, ControlPort, DEBUG_LEVEL_NORMAL );

Port::Port(std::string name, enum E_BufferType type, unsigned int buffsize, enum E_DataType datatype) 
  : m_Name(name),
    m_BufferType(type),
    m_enabled(true),
    m_buffersize(buffsize),
	m_datatype(datatype),
	m_buffer(0),
    m_buffer_attached(false)
{

	allocateInternalBuffer();
}

Port::Port(std::string name, enum E_BufferType type, unsigned int buffsize,
           enum E_DataType datatype, void* externalbuffer) 
  : m_Name(name),
    m_BufferType(type),
    m_enabled(true),
    m_buffersize(buffsize),
	m_datatype(datatype),
	m_buffer(externalbuffer),
    m_buffer_attached(true)
{
	
}

int Port::attachBuffer(void *buff) {

	if(m_buffer_attached) {
		debugFatal("already has a buffer attached\n");
		return -1;
	}

	freeInternalBuffer();

	m_buffer=buff;

	m_buffer_attached=true;

}

// detach the user buffer, allocates an internal buffer
int Port::detachBuffer() {
	if(!m_buffer_attached) {
		debugWarning("does not have a buffer attached\n");
		if(m_buffer) {
			return 0;	
		} // if no buffer present, there should be one allocated
	}

	m_buffer_attached=false;

	return allocateInternalBuffer();

}

int Port::allocateInternalBuffer() {
	int event_size=getEventSize();

	if(m_buffer_attached) {
		debugWarning("has an external buffer attached, not allocating\n");
		return -1;
	}

	if(m_buffer) {
		debugWarning("already has an internal buffer attached, re-allocating\n");
		freeInternalBuffer();
	}

	m_buffer=calloc(m_buffersize,event_size);
	if (!m_buffer) {
		debugFatal("could not allocate internal buffer\n");
		m_buffersize=0;
		return -1;
	}

	return 0;
}

void Port::freeInternalBuffer() {
	if(m_buffer_attached) {
		debugWarning("has an external buffer attached, not free-ing\n");
		return;
	}
	if(m_buffer) {
		free(m_buffer);
		m_buffer=0;
	}
}

unsigned int Port::getEventSize() {
	switch (m_datatype) {
		case E_Float:
			return sizeof(float);
		case E_Int24: // 24 bit 2's complement, packed in a 32bit integer (LSB's)
			return sizeof(uint32_t);
		case E_Byte:
			return sizeof(char);
		default:
			return 0;
	}
}

}
