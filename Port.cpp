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

Port::Port(std::string name, enum E_PortType porttype, enum E_Direction direction) 
  	: m_Name(name),
	m_SignalType(E_PeriodSignalled),
	m_BufferType(E_PointerBuffer),
	m_enabled(true),
	m_buffersize(0),
	m_eventsize(0),
	m_DataType(E_Int24),
	m_PortType(porttype),
	m_Direction(direction),
	m_buffer(0),
	m_ringbuffer(0),
	m_use_external_buffer(false)
{

}

/**
 * The idea is that you set all port parameters, and then initialize the port.
 * This allocates all resources and makes the port usable. However, you can't
 * change the port properties anymore after this.
 *
 * @return true if successfull. false if not (all resources are freed).
 */
bool Port::init() {
	if (m_initialized) {
		debugFatal("Port already initialized... (%s)\n",m_Name.c_str());
		return false;
	}	
	
	if (m_buffersize==0) {
		debugFatal("Cannot initialize a port with buffersize=0\n");
		return false;	
	}
	
	switch (m_BufferType) {
		case E_PointerBuffer:
			if (m_use_external_buffer) {
				// don't do anything
			} else if (!allocateInternalBuffer()) {
				debugFatal("Could not allocate internal buffer!\n");
				return false;
			}
			break;
			
		case E_RingBuffer:
			if (m_use_external_buffer) {
				debugFatal("Cannot use an external ringbuffer! \n");
				return false;
			} else if (!allocateInternalRingBuffer()) {
				debugFatal("Could not allocate internal ringbuffer!\n");
				return false;
			}
			break;
		default:
			debugFatal("Unsupported buffer type! (%d)\n",(int)m_BufferType);
			return false;
			break;
	}

	m_initialized=true;
	
	m_eventsize=getEventSize(); // this won't change, so cache it
	
	return m_initialized;
}

void Port::setVerboseLevel(int l) {
	setDebugLevel(l);
}

bool Port::setName(std::string name) {
	debugOutput( DEBUG_LEVEL_VERBOSE, "Setting name to %s for port %s\n",name.c_str(),m_Name.c_str());
	
	if (m_initialized) {
		debugFatal("Port already initialized... (%s)\n",m_Name.c_str());
		return false;
	}
	
	m_Name=name;
	
	return true;
}

bool Port::setBufferSize(unsigned int newsize) {
	debugOutput( DEBUG_LEVEL_VERBOSE, "Setting buffersize to %d for port %s\n",newsize,m_Name.c_str());
	if (m_initialized) {
		debugFatal("Port already initialized... (%s)\n",m_Name.c_str());
		return false;
	}

	m_buffersize=newsize;
	return true;

}

unsigned int Port::getEventSize() {
	switch (m_DataType) {
		case E_Float:
			return sizeof(float);
		case E_Int24: // 24 bit 2's complement, packed in a 32bit integer (LSB's)
			return sizeof(uint32_t);
		case E_MidiEvent:
			return sizeof(uint32_t);
		default:
			return 0;
	}
}

bool Port::setDataType(enum E_DataType d) {
	debugOutput( DEBUG_LEVEL_VERBOSE, "Setting datatype to %d for port %s\n",(int) d,m_Name.c_str());
	if (m_initialized) {
		debugFatal("Port already initialized... (%s)\n",m_Name.c_str());
		return false;
	}
	
	// do some sanity checks
	bool type_is_ok=false;
	switch (m_PortType) {
		case E_Audio:
			if(d == E_Int24) type_is_ok=true;
			if(d == E_Float) type_is_ok=true;
			break;
		case E_Midi:
			if(d == E_MidiEvent) type_is_ok=true;
			break;
		case E_Control:
			if(d == E_Default) type_is_ok=true;
			break;
		default:
			break;
	}
	
	if(!type_is_ok) {
		debugFatal("Datatype not supported by this type of port!\n");
		return false;
	}
	
	m_DataType=d;
	return true;
}

bool Port::setSignalType(enum E_SignalType s) {
	debugOutput( DEBUG_LEVEL_VERBOSE, "Setting signaltype to %d for port %s\n",(int)s,m_Name.c_str());
	if (m_initialized) {
		debugFatal("Port already initialized... (%s)\n",m_Name.c_str());
		return false;
	}
	
	// do some sanity checks
	bool type_is_ok=false;
	switch (m_PortType) {
		case E_Audio:
			if(s == E_PeriodSignalled) type_is_ok=true;
			break;
		case E_Midi:
			if(s == E_PacketSignalled) type_is_ok=true;
			break;
		case E_Control:
			if(s == E_PeriodSignalled) type_is_ok=true;
			break;
		default:
			break;
	}
	
	if(!type_is_ok) {
		debugFatal("Signalling type not supported by this type of port!\n");
		return false;
	}
	
	m_SignalType=s;
	return true;

}

bool Port::setBufferType(enum E_BufferType b) {
	debugOutput( DEBUG_LEVEL_VERBOSE, "Setting buffer type to %d for port %s\n",(int)b,m_Name.c_str());
	if (m_initialized) {
		debugFatal("Port already initialized... (%s)\n",m_Name.c_str());
		return false;
	}
	
	// do some sanity checks
	bool type_is_ok=false;
	switch (m_PortType) {
		case E_Audio:
			if(b == E_PointerBuffer) type_is_ok=true;
			break;
		case E_Midi:
			if(b == E_RingBuffer) type_is_ok=true;
			break;
		case E_Control:
			break;
		default:
			break;
	}
	
	if(!type_is_ok) {
		debugFatal("Buffer type not supported by this type of port!\n");
		return false;
	}
	
	m_BufferType=b;
	return true;

}

bool Port::useExternalBuffer(bool b) {
	
	debugOutput( DEBUG_LEVEL_VERBOSE, "Setting external buffer use to %d for port %s\n",(int)b,m_Name.c_str());
	
	if (m_initialized) {
		debugFatal("Port already initialized... (%s)\n",m_Name.c_str());
		return false;
	}

	m_use_external_buffer=b;
	return true;
}

// buffer handling api's for pointer buffers
/**
 * Get the buffer address (being the external or the internal one).
 *
 * @param buff 
 */
void *Port::getBufferAddress() {
	assert(m_BufferType==E_PointerBuffer);
	return m_buffer;
};

/**
 * Set the external buffer address.
 * only call this when specifying an external buffer before init()
 *
 * @param buff 
 */
void Port::setExternalBufferAddress(void *buff) {
	assert(m_BufferType==E_PointerBuffer);
	assert(m_use_external_buffer); // don't call this with an internal buffer!
	m_buffer=buff;
};

// buffer handling api's for ringbuffers
bool Port::writeEvent(void *event) {
	assert(m_BufferType==E_RingBuffer);
	assert(m_ringbuffer);
	
	char *byte=(char *)event;
	debugOutput( DEBUG_LEVEL_VERBOSE, "Writing event %02X to port %s\n",(*byte)&0xFF,m_Name.c_str());
	
	return (freebob_ringbuffer_write(m_ringbuffer, byte, m_eventsize)==m_eventsize);
}

bool Port::readEvent(void *event) {
	assert(m_ringbuffer);
	
	char *byte=(char *)event;
	unsigned int read=freebob_ringbuffer_read(m_ringbuffer, byte, m_eventsize);
	
	debugOutput( DEBUG_LEVEL_VERBOSE, "Reading event %X from port %s\n",(*byte),m_Name.c_str());
	return (read==m_eventsize);
}

int Port::writeEvents(void *event, unsigned int nevents) {
	assert(m_BufferType==E_RingBuffer);
	assert(m_ringbuffer);
	
	char *byte=(char *)event;
	unsigned int bytes2write=m_eventsize*nevents;
	
	return (freebob_ringbuffer_write(m_ringbuffer, byte,bytes2write)/m_eventsize);

}

int Port::readEvents(void *event, unsigned int nevents) {
	assert(m_ringbuffer);
	char *byte=(char *)event;
	
	unsigned int bytes2read=m_eventsize*nevents;
	
	freebob_ringbuffer_read(m_ringbuffer, byte, bytes2read);
	debugOutput( DEBUG_LEVEL_VERBOSE, "Reading events (%X) from port %s\n",(*byte),m_Name.c_str());
	
	return freebob_ringbuffer_read(m_ringbuffer, byte, bytes2read)/m_eventsize;
}


/* Private functions */

bool Port::allocateInternalBuffer() {
	int event_size=getEventSize();
	
	debugOutput(DEBUG_LEVEL_VERBOSE,
	            "Allocating internal buffer of %d events with size %d (%s)\n",
	            m_buffersize, event_size, m_Name.c_str());

	if(m_buffer) {
		debugWarning("already has an internal buffer attached, re-allocating\n");
		freeInternalBuffer();
	}

	m_buffer=calloc(m_buffersize,event_size);
	if (!m_buffer) {
		debugFatal("could not allocate internal buffer\n");
		m_buffersize=0;
		return false;
	}

	return true;
}

void Port::freeInternalBuffer() {
	debugOutput(DEBUG_LEVEL_VERBOSE,
	            "Freeing internal buffer (%s)\n",m_Name.c_str());

	if(m_buffer) {
		free(m_buffer);
		m_buffer=0;
	}
}

bool Port::allocateInternalRingBuffer() {
	int event_size=getEventSize();
	
	debugOutput(DEBUG_LEVEL_VERBOSE,
	            "Allocating internal buffer of %d events with size %d (%s)\n",
	            m_buffersize, event_size, m_Name.c_str());

	if(m_ringbuffer) {
		debugWarning("already has an internal ringbuffer attached, re-allocating\n");
		freeInternalRingBuffer();
	}

	m_ringbuffer=freebob_ringbuffer_create(m_buffersize * event_size);
	if (!m_ringbuffer) {
		debugFatal("could not allocate internal ringbuffer\n");
		m_buffersize=0;
		return false;
	}

	return true;
}

void Port::freeInternalRingBuffer() {
	debugOutput(DEBUG_LEVEL_VERBOSE,
	            "Freeing internal ringbuffer (%s)\n",m_Name.c_str());

	if(m_ringbuffer) {
		freebob_ringbuffer_free(m_ringbuffer);
		m_ringbuffer=0;
	}
}

}
