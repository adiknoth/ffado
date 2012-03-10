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

#include "Port.h"
#include "PortManager.h"

#include <stdlib.h>
#include <assert.h>

namespace Streaming {

IMPL_DEBUG_MODULE( Port, Port, DEBUG_LEVEL_NORMAL );

Port::Port(PortManager& m, std::string name, 
           enum E_PortType porttype, enum E_Direction direction)
    : m_Name( name )
    , m_disabled( true )
    , m_buffersize( 0 )
    , m_PortType( porttype )
    , m_Direction( direction )
    , m_buffer( NULL )
    , m_manager( m )
    , m_State( E_Created )
{
    m_manager.registerPort(this);
}

Port::~Port() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "deleting port %s\n", getName().c_str());
    m_manager.unregisterPort(this);
}

/**
 * The idea is that you set all port parameters, and then initialize the port.
 * This allocates all resources and makes the port usable. However, you can't
 * change the port properties anymore after this.
 *
 * @return true if successfull. false if not (all resources are freed).
 */
bool Port::init() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "Initialize port %s\n", m_Name.c_str());
    if (m_State != E_Created) {
        debugFatal("Port (%s) not in E_Created state: %d\n", m_Name.c_str(), m_State);
        return false;
    }

    if (m_buffersize == 0) {
        debugFatal("Cannot initialize a port with buffersize=0\n");
        return false;
    }

    m_State = E_Initialized;
    return true;
}

bool Port::reset() {
    return true;
}

bool Port::setName(std::string name) {
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting name to %s for port %s\n",name.c_str(),m_Name.c_str());

    if (m_State != E_Created) {
        debugFatal("Port (%s) not in E_Created state: %d\n",m_Name.c_str(),m_State);
        return false;
    }
    m_Name=name;
    return true;
}

bool Port::setBufferSize(unsigned int newsize) {
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting buffersize to %d for port %s\n",newsize,m_Name.c_str());
    if (m_State != E_Created && m_disabled == false) {
        debugFatal("Port (%s) not in E_Created/disabled state: %d\n",m_Name.c_str(),m_State);
        return false;
    }
    m_buffersize=newsize;
    return true;
}

unsigned int Port::getEventSize() {
    return 4; // whether it's float, int24, midi or control, it's 4
}

// buffer handling api's for pointer buffers
/**
 * Get the buffer address
 *
 * @param buff
 */
void *Port::getBufferAddress() {
    return m_buffer;
};

/**
 * Set the external buffer address.
 *
 * @param buff
 */
void Port::setBufferAddress(void *buff) {
    m_buffer=buff;
}

/// Enable the port. (this can be called anytime)
void
Port::enable()  {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "Enabling port %s...\n",m_Name.c_str());
    m_disabled = false;
}

/// Disable the port. (this can be called anytime)
void
Port::disable() {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "Disabling port %s...\n",m_Name.c_str());
    m_disabled = true;
}


/**
 * Obtain the port type in string format
 * @return type name of the port
 */
std::string
Port::getPortTypeName()
{
    switch(m_PortType) {
        case E_Audio:
            return "Audio";
        case E_Midi:
            return "MIDI";
        case E_Control:
            return "Control";
        default:
            return "Invalid";
    }
}

void Port::show() {
    debugOutput(DEBUG_LEVEL_VERBOSE,"Name          : %s\n", m_Name.c_str());
    debugOutput(DEBUG_LEVEL_VERBOSE,"Enabled?      : %d\n", m_disabled==false);
    debugOutput(DEBUG_LEVEL_VERBOSE,"State?        : %d\n", m_State);
    debugOutput(DEBUG_LEVEL_VERBOSE,"Buffer Size   : %d\n", m_buffersize);
    debugOutput(DEBUG_LEVEL_VERBOSE,"Event Size    : %d\n", getEventSize());
    debugOutput(DEBUG_LEVEL_VERBOSE,"Port Type     : %d\n", m_PortType);
    debugOutput(DEBUG_LEVEL_VERBOSE,"Direction     : %d\n", m_Direction);
}

void Port::setVerboseLevel(int l) {
    setDebugLevel(l);
}

}
