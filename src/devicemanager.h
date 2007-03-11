/* devicemanager.h
 * Copyright (C) 2005 by Daniel Wagner
 *
 * This file is part of FreeBoB.
 *
 * FreeBoB is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBoB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBoB; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */

#ifndef FREEBOBDEVICEMANAGER_H
#define FREEBOBDEVICEMANAGER_H

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

    bool saveCache( Glib::ustring fileName );
    bool loadCache( Glib::ustring fileName );

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
