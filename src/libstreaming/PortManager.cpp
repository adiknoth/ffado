/* $Id$ */

/*
 *   FreeBob porting API
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

#include "PortManager.h"
#include "Port.h"
#include <assert.h>


namespace FreebobStreaming {

IMPL_DEBUG_MODULE( PortManager, PortManager, DEBUG_LEVEL_NORMAL );

PortManager::PortManager() {

}

PortManager::~PortManager() {
// 	deleteAllPorts();
}

bool PortManager::setPortBuffersize(unsigned int newsize) {
	debugOutput( DEBUG_LEVEL_VERBOSE, "setting port buffer size to %d\n",newsize);

    for ( PortVectorIterator it = m_PacketPorts.begin();
          it != m_PacketPorts.end();
          ++it )
    {
		if(!(*it)->setBufferSize(newsize)) {
			debugFatal("Could not set buffer size for port %s\n",(*it)->getName().c_str());
			return false;
		}
    }

    for ( PortVectorIterator it = m_PeriodPorts.begin();
          it != m_PeriodPorts.end();
          ++it )
    {
		if(!(*it)->setBufferSize(newsize)) {
			debugFatal("Could not set buffer size for port %s\n",(*it)->getName().c_str());
			return false;
		}
    }

//     for ( PortVectorIterator it = m_SamplePorts.begin();
//           it != m_SamplePorts.end();
//           ++it )
//     {
/*		if(!(*it)->setBufferSize(newsize)) {
			debugFatal("Could not set buffer size for port %s\n",(*it)->getName().c_str());
			return false;
		}*/
//     }
// 	debugOutput( DEBUG_LEVEL_VERBOSE, " => port not found\n");

	return true; //not found

}

int PortManager::addPort(Port *port)
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
	assert(port);

	port->setVerboseLevel(getDebugLevel());

	switch(port->getBufferType()) {
	case (Port::E_PacketBuffered):
		debugOutput( DEBUG_LEVEL_VERBOSE, "Adding packet buffered port %s\n",port->getName().c_str());
		m_PacketPorts.push_back(port);
		break;
	case Port::E_PeriodBuffered:
		debugOutput( DEBUG_LEVEL_VERBOSE, "Adding period buffered port %s\n",port->getName().c_str());
		m_PeriodPorts.push_back(port);
		break;
/*	case Port::E_SampleBuffered:
		m_SamplePorts.push_back(port);
		break;*/
	default:
		debugFatal("Unsupported port type!");
		return -1;
	}

	return 0;
}

int PortManager::deletePort(Port *port)
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
	assert(port);
	debugOutput( DEBUG_LEVEL_VERBOSE, "deleting port %s\n",port->getName().c_str());

    for ( PortVectorIterator it = m_PacketPorts.begin();
          it != m_PacketPorts.end();
          ++it )
    {
        if ( *it == port ) { 
            m_PacketPorts.erase(it);
			delete *it;
			return 0;
        }
    }

    for ( PortVectorIterator it = m_PeriodPorts.begin();
          it != m_PeriodPorts.end();
          ++it )
    {
        if ( *it == port ) { 
            m_PeriodPorts.erase(it);
			delete *it;
			return 0;
        }
    }

//     for ( PortVectorIterator it = m_SamplePorts.begin();
//           it != m_SamplePorts.end();
//           ++it )
//     {
//         if ( *it == port ) { 
//             m_SamplePorts.erase(it);
// 			return 0;
//         }
//     }
// 	debugOutput( DEBUG_LEVEL_VERBOSE, " => port not found\n");

	return -1; //not found

}

void PortManager::deleteAllPorts()
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
	debugOutput( DEBUG_LEVEL_VERBOSE, "deleting all ports\n");

    for ( PortVectorIterator it = m_PacketPorts.begin();
          it != m_PacketPorts.end();
          ++it )
    {
        m_PacketPorts.erase(it);
		delete *it;
    }

    for ( PortVectorIterator it = m_PeriodPorts.begin();
          it != m_PeriodPorts.end();
          ++it )
    {
        m_PeriodPorts.erase(it);
		delete *it;
    }

//     for ( PortVectorIterator it = m_SamplePorts.begin();
//           it != m_SamplePorts.end();
//           ++it )
//     {
//          m_SamplePorts.erase(it);
// 			delete *it;
//     }

	return;

}

int PortManager::getPortCount(enum Port::E_PortType type) {
	int count=0;

    for ( PortVectorIterator it = m_PacketPorts.begin();
          it != m_PacketPorts.end();
          ++it )
    {
        if ( (*it)->getPortType() == type ) { 
			count++;
        }
    }

    for ( PortVectorIterator it = m_PeriodPorts.begin();
          it != m_PeriodPorts.end();
          ++it )
    {
        if ( (*it)->getPortType() == type ) { 
			count++;
        }
    }

//     for ( PortVectorIterator it = m_SamplePorts.begin();
//           it != m_SamplePorts.end();
//           ++it )
//     {
//         if ( (*it)->getPortType() == type ) { 
// 			count++;
//         }
//         }
//     }

	return count;
}

int PortManager::getPortCount() {
	int count=0;


	
	count+=m_PacketPorts.size();
	count+=m_PeriodPorts.size();
// 	count+=m_SamplePorts.size();

	return count;
}

Port * PortManager::getPortAtIdx(unsigned int index) {

	if (index<m_PacketPorts.size()) {
		return m_PacketPorts.at(index);
	}
	index -= m_PacketPorts.size();

	if (index<m_PeriodPorts.size()) {
		return m_PeriodPorts.at(index);
	}

// 	index -= m_PeriodPorts.size();
// 
// 	if (index<m_SamplePorts.size()) {
// 		return m_SamplePorts.at(index);
// 	}

	return 0;

}

void PortManager::setVerboseLevel(int i) {

	setDebugLevel(i);

    for ( PortVectorIterator it = m_PacketPorts.begin();
          it != m_PacketPorts.end();
          ++it )
    {
        (*it)->setVerboseLevel(i);
    }

    for ( PortVectorIterator it = m_PeriodPorts.begin();
          it != m_PeriodPorts.end();
          ++it )
    {
        (*it)->setVerboseLevel(i);
    }

//     for ( PortVectorIterator it = m_SamplePorts.begin();
//           it != m_SamplePorts.end();
//           ++it )
//     {
//         (*it)->setVerboseLevel(i);
//     }

}


bool PortManager::resetPorts() {
	return true;
}

bool PortManager::initPorts() {
	return true;
}

bool PortManager::preparePorts() {
	return true;
}

}
