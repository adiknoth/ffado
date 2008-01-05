/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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

#ifndef __FFADO_PORTMANAGER__
#define __FFADO_PORTMANAGER__

#include "Port.h"

#include "debugmodule/debugmodule.h"

#include <vector>

namespace Streaming {

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

    virtual bool makeNameUnique(Port *port);
    virtual bool registerPort(Port *port);
    virtual bool unregisterPort(Port *port);

    int getPortCount(enum Port::E_PortType);
    int getPortCount();

    Port *getPortAtIdx(unsigned int index);

    virtual bool resetPorts();
    virtual bool initPorts();
    virtual bool preparePorts();

     virtual void setVerboseLevel(int l);

protected:
    PortVector m_Ports;

    DECLARE_DEBUG_MODULE;
};

}

#endif /* __FFADO_PORTMANAGER__ */


