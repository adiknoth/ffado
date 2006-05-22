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
#ifndef __FREEBOB_PORT__
#define __FREEBOB_PORT__

#include "../debugmodule/debugmodule.h"
#include <string>

namespace FreebobStreaming {

class Port {

public:
	friend class PortManager;

	enum E_BufferType {
		E_PacketBuffered,
		E_PeriodBuffered,
// 		E_SampleBuffered
	};

	enum E_DataType {
		E_Float,
		E_Int24,
		E_Byte,
		E_Default,
	};

	Port(std::string name, enum E_BufferType type, unsigned int buffsize, enum E_DataType datatype);
	Port(std::string name, enum E_BufferType type, unsigned int buffsize, 
	     enum E_DataType datatype, void *externalbuffer);

	virtual ~Port() 
	  {};

	std::string getName() {return m_Name;};
	void setName(std::string name) {m_Name=name;};

	void enable()  {m_enabled=true;};
	void disable() {m_enabled=false;};
	bool enabled() {return m_enabled;};

	enum E_BufferType getBufferType() {return m_BufferType;};

	// returns the size in bytes of the events in the port buffer
	unsigned int getEventSize();

	enum E_DataType getDataType() {return m_datatype;};

	// NOT THREAD SAFE!
	// attaches a user buffer to the port.
	// deallocates the internal buffer, if there was one
	// buffersize is in 'events'
	int attachBuffer(void *buff);

	// detach the user buffer, allocates an internal buffer
	int detachBuffer();

	unsigned int getBufferSize() {return m_buffersize;};
	void *getBuffer() {return m_buffer;};

	void setBufferOffset(unsigned int n);
	// FIXME: this is not really OO, but for performance???
	void *getBufferAddress() {return m_buffer;};

protected:
	std::string m_Name;

	enum E_BufferType m_BufferType;

	bool m_enabled;
	unsigned int m_buffersize;

	enum E_DataType m_datatype;

	void *m_buffer;
	bool m_buffer_attached;

	int allocateInternalBuffer();
	void freeInternalBuffer();

	// call this when the event size is changed
	void eventSizeChanged();

    DECLARE_DEBUG_MODULE;

};

class AudioPort : public Port {

public:

	AudioPort(std::string name, unsigned int buffsize) 
	  : Port(name, E_PeriodBuffered, buffsize, E_Int24)
	{};

	AudioPort(std::string name, enum E_BufferType type, unsigned int buffsize) 
	  : Port(name, type, buffsize, E_Int24)
	{};
	AudioPort(std::string name, enum E_BufferType type, unsigned int buffsize, void *externalbuffer) 
	  : Port(name, type, buffsize, E_Int24, externalbuffer)
	{};

	AudioPort(std::string name, enum E_DataType datatype,
	          enum E_BufferType type, unsigned int buffsize) 
	  : Port(name, type, buffsize, datatype)
	{};
	AudioPort(std::string name, enum E_DataType datatype, 
	          enum E_BufferType type, unsigned int buffsize, void *externalbuffer) 
	  : Port(name, type, buffsize, datatype, externalbuffer)
	{};

	virtual ~AudioPort() {};


protected:
	enum E_DataType m_DataType;

    DECLARE_DEBUG_MODULE;

};

class MidiPort : public Port {

public:

	MidiPort(std::string name, unsigned int buffsize) 
	  : Port(name, E_PacketBuffered, buffsize, E_Byte)
	{};
	virtual ~MidiPort() {};


protected:
	enum E_DataType m_DataType;

    DECLARE_DEBUG_MODULE;

};

class ControlPort : public Port {

public:

	ControlPort(std::string name, unsigned int buffsize) 
	  : Port(name, E_PeriodBuffered, buffsize, E_Int24)
	{};
	virtual ~ControlPort() {};


protected:

    DECLARE_DEBUG_MODULE;

};

}

#endif /* __FREEBOB_PORT__ */


