/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software Foundation;
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
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
//     deleteAllPorts();
}

// bool PortManager::setPortBuffersize(unsigned int newsize) {
//     debugOutput( DEBUG_LEVEL_VERBOSE, "setting port buffer size to %d\n",newsize);
//
//
//     for ( PortVectorIterator it = m_Ports.begin();
//       it != m_Ports.end();
//       ++it )
//     {
//         if(!(*it)->setBufferSize(newsize)) {
//             debugFatal("Could not set buffer size for port %s\n",(*it)->getName().c_str());
//             return false;
//         }
//     }
//
//     return true; //not found
//
// }

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
bool PortManager::addPort(Port *port)
{
    assert(port);

    debugOutput( DEBUG_LEVEL_VERBOSE, "Adding port %s, type: %d, dir: %d, dtype: %d\n",
        port->getName().c_str(), port->getPortType(), port->getDirection(), port->getDataType());
    
    if (makeNameUnique(port)) {
        m_Ports.push_back(port);
        return true;
    } else {
        return false;
    }
}

bool PortManager::deletePort(Port *port)
{
    assert(port);
    debugOutput( DEBUG_LEVEL_VERBOSE, "deleting port %s\n",port->getName().c_str());

    for ( PortVectorIterator it = m_Ports.begin();
      it != m_Ports.end();
      ++it )
    {
        if(*it == port) {
            m_Ports.erase(it);
//             delete *it;
            return true;
        }
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, "port %s not found \n",port->getName().c_str());

    return false; //not found

}

void PortManager::deleteAllPorts()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "deleting all ports\n");

    for ( PortVectorIterator it = m_Ports.begin();
      it != m_Ports.end();
      ++it )
    {
        m_Ports.erase(it);
//         delete *it;
    }

    return;

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
            debugFatal("Could not init port %s",(*it)->getName().c_str());
            return false;
        }
    }
    return true;
}

bool PortManager::preparePorts() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "preparing ports\n");

    // clear the cache lists
    m_PeriodPorts.clear();
    m_PacketPorts.clear();

    for ( PortVectorIterator it = m_Ports.begin();
      it != m_Ports.end();
      ++it )
    {
        if(!(*it)->prepare()) {
            debugFatal("Could not prepare port %s",(*it)->getName().c_str());
            return false;
        }

        // now prepare the cache lists
        switch((*it)->getSignalType()) {
            case Port::E_PacketSignalled:
                m_PacketPorts.push_back(*it);
                break;
            case Port::E_PeriodSignalled:
                m_PeriodPorts.push_back(*it);
                break;
            default:
                debugWarning("%s has unsupported port type\n",
                             (*it)->getName().c_str());
            break;
        }
    }
    return true;
}

}
