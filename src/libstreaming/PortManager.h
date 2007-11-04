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

#ifndef __FFADO_PORTMANAGER__
#define __FFADO_PORTMANAGER__

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

    virtual bool makeNameUnique(Port *port);
    virtual bool addPort(Port *port);
    virtual bool deletePort(Port *port);
    virtual void deleteAllPorts();

    int getPortCount(enum Port::E_PortType);
    int getPortCount();

//     virtual bool setPortBuffersize(unsigned int newsize);

    Port *getPortAtIdx(unsigned int index);

    virtual bool resetPorts();
    virtual bool initPorts();
    virtual bool preparePorts();

     virtual void setVerboseLevel(int l);

protected:
    PortVector m_Ports;
    PortVector m_PacketPorts;
    PortVector m_PeriodPorts;
//     PortVector m_SamplePorts;

    DECLARE_DEBUG_MODULE;

};

}

#endif /* __FFADO_PORTMANAGER__ */


