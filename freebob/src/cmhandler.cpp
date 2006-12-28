/* cmhandler.cpp
 * Copyright (C) 2004 by Daniel Wagner
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

#include "cmhandler.h"
#include "ieee1394service.h"
#include "avdevice.h"
#include "ipchandler.h"
#include "avdevicepool.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

CMHandler* CMHandler::m_pInstance = 0;

CMHandler::CMHandler()
    : m_bInitialised( false )
{
    setDebugLevel( DEBUG_LEVEL_ALL );
}

CMHandler::~CMHandler()
{
    if ( m_pAvDevicePool ) {
        m_pAvDevicePool->shutdown();
    }
    if ( m_pIeee1394Service ) {
        m_pIeee1394Service->shutdown();
    }
    if ( m_pIPCHandler ) {
        m_pIPCHandler->shutdown();
    }
    m_pInstance = 0;
}

FBReturnCodes
CMHandler::initialize()
{
	FBReturnCodes eStatus;
	
    if ( !m_bInitialised ) {
        m_pIeee1394Service = Ieee1394Service::instance();

        if ( !m_pIeee1394Service ) {
            debugError( "Could not get an valid instance of 1394 service.\n" );
            return eFBRC_InitializeCMHandlerFailed;
        }
        //FIXME: the port setting is based upon a global variable, not good!
        //NOTE: we should have an Ieee1394Service for each port
        eStatus = m_pIeee1394Service->setPort(pMainArguments->port);
        if ( eStatus != eFBRC_Success ) {
            debugError( "setPort of 1394 service failed.\n" );
            return eStatus;
        }

        
        eStatus = m_pIeee1394Service->initialize();
        if ( eStatus != eFBRC_Success ) {
            debugError( "Initialising of 1394 service failed.\n" );
            return eStatus;
        }

        m_pIPCHandler = IPCHandler::instance();
        if ( !m_pIPCHandler ) {
            debugError( "Could not get an valid instance of IPC handler.\n" );
            return eFBRC_InitializeCMHandlerFailed;
        }

        eStatus = m_pIPCHandler->initialize();
        if ( eStatus != eFBRC_Success ) {
            debugError( "Initialising of IPC handler failed.\n" );
            return eStatus;
        }

        eStatus = m_pIPCHandler->start();
        if ( eStatus != eFBRC_Success ) {
            debugError( "Start of IPC handler failed.\n" );
            return eStatus;
        }

        m_pAvDevicePool = AvDevicePool::instance();
        if ( !m_pAvDevicePool ) {
            debugError( "Could not get an valid instance of the AV device pool.\n" );
            return eFBRC_InitializeCMHandlerFailed;
        }
        
        m_bInitialised = true;
    }
    return eFBRC_Success;
}

void
CMHandler::shutdown()
{
    m_pIPCHandler->stop();
    delete this;
}

CMHandler*
CMHandler::instance()
{
    if ( !m_pInstance ) {
        m_pInstance = new CMHandler;
    }
    return m_pInstance;
}


char*
CMHandler::getXmlConnectionInfo( octlet_t oGuid )
{
    AvDevice* pAvDevice = AvDevicePool::instance()->getAvDevice( oGuid );
    if ( !pAvDevice ) {
        debugError( "No AV device found with given GUID 0x%08x%08x\n",
                    (quadlet_t) (oGuid>>32),
                    (quadlet_t) (oGuid & 0xffffffff) );
        return 0;
    }

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
    xmlDocSetRootElement( doc,  rootNode );

    std::string res = "Connection Information for "
                      + pAvDevice->getVendorName()
                      +", "
                      + pAvDevice->getModelName()
                      + " configuration";
    if ( !xmlNewChild( rootNode,
                       0,
                       BAD_CAST "Comment",
                       BAD_CAST res.c_str() ) ) {
        debugError( "Couldn't create comment node\n" );
        xmlFreeDoc( doc );
        xmlCleanupParser();
        return 0;
    }

    FBReturnCodes eStatus = pAvDevice->addConnectionsToXml( rootNode );
    if ( eStatus != eFBRC_Success ) {
        debugError( "Could not add connection information to XML document\n" );
        xmlFreeDoc( doc );
        xmlCleanupParser();
        return 0;
    }

    xmlChar* xmlbuff;
    int buffersize;
    xmlDocDumpFormatMemory( doc, &xmlbuff, &buffersize, 1 );

    // Cleanup
    xmlFreeDoc( doc );
    xmlCleanupParser();

    return ( char* )xmlbuff;
}

char*
CMHandler::getXmlConnectionInfo()
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
    xmlDocSetRootElement( doc,  rootNode );
	
	// iterate over all devices, giving back the playback connections
    xmlNodePtr connectionSet = xmlNewChild( rootNode, 0, BAD_CAST "ConnectionSet", 0 );
    if ( !connectionSet ) {
        debugError( "Couldn't create connection set node for direction 1 (playback)\n" );
        return 0;
    }
    
    if ( !xmlNewChild( connectionSet,
                       0,
                       BAD_CAST "Direction",
                       BAD_CAST "1" ) ) {
        debugError( "Couldn't create direction node\n" );
        return 0;
    }
	
	AvDeviceVectorIterator iter=AvDevicePool::instance()->m_avDevices.begin();
	for(;iter<AvDevicePool::instance()->m_avDevices.end();iter++) {
		AvDevice* pAvDevice = *iter;
			
		if ( pAvDevice ) {
			std::string res = "Connection Information for "
							+ pAvDevice->getVendorName()
							+", "
							+ pAvDevice->getModelName()
							+ " (playback)";
			if ( !xmlNewChild( connectionSet,
							0,
							BAD_CAST "Comment",
							BAD_CAST res.c_str() ) ) {
				debugError( "Couldn't create comment node\n" );
				xmlFreeDoc( doc );
				xmlCleanupParser();
				return 0;
			}			

			FBReturnCodes eStatus = pAvDevice->addPlaybackConnectionsToXml( connectionSet );
			if ( eStatus != eFBRC_Success ) {
				debugError( "Could not add connection information to XML document\n" );
				xmlFreeDoc( doc );
				xmlCleanupParser();
				return 0;
			}
		}
	}
	
	// iterate over all devices, giving back the capture connections
    connectionSet = xmlNewChild( rootNode, 0, BAD_CAST "ConnectionSet", 0 );
    if ( !connectionSet ) {
        debugError( "Couldn't create connection set node for direction 0 (capture)\n" );
        return 0;
    }
    
    if ( !xmlNewChild( connectionSet,
                       0,
                       BAD_CAST "Direction",
                       BAD_CAST "0" ) ) {
        debugError( "Couldn't create direction node\n" );
        return 0;
    }
	
	iter=AvDevicePool::instance()->m_avDevices.begin();
	for(;iter<AvDevicePool::instance()->m_avDevices.end();iter++) {
		AvDevice* pAvDevice = *iter;
			
		if ( pAvDevice ) {
			std::string res = "Connection Information for "
							+ pAvDevice->getVendorName()
							+", "
							+ pAvDevice->getModelName()
							+ " (capture)";
			if ( !xmlNewChild( connectionSet,
							0,
							BAD_CAST "Comment",
							BAD_CAST res.c_str() ) ) {
				debugError( "Couldn't create comment node\n" );
				xmlFreeDoc( doc );
				xmlCleanupParser();
				return 0;
			}			

			FBReturnCodes eStatus = pAvDevice->addCaptureConnectionsToXml( connectionSet );
			if ( eStatus != eFBRC_Success ) {
				debugError( "Could not add connection information to XML document\n" );
				xmlFreeDoc( doc );
				xmlCleanupParser();
				return 0;
			}
			
		}
	}
	
	
    xmlChar* xmlbuff;
    int buffersize;
    xmlDocDumpFormatMemory( doc, &xmlbuff, &buffersize, 1 );

    // Cleanup
    xmlFreeDoc( doc );
    xmlCleanupParser();

    return ( char* )xmlbuff;
}


FBReturnCodes
CMHandler::freeXmlConnectionInfo( char* buff )
{
    xmlFree( ( xmlChar* )buff );
    return eFBRC_Success;
}
