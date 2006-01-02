/* devicemanager.cpp
 * Copyright (C) 2005 by Daniel Wagner
 *
 * This file is part of FreeBob.
 *
 * FreeBob is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBob is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBob; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */

#include "fbtypes.h"

#include "devicemanager.h"
#include "avdevice.h"
#include "configrom.h"

#include "libfreebobavc/ieee1394service.h"
#include "debugmodule/debugmodule.h"

#include <iostream>

using namespace std;

IMPL_DEBUG_MODULE( DeviceManager, DeviceManager, DEBUG_LEVEL_VERBOSE );

DeviceManager::DeviceManager()
    : m_1394Service( 0 )
{
}

DeviceManager::~DeviceManager()
{
    for ( AvDeviceVectorIterator it = m_avDevices.begin();
          it != m_avDevices.end();
          ++it )
    {
        delete *it;
    }

    delete m_1394Service;
}

bool
DeviceManager::initialize(int port)
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
DeviceManager::discover()
{
    for ( AvDeviceVectorIterator it = m_avDevices.begin();
          it != m_avDevices.end();
          ++it )
    {
        delete *it;
    }

    for ( fb_nodeid_t nodeId = 0;
          nodeId < m_1394Service->getNodeCount();
          ++nodeId )
    {
        ConfigRom* configRom = new ConfigRom( m_1394Service, nodeId );
        if ( !configRom->initialize() ) {
            cerr << "Could not read config rom from device (Node ID "
                 << nodeId << ")" << endl;
            delete configRom;
            return false;
        }

        if ( !configRom->isAvcDevice() ) {
            delete configRom;
            continue;
        }

        AvDevice* avDevice = new AvDevice( m_1394Service, configRom, nodeId );
        if ( !avDevice ) {
            cerr << "Could not allocate AvDevice " << endl;
            delete configRom;
            return false;
        }

        if ( !avDevice->discover() ) {
            cerr << "Could not discover device (Node ID "
                 << nodeId << ")" << endl;
            delete avDevice;
            return false;
        }

        m_avDevices.push_back( avDevice );
    }

    return true;
}

bool DeviceManager::isValidNode(int node) {
    for ( AvDeviceVectorIterator it = m_avDevices.begin();
          it != m_avDevices.end();
          ++it )
    {
        AvDevice* avDevice = *it;
        
        if (avDevice->getNodeId() == node) {
        	return true;
        }
	}
	return false;
}

int DeviceManager::getNbDevices() {
	return m_avDevices.size();
}

int DeviceManager::getDeviceNodeId(int device_nr) {
	if ( ! (device_nr < getNbDevices()) ) {
		cerr << "Device number out of range ( "
				<< device_nr << ")" << endl;
		return -1;
	}
	
	AvDevice* avDevice = m_avDevices.at(device_nr);
	
	if ( !avDevice ) {
		cerr << "Could not get device at position ( "
				<< device_nr << ")" << endl;
		return -1;
	
	}
	
	return avDevice->getNodeId();
	
}

bool DeviceManager::setNodeSampleFrequency(int node, int samplerate) {
    for ( AvDeviceVectorIterator it = m_avDevices.begin();
          it != m_avDevices.end();
          ++it )
    {
        AvDevice* avDevice = *it;
        
        if (avDevice->getNodeId() == node) {
			return avDevice->setSampleFrequency(samplerate);
        }
	}
	return false;
}

xmlDocPtr DeviceManager::getXmlDescription()
{
    xmlDocPtr doc = xmlNewDoc( BAD_CAST "1.0" );
    if ( !doc ) {
        debugError( "Couldn't create new xml doc\n" );
        return 0;
    }

    xmlNodePtr rootNode = xmlNewNode( 0,  BAD_CAST "FreeBobConnectionInfo" );
    if ( !rootNode ) {
        debugError( "Couldn't create root node\n" );
        xmlFreeDoc( doc );
        xmlCleanupParser();
        return 0;
    }
    xmlDocSetRootElement( doc, rootNode );

    for ( AvDeviceVectorIterator it = m_avDevices.begin();
          it != m_avDevices.end();
          ++it )
    {
        AvDevice* avDevice = *it;

        xmlNodePtr deviceNode = xmlNewChild( rootNode, 0,
                                             BAD_CAST "Device", 0 );
        if ( !deviceNode ) {
            debugError( "Couldn't create device node\n" );
            xmlFreeDoc( doc );
            xmlCleanupParser();
            return 0;
        }

        char* result;
        asprintf( &result, "%d", avDevice->getNodeId() );
        if ( !xmlNewChild( deviceNode,  0,
                           BAD_CAST "NodeId",  BAD_CAST result ) )
        {
            debugError( "Couldn't create 'NodeId' node" );
            return false;
        }

        std::string res = "Connection Information for "
                          + avDevice->getVendorName()
                          +", "
                          + avDevice->getModelName()
                          + " configuration";
        if ( !xmlNewChild( deviceNode,
                           0,
                           BAD_CAST "Comment",
                           BAD_CAST res.c_str() ) ) {
            debugError( "Couldn't create comment node\n" );
            xmlFreeDoc( doc );
            xmlCleanupParser();
            return 0;
        }
        
        res = avDevice->getVendorName();
        
        if ( !xmlNewChild( deviceNode,
                           0,
                           BAD_CAST "Vendor",
                           BAD_CAST res.c_str() ) ) {
            debugError( "Couldn't create vendor node\n" );
            xmlFreeDoc( doc );
            xmlCleanupParser();
            return 0;
        }
        
        res = avDevice->getModelName();
        
        if ( !xmlNewChild( deviceNode,
                           0,
                           BAD_CAST "Model",
                           BAD_CAST res.c_str() ) ) {
            debugError( "Couldn't create model node\n" );
            xmlFreeDoc( doc );
            xmlCleanupParser();
            return 0;
        }
        
        res = "";
        res = avDevice->getGuid();
        
        if ( !xmlNewChild( deviceNode,
                           0,
                           BAD_CAST "GUID",
                           BAD_CAST res.c_str() ) ) {
            debugError( "Couldn't create GUID node\n" );
            xmlFreeDoc( doc );
            xmlCleanupParser();
            return 0;
        }

        if ( !avDevice->addXmlDescription( deviceNode ) ) {
            debugError( "Adding XML description failed\n" );
            xmlFreeDoc( doc );
            xmlCleanupParser();
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

