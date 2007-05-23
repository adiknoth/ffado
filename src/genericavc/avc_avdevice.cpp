/*
 * Copyright (C) 2005-2007 by Pieter Palmers
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

#warning Generic AV/C support is currently useless

#include "genericavc/avc_avdevice.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "libavc/avc_definitions.h"

#include "debugmodule/debugmodule.h"

#include <string>
#include <stdint.h>
#include <assert.h>
#include <netinet/in.h>
#include <iostream>
#include <sstream>

#include <libraw1394/csr.h>

namespace GenericAVC {

// to define the supported devices
static VendorModelEntry supportedDeviceList[] =
{
    {0x001486, 0x00000af2, "Echo", "AudioFire2"},
};

AvDevice::AvDevice( std::auto_ptr< ConfigRom >( configRom ),
                    Ieee1394Service& ieee1394service,
                    int nodeId )
    :  IAvDevice( configRom, ieee1394service, nodeId )
    , m_model( NULL )

{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created GenericAVC::AvDevice (NodeID %d)\n",
                 nodeId );
}

AvDevice::~AvDevice()
{

}

bool
AvDevice::probe( ConfigRom& configRom )
{
    unsigned int vendorId = configRom.getNodeVendorId();
    unsigned int modelId = configRom.getModelId();

    for ( unsigned int i = 0;
          i < ( sizeof( supportedDeviceList )/sizeof( VendorModelEntry ) );
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
        }
    }

    if (m_model != NULL) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "found %s %s\n",
                m_model->vendor_name, m_model->model_name);
        return true;
    }

    return false;
}

int
AvDevice::getSamplingFrequency( ) {
    return 0;
}

bool
AvDevice::setSamplingFrequency( ESamplingFrequency samplingFrequency )
{

    return false;
}

bool
AvDevice::lock() {

    return true;
}


bool
AvDevice::unlock() {

    return true;
}

void
AvDevice::showDevice()
{
    debugOutput(DEBUG_LEVEL_VERBOSE,
        "%s %s at node %d\n", m_model->vendor_name, m_model->model_name,
        m_nodeId);
}

bool
AvDevice::prepare() {

    return true;
}

int
AvDevice::getStreamCount() {
    return 0;
}

Streaming::StreamProcessor *
AvDevice::getStreamProcessorByIndex(int i) {

    return NULL;
}

bool
AvDevice::startStreamByIndex(int i) {
    return false;
}

bool
AvDevice::stopStreamByIndex(int i) {
    return false;
}

}
