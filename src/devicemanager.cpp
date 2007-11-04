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

#include "fbtypes.h"

#include "devicemanager.h"
#include "ffadodevice.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "libstreaming/StreamProcessor.h"

#include "debugmodule/debugmodule.h"

#ifdef ENABLE_BEBOB
#include "bebob/bebob_avdevice.h"
#include "maudio/maudio_avdevice.h"
#endif

#ifdef ENABLE_GENERICAVC
    #include "genericavc/avc_avdevice.h"
#endif

#ifdef ENABLE_FIREWORKS
    #include "fireworks/fireworks_device.h"
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
    : Control::Container("devicemanager")
    , m_1394Service( 0 )
{
    addOption(Util::OptionContainer::Option("slaveMode",false));
    addOption(Util::OptionContainer::Option("snoopMode",false));
}

DeviceManager::~DeviceManager()
{
    for ( FFADODeviceVectorIterator it = m_avDevices.begin();
          it != m_avDevices.end();
          ++it )
    {
        if (!deleteElement(*it)) {
            debugWarning("failed to remove AvDevice from Control::Container\n");
        }
        delete *it;
    }

    delete m_1394Service;
}

void
DeviceManager::setVerboseLevel(int l)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting verbose level to %d...\n", l );
    setDebugLevel(l);

    if (m_1394Service) m_1394Service->setVerboseLevel(l);
    Control::Element::setVerboseLevel(l);

    for ( FFADODeviceVectorIterator it = m_avDevices.begin();
          it != m_avDevices.end();
          ++it )
    {
        (*it)->setVerboseLevel(l);
    }
}

void
DeviceManager::show() {
    debugOutput(DEBUG_LEVEL_NORMAL, "===== Device Manager =====\n");
    Control::Element::show();
    
    if (m_1394Service) debugOutput(DEBUG_LEVEL_NORMAL, "1394 port: %d\n", m_1394Service->getPort());

    int i=0;
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
            debugOutput(DEBUG_LEVEL_NORMAL, " Type: %s, Id: %d, Valid: %d, Active: %d, Description: %s\n",
                FFADODevice::ClockSourceTypeToString(c.type), c.id, c.valid, c.active, c.description.c_str());
        }

    }
}

bool
DeviceManager::initialize( int port )
{
    m_1394Service = new Ieee1394Service();
    if ( !m_1394Service ) {
        debugFatal( "Could not create Ieee1349Service object\n" );
        return false;
    }

    if ( !m_1394Service->initialize( port ) ) {
        debugFatal( "Could not initialize Ieee1349Service object\n" );
        delete m_1394Service;
        m_1394Service = 0;
        return false;
    }

    // add the bus reset handler
    m_busreset_functor = new MemberFunctor0< DeviceManager*,
                void (DeviceManager::*)() >
                ( this, &DeviceManager::busresetHandler, false );
    m_1394Service->addBusResetHandler( m_busreset_functor );

    setVerboseLevel(getDebugLevel());
    return true;
}

void
DeviceManager::busresetHandler()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Bus reset...\n" );

    // propagate the bus reset to all avDevices
    for ( FFADODeviceVectorIterator it = m_avDevices.begin();
          it != m_avDevices.end();
          ++it )
    {
        (*it)->handleBusReset();
    }
}

bool
DeviceManager::discover( )
{
    bool slaveMode=false;
    if(!getOption("slaveMode", slaveMode)) {
        debugWarning("Could not retrieve slaveMode parameter, defauling to false\n");
    }
    bool snoopMode=false;
    if(!getOption("snoopMode", snoopMode)) {
        debugWarning("Could not retrieve snoopMode parameter, defauling to false\n");
    }

    setVerboseLevel(getDebugLevel());

    for ( FFADODeviceVectorIterator it = m_avDevices.begin();
          it != m_avDevices.end();
          ++it )
    {
        if (!deleteElement(*it)) {
            debugWarning("failed to remove AvDevice from Control::Container\n");
        }
        delete *it;
    }
    m_avDevices.clear();

    if (!slaveMode) {
        for ( fb_nodeid_t nodeId = 0;
              nodeId < m_1394Service->getNodeCount();
              ++nodeId )
        {
            debugOutput( DEBUG_LEVEL_VERBOSE, "Probing node %d...\n", nodeId );

            if (nodeId == m_1394Service->getLocalNodeId()) {
                debugOutput( DEBUG_LEVEL_VERBOSE, "Skipping local node (%d)...\n", nodeId );
                continue;
            }

            std::auto_ptr<ConfigRom> configRom =
                std::auto_ptr<ConfigRom>( new ConfigRom( *m_1394Service,
                                                         nodeId ) );
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

            FFADODevice* avDevice = getDriverForDevice( configRom,
                                                        nodeId );

            if ( avDevice ) {
                debugOutput( DEBUG_LEVEL_NORMAL,
                             "driver found for device %d\n",
                             nodeId );

                avDevice->setVerboseLevel( getDebugLevel() );
                bool isFromCache = false;
                if ( avDevice->loadFromCache() ) {
                    debugOutput( DEBUG_LEVEL_VERBOSE, "could load from cache\n" );
                    isFromCache = true;
                } else if ( avDevice->discover() ) {
                    debugOutput( DEBUG_LEVEL_VERBOSE, "discovering successful\n" );
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
                    debugWarning("failed to add AvDevice to Control::Container\n");
                }
                
                debugOutput( DEBUG_LEVEL_NORMAL, "discovery of node %d done...\n", nodeId );

            }
        }

        debugOutput( DEBUG_LEVEL_NORMAL, "Discovery finished...\n" );

        // sort the m_avDevices vector on their GUID
        // then assign reassign the id's to the devices
        // the side effect of this is that for the same set of attached devices,
        // a device id always corresponds to the same device
        sort(m_avDevices.begin(), m_avDevices.end(), FFADODevice::compareGUID);
        
        int i=0;
        for ( FFADODeviceVectorIterator it = m_avDevices.begin();
            it != m_avDevices.end();
            ++it )
        {
            if ( !(*it)->setId( i++ ) ) {
                debugError( "setting Id failed\n" );
            }
        }

        show();
        return true;

    } else { // slave mode
        fb_nodeid_t nodeId = m_1394Service->getLocalNodeId();
        debugOutput( DEBUG_LEVEL_VERBOSE, "Starting in slave mode on node %d...\n", nodeId );

        std::auto_ptr<ConfigRom> configRom =
            std::auto_ptr<ConfigRom>( new ConfigRom( *m_1394Service,
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

            if ( !avDevice->setId( m_avDevices.size() ) ) {
                debugError( "setting Id failed\n" );
            }
            if ( getDebugLevel() >= DEBUG_LEVEL_VERBOSE ) {
                avDevice->showDevice();
            }

            m_avDevices.push_back( avDevice );

            debugOutput( DEBUG_LEVEL_NORMAL, "discovery of node %d done...\n", nodeId );
        }

        debugOutput( DEBUG_LEVEL_NORMAL, "discovery finished...\n" );

        return true;
    }
}

FFADODevice*
DeviceManager::getDriverForDevice( std::auto_ptr<ConfigRom>( configRom ),
                                   int id )
{
#ifdef ENABLE_BEBOB
    debugOutput( DEBUG_LEVEL_VERBOSE, "Trying BeBoB...\n" );
    if ( BeBoB::AvDevice::probe( *configRom.get() ) ) {
        return BeBoB::AvDevice::createDevice( *m_1394Service, configRom );
    }
#endif

#ifdef ENABLE_GENERICAVC
    debugOutput( DEBUG_LEVEL_VERBOSE, "Trying Generic AV/C...\n" );
    if ( GenericAVC::AvDevice::probe( *configRom.get() ) ) {
        return GenericAVC::AvDevice::createDevice( *m_1394Service, configRom );
    }
#endif

#ifdef ENABLE_FIREWORKS
    debugOutput( DEBUG_LEVEL_VERBOSE, "Trying ECHO Audio FireWorks...\n" );
    if ( FireWorks::Device::probe( *configRom.get() ) ) {
        return FireWorks::Device::createDevice( *m_1394Service, configRom );
    }
#endif

#ifdef ENABLE_BEBOB
    debugOutput( DEBUG_LEVEL_VERBOSE, "Trying M-Audio...\n" );
    if ( MAudio::AvDevice::probe( *configRom.get() ) ) {
        return MAudio::AvDevice::createDevice( *m_1394Service, configRom );
    }
#endif

#ifdef ENABLE_MOTU
    debugOutput( DEBUG_LEVEL_VERBOSE, "Trying Motu...\n" );
    if ( Motu::MotuDevice::probe( *configRom.get() ) ) {
        return Motu::MotuDevice::createDevice( *m_1394Service, configRom );
    }
#endif

#ifdef ENABLE_DICE
    debugOutput( DEBUG_LEVEL_VERBOSE, "Trying Dice...\n" );
    if ( Dice::DiceAvDevice::probe( *configRom.get() ) ) {
        return Dice::DiceAvDevice::createDevice( *m_1394Service, configRom );
    }
#endif

#ifdef ENABLE_METRIC_HALO
    debugOutput( DEBUG_LEVEL_VERBOSE, "Trying Metric Halo...\n" );
    if ( MetricHalo::MHAvDevice::probe( *configRom.get() ) ) {
        return MetricHalo::MHAvDevice::createDevice( *m_1394Service, configRom );
    }
#endif

#ifdef ENABLE_RME
    debugOutput( DEBUG_LEVEL_VERBOSE, "Trying RME...\n" );
    if ( Rme::RmeDevice::probe( *configRom.get() ) ) {
        return Rme::RmeDevice::createDevice( *m_1394Service, configRom );
    }
#endif

#ifdef ENABLE_BOUNCE
    debugOutput( DEBUG_LEVEL_VERBOSE, "Trying Bounce...\n" );
    if ( Bounce::BounceDevice::probe( *configRom.get() ) ) {
        return Bounce::BounceDevice::createDevice( *m_1394Service, configRom );
    }
#endif

    return 0;
}

FFADODevice*
DeviceManager::getSlaveDriver( std::auto_ptr<ConfigRom>( configRom ) )
{

#ifdef ENABLE_BOUNCE
    if ( Bounce::BounceSlaveDevice::probe( *configRom.get() ) ) {
        return Bounce::BounceSlaveDevice::createDevice( *m_1394Service, configRom );
    }
#endif

    return 0;
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
        debugWarning("Could not retrieve slaveMode parameter, defauling to false\n");
    }

    #warning TEST CODE FOR BOUNCE DEVICE !!
    // this makes the bounce slave use the xmit SP as sync source
    if (slaveMode) {
        return device->getStreamProcessorByIndex(1);
    } else {
        return device->getStreamProcessorByIndex(0);
    }
}

bool
DeviceManager::deinitialize()
{
    return true;
}
