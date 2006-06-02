/* bounce_avdevice.cpp
 * Copyright (C) 2006 by Pieter Palmers
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

#include "bounce/bounce_avdevice.h"
#include "configrom.h"

#include "libfreebobavc/ieee1394service.h"
#include "libfreebobavc/avc_definitions.h"

#include "debugmodule/debugmodule.h"

#include <string>
#include <stdint.h>

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

}

ConfigRom&
BounceDevice::getConfigRom() const
{
    return *m_configRom;
}

bool
BounceDevice::discover()
{
    std::string vendor = std::string(FREEBOB_BOUNCE_SERVER_VENDORNAME);
    std::string model = std::string(FREEBOB_BOUNCE_SERVER_MODELNAME);

    if (m_configRom->getVendorName().compare(0,vendor.length(),vendor,0,vendor.length())==0) {
        if(m_configRom->getModelName().compare(0,model.length(),model,0,model.length())==0) {
            return true;
        }
    }
    return false;
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
    printf( "Vendor :  %s\n", m_configRom->getVendorName().c_str());
    printf( "Model  :  %s\n", m_configRom->getModelName().c_str());
    printf( "GUID   :  0x%016llX\n", m_configRom->getGuid());
    printf( "\n" );
}

bool
BounceDevice::addXmlDescription( xmlNodePtr deviceNode )
{
    return true;
}

bool
BounceDevice::setId(unsigned int id)
{
    return true;
}

}
