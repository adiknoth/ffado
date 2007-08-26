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

#warning Metric Halo support is currently useless

#include "metrichalo/mh_avdevice.h"

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

namespace MetricHalo {

// to define the supported devices
static VendorModelEntry supportedDeviceList[] =
{
    {0x00000000, 0x0000, "Metric Halo", "XXX"},
};

MHAvDevice::MHAvDevice( std::auto_ptr< ConfigRom >( configRom ),
                    Ieee1394Service& ieee1394service,
                    int nodeId )
    :  FFADODevice( configRom, ieee1394service, nodeId )
    , m_model( NULL )

{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created MetricHalo::MHAvDevice (NodeID %d)\n",
                 nodeId );
}

MHAvDevice::~MHAvDevice()
{

}

bool
MHAvDevice::probe( ConfigRom& configRom )
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
MHAvDevice::discover()
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
MHAvDevice::getSamplingFrequency( ) {
    return 0;
}

int
MHAvDevice::getConfigurationId( ) {
    return 0;
}

bool
MHAvDevice::setSamplingFrequency( int samplingFrequency )
{

    return false;
}

bool
MHAvDevice::lock() {

    return true;
}


bool
MHAvDevice::unlock() {

    return true;
}

void
MHAvDevice::showDevice()
{
    debugOutput(DEBUG_LEVEL_VERBOSE,
        "%s %s at node %d\n", m_model->vendor_name, m_model->model_name,
        m_nodeId);
}

bool
MHAvDevice::prepare() {

    return true;
}

int
MHAvDevice::getStreamCount() {
    return 0;
}

Streaming::StreamProcessor *
MHAvDevice::getStreamProcessorByIndex(int i) {

    return NULL;
}

bool
MHAvDevice::startStreamByIndex(int i) {
    return false;
}

bool
MHAvDevice::stopStreamByIndex(int i) {
    return false;
}

}