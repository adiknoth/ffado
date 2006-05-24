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

	enum E_PortType {
		E_Audio,
		E_Midi,
		E_Control,
	};

	enum E_Direction {
		E_Playback,
		E_Capture,
	};

	Port(std::string name, enum E_BufferType type, unsigned int buffsize, 
	     enum E_DataType datatype, enum E_PortType porttype, enum E_Direction direction);
	Port(std::string name, enum E_BufferType type, unsigned int buffsize, 
	     enum E_DataType datatype, void *externalbuffer, enum E_PortType porttype, enum E_Direction direction);

	virtual ~Port() 
	  {};

	std::string getName() {return m_Name;};
	void setName(std::string name) {m_Name=name;};

	void enable()  {m_enabled=true;};
	void disable() {m_enabled=false;};
	bool isEnabled() {return m_enabled;};

	enum E_BufferType getBufferType() {return m_BufferType;};

	// returns the size in bytes of the events in the port buffer
	unsigned int getEventSize();

	enum E_DataType getDataType() {return m_datatype;};
	void setDataType(enum E_DataType datatype) {m_datatype=datatype;};

	enum E_PortType getPortType() {return m_porttype;};
	enum E_Direction getDirection() {return m_direction;};

	// NOT THREAD SAFE!
	// attaches a user buffer to the port.
	// deallocates the internal buffer, if there was one
	// buffersize is in 'events'

// 	int attachBuffer(void *buff);

	// detach the user buffer, allocates an internal buffer
// 	int detachBuffer();

	unsigned int getBufferSize() {return m_buffersize;};
	bool setBufferSize(unsigned int);

	void setBufferOffset(unsigned int n);

	// FIXME: this is not really OO, but for performance???
	void *getBufferAddress() {return m_buffer;};
	void setBufferAddress(void *buff) {m_buffer=buff;};

 	virtual void setVerboseLevel(int l) { setDebugLevel(l);  };

protected:
	std::string m_Name;

	enum E_BufferType m_BufferType;

	bool m_enabled;
	unsigned int m_buffersize;

	enum E_DataType m_datatype;
	enum E_PortType m_porttype;
	enum E_Direction m_direction;

	void *m_buffer;
	bool m_buffer_attached;

	bool allocateInternalBuffer();
	void freeInternalBuffer();

	// call this when the event size is changed
	void eventSizeChanged();

    DECLARE_DEBUG_MODULE;

};

class AudioPort : public Port {

public:

	AudioPort(std::string name, unsigned int buffsize, enum E_Direction direction) 
	  : Port(name, E_PeriodBuffered, buffsize, E_Int24, E_Audio, direction)
	{};

	AudioPort(std::string name, enum E_BufferType type, unsigned int buffsize, 
	          enum E_Direction direction) 
	  : Port(name, type, buffsize, E_Int24, E_Audio, direction)
	{};
	AudioPort(std::string name, enum E_BufferType type, unsigned int buffsize, 
	          void *externalbuffer, enum E_Direction direction) 
	  : Port(name, type, buffsize, E_Int24, externalbuffer, E_Audio, direction)
	{};

	AudioPort(std::string name, enum E_DataType datatype,
	          enum E_BufferType type, unsigned int buffsize, enum E_Direction direction) 
	  : Port(name, type, buffsize, datatype, E_Audio, direction)
	{};
	AudioPort(std::string name, enum E_DataType datatype, 
	          enum E_BufferType type, unsigned int buffsize, void *externalbuffer, 
	          enum E_Direction direction) 
	  : Port(name, type, buffsize, datatype, externalbuffer, E_Audio, direction)
	{};

	virtual ~AudioPort() {};

protected:
	enum E_DataType m_DataType;

  
};

class MidiPort : public Port {

public:

	MidiPort(std::string name, unsigned int buffsize, enum E_Direction direction) 
	  : Port(name, E_PacketBuffered, buffsize, E_Byte, E_Midi, direction)
	{};
	virtual ~MidiPort() {};


protected:
	enum E_DataType m_DataType;


};

class ControlPort : public Port {

public:

	ControlPort(std::string name, unsigned int buffsize, enum E_Direction direction) 
	  : Port(name, E_PeriodBuffered, buffsize, E_Int24, E_Control, direction)
	{};
	virtual ~ControlPort() {};


protected:


};

}

#endif /* __FREEBOB_PORT__ */


