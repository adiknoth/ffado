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

namespace FreebobStreaming {

IMPL_DEBUG_MODULE( PortManager, PortManager, DEBUG_LEVEL_NORMAL );

PortManager::PortManager() {

}

PortManager::~PortManager() {

}

int PortManager::addPort(Port *port)
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
	assert(port);

	m_Ports.push_back(port);

	return 0;
}

int PortManager::deletePort(Port *port)
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
	assert(port);

    for ( PortVectorIterator it = m_Ports.begin();
          it != m_Ports.end();
          ++it )
    {
        if ( *it == port ) { 
            m_Ports.erase(it);
			return 0;
        }
    }

	return -1; //not found

}

}
