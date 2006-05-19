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

class PortManager;

class Port {

public:
	friend class PortManager;

	enum E_BufferType {
		E_PacketBuffered,
		E_PeriodBuffered,
		E_SampleBuffered
	};

	Port() 
	  : m_Name(std::string("Undefined")), 
	    m_manager(0),
	    m_BufferType(E_PacketBuffered),
		m_enabled(true)
	  {};

	Port(std::string name) 
	  : m_Name(name),
	    m_manager(0),
	    m_BufferType(E_PacketBuffered),
		m_enabled(true)
	  {};

	Port(enum E_BufferType type) 
	  : m_Name(std::string("Undefined")),
	    m_manager(0),
	    m_BufferType(type),
		m_enabled(true)
	  {};

	Port(enum E_BufferType type, std::string name) 
	  : m_Name(name),
	    m_manager(0),
	    m_BufferType(type),
		m_enabled(true)
	  {};

	Port(std::string name, enum E_BufferType type) 
	  : m_Name(name),
	    m_manager(0),
	    m_BufferType(type),
		m_enabled(true)
	  {};

	virtual ~Port();

	std::string getName() {return m_Name;};
	void setName(std::string name) {m_Name=name;};

	void enable()  {m_enabled=true;};
	void disable() {m_enabled=false;};
	bool enabled() {return m_enabled;};

	enum E_BufferType getBufferType() {return m_BufferType;};

protected:
	std::string m_Name;
	PortManager *m_manager;

	enum E_BufferType m_BufferType;

	bool m_enabled;


    DECLARE_DEBUG_MODULE;

};

class AudioPort : public Port {

public:
	enum E_DataType {
		E_Float,
		E_UInt24
	};

	AudioPort();
	virtual ~AudioPort();

	enum E_DataType getType() {return m_DataType;};

protected:
	enum E_DataType m_DataType;

    DECLARE_DEBUG_MODULE;

};

class MidiPort : public Port {

public:
	enum E_DataType {
		E_Byte
	};

	MidiPort();
	virtual ~MidiPort();

	enum E_DataType getType() {return m_DataType;};

protected:
	enum E_DataType m_DataType;

    DECLARE_DEBUG_MODULE;

};

class ControlPort : public Port {

public:
	enum E_DataType {
		E_Default
	};

	ControlPort();
	virtual ~ControlPort();

	enum E_DataType getType() {return m_DataType;};

protected:
	enum E_DataType m_DataType;

    DECLARE_DEBUG_MODULE;

};

}

#endif /* __FREEBOB_PORT__ */


