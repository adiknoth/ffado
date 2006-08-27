/* maudio_avdevice.cpp
 * Copyright (C) 2006 by Daniel Wagner
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

#include "maudio/maudio_avdevice.h"
#include "configrom.h"

#include "libfreebobavc/ieee1394service.h"
#include "libfreebobavc/avc_definitions.h"

#include "debugmodule/debugmodule.h"

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include <string>
#include <stdint.h>

namespace MAudio {

IMPL_DEBUG_MODULE( AvDevice, AvDevice, DEBUG_LEVEL_NORMAL );

AvDevice::AvDevice( std::auto_ptr< ConfigRom >( configRom ),
                    Ieee1394Service& ieee1394service,
                    int iNodeId,
                    int iVerboseLevel )
    :  m_pConfigRom( configRom )
    , m_p1394Service( &ieee1394service )
    , m_iNodeId( iNodeId )
    , m_iVerboseLevel( iVerboseLevel )
    , m_pFilename( 0 )
{
    if ( m_iVerboseLevel ) {
        setDebugLevel( DEBUG_LEVEL_VERBOSE );
    }
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created MAudio::AvDevice (NodeID %d)\n",
                 iNodeId );
}

AvDevice::~AvDevice()
{
}

ConfigRom&
AvDevice::getConfigRom() const
{
    return *m_pConfigRom;
}

struct VendorModelEntry {
    unsigned int m_iVendorId;
    unsigned int m_iModelId;
    const char*  m_pFilename;
};

static VendorModelEntry supportedDeviceList[] =
{
    {0x0007f5, 0x00010048, "refdesign.xml"},  // BridgeCo, RD Audio1
};

bool
AvDevice::probe( ConfigRom& configRom )
{
    unsigned int iVendorId = configRom.getNodeVendorId();
    unsigned int iModelId = configRom.getModelId();

    for ( unsigned int i = 0;
          i < ( sizeof( supportedDeviceList )/sizeof( VendorModelEntry ) );
          ++i )
    {
        if ( ( supportedDeviceList[i].m_iVendorId == iVendorId )
             && ( supportedDeviceList[i].m_iModelId == iModelId ) )
        {
            return true;
        }
    }

    return false;
}

bool
AvDevice::discover()
{
    unsigned int iVendorId = m_pConfigRom->getNodeVendorId();
    unsigned int iModelId = m_pConfigRom->getModelId();
    for ( unsigned int i = 0;
          i < ( sizeof( supportedDeviceList )/sizeof( VendorModelEntry ) );
          ++i )
    {
        if ( ( supportedDeviceList[i].m_iVendorId == iVendorId )
             && ( supportedDeviceList[i].m_iModelId == iModelId ) )
        {
            m_pFilename = supportedDeviceList[i].m_pFilename;
            break;
        }
    }

    return m_pFilename != 0;
}

bool
AvDevice::setSamplingFrequency( ESamplingFrequency eSamplingFrequency )
{
    // not supported
    return false;
}

void
AvDevice::showDevice() const
{
}

bool
AvDevice::addXmlDescription( xmlNodePtr pDeviceNode )
{
    char* pFilename;
    if ( asprintf( &pFilename, "%s/libfreebob/maudio/%s", DATADIR, m_pFilename ) < 0 ) {
        debugError( "addXmlDescription: Could not create filename string\n" );
        return false;
    }

    xmlDocPtr pDoc = xmlParseFile( pFilename );

    if ( !pDoc ) {
        debugError( "addXmlDescription: No file '%s' found'\n", pFilename );
        free( pFilename );
        return false;;
    }

    xmlNodePtr pCur = xmlDocGetRootElement( pDoc );
    if ( !pCur ) {
        debugError( "addXmlDescription: Document '%s' has not root element\n", pFilename );
        xmlFreeDoc( pDoc );
        free( pFilename );
        return false;
    }

    if ( xmlStrcmp( pCur->name, ( const xmlChar * ) "FreeBoBConnectionInfo" ) ) {
        debugError( "addXmlDescription: No node 'FreeBoBConnectionInfo' found\n" );
        xmlFreeDoc( pDoc );
        free( pFilename );
        return false;
    }

    pCur = pCur->xmlChildrenNode;
    while ( pCur ) {
        if ( !xmlStrcmp( pCur->name, ( const xmlChar * ) "Device" ) ) {
            break;
        }
        pCur = pCur->next;
    }

    if ( pCur ) {
        pCur = pCur->xmlChildrenNode;
        while ( pCur ) {
            if ( ( !xmlStrcmp( pCur->name, ( const xmlChar * ) "ConnectionSet" ) ) ) {
                xmlNodePtr pDevDesc = xmlCopyNode( pCur, 1 );
                if ( !pDevDesc ) {
                    debugError( "addXmlDescription: Could not copy node 'ConnectionSet'\n" );
                    xmlFreeDoc( pDoc );
                    free( pFilename );
                    return false;
                }
                xmlAddChild( pDeviceNode, pDevDesc );
            }
            if ( ( !xmlStrcmp( pCur->name, ( const xmlChar * ) "StreamFormats" ) ) ) {
                xmlNodePtr pDevDesc = xmlCopyNode( pCur, 1 );
                if ( !pDevDesc ) {
                    debugError( "addXmlDescription: Could not copy node 'StreamFormats'\n" );
                    xmlFreeDoc( pDoc );
                    free( pFilename );
                    return false;
                }
                xmlAddChild( pDeviceNode, pDevDesc );
            }

            pCur = pCur->next;
        }
    }

    xmlFreeDoc( pDoc );
    free( pFilename );

    return true;
}

bool
AvDevice::setId(unsigned int id)
{
    return true;
}

}
