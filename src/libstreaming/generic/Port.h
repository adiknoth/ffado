/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __FFADO_PORT__
#define __FFADO_PORT__

#include "libutil/ringbuffer.h"

#include "debugmodule/debugmodule.h"

#include <string>
#include <stdint.h>

namespace Streaming {
class PortManager;

/*!
\brief The Base Class for Ports

 Ports are the entities that provide the interface between the ISO streaming
 layer and the datatype-specific layer. You can define port types by subclassing
 the base port class.

 After creating a port, you have to set its parameters and then call the init() function.
 This is because a port needs information from two sources to operate:
 1) the stream composition information from the AvDevice
 2) the streaming API setup (buffer type, data type, ...)

 \note There are not much virtual functions here because of the high frequency of
       calling. We try to do everything with a base class getter, and a child class
       setter. If this isn't possible, we do a static_cast. This however can only be
       done inside the streamprocessor that handles the specific sub-class types of
       the ports. i.e. by design you should make sure that the static_cast will be
       OK.

*/
class Port {

public:
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

    Port(PortManager&, std::string name, enum E_PortType, enum E_Direction);

    virtual ~Port();


    /// Enable the port. (this can be called anytime)
    void enable();
    /// Disable the port. (this can be called anytime)
    void disable();
    /// is the port disabled? (this can be called anytime)
    bool isDisabled() {return m_disabled;};

    /*!
    \brief Initialize the port
    */
    bool init();

    bool prepare() {return true;};
    bool reset();

    std::string getName() {return m_Name;};
    bool setName(std::string name);

    /**
     * \brief returns the size of the events in the port buffer, in bytes
     *
     */
    unsigned int getEventSize();

    enum E_PortType getPortType() {return m_PortType;}; ///< returns the port type (is fixed)
    std::string getPortTypeName();
    enum E_Direction getDirection() {return m_Direction;}; ///< returns the direction (is fixed)

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
     * \note use before calling init()
     */
    virtual bool setBufferSize(unsigned int);

    void setBufferAddress(void *buff);
    void *getBufferAddress();

    PortManager& getManager() { return m_manager; };

    virtual void setVerboseLevel(int l);
    virtual void show();

protected:
    std::string m_Name; ///< Port name, [at construction]
    bool m_disabled; ///< is the port disabled?, [anytime]

    unsigned int m_buffersize;

    enum E_PortType m_PortType;
    enum E_Direction m_Direction;

    void *m_buffer;

    PortManager& m_manager;

    DECLARE_DEBUG_MODULE;

// the state machine
protected:
    enum EStates {
        E_Created,
        E_Initialized,
        E_Prepared,
        E_Running,
        E_Error
    };

    enum EStates m_State;
};

/*!
\brief The Base Class for an Audio Port


*/
class AudioPort : public Port {

public:

    AudioPort(PortManager& m, std::string name, enum E_Direction direction)
      : Port(m, name, E_Audio, direction)
    {};

    virtual ~AudioPort() {};
};

/*!
\brief The Base Class for a Midi Port


*/
class MidiPort : public Port {

public:

    MidiPort(PortManager& m, std::string name, enum E_Direction direction)
      : Port(m, name, E_Midi, direction)
    {};
    virtual ~MidiPort() {};
};

/*!
\brief The Base Class for a control port


*/
class ControlPort : public Port {

public:

    ControlPort(PortManager& m, std::string name, enum E_Direction direction)
      : Port(m, name, E_Control, direction)
    {};
    virtual ~ControlPort() {};
};

}

#endif /* __FFADO_PORT__ */


