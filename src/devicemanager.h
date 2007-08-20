/*
 * Copyright (C) 2005-2007 by Daniel Wagner
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

#ifndef FFADODEVICEMANAGER_H
#define FFADODEVICEMANAGER_H

#include "debugmodule/debugmodule.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "libutil/OptionContainer.h"
#include "libosc/OscServer.h"
#include "libosc/OscNode.h"

#include <glibmm/ustring.h>

#include <vector>

class Ieee1394Service;
class IAvDevice;
namespace Streaming {
    class StreamProcessor;
}

typedef std::vector< IAvDevice* > IAvDeviceVector;
typedef std::vector< IAvDevice* >::iterator IAvDeviceVectorIterator;

class DeviceManager
    : public Util::OptionContainer,
      public OSC::OscNode
{
public:
    DeviceManager();
    ~DeviceManager();

    bool initialize( int port );
    bool deinitialize();

    bool discover( );

    bool isValidNode( int node );
    int getNbDevices();
    int getDeviceNodeId( int deviceNr );

    IAvDevice* getAvDevice( int nodeId );
    IAvDevice* getAvDeviceByIndex( int idx );
    unsigned int getAvDeviceCount();

    Streaming::StreamProcessor *getSyncSource();

protected:
    IAvDevice* getDriverForDevice( std::auto_ptr<ConfigRom>( configRom ),
                                   int id );
    IAvDevice* getSlaveDriver( std::auto_ptr<ConfigRom>( configRom ) );

protected:
    Ieee1394Service* m_1394Service;
    IAvDeviceVector  m_avDevices;

    OSC::OscServer*  m_oscServer;

// debug stuff
public:
    void setVerboseLevel(int l);
private:
    int m_verboseLevel;
    DECLARE_DEBUG_MODULE;
};

#endif
