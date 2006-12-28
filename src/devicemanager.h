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

#include "libfreebob/xmlparser.h"

#include <vector>

class Ieee1394Service;
class IAvDevice;

typedef std::vector< IAvDevice* > IAvDeviceVector;
typedef std::vector< IAvDevice* >::iterator IAvDeviceVectorIterator;

class ConfigRom;


class DeviceManager{
 public:
    DeviceManager();
    ~DeviceManager();

    bool initialize( int port );
    bool deinitialize();

    bool discover( int verboseLevel );

    bool isValidNode( int node );
    int getNbDevices();
    int getDeviceNodeId( int deviceNr );

    IAvDevice* getAvDevice( int nodeId );
	IAvDevice* getAvDeviceByIndex( int idx );
	unsigned int getAvDeviceCount();

    xmlDocPtr getXmlDescription();

protected:
    IAvDevice* getDriverForDevice( std::auto_ptr<ConfigRom>( configRom ),
                                   int id,
                                   int level );

protected:
    Ieee1394Service* m_1394Service;
    IAvDeviceVector  m_avDevices;

    DECLARE_DEBUG_MODULE;
};

#endif
