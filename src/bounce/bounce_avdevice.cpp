/* bounce_avdevice.cpp
 * Copyright (C) 2006 by Pieter Palmers
 * Copyright (C) 2006 by Daniel Wagner
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

#include "bounce/bounce_avdevice.h"
#include "configrom.h"

#include "libfreebobavc/ieee1394service.h"
#include "libfreebobavc/avc_definitions.h"

#include "debugmodule/debugmodule.h"

#include <string>
#include <stdint.h>
#include <iostream>
#include <netinet/in.h>

namespace Bounce {

IMPL_DEBUG_MODULE( BounceDevice, BounceDevice, DEBUG_LEVEL_NORMAL );

BounceDevice::BounceDevice( Ieee1394Service& ieee1394service,
                            int nodeId,
                            int verboseLevel )
    : m_1394Service( &ieee1394service )
    , m_nodeId( nodeId )
    , m_verboseLevel( verboseLevel )
{
    if ( m_verboseLevel ) {
        setDebugLevel( DEBUG_LEVEL_VERBOSE );
    }
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Bounce::BounceDevice (NodeID %d)\n",
                 nodeId );
    m_configRom = new ConfigRom( m_1394Service, m_nodeId );
    m_configRom->initialize();
}

BounceDevice::~BounceDevice()
{
	delete m_configRom;
}

ConfigRom&
BounceDevice::getConfigRom() const
{
    return *m_configRom;
}

bool
BounceDevice::discover()
{
	unsigned int resp_len=0;
	quadlet_t request[6];
	quadlet_t *resp;

	std::string vendor=std::string(FREEBOB_BOUNCE_SERVER_VENDORNAME);
	std::string model=std::string(FREEBOB_BOUNCE_SERVER_MODELNAME);
	
	if (!(m_configRom->getVendorName().compare(0,vendor.length(),vendor,0,vendor.length())==0)
	    || !(m_configRom->getModelName().compare(0,model.length(),model,0,model.length())==0)) {
		return false;
	}

// AVC1394_COMMAND_INPUT_PLUG_SIGNAL_FORMAT
	request[0] = htonl( AVC1394_CTYPE_STATUS | (AVC1394_SUBUNIT_TYPE_FREEBOB_BOUNCE_SERVER << 19) | (0 << 16)
			| AVC1394_COMMAND_INPUT_PLUG_SIGNAL_FORMAT | 0x00);

	request[1] =  0xFFFFFFFF;
        resp = m_1394Service->transactionBlock( m_nodeId,
                                                       request,
                                                       2,
		                                               &resp_len );
// 	hexDump((unsigned char *)request,6*4);
	if(resp) {
		char *buffer=(char *)&resp[1];
		xmlDescription=buffer;
// 		hexDump((unsigned char *)resp,6*4);
	}

	return true;
}

bool
BounceDevice::setSamplingFrequency( ESamplingFrequency samplingFrequency )
{
    return true;
}

void
BounceDevice::showDevice() const
{
    printf( "\nI am the bouncedevice, the bouncedevice I am...\n" );
    printf( "Vendor            :  %s\n", m_configRom->getVendorName().c_str());
    printf( "Model             :  %s\n", m_configRom->getModelName().c_str());
    printf( "Node              :  %d\n", m_nodeId);
    printf( "GUID              :  0x%016llX\n", m_configRom->getGuid());
    printf( "ACV test response :  %s\n", xmlDescription.c_str());
    printf( "\n" );
}

bool
BounceDevice::addXmlDescription( xmlNodePtr deviceNode )
{

	xmlDocPtr doc;
	xmlNodePtr cur;
	xmlNodePtr copy;

	doc = xmlParseFile("freebob_bouncedevice.xml");
	
	if (doc == NULL ) {
		debugError( "freebob_bouncedevice.xml not parsed successfully. \n");
		return false;
	}
	
	cur = xmlDocGetRootElement(doc);
	
	if (cur == NULL) {
		debugError( "empty document\n");
		xmlFreeDoc(doc);
		return false;
	}
	
	if (xmlStrcmp(cur->name, (const xmlChar *) "FreeBobBounceDevice")) {
		debugError( "document of the wrong type, root node != FreeBobBounceDevice\n");
		xmlFreeDoc(doc);
		return false;
	}
	
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		char *result;
		
		copy=xmlCopyNode(cur,1);
		
// 		debugOutput(DEBUG_LEVEL_NORMAL,"copying %s\n",cur->name);
		
		
		if (!copy || !xmlAddChild(deviceNode, copy)) {
			debugError( "could not add child node\n");
			cur = cur->next;
			continue;
		}
		
		// add the node id
// 		if (xmlStrcmp(copy->name, (const xmlChar *) "ConnectionSet")) {
// 			asprintf( &result, "%d",  m_nodeId);
// 			if ( !xmlNewChild( copy,  0,
// 			      BAD_CAST "Node",  BAD_CAST result ) ) {
// 				      debugError( "Couldn't create 'Node' node\n" );
// 				      return false;
// 				      free(result);
// 			}
// 			free(result);
// 		}
		
		cur = cur->next;
	}
	
	xmlFreeDoc(doc);

    return true;
}

}
