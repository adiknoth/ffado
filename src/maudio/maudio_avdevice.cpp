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
#include "bebob/bebob_avdevice.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "libavc/avc_definitions.h"

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
    : BeBoB::AvDevice( configRom,
                    ieee1394service,
                    iNodeId,
                    iVerboseLevel )
    , m_model ( NULL )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created MAudio::AvDevice (NodeID %d)\n",
                 iNodeId );
}

AvDevice::~AvDevice()
{
}

static VendorModelEntry supportedDeviceList[] =
{
    //{0x0007f5, 0x00010048, "BridgeCo", "RD Audio1", "refdesign.xml"},

    {0x000d6c, 0x00010046, "M-Audio", "FW 410", "fw410.xml"},
    {0x000d6c, 0x00010058, "M-Audio", "FW 410", "fw410.xml"},       // Version 5.10.0.5036
    {0x000d6c, 0x00010060, "M-Audio", "FW Audiophile", "fwap.xml"},
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
        if ( ( supportedDeviceList[i].vendor_id == iVendorId )
             && ( supportedDeviceList[i].model_id == iModelId ) )
        {
            return true;
        }
    }
    return false;
}

bool
AvDevice::discover()
{
    unsigned int vendorId = m_pConfigRom->getNodeVendorId();
    unsigned int modelId = m_pConfigRom->getModelId();

    for ( unsigned int i = 0;
          i < ( sizeof( supportedDeviceList )/sizeof( VendorModelEntry ) );
          ++i )
    {
        if ( ( supportedDeviceList[i].vendor_id == vendorId )
             && ( supportedDeviceList[i].model_id == modelId ) 
           )
        {
            m_model = &(supportedDeviceList[i]);
            break;
        }
    }

    if (m_model != NULL) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "found %s %s\n",
                m_model->vendor_name, m_model->model_name);
    } else return false;
    
    return true;
}

bool
AvDevice::setSamplingFrequency( ESamplingFrequency eSamplingFrequency )
{
    // not supported
    return false;
}

int AvDevice::getSamplingFrequency( ) {
    return 44100;
}

void
AvDevice::showDevice() const
{
}

bool
AvDevice::addXmlDescription( xmlNodePtr pDeviceNode )
{
    char* pFilename;
    if ( asprintf( &pFilename, "%s/libfreebob/maudio/%s", DATADIR, m_model->filename ) < 0 ) {
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

                // set correct node id
                for ( xmlNodePtr pNode = pDevDesc->xmlChildrenNode; pNode; pNode = pNode->next ) {
                    if ( ( !xmlStrcmp( pNode->name,  ( const xmlChar * ) "Connection" ) ) ) {
                        for ( xmlNodePtr pSubNode = pNode->xmlChildrenNode; pSubNode; pSubNode = pSubNode->next ) {
                            if ( ( !xmlStrcmp( pSubNode->name,  ( const xmlChar * ) "Node" ) ) ) {
                                char* result;
                                asprintf( &result, "%d", m_nodeId );
                                xmlNodeSetContent( pSubNode, BAD_CAST result );
                                free( result );
                            }
                        }
                    }
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
AvDevice::prepare() {

    return true;
}

}
