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

#include "config.h"
#include "fbtypes.h"

#include "devicemanager.h"
#include "ffadodevice.h"
#include "DeviceStringParser.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"
#include "libieee1394/IsoHandlerManager.h"

#include "libstreaming/generic/StreamProcessor.h"
#include "libstreaming/StreamProcessorManager.h"

#include "debugmodule/debugmodule.h"

#include "libutil/PosixMutex.h"

#ifdef ENABLE_BEBOB
#include "bebob/bebob_avdevice.h"
#endif

#ifdef ENABLE_MAUDIO
#include "maudio/maudio_avdevice.h"
#endif

#ifdef ENABLE_GENERICAVC
    #include "genericavc/avc_avdevice.h"
#endif

#ifdef ENABLE_FIREWORKS
    #include "fireworks/fireworks_device.h"
#endif

#ifdef ENABLE_OXFORD
    #include "oxford/oxford_device.h"
#endif

#ifdef ENABLE_BOUNCE
#include "bounce/bounce_avdevice.h"
#include "bounce/bounce_slave_avdevice.h"
#endif

#ifdef ENABLE_MOTU
#include "motu/motu_avdevice.h"
#endif

#ifdef ENABLE_RME
#include "rme/rme_avdevice.h"
#endif

#ifdef ENABLE_DICE
#include "dice/dice_avdevice.h"
#endif

#ifdef ENABLE_METRIC_HALO
#include "metrichalo/mh_avdevice.h"
#endif

#include <iostream>
#include <sstream>

#include <algorithm>

using namespace std;

IMPL_DEBUG_MODULE( DeviceManager, DeviceManager, DEBUG_LEVEL_NORMAL );

DeviceManager::DeviceManager()
    : Control::Container(NULL, "devicemanager") // this is the control root node
    , m_DeviceListLock( new Util::PosixMutex("DEVLST") )
    , m_BusResetLock( new Util::PosixMutex("DEVBR") )
    , m_processorManager( new Streaming::StreamProcessorManager( *this ) )
    , m_deviceStringParser( new DeviceStringParser() )
    , m_configuration ( new Util::Configuration() )
    , m_used_cache_last_time( false )
    , m_thread_realtime( false )
    , m_thread_priority( 0 )
{
    addOption(Util::OptionContainer::Option("slaveMode", false));
    addOption(Util::OptionContainer::Option("snoopMode", false));
}

DeviceManager::~DeviceManager()
{
    // save configuration
    if(!m_configuration->save()) {
        debugWarning("could not save configuration\n");
    }

    m_BusResetLock->Lock(); // make sure we are not handling a busreset.
    m_DeviceListLock->Lock(); // make sure nobody is using this
    for ( FFADODeviceVectorIterator it = m_avDevices.begin();
          it != m_avDevices.end();
          ++it )
    {
        if (!deleteElement(*it)) {
            debugWarning("failed to remove Device from Control::Container\n");
        }
        delete *it;
    }
    m_DeviceListLock->Unlock();

    // the SP's are automatically unregistered from the SPM
    delete m_processorManager;

    // the device list is empty, so wake up any waiting
    // reset handlers
    m_BusResetLock->Unlock();

    // remove the bus-reset handlers
    for ( FunctorVectorIterator it = m_busreset_functors.begin();
          it != m_busreset_functors.end();
          ++it )
    {
        delete *it;
    }

    for ( Ieee1394ServiceVectorIterator it = m_1394Services.begin();
          it != m_1394Services.end();
          ++it )
    {
        delete *it;
    }

    delete m_DeviceListLock;
    delete m_BusResetLock;
    delete m_deviceStringParser;
}

bool
DeviceManager::setThreadParameters(bool rt, int priority) {
    if (!m_processorManager->setThreadParameters(rt, priority)) {
        debugError("Could not set processor manager thread parameters\n");
        return false;
    }
    for ( Ieee1394ServiceVectorIterator it = m_1394Services.begin();
          it != m_1394Services.end();
          ++it )
    {
        if (!(*it)->setThreadParameters(rt, priority)) {
            debugError("Could not set 1394 service thread parameters\n");
            return false;
        }
    }
    m_thread_realtime = rt;
    m_thread_priority = priority;
    return true;
}

bool
DeviceManager::initialize()
{
    assert(m_1394Services.size() == 0);
    assert(m_busreset_functors.size() == 0);

    m_configuration->openFile( "temporary", Util::Configuration::eFM_Temporary );
    m_configuration->openFile( USER_CONFIG_FILE, Util::Configuration::eFM_ReadWrite );
    m_configuration->openFile( SYSTEM_CONFIG_FILE, Util::Configuration::eFM_ReadOnly );

    int nb_detected_ports = Ieee1394Service::detectNbPorts();
    if (nb_detected_ports < 0) {
        debugFatal("Failed to detect the number of 1394 adapters. Is the IEEE1394 stack loaded (raw1394)?\n");
        return false;
    }
    if (nb_detected_ports == 0) {
        debugFatal("No firewire adapters (ports) found.\n");
        return false;
    }
    debugOutput( DEBUG_LEVEL_VERBOSE, "Found %d firewire adapters (ports)\n", nb_detected_ports);
    for (unsigned int port = 0; port < (unsigned int)nb_detected_ports; port++) {
        Ieee1394Service* tmp1394Service = new Ieee1394Service();
        if ( !tmp1394Service ) {
            debugFatal( "Could not create Ieee1349Service object for port %d\n", port );
            return false;
        }
        tmp1394Service->setVerboseLevel( getDebugLevel() );
        m_1394Services.push_back(tmp1394Service);

        if(!tmp1394Service->useConfiguration(m_configuration)) {
            debugWarning("Could not load config to 1394service\n");
        }

        tmp1394Service->setThreadParameters(m_thread_realtime, m_thread_priority);
        if ( !tmp1394Service->initialize( port ) ) {
            debugFatal( "Could not initialize Ieee1349Service object for port %d\n", port );
            return false;
        }
        // add the bus reset handler
        Util::Functor* tmp_busreset_functor = new Util::MemberFunctor1< DeviceManager*,
                    void (DeviceManager::*)(Ieee1394Service &), Ieee1394Service & >
                    ( this, &DeviceManager::busresetHandler, *tmp1394Service, false );
        if ( !tmp_busreset_functor ) {
            debugFatal( "Could not create busreset handler for port %d\n", port );
            return false;
        }
        m_busreset_functors.push_back(tmp_busreset_functor);

        tmp1394Service->addBusResetHandler( tmp_busreset_functor );
    }

    return true;
}

bool
DeviceManager::addSpecString(char *s) {
    std::string spec = s;
    if(isSpecStringValid(spec)) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Adding spec string %s\n", spec.c_str());
        assert(m_deviceStringParser);
        m_deviceStringParser->parseString(spec);
        return true;
    } else {
        debugError("Invalid spec string: %s\n", spec.c_str());
        return false;
    }
}

bool
DeviceManager::isSpecStringValid(std::string s) {
    assert(m_deviceStringParser);
    return m_deviceStringParser->isValidString(s);
}

void
DeviceManager::busresetHandler(Ieee1394Service &service)
{
    // serialize bus reset handling since it can be that a new one occurs while we're
    // doing stuff.
    debugOutput( DEBUG_LEVEL_NORMAL, "Bus reset detected on service %p...\n", &service );
    Util::MutexLockHelper lock(*m_BusResetLock);
    debugOutput( DEBUG_LEVEL_NORMAL, " handling busreset...\n" );

    // FIXME: what if the devices are gone? (device should detect this!)
    // propagate the bus reset to all avDevices
    m_DeviceListLock->Lock(); // make sure nobody is using this
    for ( FFADODeviceVectorIterator it = m_avDevices.begin();
          it != m_avDevices.end();
          ++it )
    {
        if(&service == &((*it)->get1394Service())) {
            debugOutput(DEBUG_LEVEL_NORMAL,
                        "issue busreset on device GUID %s\n",
                        (*it)->getConfigRom().getGuidString().c_str());
            (*it)->handleBusReset();
        } else {
            debugOutput(DEBUG_LEVEL_NORMAL,
                        "skipping device GUID %s since not on service %p\n",
                        (*it)->getConfigRom().getGuidString().c_str(), &service);
        }
    }
    m_DeviceListLock->Unlock();

    // now that the devices have been updates, we can request to update the iso streams
    if(!service.getIsoHandlerManager().handleBusReset()) {
        debugError("IsoHandlerManager failed to handle busreset\n");
    }

    // notify the streamprocessormanager of the busreset
//     if(m_processorManager) {
//         m_processorManager->handleBusReset(service);
//     } else {
//         debugWarning("No valid SPM\n");
//     }

    // rediscover to find new devices
    // (only for the control server ATM, streaming can't dynamically add/remove devices)
    if(!discover(m_used_cache_last_time, true)) {
        debugError("Could not rediscover devices\n");
    }

    // notify any clients
    signalNotifiers(m_busResetNotifiers);

    // display the new state
    if(getDebugLevel() >= DEBUG_LEVEL_VERBOSE) {
        showDeviceInfo();
    }
}

void
DeviceManager::signalNotifiers(notif_vec_t& list)
{
    for ( notif_vec_t::iterator it = list.begin();
          it != list.end();
          ++it )
    {
        Util::Functor* func = *it;
        debugOutput( DEBUG_LEVEL_VERBOSE, " running notifier %p...\n", func );
        ( *func )();
    }
}

bool
DeviceManager::registerNotification(notif_vec_t& list, Util::Functor *handler)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "register %p...\n", handler);
    assert(handler);
    for ( notif_vec_t::iterator it = list.begin();
      it != list.end();
      ++it )
    {
        if ( *it == handler ) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "already registered\n");
            return false;
        }
    }
    list.push_back(handler);
    return true;
}

bool
DeviceManager::unregisterNotification(notif_vec_t& list, Util::Functor *handler)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "unregister %p...\n", handler);
    assert(handler);

    for ( notif_vec_t::iterator it = list.begin();
      it != list.end();
      ++it )
    {
        if ( *it == handler ) {
            list.erase(it);
            return true;
        }
    }
    debugError("Could not find handler (%p)\n", handler);
    return false; //not found
}

bool
DeviceManager::discover( bool useCache, bool rediscover )
{
    debugOutput( DEBUG_LEVEL_NORMAL, "Starting discovery...\n" );
    useCache = useCache && ENABLE_DISCOVERY_CACHE;
    m_used_cache_last_time = useCache;
    bool slaveMode=false;
    if(!getOption("slaveMode", slaveMode)) {
        debugWarning("Could not retrieve slaveMode parameter, defauling to false\n");
    }
    bool snoopMode=false;
    if(!getOption("snoopMode", snoopMode)) {
        debugWarning("Could not retrieve snoopMode parameter, defauling to false\n");
    }

    setVerboseLevel(getDebugLevel());

    // FIXME: it could be that a 1394service has disappeared (cardbus)

    ConfigRomVector configRoms;
    // build a list of configroms on the bus.
    for ( Ieee1394ServiceVectorIterator it = m_1394Services.begin();
        it != m_1394Services.end();
        ++it )
    {
        Ieee1394Service *portService = *it;
        for ( fb_nodeid_t nodeId = 0;
            nodeId < portService->getNodeCount();
            ++nodeId )
        {
            debugOutput( DEBUG_LEVEL_VERBOSE, "Probing node %d...\n", nodeId );

            if (nodeId == portService->getLocalNodeId()) {
                debugOutput( DEBUG_LEVEL_VERBOSE, "Skipping local node (%d)...\n", nodeId );
                continue;
            }

            ConfigRom * configRom = new ConfigRom( *portService, nodeId );
            if ( !configRom->initialize() ) {
                // \todo If a PHY on the bus is in power safe mode then
                // the config rom is missing. So this might be just
                // such this case and we can safely skip it. But it might
                // be there is a real software problem on our side.
                // This should be handlede more carefuly.
                debugOutput( DEBUG_LEVEL_NORMAL,
                            "Could not read config rom from device (node id %d). "
                            "Skip device discovering for this node\n",
                            nodeId );
                continue;
            }
            configRoms.push_back(configRom);
        }
    }


    // notify that we are going to manipulate the list
    signalNotifiers(m_preUpdateNotifiers);
    m_DeviceListLock->Lock(); // make sure nobody starts using the list
    if(rediscover) {

        FFADODeviceVector discovered_devices_on_bus;
        for ( FFADODeviceVectorIterator it = m_avDevices.begin();
            it != m_avDevices.end();
            ++it )
        {
            bool seen_device = false;
            for ( ConfigRomVectorIterator it2 = configRoms.begin();
                it2 != configRoms.end();
                ++it2 )
            {
                seen_device |= ((*it)->getConfigRom().getGuid() == (*it2)->getGuid());
            }

            if(seen_device) {
                debugOutput( DEBUG_LEVEL_VERBOSE,
                            "Already discovered device with GUID: %s\n",
                            (*it)->getConfigRom().getGuidString().c_str() );
                // we already discovered this device, and it is still here. keep it
                discovered_devices_on_bus.push_back(*it);
            } else {
                debugOutput( DEBUG_LEVEL_VERBOSE,
                            "Device with GUID: %s disappeared from bus, removing...\n",
                            (*it)->getConfigRom().getGuidString().c_str() );

                // the device has disappeared, remove it from the control tree
                if (!deleteElement(*it)) {
                    debugWarning("failed to remove Device from Control::Container\n");
                }
                // delete the device
                // FIXME: this will mess up the any code that waits for bus resets to
                //        occur
                delete *it;
            }
        }
        // prune the devices that disappeared
        m_avDevices = discovered_devices_on_bus;
    } else { // remove everything since we are not rediscovering
        for ( FFADODeviceVectorIterator it = m_avDevices.begin();
            it != m_avDevices.end();
            ++it )
        {
            if (!deleteElement(*it)) {
                debugWarning("failed to remove Device from Control::Container\n");
            }
            delete *it;
        }

        m_avDevices.clear();
    }

    // delete the config rom list entries
    // FIXME: we should reuse it
    for ( ConfigRomVectorIterator it = configRoms.begin();
        it != configRoms.end();
        ++it )
    {
        delete *it;
    }

    assert(m_deviceStringParser);
    // show the spec strings we're going to use
    if(getDebugLevel() >= DEBUG_LEVEL_VERBOSE) {
        m_deviceStringParser->show();
    }

    if (!slaveMode) {
        // for the devices that are still in the list check if they require re-discovery
        FFADODeviceVector failed_to_rediscover;
        for ( FFADODeviceVectorIterator it_dev = m_avDevices.begin();
            it_dev != m_avDevices.end();
            ++it_dev )
        {
            FFADODevice* avDevice = *it_dev;
            if(avDevice->needsRediscovery()) {
                debugOutput( DEBUG_LEVEL_NORMAL,
                             "Device with GUID %s requires rediscovery (state changed)...\n",
                             avDevice->getConfigRom().getGuidString().c_str());

                bool isFromCache = false;
                if ( useCache && avDevice->loadFromCache() ) {
                    debugOutput( DEBUG_LEVEL_VERBOSE, "could load from cache\n" );
                    isFromCache = true;
                    // restore the debug level for everything that was loaded
                    avDevice->setVerboseLevel( getDebugLevel() );
                } else if ( avDevice->discover() ) {
                    debugOutput( DEBUG_LEVEL_VERBOSE, "discovery successful\n" );
                } else {
                    debugError( "could not discover device\n" );
                    failed_to_rediscover.push_back(avDevice);
                    continue;
                }
                if ( !isFromCache && !avDevice->saveCache() ) {
                    debugOutput( DEBUG_LEVEL_VERBOSE, "No cached version of AVC model created\n" );
                }
            } else {
                debugOutput( DEBUG_LEVEL_NORMAL,
                             "Device with GUID %s does not require rediscovery...\n",
                             avDevice->getConfigRom().getGuidString().c_str());
            }
        }
        // remove devices that failed to rediscover
        // FIXME: surely there has to be a better way to do this
        FFADODeviceVector to_keep;
        for ( FFADODeviceVectorIterator it = m_avDevices.begin();
            it != m_avDevices.end();
            ++it )
        {
            bool keep_this_device = true;
            for ( FFADODeviceVectorIterator it2 = failed_to_rediscover.begin();
                it2 != failed_to_rediscover.end();
                ++it2 )
            {
                if(*it == *it2) {
                    debugOutput( DEBUG_LEVEL_NORMAL,
                                "Removing device with GUID %s due to failed discovery...\n",
                                (*it)->getConfigRom().getGuidString().c_str());
                    keep_this_device = false;
                    break;
                }
            }
            if(keep_this_device) {
                to_keep.push_back(*it);
            }
        }
        for ( FFADODeviceVectorIterator it2 = failed_to_rediscover.begin();
            it2 != failed_to_rediscover.end();
            ++it2 )
        {
            if (!deleteElement(*it2)) {
                debugWarning("failed to remove Device from Control::Container\n");
            }
            delete *it2;
        }
        m_avDevices = to_keep;

        // pick up new devices
        for ( Ieee1394ServiceVectorIterator it = m_1394Services.begin();
            it != m_1394Services.end();
            ++it )
        {
            Ieee1394Service *portService = *it;
            for ( fb_nodeid_t nodeId = 0;
                nodeId < portService->getNodeCount();
                ++nodeId )
            {
                debugOutput( DEBUG_LEVEL_VERBOSE, "Probing node %d...\n", nodeId );
    
                if (nodeId == portService->getLocalNodeId()) {
                    debugOutput( DEBUG_LEVEL_VERBOSE, "Skipping local node (%d)...\n", nodeId );
                    continue;
                }
    
                ConfigRom *configRom = new ConfigRom( *portService, nodeId );
                if ( !configRom->initialize() ) {
                    // \todo If a PHY on the bus is in power safe mode then
                    // the config rom is missing. So this might be just
                    // such this case and we can safely skip it. But it might
                    // be there is a real software problem on our side.
                    // This should be handlede more carefuly.
                    debugOutput( DEBUG_LEVEL_NORMAL,
                                "Could not read config rom from device (node id %d). "
                                "Skip device discovering for this node\n",
                                nodeId );
                    continue;
                }

                bool already_in_vector = false;
                for ( FFADODeviceVectorIterator it_dev = m_avDevices.begin();
                    it_dev != m_avDevices.end();
                    ++it_dev )
                {
                    if ((*it_dev)->getConfigRom().getGuid() == configRom->getGuid()) {
                        already_in_vector = true;
                        break;
                    }
                }
                if(already_in_vector) {
                    if(!rediscover) {
                        debugWarning("Device with GUID %s already discovered on other port, skipping device...\n",
                                    configRom->getGuidString().c_str());
                    }
                    continue;
                }

                if(getDebugLevel() >= DEBUG_LEVEL_VERBOSE) {
                    configRom->printConfigRomDebug();
                }

                // if spec strings are given, only add those devices
                // that match the spec string(s).
                // if no (valid) spec strings are present, grab all
                // supported devices.
                if(m_deviceStringParser->countDeviceStrings() &&
                  !m_deviceStringParser->match(*configRom)) {
                    debugOutput(DEBUG_LEVEL_VERBOSE, "Device doesn't match any of the spec strings. skipping...\n");
                    continue;
                }

                // find a driver
                FFADODevice* avDevice = getDriverForDevice( configRom,
                                                            nodeId );

                if ( avDevice ) {
                    debugOutput( DEBUG_LEVEL_NORMAL,
                                "driver found for device %d\n",
                                nodeId );

                    avDevice->setVerboseLevel( getDebugLevel() );
                    bool isFromCache = false;
                    if ( useCache && avDevice->loadFromCache() ) {
                        debugOutput( DEBUG_LEVEL_VERBOSE, "could load from cache\n" );
                        isFromCache = true;
                        // restore the debug level for everything that was loaded
                        avDevice->setVerboseLevel( getDebugLevel() );
                    } else if ( avDevice->discover() ) {
                        debugOutput( DEBUG_LEVEL_VERBOSE, "discovery successful\n" );
                    } else {
                        debugError( "could not discover device\n" );
                        delete avDevice;
                        continue;
                    }

                    if (snoopMode) {
                        debugOutput( DEBUG_LEVEL_VERBOSE,
                                    "Enabling snoop mode on node %d...\n", nodeId );

                        if(!avDevice->setOption("snoopMode", snoopMode)) {
                            debugWarning("Could not set snoop mode for device on node %d\n", nodeId);
                            delete avDevice;
                            continue;
                        }
                    }

                    if ( !isFromCache && !avDevice->saveCache() ) {
                        debugOutput( DEBUG_LEVEL_VERBOSE, "No cached version of AVC model created\n" );
                    }
                    m_avDevices.push_back( avDevice );

                    if (!addElement(avDevice)) {
                        debugWarning("failed to add Device to Control::Container\n");
                    }

                    debugOutput( DEBUG_LEVEL_NORMAL, "discovery of node %d on port %d done...\n", nodeId, portService->getPort() );
                } else {
                    // we didn't get a device, hence we have to delete the configrom ptr manually
                    delete configRom;
                }
            }
        }

        debugOutput( DEBUG_LEVEL_NORMAL, "Discovery finished...\n" );
        // FIXME: do better sorting
        // sort the m_avDevices vector on their GUID
        // then assign reassign the id's to the devices
        // the side effect of this is that for the same set of attached devices,
        // a device id always corresponds to the same device
        sort(m_avDevices.begin(), m_avDevices.end(), FFADODevice::compareGUID);

        int i=0;
        if(m_deviceStringParser->countDeviceStrings()) { // only if there are devicestrings
            // first map the devices to a position using the device spec strings
            std::map<fb_octlet_t, int> positionMap;
            for ( FFADODeviceVectorIterator it = m_avDevices.begin();
                it != m_avDevices.end();
                ++it )
            {
                int pos = m_deviceStringParser->matchPosition((*it)->getConfigRom());
                fb_octlet_t guid = (*it)->getConfigRom().getGuid();
                positionMap[guid] = pos;
                debugOutput( DEBUG_LEVEL_VERBOSE, "Mapping %s to position %d...\n", (*it)->getConfigRom().getGuidString().c_str(), pos );
            }
    
            // now run over all positions, and add the devices that belong to it
            FFADODeviceVector sorted;
            int nbPositions = m_deviceStringParser->countDeviceStrings();
            for (i=0; i < nbPositions; i++) {
                for ( FFADODeviceVectorIterator it = m_avDevices.begin();
                    it != m_avDevices.end();
                    ++it )
                {
                    fb_octlet_t guid = (*it)->getConfigRom().getGuid();
                    if(positionMap[guid] == i) {
                        sorted.push_back(*it);
                    }
                }
            }
            // assign the new vector
            flushDebugOutput();
            assert(sorted.size() == m_avDevices.size());
            m_avDevices = sorted;
        }

        showDeviceInfo();

    } else { // slave mode
        // notify any clients
        signalNotifiers(m_preUpdateNotifiers);
        Ieee1394Service *portService = m_1394Services.at(0);
        fb_nodeid_t nodeId = portService->getLocalNodeId();
        debugOutput( DEBUG_LEVEL_VERBOSE, "Starting in slave mode on node %d...\n", nodeId );

        std::auto_ptr<ConfigRom> configRom =
            std::auto_ptr<ConfigRom>( new ConfigRom( *portService,
                                                     nodeId ) );
        if ( !configRom->initialize() ) {
            // \todo If a PHY on the bus is in power safe mode then
            // the config rom is missing. So this might be just
            // such this case and we can safely skip it. But it might
            // be there is a real software problem on our side.
            // This should be handled more carefuly.
            debugOutput( DEBUG_LEVEL_NORMAL,
                         "Could not read config rom from device (node id %d). "
                         "Skip device discovering for this node\n",
                         nodeId );
            return false;
        }

        // remove any already present devices
        for ( FFADODeviceVectorIterator it = m_avDevices.begin();
            it != m_avDevices.end();
            ++it )
        {
            if (!deleteElement(*it)) {
                debugWarning("failed to remove Device from Control::Container\n");
            }
            delete *it;
        }

        m_avDevices.clear();

        // get the slave driver
        FFADODevice* avDevice = getSlaveDriver( configRom );
        if ( avDevice ) {
            debugOutput( DEBUG_LEVEL_NORMAL,
                         "driver found for device %d\n",
                         nodeId );

            avDevice->setVerboseLevel( getDebugLevel() );

            if ( !avDevice->discover() ) {
                debugError( "could not discover device\n" );
                delete avDevice;
                return false;
            }

            if ( getDebugLevel() >= DEBUG_LEVEL_VERBOSE ) {
                avDevice->showDevice();
            }
            m_avDevices.push_back( avDevice );
            debugOutput( DEBUG_LEVEL_NORMAL, "discovery of node %d on port %d done...\n", nodeId, portService->getPort() );
        }

        debugOutput( DEBUG_LEVEL_NORMAL, "discovery finished...\n" );
    }

    m_DeviceListLock->Unlock();
    // notify any clients
    signalNotifiers(m_postUpdateNotifiers);
    return true;
}

bool
DeviceManager::initStreaming()
{
    // iterate over the found devices
    // add the stream processors of the devices to the managers
    for ( FFADODeviceVectorIterator it = m_avDevices.begin();
        it != m_avDevices.end();
        ++it )
    {
        FFADODevice *device = *it;
        assert(device);

        debugOutput(DEBUG_LEVEL_VERBOSE, "Locking device (%p)\n", device);

        if (!device->lock()) {
            debugWarning("Could not lock device, skipping device (%p)!\n", device);
            continue;
        }

        debugOutput(DEBUG_LEVEL_VERBOSE, "Setting samplerate to %d for (%p)\n",
                    m_processorManager->getNominalRate(), device);

        // Set the device's sampling rate to that requested
        // FIXME: does this really belong here?  If so we need to handle errors.
        if (!device->setSamplingFrequency(m_processorManager->getNominalRate())) {
            debugOutput(DEBUG_LEVEL_VERBOSE, " => Retry setting samplerate to %d for (%p)\n",
                        m_processorManager->getNominalRate(), device);

            // try again:
            if (!device->setSamplingFrequency(m_processorManager->getNominalRate())) {
                debugFatal("Could not set sampling frequency to %d\n",m_processorManager->getNominalRate());
                return false;
            }
        }
        // prepare the device
        device->prepare();
    }

    // set the sync source
    if (!m_processorManager->setSyncSource(getSyncSource())) {
        debugWarning("Could not set processorManager sync source (%p)\n",
            getSyncSource());
    }

    return true;
}

bool
DeviceManager::prepareStreaming()
{
    if (!m_processorManager->prepare()) {
        debugFatal("Could not prepare streaming...\n");
        return false;
    }
    return true;
}

bool
DeviceManager::finishStreaming() {
    bool result = true;
    // iterate over the found devices
    for ( FFADODeviceVectorIterator it = m_avDevices.begin();
        it != m_avDevices.end();
        ++it )
    {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Unlocking device (%p)\n", *it);

        if (!(*it)->unlock()) {
            debugWarning("Could not unlock device (%p)!\n", *it);
            result = false;
        }
    }
    return result;
}

bool
DeviceManager::startStreamingOnDevice(FFADODevice *device)
{
    assert(device);

    int j=0;
    bool all_streams_started = true;
    bool device_start_failed = false;

    if (device->resetForStreaming() == false)
        return false;

    for(j=0; j < device->getStreamCount(); j++) {
        debugOutput(DEBUG_LEVEL_VERBOSE,"Starting stream %d of device %p\n", j, device);
        // start the stream
        if (!device->startStreamByIndex(j)) {
            debugWarning("Could not start stream %d of device %p\n", j, device);
            all_streams_started = false;
            break;
        }
    }

    if(!all_streams_started) {
        // disable all streams that did start up correctly
        for(j = j-1; j >= 0; j--) {
            debugOutput(DEBUG_LEVEL_VERBOSE,"Stopping stream %d of device %p\n", j, device);
            // stop the stream
            if (!device->stopStreamByIndex(j)) {
                debugWarning("Could not stop stream %d of device %p\n", j, device);
            }
        }
        device_start_failed = true;
    } else {
        if (!device->enableStreaming()) {
            debugWarning("Could not enable streaming on device %p!\n", device);
            device_start_failed = true;
        }
    }
    return !device_start_failed;
}

bool
DeviceManager::startStreaming() {
    bool device_start_failed = false;
    FFADODeviceVectorIterator it;

    // create the connections for all devices
    // iterate over the found devices
    for ( it = m_avDevices.begin();
        it != m_avDevices.end();
        ++it )
    {
        if (!startStreamingOnDevice(*it)) {
            debugWarning("Could not start streaming on device %p!\n", *it);
            device_start_failed = true;
            break;
        }
    }

    // if one of the devices failed to start,
    // the previous routine should have cleaned up the failing one.
    // we still have to stop all devices that were started before this one.
    if(device_start_failed) {
        for (FFADODeviceVectorIterator it2 = m_avDevices.begin();
            it2 != it;
            ++it2 )
        {
            if (!stopStreamingOnDevice(*it2)) {
                debugWarning("Could not stop streaming on device %p!\n", *it2);
            }
        }
        return false;
    }

    // start the stream processor manager to tune in to the channels
    if(m_processorManager->start()) {
        return true;
    } else {
        debugWarning("Failed to start SPM!\n");
        for( it = m_avDevices.begin();
             it != m_avDevices.end();
             ++it )
        {
            if (!stopStreamingOnDevice(*it)) {
                debugWarning("Could not stop streaming on device %p!\n", *it);
            }
        }
        return false;
    }
}

bool
DeviceManager::resetStreaming() {
    return true;
}

bool
DeviceManager::stopStreamingOnDevice(FFADODevice *device)
{
    assert(device);
    bool result = true;

    if (!device->disableStreaming()) {
        debugWarning("Could not disable streaming on device %p!\n", device);
    }

    int j=0;
    for(j=0; j < device->getStreamCount(); j++) {
        debugOutput(DEBUG_LEVEL_VERBOSE,"Stopping stream %d of device %p\n", j, device);
        // stop the stream
        // start the stream
        if (!device->stopStreamByIndex(j)) {
            debugWarning("Could not stop stream %d of device %p\n", j, device);
            result = false;
            continue;
        }
    }
    return result;
}

bool
DeviceManager::stopStreaming()
{
    bool result = true;
    m_processorManager->stop();

    // create the connections for all devices
    // iterate over the found devices
    // add the stream processors of the devices to the managers
    for ( FFADODeviceVectorIterator it = m_avDevices.begin();
        it != m_avDevices.end();
        ++it )
    {
        stopStreamingOnDevice(*it);
    }
    return result;
}

enum DeviceManager::eWaitResult
DeviceManager::waitForPeriod() {
    if(m_processorManager->waitForPeriod()) {
        return eWR_OK;
    } else {
        if(m_processorManager->shutdownNeeded()) {
            debugWarning("Shutdown requested\n");
            return eWR_Shutdown;
        } else {
            debugWarning("XRUN detected\n");
            // do xrun recovery
            if(m_processorManager->handleXrun()) {
                return eWR_Xrun;
            } else {
                debugError("Could not handle XRUN\n");
                return eWR_Error;
            }
        }
    }
}

bool
DeviceManager::setPeriodSize(unsigned int period) {
    // Useful for cases where only the period size needs adjusting
    m_processorManager->setPeriodSize(period);
    return true;
}

bool
DeviceManager::setStreamingParams(unsigned int period, unsigned int rate, unsigned int nb_buffers) {
    m_processorManager->setPeriodSize(period);
    m_processorManager->setNominalRate(rate);
    m_processorManager->setNbBuffers(nb_buffers);
    return true;
}

FFADODevice*
DeviceManager::getDriverForDeviceDo( ConfigRom *configRom,
                                   int id, bool generic )
{
#ifdef ENABLE_BEBOB
    debugOutput( DEBUG_LEVEL_VERBOSE, "Trying BeBoB...\n" );
    if ( BeBoB::Device::probe( getConfiguration(), *configRom, generic ) ) {
        return BeBoB::Device::createDevice( *this, std::auto_ptr<ConfigRom>( configRom ) );
    }
#endif

#ifdef ENABLE_FIREWORKS
    debugOutput( DEBUG_LEVEL_VERBOSE, "Trying ECHO Audio FireWorks...\n" );
    if ( FireWorks::Device::probe( getConfiguration(), *configRom, generic ) ) {
        return FireWorks::Device::createDevice( *this, std::auto_ptr<ConfigRom>( configRom ) );
    }
#endif

#ifdef ENABLE_OXFORD
    debugOutput( DEBUG_LEVEL_VERBOSE, "Trying Oxford FW90x...\n" );
    if ( Oxford::Device::probe( getConfiguration(), *configRom, generic ) ) {
        return Oxford::Device::createDevice( *this, std::auto_ptr<ConfigRom>( configRom ) );
    }
#endif

#ifdef ENABLE_MAUDIO
    debugOutput( DEBUG_LEVEL_VERBOSE, "Trying M-Audio...\n" );
    if ( MAudio::Device::probe( getConfiguration(), *configRom, generic ) ) {
        return MAudio::Device::createDevice( *this, std::auto_ptr<ConfigRom>( configRom ) );
    }
#endif

// we want to try the non-generic AV/C platforms before trying the generic ones
#ifdef ENABLE_GENERICAVC
    debugOutput( DEBUG_LEVEL_VERBOSE, "Trying Generic AV/C...\n" );
    if ( GenericAVC::Device::probe( getConfiguration(), *configRom, generic ) ) {
        return GenericAVC::Device::createDevice( *this, std::auto_ptr<ConfigRom>( configRom ) );
    }
#endif

#ifdef ENABLE_MOTU
    debugOutput( DEBUG_LEVEL_VERBOSE, "Trying Motu...\n" );
    if ( Motu::MotuDevice::probe( getConfiguration(), *configRom, generic ) ) {
        return Motu::MotuDevice::createDevice( *this, std::auto_ptr<ConfigRom>( configRom ) );
    }
#endif

#ifdef ENABLE_DICE
    debugOutput( DEBUG_LEVEL_VERBOSE, "Trying Dice...\n" );
    if ( Dice::Device::probe( getConfiguration(), *configRom, generic ) ) {
        return Dice::Device::createDevice( *this, std::auto_ptr<ConfigRom>( configRom ) );
    }
#endif

#ifdef ENABLE_METRIC_HALO
    debugOutput( DEBUG_LEVEL_VERBOSE, "Trying Metric Halo...\n" );
    if ( MetricHalo::Device::probe( getConfiguration(), *configRom, generic ) ) {
        return MetricHalo::Device::createDevice( *this, std::auto_ptr<ConfigRom>( configRom ) );
    }
#endif

#ifdef ENABLE_RME
    debugOutput( DEBUG_LEVEL_VERBOSE, "Trying RME...\n" );
    if ( Rme::Device::probe( getConfiguration(), *configRom, generic ) ) {
        return Rme::Device::createDevice( *this, std::auto_ptr<ConfigRom>( configRom ) );
    }
#endif

#ifdef ENABLE_BOUNCE
    debugOutput( DEBUG_LEVEL_VERBOSE, "Trying Bounce...\n" );
    if ( Bounce::Device::probe( getConfiguration(), *configRom, generic ) ) {
        return Bounce::Device::createDevice( *this, std::auto_ptr<ConfigRom>( configRom ) );
    }
#endif

    return NULL;
}

FFADODevice*
DeviceManager::getDriverForDevice( ConfigRom *configRom,
                                   int id )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Probing for supported device...\n" );
    FFADODevice* dev = getDriverForDeviceDo(configRom, id, false);
    if(dev) {
        debugOutput( DEBUG_LEVEL_VERBOSE, " found supported device...\n" );
        dev->setVerboseLevel(getDebugLevel());
        return dev;
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, " no supported device found, trying generic support...\n" );
    dev = getDriverForDeviceDo(configRom, id, true);
    if(dev) {
        debugOutput( DEBUG_LEVEL_VERBOSE, " found generic support for device...\n" );
        dev->setVerboseLevel(getDebugLevel());
        return dev;
    }
    debugOutput( DEBUG_LEVEL_VERBOSE, " device not supported...\n" );
    return NULL;
}

FFADODevice*
DeviceManager::getSlaveDriver( std::auto_ptr<ConfigRom>( configRom ) )
{
#ifdef ENABLE_BOUNCE
    if ( Bounce::SlaveDevice::probe( getConfiguration(), *configRom, false ) ) {
        return Bounce::SlaveDevice::createDevice(  *this, std::auto_ptr<ConfigRom>( configRom ) );
    }
#endif
    return NULL;
}

bool
DeviceManager::isValidNode(int node)
{
    for ( FFADODeviceVectorIterator it = m_avDevices.begin();
          it != m_avDevices.end();
          ++it )
    {
        FFADODevice* avDevice = *it;

        if (avDevice->getConfigRom().getNodeId() == node) {
            return true;
    }
    }
    return false;
}

int
DeviceManager::getNbDevices()
{
    return m_avDevices.size();
}

int
DeviceManager::getDeviceNodeId( int deviceNr )
{
    if ( ! ( deviceNr < getNbDevices() ) ) {
        debugError( "Device number out of range (%d)\n", deviceNr );
        return -1;
    }

    FFADODevice* avDevice = m_avDevices.at( deviceNr );

    if ( !avDevice ) {
        debugError( "Could not get device at position (%d)\n",  deviceNr );
    }

    return avDevice->getConfigRom().getNodeId();
}

FFADODevice*
DeviceManager::getAvDevice( int nodeId )
{
    for ( FFADODeviceVectorIterator it = m_avDevices.begin();
          it != m_avDevices.end();
          ++it )
    {
        FFADODevice* avDevice = *it;
        if ( avDevice->getConfigRom().getNodeId() == nodeId ) {
            return avDevice;
        }
    }

    return 0;
}

FFADODevice*
DeviceManager::getAvDeviceByIndex( int idx )
{
    return m_avDevices.at(idx);
}

unsigned int
DeviceManager::getAvDeviceCount( )
{
    return m_avDevices.size();
}

/**
 * Return the streamprocessor that is to be used as
 * the sync source.
 *
 * Algorithm still to be determined
 *
 * @return StreamProcessor that is sync source
 */
Streaming::StreamProcessor *
DeviceManager::getSyncSource() {
    FFADODevice* device = getAvDeviceByIndex(0);

    bool slaveMode=false;
    if(!getOption("slaveMode", slaveMode)) {
        debugOutput(DEBUG_LEVEL_NORMAL, 
                    "Could not retrieve slaveMode parameter, defauling to false\n");
    }
    return device->getStreamProcessorByIndex(0);
/*
    #warning TEST CODE FOR BOUNCE DEVICE !!
    // this makes the bounce slave use the xmit SP as sync source
    if (slaveMode) {
        return device->getStreamProcessorByIndex(1);
    } else {
        return device->getStreamProcessorByIndex(0);
    }*/
}

bool
DeviceManager::deinitialize()
{
    return true;
}

void
DeviceManager::setVerboseLevel(int l)
{
    setDebugLevel(l);
    Control::Element::setVerboseLevel(l);
    m_processorManager->setVerboseLevel(l);
    m_deviceStringParser->setVerboseLevel(l);
    m_configuration->setVerboseLevel(l);
    for ( FFADODeviceVectorIterator it = m_avDevices.begin();
          it != m_avDevices.end();
          ++it )
    {
        (*it)->setVerboseLevel(l);
    }
    for ( Ieee1394ServiceVectorIterator it = m_1394Services.begin();
          it != m_1394Services.end();
          ++it )
    {
        (*it)->setVerboseLevel(l);
    }
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting verbose level to %d...\n", l );
}

void
DeviceManager::showDeviceInfo() {
    debugOutput(DEBUG_LEVEL_NORMAL, "===== Device Manager =====\n");
    Control::Element::show();

    int i=0;
    for ( Ieee1394ServiceVectorIterator it = m_1394Services.begin();
          it != m_1394Services.end();
          ++it )
    {
        debugOutput(DEBUG_LEVEL_NORMAL, "--- IEEE1394 Service %2d ---\n", i++);
        (*it)->show();
    }

    i=0;
    for ( FFADODeviceVectorIterator it = m_avDevices.begin();
        it != m_avDevices.end();
        ++it )
    {
        FFADODevice* avDevice = *it;
        debugOutput(DEBUG_LEVEL_NORMAL, "--- Device %2d ---\n", i++);
        avDevice->showDevice();

        debugOutput(DEBUG_LEVEL_NORMAL, "Clock sync sources:\n");
        FFADODevice::ClockSourceVector sources=avDevice->getSupportedClockSources();
        for ( FFADODevice::ClockSourceVector::const_iterator it
                = sources.begin();
            it != sources.end();
            ++it )
        {
            FFADODevice::ClockSource c=*it;
            debugOutput(DEBUG_LEVEL_NORMAL, " Type: %s, Id: %2d, Valid: %1d, Active: %1d, Locked %1d, Slipping: %1d, Description: %s\n",
                FFADODevice::ClockSourceTypeToString(c.type), c.id, c.valid, c.active, c.locked, c.slipping, c.description.c_str());
        }
    }
}
void
DeviceManager::showStreamingInfo() {
    m_processorManager->dumpInfo();
}
