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

/*!
\brief The Base Class for Ports

 Ports are the entities that provide the interface between the ISO streaming
 layer and the datatype-specific layer. You can define plug types by subclassing
 the base port type.
*/
class Port {

public:
	friend class PortManager;
	
	/*!
	\brief Specifies the buffering type for ports
	*/
	enum E_BufferType {
		E_PacketBuffered, ///< the port is to be processed for every packet
		E_PeriodBuffered, ///< the port is to be processed after a period of frames
// 		E_SampleBuffered ///< the port is to be processed after each frame (sample)
	};

	/*!
	\brief The datatype of the port buffer
	*/
	enum E_DataType {
		E_Float,
		E_Int24,
		E_Byte,
		E_Default,
	};

	/*!
	\brief The port type
	*/
	enum E_PortType {
		E_Audio,
		E_Midi,
		E_Control,
	};

	/*!
	\brief The port direction
	*/
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


	enum E_DataType getDataType() {return m_datatype;};
	bool setDataType(enum E_DataType datatype);

	enum E_PortType getPortType() {return m_porttype;};
	enum E_Direction getDirection() {return m_direction;};

	// NOT THREAD SAFE!
	// attaches a user buffer to the port.
	// deallocates the internal buffer, if there was one
	// buffersize is in 'events'

// 	int attachBuffer(void *buff);

	// detach the user buffer, allocates an internal buffer
// 	int detachBuffer();

	/**
	 * \brief returns the size of the events in the port buffer, in bytes
	 *
	 */
	unsigned int getEventSize();
	
	/**
	 * \brief returns the size of the port buffer
	 *
	 * counted in number of E_DataType units (events), not in bytes
	 *
	 */
	unsigned int getBufferSize() {return m_buffersize;};
	
	/**
	 * \brief sets the size of the port buffer
	 *
	 * counted in number of E_DataType units, not in bytes
	 *
	 * if there is an external buffer assigned, it should
	 * be large enough
	 * if there is an internal buffer, it will be resized
	 *
	 */
	bool setBufferSize(unsigned int);

	// FIXME: this is not really OO, but for performance???
	void *getBufferAddress() {return m_buffer;};
	void setBufferAddress(void *buff) {m_buffer=buff;};

 	virtual void setVerboseLevel(int l);

protected:
	std::string m_Name; ///< Port name, [at construction]

	enum E_BufferType m_BufferType; ///< Buffer type, [at construction]

	bool m_enabled; ///< is the port enabled?, [anytime]
	
	/**
	 * \brief size of the buffer
	 *
	 * counted in number of E_DataType units, not in bytes
	 *
	 * []
	 */
	unsigned int m_buffersize; 

	enum E_DataType m_datatype; ///< size of the buffer, number of E_DataType units []
	enum E_PortType m_porttype;
	enum E_Direction m_direction;

	void *m_buffer;
	bool m_buffer_attached;

	bool allocateInternalBuffer();
	void freeInternalBuffer();

	// call this when the event size is changed
	virtual bool eventSizeChanged(); ///< this is called whenever the event size changes.

    DECLARE_DEBUG_MODULE;

};

/*!
\brief The Base Class for an Audio Port


*/
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

/*!
\brief The Base Class for a Midi Port


*/
class MidiPort : public Port {

public:

	MidiPort(std::string name, unsigned int buffsize, enum E_Direction direction) 
	  : Port(name, E_PacketBuffered, buffsize, E_Byte, E_Midi, direction)
	{};
	virtual ~MidiPort() {};


protected:
	enum E_DataType m_DataType;


};

/*!
\brief The Base Class for a control port


*/
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


