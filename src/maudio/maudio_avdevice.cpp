/*
 * Copyright (C) 2005-2008 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "maudio/maudio_avdevice.h"
#include "bebob/bebob_avdevice.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "libavc/avc_definitions.h"

#include "debugmodule/debugmodule.h"

#include <string>
#include <stdint.h>

namespace MAudio {

AvDevice::AvDevice( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ))
    : BeBoB::AvDevice( d, configRom)
    , m_model ( NULL )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created MAudio::AvDevice (NodeID %d)\n",
                 getConfigRom().getNodeId() );
}

AvDevice::~AvDevice()
{
}

static VendorModelEntry supportedDeviceList[] =
{
    //{FW_VENDORID_BRIDGECO, 0x00010048, "BridgeCo", "RD Audio1", "refdesign.xml"},

    {FW_VENDORID_MAUDIO, 0x00010046, "M-Audio", "FW 410", "fw410.xml"},
    {FW_VENDORID_MAUDIO, 0x00010058, "M-Audio", "FW 410", "fw410.xml"},       // Version 5.10.0.5036
    {FW_VENDORID_MAUDIO, 0x00010060, "M-Audio", "FW Audiophile", "fwap.xml"},
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
AvDevice::createDevice(DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ))
{
    return new AvDevice( d, configRom );
}

bool
AvDevice::discover()
{
    unsigned int vendorId = getConfigRom().getNodeVendorId();
    unsigned int modelId = getConfigRom().getModelId();

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

uint64_t
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
