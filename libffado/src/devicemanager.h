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

#include "libutil/OptionContainer.h"
#include "libcontrol/BasicElements.h"

#include "libutil/Functors.h"
#include "libutil/Mutex.h"
#include "libutil/Configuration.h"

#include "libffado/ffado.h"

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

typedef std::vector< ConfigRom* > ConfigRomVector;
typedef std::vector< ConfigRom* >::iterator ConfigRomVectorIterator;

class DeviceManager
    : public Util::OptionContainer,
      public Control::Container
{
public:
    enum eWaitResult {
        eWR_OK,
        eWR_Xrun,
        eWR_Error,
        eWR_Shutdown,
    };

    DeviceManager();
    ~DeviceManager();

    bool setThreadParameters(bool rt, int priority);

    bool initialize();
    bool deinitialize();

    bool addSpecString(char *);
    bool isSpecStringValid(std::string s);

    bool discover(bool useCache=true, bool rediscover=false);

    bool startStreamingOnDevice(unsigned int i);
    bool stopStreamingOnDevice(unsigned int i);

public: // FIXME: this should be better

    bool initStreaming(unsigned int samplerate);
    bool finishStreaming();

    bool isValidNode( int node );
    int getDeviceNodeId( int deviceNr );

    FFADODevice* getAvDevice( int nodeId );
    FFADODevice* getAvDeviceByIndex( int idx );
    unsigned int getAvDeviceCount();

    /**
     * prevents the busreset handler from running. use with care!
     */
    void lockBusResetHandler() {m_BusResetLock->Lock();};
    /**
     * releases the busreset handlers
     */
    void unlockBusResetHandler() {m_BusResetLock->Unlock();};
    bool registerBusresetNotification(Util::Functor *f)
        {return registerNotification(m_busResetNotifiers, f);};
    bool unregisterBusresetNotification(Util::Functor *f)
        {return unregisterNotification(m_busResetNotifiers, f);};

    bool registerPreUpdateNotification(Util::Functor *f)
        {return registerNotification(m_preUpdateNotifiers, f);};
    bool unregisterPreUpdateNotification(Util::Functor *f)
        {return unregisterNotification(m_preUpdateNotifiers, f);};

    bool registerPostUpdateNotification(Util::Functor *f)
        {return registerNotification(m_postUpdateNotifiers, f);};
    bool unregisterPostUpdateNotification(Util::Functor *f)
        {return unregisterNotification(m_postUpdateNotifiers, f);};


    Util::Configuration& getConfiguration() {return *m_configuration;};

    void showDeviceInfo();

    // the Control::Container functions
    virtual std::string getName() 
        {return "DeviceManager";};
    virtual bool setName( std::string n )
        { return false;};

protected:
    FFADODevice* getDriverForDeviceDo( ConfigRom *configRom,
                                       int id, bool generic );
    FFADODevice* getDriverForDevice( ConfigRom *configRom,
                                     int id );
    FFADODevice* getSlaveDriver( std::auto_ptr<ConfigRom>( configRom ) );

    void busresetHandler(Ieee1394Service &);

protected:
    // we have one service for each port
    // found on the system. We don't allow dynamic addition of ports (yet)
    Ieee1394ServiceVector   m_1394Services;
    FFADODeviceVector       m_avDevices;
    FunctorVector           m_busreset_functors;

    // the lock protecting the device list
    Util::Mutex*            m_DeviceListLock;
    // the lock to serialize bus reset handling
    Util::Mutex*            m_BusResetLock;

private:
    DeviceStringParser*                 m_deviceStringParser;
    Util::Configuration*                m_configuration;
    bool                                m_used_cache_last_time;

    typedef std::vector< Util::Functor* > notif_vec_t;
    notif_vec_t                           m_busResetNotifiers;
    notif_vec_t                           m_preUpdateNotifiers;
    notif_vec_t                           m_postUpdateNotifiers;

    bool registerNotification(notif_vec_t&, Util::Functor *);
    bool unregisterNotification(notif_vec_t&, Util::Functor *);
    void signalNotifiers(notif_vec_t& list);

#warning TODO
    int m_nominal_samplerate;

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
