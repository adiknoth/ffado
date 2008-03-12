/*
 * Copyright (C) 2005-2008 by Daniel Wagner
 * Copyright (C) 2005-2008 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) version 3 of the License.
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

#include "libstreaming/StreamProcessorManager.h"

#include "libutil/OptionContainer.h"
#include "libcontrol/BasicElements.h"

#include <glibmm/ustring.h>

#include <vector>
#include <string>

class Ieee1394Service;
class FFADODevice;
class DeviceStringParser;

namespace Streaming {
    class StreamProcessor;
}

typedef std::vector< FFADODevice* > FFADODeviceVector;
typedef std::vector< FFADODevice* >::iterator FFADODeviceVectorIterator;

typedef std::vector< Ieee1394Service* > Ieee1394ServiceVector;
typedef std::vector< Ieee1394Service* >::iterator Ieee1394ServiceVectorIterator;

typedef std::vector< Util::Functor* > FunctorVector;
typedef std::vector< Util::Functor* >::iterator FunctorVectorIterator;

class DeviceManager
    : public Util::OptionContainer,
      public Control::Container
{
public:
    enum eWaitResult {
        eWR_OK,
        eWR_Xrun,
        eWR_Error,
    };

    DeviceManager();
    ~DeviceManager();

    bool setThreadParameters(bool rt, int priority);

    bool initialize();
    bool deinitialize();

    bool addSpecString(char *);
    bool isSpecStringValid(std::string s);

    bool discover(bool userCache=true);
    bool initStreaming();
    bool prepareStreaming();
    bool finishStreaming();
    bool startStreaming();
    bool stopStreaming();
    bool resetStreaming();
    enum eWaitResult waitForPeriod();
    bool setStreamingParams(unsigned int period, unsigned int rate, unsigned int nb_buffers);

    bool isValidNode( int node );
    int getNbDevices();
    int getDeviceNodeId( int deviceNr );

    FFADODevice* getAvDevice( int nodeId );
    FFADODevice* getAvDeviceByIndex( int idx );
    unsigned int getAvDeviceCount();

    Streaming::StreamProcessor *getSyncSource();

    void showDeviceInfo();
    void showStreamingInfo();

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

public: // FIXME: this should be better
    Streaming::StreamProcessorManager&  getStreamProcessorManager() 
        {return *m_processorManager;};
private:
    Streaming::StreamProcessorManager*  m_processorManager;
    DeviceStringParser*                 m_deviceStringParser;
protected:
    std::vector<std::string>            m_SpecStrings;

    bool m_thread_realtime;
    int m_thread_priority;

// debug stuff
public:
    void setVerboseLevel(int l);
private:
    DECLARE_DEBUG_MODULE;
};

#endif
