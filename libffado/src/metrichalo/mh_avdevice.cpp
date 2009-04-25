/*
 * Copyright (C) 2005-2008 by Pieter Palmers
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

#warning Metric Halo support is currently useless

#include "metrichalo/mh_avdevice.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "libavc/avc_definitions.h"

#include "debugmodule/debugmodule.h"

#include "devicemanager.h"

#include <string>
#include <stdint.h>
#include <assert.h>
#include "libutil/ByteSwap.h"
#include <iostream>
#include <sstream>

#include <libraw1394/csr.h>

namespace MetricHalo {

Device::Device( DeviceManager& d,
                        std::auto_ptr<ConfigRom>( configRom ))
    : FFADODevice( d, configRom )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created MetricHalo::Device (NodeID %d)\n",
                 getConfigRom().getNodeId() );
}

Device::~Device()
{

}

bool
Device::probe( Util::Configuration& c, ConfigRom& configRom, bool generic )
{
    if (generic) {
        return false;
    } else {
        // check if device is in supported devices list
        unsigned int vendorId = configRom.getNodeVendorId();
        unsigned int modelId = configRom.getModelId();

        Util::Configuration::VendorModelEntry vme = c.findDeviceVME( vendorId, modelId );
        return c.isValid(vme) && vme.driver == Util::Configuration::eD_MetricHalo;
    }
}

FFADODevice *
Device::createDevice( DeviceManager& d,
                          std::auto_ptr<ConfigRom>( configRom ))
{
    return new Device(d, configRom );
}

bool
Device::discover()
{
    unsigned int vendorId = getConfigRom().getNodeVendorId();
    unsigned int modelId = getConfigRom().getModelId();

    Util::Configuration &c = getDeviceManager().getConfiguration();
    Util::Configuration::VendorModelEntry vme = c.findDeviceVME( vendorId, modelId );

    if (c.isValid(vme) && vme.driver == Util::Configuration::eD_MetricHalo) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "found %s %s\n",
                     vme.vendor_name.c_str(),
                     vme.model_name.c_str());
    } else {
        debugWarning("Using generic Metric Halo support for unsupported device '%s %s'\n",
                     getConfigRom().getVendorName().c_str(), getConfigRom().getModelName().c_str());
    }

    return false;
}

int
Device::getSamplingFrequency( ) {
    return 0;
}

std::vector<int>
Device::getSupportedSamplingFrequencies()
{
    std::vector<int> frequencies;
    return frequencies;
}

FFADODevice::ClockSourceVector
Device::getSupportedClockSources() {
    FFADODevice::ClockSourceVector r;
    return r;
}

bool
Device::setActiveClockSource(ClockSource s) {
    return false;
}

FFADODevice::ClockSource
Device::getActiveClockSource() {
    ClockSource s;
    return s;
}


int
Device::getConfigurationId( ) {
    return 0;
}

bool
Device::setSamplingFrequency( int samplingFrequency )
{

    return false;
}

bool
Device::lock() {

    return true;
}


bool
Device::unlock() {

    return true;
}

void
Device::showDevice()
{
    unsigned int vendorId = getConfigRom().getNodeVendorId();
    unsigned int modelId = getConfigRom().getModelId();

    Util::Configuration &c = getDeviceManager().getConfiguration();
    Util::Configuration::VendorModelEntry vme = c.findDeviceVME( vendorId, modelId );

    debugOutput(DEBUG_LEVEL_VERBOSE,
        "%s %s at node %d\n", vme.vendor_name.c_str(), vme.model_name.c_str(), getNodeId());
}

bool
Device::prepare() {

    return true;
}

int
Device::getStreamCount() {
    return 0;
}

Streaming::StreamProcessor *
Device::getStreamProcessorByIndex(int i) {

    return NULL;
}

bool
Device::startStreamByIndex(int i) {
    return false;
}

bool
Device::stopStreamByIndex(int i) {
    return false;
}

}
