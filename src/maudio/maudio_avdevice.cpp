/*
 * Copyright (C) 2005-2007 by Daniel Wagner
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

AvDevice::AvDevice( Ieee1394Service& ieee1394Service,
                    std::auto_ptr<ConfigRom>( configRom ))
    : BeBoB::AvDevice( ieee1394Service, configRom)
    , m_model ( NULL )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created MAudio::AvDevice (NodeID %d)\n",
                 configRom->getNodeId() );
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

FFADODevice *
AvDevice::createDevice( Ieee1394Service& ieee1394Service,
                        std::auto_ptr<ConfigRom>( configRom ))
{
    return new AvDevice(ieee1394Service, configRom );
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
AvDevice::setSamplingFrequency( int eSamplingFrequency )
{
    // not supported
    return false;
}

int AvDevice::getSamplingFrequency( ) {
    return 44100;
}

int
AvDevice::getConfigurationId()
{
    return 0;
}

void
AvDevice::showDevice()
{
}

bool
AvDevice::prepare() {

    return true;
}

}
