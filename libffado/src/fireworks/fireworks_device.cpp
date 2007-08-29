/*
 * Copyright (C) 2007 by Pieter Palmers
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

#include "fireworks_device.h"

#include "audiofire/audiofire_device.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

// FireWorks is the platform used and developed by ECHO AUDIO
namespace FireWorks {

// to define the supported devices
static GenericAVC::VendorModelEntry supportedDeviceList[] =
{
    {FW_VENDORID_ECHO, 0x00000af2, "Echo", "AudioFire2"},
};

Device::Device( Ieee1394Service& ieee1394Service,
                            std::auto_ptr<ConfigRom>( configRom ))
    : GenericAVC::AvDevice( ieee1394Service, configRom)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created FireWorks::Device (NodeID %d)\n",
                 getConfigRom().getNodeId() );
}

Device::~Device()
{
}

void
Device::showDevice()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "This is a FireWorks::Device\n");
    GenericAVC::AvDevice::showDevice();
}


bool
Device::probe( ConfigRom& configRom )
{
    unsigned int vendorId = configRom.getNodeVendorId();
    unsigned int modelId = configRom.getModelId();

    for ( unsigned int i = 0;
          i < ( sizeof( supportedDeviceList )/sizeof( GenericAVC::VendorModelEntry ) );
          ++i )
    {
        if ( ( supportedDeviceList[i].vendor_id == vendorId )
             && ( supportedDeviceList[i].model_id == modelId )
           )
        {
            return true;
        }
    }

    return false;
}

bool
Device::discover()
{
    unsigned int vendorId = m_pConfigRom->getNodeVendorId();
    unsigned int modelId = m_pConfigRom->getModelId();

    for ( unsigned int i = 0;
          i < ( sizeof( supportedDeviceList )/sizeof( GenericAVC::VendorModelEntry ) );
          ++i )
    {
        if ( ( supportedDeviceList[i].vendor_id == vendorId )
             && ( supportedDeviceList[i].model_id == modelId )
           )
        {
            m_model = &(supportedDeviceList[i]);
        }
    }

    if (m_model == NULL) {
        return false;
    }
    debugOutput( DEBUG_LEVEL_VERBOSE, "found %s %s\n",
            m_model->vendor_name, m_model->model_name);

    if ( !GenericAVC::AvDevice::discover() ) {
        debugError( "Could not discover GenericAVC::AvDevice\n" );
        return false;
    }

    return true;
}

FFADODevice *
Device::createDevice( Ieee1394Service& ieee1394Service,
                      std::auto_ptr<ConfigRom>( configRom ))
{
    unsigned int vendorId = configRom->getNodeVendorId();
    unsigned int modelId = configRom->getModelId();
    
    switch(vendorId) {
        case FW_VENDORID_ECHO: return new ECHO::AudioFire(ieee1394Service, configRom );
        default: return new Device(ieee1394Service, configRom );
    }
}


} // FireWorks
