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

#include <vector>

namespace FreebobStreaming {

class Port;
typedef std::vector<Port *> PortVector;
typedef std::vector<Port *>::iterator PortVectorIterator;

class PortManager {

public:

	PortManager();
	virtual ~PortManager();

	virtual int addPort(Port *port);
	virtual int deletePort(Port *port);

	void reset();
	void prepare();	

 	void setVerboseLevel(int l) {  };

protected:
	PortVector m_PacketPorts;
	PortVector m_PeriodPorts;
// 	PortVector m_SamplePorts;

    DECLARE_DEBUG_MODULE;

};

}

#endif /* __FREEBOB_PORTMANAGER__ */


