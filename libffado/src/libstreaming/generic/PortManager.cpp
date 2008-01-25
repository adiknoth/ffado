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

#include "PortManager.h"
#include "Port.h"

#include <assert.h>

#include <iostream>
#include <sstream>


namespace Streaming {

IMPL_DEBUG_MODULE( PortManager, PortManager, DEBUG_LEVEL_NORMAL );

PortManager::PortManager() {
}

PortManager::~PortManager() {
    flushDebugOutput();
    // delete all ports that are still registered to the manager
    for ( PortVectorIterator it = m_Ports.begin();
    it != m_Ports.end();
    // See note below as to why there's no need to increment "it" here
    // ++it 
    )
    {
        debugOutput( DEBUG_LEVEL_VERBOSE, "deleting port %s at %p\n", (*it)->getName().c_str(), *it);
        flushDebugOutput();
        delete *it;

        // Note: it seems that after the delete operation "it" points to
        // the port in the list which *follows* the one which has been
        // deleted.  Therefore there is no need to increment "it" in the
        // for() statement.   Doing so causes only every second port to be
        // deleted, and (if there's an odd number of ports) "it" will point
        // to invalid memory in the final iteration (causing a segv).
    }
}

bool PortManager::makeNameUnique(Port *port)
{
    bool done=false;
    int idx=0;
    std::string portname_orig=port->getName();
    
    while(!done && idx<10000) {
        bool is_unique=true;
        
        for ( PortVectorIterator it = m_Ports.begin();
        it != m_Ports.end();
        ++it )
        {
            is_unique &= !((*it)->getName() == port->getName());
        }
        
        if (is_unique) {
            done=true;
        } else {
            std::ostringstream portname;
            portname << portname_orig << idx++;
            
            port->setName(portname.str());
        }
    }
    
    if(idx<10000) return true;
    else return false;
}

/**
 *
 * @param port
 * @return
 */
bool PortManager::registerPort(Port *port)
{
    assert(port);

    debugOutput( DEBUG_LEVEL_VERBOSE, "Adding port %s, type: %d, dir: %d\n",
        port->getName().c_str(), port->getPortType(), port->getDirection());

    port->setVerboseLevel(getDebugLevel());

    if (makeNameUnique(port)) {
        m_Ports.push_back(port);
        return true;
    } else {
        return false;
    }
}

bool PortManager::unregisterPort(Port *port)
{
    assert(port);
    debugOutput( DEBUG_LEVEL_VERBOSE, "unregistering port %s\n",port->getName().c_str());

    for ( PortVectorIterator it = m_Ports.begin();
      it != m_Ports.end();
      ++it )
    {
        if(*it == port) {
            m_Ports.erase(it);
            return true;
        }
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, "port %s not found \n",port->getName().c_str());

    return false; //not found

}

int PortManager::getPortCount(enum Port::E_PortType type) {
    int count=0;

    for ( PortVectorIterator it = m_Ports.begin();
      it != m_Ports.end();
      ++it )
    {
        if ( (*it)->getPortType() == type ) {
            count++;
        }
    }
    return count;
}

int PortManager::getPortCount() {
    int count=0;

    count+=m_Ports.size();

    return count;
}

Port * PortManager::getPortAtIdx(unsigned int index) {

    return m_Ports.at(index);

}

void PortManager::setVerboseLevel(int i) {
    setDebugLevel(i);
    for ( PortVectorIterator it = m_Ports.begin();
      it != m_Ports.end();
      ++it )
    {
        (*it)->setVerboseLevel(i);
    }
}


bool PortManager::resetPorts() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "reset ports\n");

    for ( PortVectorIterator it = m_Ports.begin();
      it != m_Ports.end();
      ++it )
    {
        if(!(*it)->reset()) {
            debugFatal("Could not reset port %s",(*it)->getName().c_str());
            return false;
        }
    }
    return true;
}

bool PortManager::initPorts() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "init ports\n");

    for ( PortVectorIterator it = m_Ports.begin();
      it != m_Ports.end();
      ++it )
    {
        if(!(*it)->init()) {
            debugFatal("Could not init port %s\n", (*it)->getName().c_str());
            return false;
        }
    }
    return true;
}

bool PortManager::preparePorts() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "preparing ports\n");

    for ( PortVectorIterator it = m_Ports.begin();
      it != m_Ports.end();
      ++it )
    {
        if(!(*it)->prepare()) {
            debugFatal("Could not prepare port %s",(*it)->getName().c_str());
            return false;
        }

    }
    return true;
}

}
