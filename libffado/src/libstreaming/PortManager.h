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
#ifndef __FREEBOB_PORTMANAGER__
#define __FREEBOB_PORTMANAGER__

#include "../debugmodule/debugmodule.h"
#include "Port.h"

#include <vector>

namespace Streaming {

class Port;
typedef std::vector<Port *> PortVector;
typedef std::vector<Port *>::iterator PortVectorIterator;
/*!
\brief The Base Class for any class that maintains a collection of ports.

 Contains helper classes that allow the easy maintaining of Port collections.
 
*/
class PortManager {

public:

	PortManager();
	virtual ~PortManager();

	virtual bool addPort(Port *port);
	virtual bool deletePort(Port *port);
	virtual void deleteAllPorts();

	int getPortCount(enum Port::E_PortType);
	int getPortCount();

// 	virtual bool setPortBuffersize(unsigned int newsize);

	Port *getPortAtIdx(unsigned int index);

	virtual bool resetPorts();
	virtual bool initPorts();	
	virtual bool preparePorts();	

 	virtual void setVerboseLevel(int l);

protected:
	PortVector m_Ports;
	PortVector m_PacketPorts;
	PortVector m_PeriodPorts;
// 	PortVector m_SamplePorts;

    DECLARE_DEBUG_MODULE;

};

}

#endif /* __FREEBOB_PORTMANAGER__ */


