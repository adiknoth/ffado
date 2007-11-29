/*
 * Copyright (C) 2005-2007 by Daniel Wagner
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

#ifndef FFADODEVICEMANAGER_H
#define FFADODEVICEMANAGER_H

#include "debugmodule/debugmodule.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "libutil/OptionContainer.h"
#include "libcontrol/BasicElements.h"

#include <glibmm/ustring.h>

#include <vector>
#include <string>

class Ieee1394Service;
class FFADODevice;
namespace Streaming {
    class StreamProcessor;
}

typedef std::vector< FFADODevice* > FFADODeviceVector;
typedef std::vector< FFADODevice* >::iterator FFADODeviceVectorIterator;

typedef std::vector< Ieee1394Service* > Ieee1394ServiceVector;
typedef std::vector< Ieee1394Service* >::iterator Ieee1394ServiceVectorIterator;

typedef std::vector< Functor* > FunctorVector;
typedef std::vector< Functor* >::iterator FunctorVectorIterator;

class DeviceManager
    : public Util::OptionContainer,
      public Control::Container
{
public:
    DeviceManager();
    ~DeviceManager();

    bool initialize();
    bool deinitialize();

    bool addSpecString(char *);
    bool isSpecStringValid(std::string s);

    bool discover();

    bool isValidNode( int node );
    int getNbDevices();
    int getDeviceNodeId( int deviceNr );

    FFADODevice* getAvDevice( int nodeId );
    FFADODevice* getAvDeviceByIndex( int idx );
    unsigned int getAvDeviceCount();

    Streaming::StreamProcessor *getSyncSource();

    void show();

    // the Control::Container functions
    virtual std::string getName() 
        {return "DeviceManager";};
    virtual bool setName( std::string n )
        { return false;};

protected:
    FFADODevice* getDriverForDevice( std::auto_ptr<ConfigRom>( configRom ),
                                     int id );
    FFADODevice* getSlaveDriver( std::auto_ptr<ConfigRom>( configRom ) );

    void busresetHandler();

protected:
    // we have one service for each port
    // found on the system. We don't allow dynamic addition of ports (yet)
    Ieee1394ServiceVector   m_1394Services;
    FFADODeviceVector       m_avDevices;
    FunctorVector           m_busreset_functors;

    std::vector<std::string>          m_SpecStrings;

// debug stuff
public:
    void setVerboseLevel(int l);
private:
    DECLARE_DEBUG_MODULE;
};

#endif
