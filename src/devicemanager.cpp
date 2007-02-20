 /* devicemanager.cpp
 * Copyright (C) 2005,06,07 by Daniel Wagner
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

#include "fbtypes.h"

#include "devicemanager.h"
#include "iavdevice.h"
#include "configrom.h"

#include "libfreebobavc/ieee1394service.h"
#include "debugmodule/debugmodule.h"

#include <iostream>
#include <sstream>

#include <unistd.h>

#include "libstreaming/StreamProcessor.h"

#ifdef ENABLE_BEBOB
    #include "bebob/bebob_avdevice.h"
    #include "maudio/maudio_avdevice.h"
#endif

#ifdef ENABLE_BOUNCE
    #include "bounce/bounce_avdevice.h"
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

using namespace std;

IMPL_DEBUG_MODULE( DeviceManager, DeviceManager, DEBUG_LEVEL_NORMAL );

DeviceManager::DeviceManager()
    : m_1394Service( 0 )
{

}

DeviceManager::~DeviceManager()
{
    for ( IAvDeviceVectorIterator it = m_avDevices.begin();
          it != m_avDevices.end();
          ++it )
    {
        delete *it;
    }

    delete m_1394Service;
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

    return true;
}

bool
DeviceManager::discover( int verboseLevel )
{

    setDebugLevel( verboseLevel );
    m_1394Service->setVerbose( verboseLevel );

    for ( IAvDeviceVectorIterator it = m_avDevices.begin();
          it != m_avDevices.end();
          ++it )
    {
        delete *it;
    }
    m_avDevices.clear();

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
            // This should be handled more carefuly.
            debugOutput( DEBUG_LEVEL_NORMAL,
                         "Could not read config rom from device (node id %d). "
                         "Skip device discovering for this node\n",
                         nodeId );
            continue;
        }

        IAvDevice* avDevice = getDriverForDevice( configRom,
                                                  nodeId,
                                                  verboseLevel );
        if ( avDevice ) {
            debugOutput( DEBUG_LEVEL_NORMAL,
                         "discover: driver found for device %d\n",
                         nodeId );

            if ( !avDevice->discover() ) {
                debugError( "discover: could not discover device\n" );
                delete avDevice;
                continue;
            }

            if ( !avDevice->setId( m_avDevices.size() ) ) {
                debugError( "setting Id failed\n" );
            }
            if ( verboseLevel ) {
                avDevice->showDevice();
            }

            m_avDevices.push_back( avDevice );
        }
    }

    return true;
}


IAvDevice*
DeviceManager::getDriverForDevice( std::auto_ptr<ConfigRom>( configRom ),
                                   int id,  int level )
{
#ifdef ENABLE_BEBOB
    if ( BeBoB::AvDevice::probe( *configRom.get() ) ) {
        return new BeBoB::AvDevice( configRom, *m_1394Service, id, level );
    }
#endif

#ifdef ENABLE_BEBOB
    if ( MAudio::AvDevice::probe( *configRom.get() ) ) {
        return new MAudio::AvDevice( configRom, *m_1394Service, id, level );
    }
#endif

#ifdef ENABLE_MOTU
    if ( Motu::MotuDevice::probe( *configRom.get() ) ) {
        return new Motu::MotuDevice( configRom, *m_1394Service, id, level );
    }
#endif

#ifdef ENABLE_DICE
    if ( Dice::DiceAvDevice::probe( *configRom.get() ) ) {
        return new Dice::DiceAvDevice( configRom, *m_1394Service, id, level );
    }
#endif

#ifdef ENABLE_METRIC_HALO
    if ( MetricHalo::MHAvDevice::probe( *configRom.get() ) ) {
        return new MetricHalo::MHAvDevice( configRom, *m_1394Service, id, level );
    }
#endif

#ifdef ENABLE_RME
    if ( Rme::RmeDevice::probe( *configRom.get() ) ) {
        return new Rme::RmeDevice( configRom, *m_1394Service, id, level );
    }
#endif

#ifdef ENABLE_BOUNCE
    if ( Bounce::BounceDevice::probe( *configRom.get() ) ) {
        return new Bounce::BounceDevice( configRom, *m_1394Service, id, level );
    }
#endif

    return 0;
}

bool
DeviceManager::isValidNode(int node)
{
    for ( IAvDeviceVectorIterator it = m_avDevices.begin();
          it != m_avDevices.end();
          ++it )
    {
        IAvDevice* avDevice = *it;

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

    IAvDevice* avDevice = m_avDevices.at( deviceNr );

    if ( !avDevice ) {
        debugError( "Could not get device at position (%d)\n",  deviceNr );
    }

    return avDevice->getConfigRom().getNodeId();
}

IAvDevice*
DeviceManager::getAvDevice( int nodeId )
{
    for ( IAvDeviceVectorIterator it = m_avDevices.begin();
          it != m_avDevices.end();
          ++it )
    {
        IAvDevice* avDevice = *it;
        if ( avDevice->getConfigRom().getNodeId() == nodeId ) {
            return avDevice;
        }
    }

    return 0;
}

IAvDevice*
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
FreebobStreaming::StreamProcessor *
DeviceManager::getSyncSource() {
    IAvDevice* device = getAvDeviceByIndex(0);
    
    #warning TEST CODE FOR BOUNCE DEVICE !!
    if (device->getConfigRom().getNodeId()==0) {
        return device->getStreamProcessorByIndex(0);
    } else {
        return device->getStreamProcessorByIndex(1);
    }
    
}

xmlDocPtr
DeviceManager::getXmlDescription()
{
    xmlDocPtr doc = xmlNewDoc( BAD_CAST "1.0" );
    if ( !doc ) {
        debugError( "Couldn't create new xml doc\n" );
        return 0;
    }

    xmlNodePtr rootNode = xmlNewNode( 0,  BAD_CAST "FreeBoBConnectionInfo" );
    if ( !rootNode ) {
        debugError( "Couldn't create root node\n" );
        xmlFreeDoc( doc );
        xmlCleanupParser();
        return 0;
    }
    xmlDocSetRootElement( doc, rootNode );

    for ( IAvDeviceVectorIterator it = m_avDevices.begin();
          it != m_avDevices.end();
          ++it )
    {
        IAvDevice* avDevice = *it;

        xmlNodePtr deviceNode = xmlNewChild( rootNode, 0,
                                             BAD_CAST "Device", 0 );
        if ( !deviceNode ) {
            debugError( "Couldn't create device node\n" );
            xmlFreeDoc( doc );
            xmlCleanupParser();
            return 0;
        }

        char* result;
        asprintf( &result, "%d", avDevice->getConfigRom().getNodeId() );
        if ( !xmlNewChild( deviceNode,  0,
                           BAD_CAST "NodeId",  BAD_CAST result ) )
        {
            debugError( "Couldn't create 'NodeId' node" );
            free(result);
            return 0;
        }
        free( result );

        std::string res = "Connection Information for "
                          + avDevice->getConfigRom().getVendorName()
                          +", "
                          + avDevice->getConfigRom().getModelName()
                          + " configuration";
        if ( !xmlNewChild( deviceNode,
                           0,
                           BAD_CAST "Comment",
                           BAD_CAST res.c_str() ) ) {
            debugError( "Couldn't create comment node\n" );
            xmlFreeDoc( doc );
            xmlCleanupParser();
            free(result);
            return 0;
        }

        res = avDevice->getConfigRom().getVendorName();

        if ( !xmlNewChild( deviceNode,
                           0,
                           BAD_CAST "Vendor",
                           BAD_CAST res.c_str() ) ) {
            debugError( "Couldn't create vendor node\n" );
            xmlFreeDoc( doc );
            xmlCleanupParser();
            free(result);
            return 0;
        }

        res = avDevice->getConfigRom().getModelName();

        if ( !xmlNewChild( deviceNode,
                           0,
                           BAD_CAST "Model",
                           BAD_CAST res.c_str() ) ) {
            debugError( "Couldn't create model node\n" );
            xmlFreeDoc( doc );
            xmlCleanupParser();
            free(result);
            return 0;
        }

        asprintf( &result, "%08x%08x",
                  ( quadlet_t )( avDevice->getConfigRom().getGuid() >> 32 ),
                  ( quadlet_t )( avDevice->getConfigRom().getGuid() & 0xfffffff ) );
        if ( !xmlNewChild( deviceNode,  0,
                           BAD_CAST "GUID",  BAD_CAST result ) ) {
            debugError( "Couldn't create 'GUID' node\n" );
            xmlFreeDoc( doc );
            xmlCleanupParser();

            free(result);
            return 0;
        }
        free( result );


        if ( !avDevice->addXmlDescription( deviceNode ) ) {
            debugError( "Adding XML description failed\n" );
            xmlFreeDoc( doc );
            xmlCleanupParser();
            free(result);
            return 0;
        }

    }

    return doc;
}

bool
DeviceManager::deinitialize()
{
    return true;
}

bool
DeviceManager::saveCache( Glib::ustring fileName )
{
    int i;
    i=0; // avoids unused warning
    
    for ( IAvDeviceVectorIterator it = m_avDevices.begin();
          it != m_avDevices.end();
          ++it )
    {
        IAvDevice* pAvDevice;
        pAvDevice = *it; // avoids unused warning
        
        #ifdef ENABLE_BEBOB
        BeBoB::AvDevice* pBeBoBDevice = reinterpret_cast< BeBoB::AvDevice* >( pAvDevice );
        if ( pBeBoBDevice ) {
            // Let's use a very simple path to find the devices later.
            // Since under hood there will an xml node name this should
            // adhere the needed naming rules. Of course we could give it
            // a good node name and add an attribute but that would mean
            // we have to expose a bigger interface from IOSerialize/IODesialize,
            // which is just not needed. KISS.
            ostringstream strstrm;
            strstrm << "id" << i;
            Glib::ustring basePath = "BeBoB/" + strstrm.str() + "/";

            Util::XMLSerialize ser( fileName );
            pBeBoBDevice->serialize( basePath, ser );

            i++;
            std::cout << "a bebob device serialized" << std::endl;
        }
        #endif
    }
    return true;
}

bool
DeviceManager::loadCache( Glib::ustring fileName )
{
    int i;
    i=0; // avoids unused warning
    Util::XMLDeserialize deser( fileName );

    typedef std::vector<ConfigRom*> ConfigRomVector;
    ConfigRomVector configRoms;

    for ( fb_nodeid_t nodeId = 0;
          nodeId < m_1394Service->getNodeCount();
          ++nodeId )
    {
        ConfigRom* pConfigRom  =  new ConfigRom( *m_1394Service, nodeId );
        if ( !pConfigRom->initialize() ) {
            // \todo If a PHY on the bus is in power safe mode then
            // the config rom is missing. So this might be just
            // such this case and we can safely skip it. But it might
            // be there is a real software problem on our side.
            // This should be handled more carefuly.
            debugOutput( DEBUG_LEVEL_NORMAL,
                         "Could not read config rom from device (node id %d). "
                         "Skip device discovering for this node\n",
                         nodeId );
            delete pConfigRom;
            continue;
        }
        configRoms.push_back( pConfigRom );
    }

    #ifdef ENABLE_BEBOB
    BeBoB::AvDevice* pBeBoBDevice = 0;
    do {
        ostringstream strstrm;
        strstrm << "id" << i;
        pBeBoBDevice = BeBoB::AvDevice::deserialize(
            "BeBoB/" + strstrm.str() + "/",
            deser,
            *m_1394Service );

        ++i;
        if ( pBeBoBDevice ) {
            for (ConfigRomVector::iterator it = configRoms.begin();
                 it != configRoms.end();
                 ++it )
            {
                ConfigRom* pConfigRom = *it;

                if ( pBeBoBDevice->getConfigRom() == *pConfigRom ) {
                    pBeBoBDevice->getConfigRom().setNodeId( pConfigRom->getNodeId() );
                    // m_avDevices.push_back( pBeBoBDevice );

                }
            }
        }
    } while ( pBeBoBDevice );
    #endif
    
    return true;
}
