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

	Port(std::string name, enum E_BufferType type, int buffsize);
	Port(std::string name, enum E_BufferType type, int buffsize, void *externalbuffer);

	virtual ~Port() 
	  {};

	std::string getName() {return m_Name;};
	void setName(std::string name) {m_Name=name;};

	void enable()  {m_enabled=true;};
	void disable() {m_enabled=false;};
	bool enabled() {return m_enabled;};

	enum E_BufferType getBufferType() {return m_BufferType;};

	// returns the size in bytes of the events in the port buffer
	virtual unsigned int getEventSize() = 0;

	// NOT THREAD SAFE!
	// attaches a user buffer to the port.
	// deallocates the internal buffer, if there was one
	// buffersize is in 'events'
	int attachBuffer(void *buff);

	// detach the user buffer, allocates an internal buffer
	int detachBuffer();

	int getBufferSize() {return m_buffersize;};
	void *getBuffer() {return m_buffer;};

protected:
	std::string m_Name;

	enum E_BufferType m_BufferType;

	bool m_enabled;
	int m_buffersize;
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
	enum E_DataType {
		E_Float,
		E_Int24
	};

	AudioPort(std::string name, int buffsize) 
	  : Port(name, E_PeriodBuffered, buffsize),
	    m_DataType(E_Int24)
	{};

	AudioPort(std::string name, enum E_BufferType type, int buffsize) 
	  : Port(name, type, buffsize),
	    m_DataType(E_Int24)
	{};
	AudioPort(std::string name, enum E_BufferType type, int buffsize, void *externalbuffer) 
	  : Port(name, type, buffsize, externalbuffer),
	    m_DataType(E_Int24)
	{};

	AudioPort(std::string name, enum E_DataType datatype,
	          enum E_BufferType type, int buffsize) 
	  : Port(name, type, buffsize),
	    m_DataType(datatype)
	{};
	AudioPort(std::string name, enum E_DataType datatype, 
	          enum E_BufferType type, int buffsize, void *externalbuffer) 
	  : Port(name, type, buffsize, externalbuffer),
	    m_DataType(datatype)
	{};

	virtual ~AudioPort();

	enum E_DataType getType() {return m_DataType;};
	unsigned int getEventSize();

protected:
	enum E_DataType m_DataType;

    DECLARE_DEBUG_MODULE;

};

class MidiPort : public Port {

public:
	enum E_DataType {
		E_Byte
	};

	MidiPort(std::string name, int buffsize) 
	  : Port(name, E_PacketBuffered, buffsize), 
	    m_DataType(E_Byte)
	{};
	virtual ~MidiPort();

	enum E_DataType getType() {return m_DataType;};
	unsigned int getEventSize();

protected:
	enum E_DataType m_DataType;

    DECLARE_DEBUG_MODULE;

};

class ControlPort : public Port {

public:
	enum E_DataType {
		E_Default
	};

	ControlPort(std::string name, int buffsize) 
	  : Port(name, E_PeriodBuffered, buffsize),
	    m_DataType(E_Default)
	{};
	virtual ~ControlPort();

	enum E_DataType getType() {return m_DataType;};
	unsigned int getEventSize();

protected:
	enum E_DataType m_DataType;

    DECLARE_DEBUG_MODULE;

};

}

#endif /* __FREEBOB_PORT__ */


