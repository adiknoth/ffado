/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 * Copyright (C) 2005-2008 by Jonathan Woithe
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

#include "motu/motu_avdevice.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "libavc/avc_definitions.h"

#include "debugmodule/debugmodule.h"

#include "libstreaming/motu/MotuReceiveStreamProcessor.h"
#include "libstreaming/motu/MotuTransmitStreamProcessor.h"
#include "libstreaming/motu/MotuPort.h"

#include "libutil/DelayLockedLoop.h"
#include "libutil/Time.h"

#include <string>
#include <stdint.h>
#include <assert.h>
#include <netinet/in.h>
#include <iostream>
#include <sstream>

#include <libraw1394/csr.h>

namespace Motu {

// Define the supported devices.  Device ordering is arbitary here.
static VendorModelEntry supportedDeviceList[] =
{
//  {vendor_id, model_id, unit_version, unit_specifier_id, model, vendor_name,model_name}
    {FW_VENDORID_MOTU, 0, 0x00000003, 0x000001f2, MOTUFW_MODEL_828mkII, "MOTU", "828MkII"},
    {FW_VENDORID_MOTU, 0, 0x00000009, 0x000001f2, MOTUFW_MODEL_TRAVELER, "MOTU", "Traveler"},
    {FW_VENDORID_MOTU, 0, 0x0000000d, 0x000001f2, MOTUFW_MODEL_ULTRALITE, "MOTU", "UltraLite"},
    {FW_VENDORID_MOTU, 0, 0x0000000f, 0x000001f2, MOTUFW_MODEL_8PRE, "MOTU", "8pre"},
    {FW_VENDORID_MOTU, 0, 0x00000001, 0x000001f2, MOTUFW_MODEL_828MkI, "MOTU", "828MkI"},
};

// Ports declarations
const PortEntry Ports_828MKI[] =
{
    {"Analog1", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 10},
    {"Analog2", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 13},
    {"Analog3", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 16},
    {"Analog4", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 19},
    {"Analog5", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 22},
    {"Analog6", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 25},
    {"Analog7", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 28},
    {"Analog8", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 31},
    {"SPDIF1", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 34},
    {"SPDIF2", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 37},
    {"ADAT1", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ADAT, 40},
    {"ADAT2", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ADAT, 43},
    {"ADAT3", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ADAT, 46},
    {"ADAT4", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ADAT, 49},
    {"ADAT5", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_1x|MOTUFW_PA_OPTICAL_ADAT, 52},
    {"ADAT6", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_1x|MOTUFW_PA_OPTICAL_ADAT, 55},
    {"ADAT7", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_1x|MOTUFW_PA_OPTICAL_ADAT, 58},
    {"ADAT8", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_1x|MOTUFW_PA_OPTICAL_ADAT, 61},
};

const PortEntry Ports_828MKII[] =
{
    {"Main-L", MOTUFW_DIR_OUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 40},
    {"Main-R", MOTUFW_DIR_OUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 43},
    {"Mix-L", MOTUFW_DIR_IN, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 10},
    {"Mix-R", MOTUFW_DIR_IN, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 13},
    {"Analog1", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 16},
    {"Analog2", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 19},
    {"Analog3", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 22},
    {"Analog4", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 25},
    {"Analog5", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 28},
    {"Analog6", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 31},
    {"Analog7", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 34},
    {"Analog8", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 37},
    {"Phones-L", MOTUFW_DIR_OUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 10},
    {"Phones-R", MOTUFW_DIR_OUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 13},
    {"Mic1", MOTUFW_DIR_IN, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 40},
    {"Mic2", MOTUFW_DIR_IN, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 43},
    {"SPDIF1", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 46},
    {"SPDIF2", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 49},
    {"ADAT1", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ADAT, 52},
    {"ADAT2", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ADAT, 55},
    {"ADAT3", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ADAT, 58},
    {"ADAT4", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ADAT, 61},
    {"ADAT5", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_1x|MOTUFW_PA_OPTICAL_ADAT, 63},
    {"ADAT6", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_1x|MOTUFW_PA_OPTICAL_ADAT, 66},
    {"ADAT7", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_1x|MOTUFW_PA_OPTICAL_ADAT, 69},
    {"ADAT8", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_1x|MOTUFW_PA_OPTICAL_ADAT, 72},
};

const PortEntry Ports_TRAVELER[] = 
{
    {"Mix-L", MOTUFW_DIR_IN, MOTUFW_PA_RATE_1x2x|MOTUFW_PA_OPTICAL_ANY, 10},
    {"Mix-R", MOTUFW_DIR_IN, MOTUFW_PA_RATE_1x2x|MOTUFW_PA_OPTICAL_ANY, 13},
    {"Phones-L", MOTUFW_DIR_OUT, MOTUFW_PA_RATE_1x2x|MOTUFW_PA_OPTICAL_ANY, 10},
    {"Phones-R", MOTUFW_DIR_OUT, MOTUFW_PA_RATE_1x2x|MOTUFW_PA_OPTICAL_ANY, 13},
    {"Analog1", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 16},
    {"Analog2", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 19},
    {"Analog3", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 22},
    {"Analog4", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 25},
    {"Analog5", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 28},
    {"Analog6", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 31},
    {"Analog7", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 34},
    {"Analog8", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 37},
    {"AES/EBU1", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_1x2x|MOTUFW_PA_OPTICAL_ANY, 40},
    {"AES/EBU2", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_1x2x|MOTUFW_PA_OPTICAL_ANY, 43},
    {"SPDIF1", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_1x2x|MOTUFW_PA_OPTICAL_OFF|MOTUFW_PA_OPTICAL_ADAT, 46},
    {"SPDIF2", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_1x2x|MOTUFW_PA_OPTICAL_OFF|MOTUFW_PA_OPTICAL_ADAT, 49},
    {"Toslink1", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_1x2x|MOTUFW_PA_OPTICAL_TOSLINK, 46},
    {"Toslink2", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_1x2x|MOTUFW_PA_OPTICAL_TOSLINK, 49},
    {"ADAT1", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_1x2x|MOTUFW_PA_OPTICAL_ADAT, 52},
    {"ADAT2", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_1x2x|MOTUFW_PA_OPTICAL_ADAT, 55},
    {"ADAT3", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_1x2x|MOTUFW_PA_OPTICAL_ADAT, 58},
    {"ADAT4", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_1x2x|MOTUFW_PA_OPTICAL_ADAT, 61},
    {"ADAT5", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_1x|MOTUFW_PA_OPTICAL_ADAT, 63},
    {"ADAT6", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_1x|MOTUFW_PA_OPTICAL_ADAT, 66},
    {"ADAT7", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_1x|MOTUFW_PA_OPTICAL_ADAT, 69},
    {"ADAT8", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_1x|MOTUFW_PA_OPTICAL_ADAT, 72},
};

const PortEntry Ports_ULTRALITE[] =
{
    {"Main-L", MOTUFW_DIR_OUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 40},
    {"Main-R", MOTUFW_DIR_OUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 43},
    {"Mix-L", MOTUFW_DIR_IN, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 10},
    {"Mix-R", MOTUFW_DIR_IN, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 13},
    {"Mic1", MOTUFW_DIR_IN, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 16},
    {"Mic2", MOTUFW_DIR_IN, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 19},
    {"Analog1", MOTUFW_DIR_OUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 16},
    {"Analog2", MOTUFW_DIR_OUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 19},
    {"Analog3", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 22},
    {"Analog4", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 25},
    {"Analog5", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 28},
    {"Analog6", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 31},
    {"Analog7", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 34},
    {"Analog8", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 37},
    {"Phones-L", MOTUFW_DIR_OUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 10},
    {"Phones-R", MOTUFW_DIR_OUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 13},
    {"SPDIF1", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 46},
    {"SPDIF2", MOTUFW_DIR_INOUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 49},
};

const PortEntry Ports_8PRE[] =
{
    {"Analog1", MOTUFW_DIR_IN, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 16},
    {"Analog2", MOTUFW_DIR_IN, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 19},
    {"Analog3", MOTUFW_DIR_IN, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 22},
    {"Analog4", MOTUFW_DIR_IN, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 25},
    {"Analog5", MOTUFW_DIR_IN, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 28},
    {"Analog6", MOTUFW_DIR_IN, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 31},
    {"Analog7", MOTUFW_DIR_IN, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 34},
    {"Analog8", MOTUFW_DIR_IN, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 37},
    {"Mix-L", MOTUFW_DIR_IN, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 10},
    {"Mix-R", MOTUFW_DIR_IN, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 13},
    {"Main-L", MOTUFW_DIR_OUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 16},
    {"Main-R", MOTUFW_DIR_OUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 19},
    {"Phones-L", MOTUFW_DIR_OUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 10},
    {"Phones-R", MOTUFW_DIR_OUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ANY, 13},
    {"ADAT1", MOTUFW_DIR_IN, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ADAT, 40},
    {"ADAT1", MOTUFW_DIR_OUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ADAT, 22},
    {"ADAT2", MOTUFW_DIR_IN, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ADAT, 43},
    {"ADAT2", MOTUFW_DIR_OUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ADAT, 25},
    {"ADAT3", MOTUFW_DIR_IN, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ADAT, 46},
    {"ADAT3", MOTUFW_DIR_OUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ADAT, 28},
    {"ADAT4", MOTUFW_DIR_IN, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ADAT, 49},
    {"ADAT4", MOTUFW_DIR_OUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ADAT, 31},
    {"ADAT5", MOTUFW_DIR_IN, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ADAT, 52},
    {"ADAT5", MOTUFW_DIR_OUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ADAT, 34},
    {"ADAT6", MOTUFW_DIR_IN, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ADAT, 55},
    {"ADAT6", MOTUFW_DIR_OUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ADAT, 37},
    {"ADAT7", MOTUFW_DIR_IN, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ADAT, 58},
    {"ADAT7", MOTUFW_DIR_OUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ADAT, 40},
    {"ADAT8", MOTUFW_DIR_IN, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ADAT, 61},
    {"ADAT8", MOTUFW_DIR_OUT, MOTUFW_PA_RATE_ANY|MOTUFW_PA_OPTICAL_ADAT, 43},
};

/* The order of DevicesProperty entries must match the numeric order of the
 * MOTU model enumeration (EMotuModel).
 */
const DevicePropertyEntry DevicesProperty[] = {
//  { Ports_map,       sizeof( Ports_map ),        MaxSR },
    { Ports_828MKII,   sizeof( Ports_828MKII ),    96000 },
    { Ports_TRAVELER,  sizeof( Ports_TRAVELER ),  192000 },
    { Ports_ULTRALITE, sizeof( Ports_ULTRALITE ),  96000 },
    { Ports_8PRE,      sizeof( Ports_8PRE ),       96000 },
    { Ports_828MKI,    sizeof( Ports_828MKI ),     48000 },
};

MotuDevice::MotuDevice( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ))
    : FFADODevice( d, configRom )
    , m_motu_model( MOTUFW_MODEL_NONE )
    , m_iso_recv_channel ( -1 )
    , m_iso_send_channel ( -1 )
    , m_rx_bandwidth ( -1 )
    , m_tx_bandwidth ( -1 )
    , m_receiveProcessor ( 0 )
    , m_transmitProcessor ( 0 )

{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Motu::MotuDevice (NodeID %d)\n",
                 getConfigRom().getNodeId() );

}

MotuDevice::~MotuDevice()
{
    delete m_receiveProcessor;
    delete m_transmitProcessor;

    // Free ieee1394 bus resources if they have been allocated
    if (m_iso_recv_channel>=0 && !get1394Service().freeIsoChannel(m_iso_recv_channel)) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Could not free recv iso channel %d\n", m_iso_recv_channel);
    }
    if (m_iso_send_channel>=0 && !get1394Service().freeIsoChannel(m_iso_send_channel)) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Could not free send iso channel %d\n", m_iso_send_channel);
    }
}

bool
MotuDevice::probe( ConfigRom& configRom )
{
    unsigned int vendorId = configRom.getNodeVendorId();
//     unsigned int modelId = configRom.getModelId();
    unsigned int unitVersion = configRom.getUnitVersion();
    unsigned int unitSpecifierId = configRom.getUnitSpecifierId();

    for ( unsigned int i = 0;
          i < ( sizeof( supportedDeviceList )/sizeof( VendorModelEntry ) );
          ++i )
    {
        if ( ( supportedDeviceList[i].vendor_id == vendorId )
//              && ( supportedDeviceList[i].model_id == modelId )
             && ( supportedDeviceList[i].unit_version == unitVersion )
             && ( supportedDeviceList[i].unit_specifier_id == unitSpecifierId )
           )
        {
            return true;
        }
    }

    return false;
}

FFADODevice *
MotuDevice::createDevice(DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ))
{
    return new MotuDevice(d, configRom);
}

bool
MotuDevice::discover()
{
    unsigned int vendorId = getConfigRom().getNodeVendorId();
//     unsigned int modelId = getConfigRom().getModelId();
    unsigned int unitVersion = getConfigRom().getUnitVersion();
    unsigned int unitSpecifierId = getConfigRom().getUnitSpecifierId();

    for ( unsigned int i = 0;
          i < ( sizeof( supportedDeviceList )/sizeof( VendorModelEntry ) );
          ++i )
    {
        if ( ( supportedDeviceList[i].vendor_id == vendorId )
//              && ( supportedDeviceList[i].model_id == modelId )
             && ( supportedDeviceList[i].unit_version == unitVersion )
             && ( supportedDeviceList[i].unit_specifier_id == unitSpecifierId )
           )
        {
            m_model = &(supportedDeviceList[i]);
            m_motu_model=supportedDeviceList[i].model;
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
MotuDevice::getSamplingFrequency( ) {
/*
 * Retrieve the current sample rate from the MOTU device.
 */
    quadlet_t q = ReadRegister(MOTUFW_REG_CLK_CTRL);
    int rate = 0;

    switch (q & MOTUFW_RATE_BASE_MASK) {
        case MOTUFW_RATE_BASE_44100:
            rate = 44100;
            break;
        case MOTUFW_RATE_BASE_48000:
            rate = 48000;
            break;
    }
    switch (q & MOTUFW_RATE_MULTIPLIER_MASK) {
        case MOTUFW_RATE_MULTIPLIER_2X:
            rate *= 2;
            break;
        case MOTUFW_RATE_MULTIPLIER_4X:
            rate *= 4;
            break;
    }
    return rate;
}

int
MotuDevice::getConfigurationId()
{
    return 0;
}

bool
MotuDevice::setSamplingFrequency( int samplingFrequency )
{
/*
 * Set the MOTU device's samplerate.
 */
    char *src_name;
    quadlet_t q, new_rate=0;
    int i, supported=true, cancel_adat=false;

    if ( samplingFrequency > DevicesProperty[m_motu_model-1].MaxSampleRate )
       return false; 

    switch ( samplingFrequency ) {
        case 22050:
        case 24000:
        case 32000:
            supported=false;
            break;
        case 44100:
            new_rate = MOTUFW_RATE_BASE_44100 | MOTUFW_RATE_MULTIPLIER_1X;
            break;
        case 48000:
            new_rate = MOTUFW_RATE_BASE_48000 | MOTUFW_RATE_MULTIPLIER_1X;
            break;
        case 88200:
            new_rate = MOTUFW_RATE_BASE_44100 | MOTUFW_RATE_MULTIPLIER_2X;
            break;
        case 96000:
            new_rate = MOTUFW_RATE_BASE_48000 | MOTUFW_RATE_MULTIPLIER_2X;
            break;
        case 176400:
            new_rate = MOTUFW_RATE_BASE_44100 | MOTUFW_RATE_MULTIPLIER_4X;
            cancel_adat = true;  // current ADAT protocol doesn't support sample rate > 96000
            break;
        case 192000:
            new_rate = MOTUFW_RATE_BASE_48000 | MOTUFW_RATE_MULTIPLIER_4X;
            cancel_adat = true;
            break;
        default:
            supported=false;
    }

    // Update the clock control register.  FIXME: while this is now rather
    // comprehensive there may still be a need to manipulate MOTUFW_REG_CLK_CTRL
    // a little more than we do.
    if (supported) {
        quadlet_t value=ReadRegister(MOTUFW_REG_CLK_CTRL);

        // If optical port must be disabled (because a 4x sample rate has
        // been selected) then do so before changing the sample rate.  At
        // this stage it will be up to the user to re-enable the optical
        // port if the sample rate is set to a 1x or 2x rate later.
        if (cancel_adat) {
            setOpticalMode(MOTUFW_DIR_INOUT, MOTUFW_OPTICAL_MODE_OFF);
        }

        value &= ~(MOTUFW_RATE_BASE_MASK|MOTUFW_RATE_MULTIPLIER_MASK);
        value |= new_rate;

        // In other OSes bit 26 of MOTUFW_REG_CLK_CTRL always seems
        // to be set when this register is written to although the
        // reason isn't currently known.  When we set it, it appears
        // to prevent output being produced so we'll leave it unset
        // until we work out what's going on.  Other systems write
        // to MOTUFW_REG_CLK_CTRL multiple times, so that may be
        // part of the mystery.
        //   value |= 0x04000000;
        if (WriteRegister(MOTUFW_REG_CLK_CTRL, value) == 0) {
            supported=true;
        } else {
            supported=false;
        }
        // A write to the rate/clock control register requires the
        // textual name of the current clock source be sent to the
        // clock source name registers.
        switch (value & MOTUFW_CLKSRC_MASK) {
            case MOTUFW_CLKSRC_INTERNAL:
                src_name = "Internal        ";
                break;
            case MOTUFW_CLKSRC_ADAT_OPTICAL:
                src_name = "ADAT Optical    ";
                break;
            case MOTUFW_CLKSRC_SPDIF_TOSLINK:
                if (getOpticalMode(MOTUFW_DIR_IN)  == MOTUFW_OPTICAL_MODE_TOSLINK)
                    src_name = "TOSLink         ";
                else
                    src_name = "SPDIF           ";
                break;
            case MOTUFW_CLKSRC_SMTPE:
                src_name = "SMPTE           ";
                break;
            case MOTUFW_CLKSRC_WORDCLOCK:
                src_name = "Word Clock In   ";
                break;
            case MOTUFW_CLKSRC_ADAT_9PIN:
                src_name = "ADAT 9-pin      ";
                break;
            case MOTUFW_CLKSRC_AES_EBU:
                src_name = "AES-EBU         ";
                break;
            default:
                src_name = "Unknown         ";
        }
        for (i=0; i<16; i+=4) {
            q = (src_name[i]<<24) | (src_name[i+1]<<16) |
                (src_name[i+2]<<8) | src_name[i+3];
            WriteRegister(MOTUFW_REG_CLKSRC_NAME0+i, q);
        }
    }
    return supported;
}

FFADODevice::ClockSourceVector
MotuDevice::getSupportedClockSources() {
    FFADODevice::ClockSourceVector r;
    return r;
}

bool
MotuDevice::setActiveClockSource(ClockSource s) {
    return false;
}

FFADODevice::ClockSource
MotuDevice::getActiveClockSource() {
    ClockSource s;
    return s;
}

bool
MotuDevice::lock() {

    return true;
}


bool
MotuDevice::unlock() {

    return true;
}

void
MotuDevice::showDevice()
{
    debugOutput(DEBUG_LEVEL_VERBOSE,
        "%s %s at node %d\n", m_model->vendor_name, m_model->model_name,
        getNodeId());
}

bool
MotuDevice::prepare() {

    int samp_freq = getSamplingFrequency();
    unsigned int optical_in_mode = getOpticalMode(MOTUFW_DIR_IN);
    unsigned int optical_out_mode = getOpticalMode(MOTUFW_DIR_OUT);
    unsigned int event_size_in = getEventSize(MOTUFW_DIR_IN);
    unsigned int event_size_out= getEventSize(MOTUFW_DIR_OUT);

    debugOutput(DEBUG_LEVEL_NORMAL, "Preparing MotuDevice...\n" );

    // Allocate bandwidth if not previously done.
    // FIXME: The bandwidth allocation calculation can probably be
    // refined somewhat since this is currently based on a rudimentary
    // understanding of the ieee1394 iso protocol.
    // Currently we assume the following.
    //   * Ack/iso gap = 0.05 us
    //   * DATA_PREFIX = 0.16 us
    //   * DATA_END    = 0.26 us
    // These numbers are the worst-case figures given in the ieee1394
    // standard.  This gives approximately 0.5 us of overheads per packet -
    // around 25 bandwidth allocation units (from the ieee1394 standard 1
    // bandwidth allocation unit is 125/6144 us).  We further assume the
    // MOTU is running at S400 (which it should be) so one allocation unit
    // is equivalent to 1 transmitted byte; thus the bandwidth allocation
    // required for the packets themselves is just the size of the packet. 
    // We used to allocate based on the maximum packet size (1160 bytes at
    // 192 kHz for the traveler) but now do this based on the actual device
    // state by utilising the result from getEventSize() and remembering
    // that each packet has an 8 byte CIP header.  Note that bandwidth is
    // allocated on a *per stream* basis - it must be allocated for both the
    // transmit and receive streams.  While most MOTU modules are close to
    // symmetric in terms of the number of in/out channels there are
    // exceptions, so we deal with receive and transmit bandwidth separately.
    signed int n_events_per_packet = samp_freq<=48000?8:(samp_freq<=96000?16:32);
    m_rx_bandwidth = 25 + (n_events_per_packet*event_size_in);
    m_tx_bandwidth = 25 + (n_events_per_packet*event_size_out);

    // Assign iso channels if not already done
    if (m_iso_recv_channel < 0)
        m_iso_recv_channel = get1394Service().allocateIsoChannelGeneric(m_rx_bandwidth);

    if (m_iso_send_channel < 0)
        m_iso_send_channel = get1394Service().allocateIsoChannelGeneric(m_tx_bandwidth);

    debugOutput(DEBUG_LEVEL_VERBOSE, "recv channel = %d, send channel = %d\n",
        m_iso_recv_channel, m_iso_send_channel);

    if (m_iso_recv_channel<0 || m_iso_send_channel<0) {
        // be nice and deallocate
        if (m_iso_recv_channel >= 0)
            get1394Service().freeIsoChannel(m_iso_recv_channel);
        if (m_iso_send_channel >= 0)
            get1394Service().freeIsoChannel(m_iso_send_channel);

        debugFatal("Could not allocate iso channels!\n");
        return false;
    }

    m_receiveProcessor=new Streaming::MotuReceiveStreamProcessor(*this, event_size_in);

    // The first thing is to initialize the processor.  This creates the
    // data structures.
    if(!m_receiveProcessor->init()) {
        debugFatal("Could not initialize receive processor!\n");
        return false;
    }
    m_receiveProcessor->setVerboseLevel(getDebugLevel());

    // Now we add ports to the processor
    debugOutput(DEBUG_LEVEL_VERBOSE,"Adding ports to receive processor\n");

    char *buff;
    Streaming::Port *p=NULL;

    // retrieve the ID
    std::string id=std::string("dev?");
    if(!getOption("id", id)) {
        debugWarning("Could not retrieve id parameter, defauling to 'dev?'\n");
    }

    // Add audio capture ports
    if (!addDirPorts(Streaming::Port::E_Capture, samp_freq, optical_in_mode)) {
        return false;
    }

    // Add MIDI port.  The MOTU only has one MIDI input port, with each
    // MIDI byte sent using a 3 byte sequence starting at byte 4 of the
    // event data.
    asprintf(&buff,"%s_cap_MIDI0",id.c_str());
    p = new Streaming::MotuMidiPort(*m_receiveProcessor, buff,
        Streaming::Port::E_Capture, 4);
    if (!p) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n", buff);
    }
    free(buff);

    // example of adding an control port:
//    asprintf(&buff,"%s_cap_%s",id.c_str(),"myportnamehere");
//    p=new Streaming::MotuControlPort(
//            buff,
//            Streaming::Port::E_Capture,
//            0 // you can add all other port specific stuff you
//              // need to pass by extending MotuXXXPort and MotuPortInfo
//    );
//    free(buff);
//
//    if (!p) {
//        debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n",buff);
//    } else {
//
//        if (!m_receiveProcessor->addPort(p)) {
//            debugWarning("Could not register port with stream processor\n");
//            return false;
//        } else {
//            debugOutput(DEBUG_LEVEL_VERBOSE, "Added port %s\n",buff);
//        }
//    }

    // Do the same for the transmit processor
    m_transmitProcessor=new Streaming::MotuTransmitStreamProcessor(*this, event_size_out);

    m_transmitProcessor->setVerboseLevel(getDebugLevel());

    if(!m_transmitProcessor->init()) {
        debugFatal("Could not initialize transmit processor!\n");
        return false;
    }

    // Now we add ports to the processor
    debugOutput(DEBUG_LEVEL_VERBOSE,"Adding ports to transmit processor\n");

    // Add audio playback ports
    if (!addDirPorts(Streaming::Port::E_Playback, samp_freq, optical_out_mode)) {
        return false;
    }

    // Add MIDI port.  The MOTU only has one output MIDI port, with each
    // MIDI byte transmitted using a 3 byte sequence starting at byte 4
    // of the event data.
    asprintf(&buff,"%s_pbk_MIDI0",id.c_str());
    p = new Streaming::MotuMidiPort(*m_transmitProcessor, buff,
        Streaming::Port::E_Capture, 4);
    if (!p) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n", buff);
    }
    free(buff);

    // example of adding an control port:
//    asprintf(&buff,"%s_pbk_%s",id.c_str(),"myportnamehere");
//
//    p=new Streaming::MotuControlPort(
//            buff,
//            Streaming::Port::E_Playback,
//            0 // you can add all other port specific stuff you
//              // need to pass by extending MotuXXXPort and MotuPortInfo
//    );
//    free(buff);
//
//    if (!p) {
//        debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n",buff);
//    } else {
//        if (!m_transmitProcessor->addPort(p)) {
//            debugWarning("Could not register port with stream processor\n");
//            return false;
//        } else {
//            debugOutput(DEBUG_LEVEL_VERBOSE, "Added port %s\n",buff);
//        }
//    }

    return true;
}

int
MotuDevice::getStreamCount() {
     return 2; // one receive, one transmit
}

Streaming::StreamProcessor *
MotuDevice::getStreamProcessorByIndex(int i) {
    switch (i) {
    case 0:
        return m_receiveProcessor;
    case 1:
         return m_transmitProcessor;
    default:
        return NULL;
    }
    return 0;
}

bool
MotuDevice::startStreamByIndex(int i) {

quadlet_t isoctrl = ReadRegister(MOTUFW_REG_ISOCTRL);

    // NOTE: this assumes that you have two streams
    switch (i) {
    case 0:
        // TODO: do the stuff that is nescessary to make the device
        // receive a stream

        // Set the streamprocessor channel to the one obtained by
        // the connection management
        m_receiveProcessor->setChannel(m_iso_recv_channel);

        // Mask out current transmit settings of the MOTU and replace
        // with new ones.  Turn bit 24 on to enable changes to the
        // MOTU's iso transmit settings when the iso control register
        // is written.  Bit 23 enables iso transmit from the MOTU.
        isoctrl &= 0xff00ffff;
        isoctrl |= (m_iso_recv_channel << 16);
        isoctrl |= 0x00c00000;
        WriteRegister(MOTUFW_REG_ISOCTRL, isoctrl);
        break;
    case 1:
        // TODO: do the stuff that is nescessary to make the device
        // transmit a stream

        // Set the streamprocessor channel to the one obtained by
        // the connection management
        m_transmitProcessor->setChannel(m_iso_send_channel);

        // Mask out current receive settings of the MOTU and replace
        // with new ones.  Turn bit 31 on to enable changes to the
        // MOTU's iso receive settings when the iso control register
        // is written.  Bit 30 enables iso receive by the MOTU.
        isoctrl &= 0x00ffffff;
        isoctrl |= (m_iso_send_channel << 24);
        isoctrl |= 0xc0000000;
        WriteRegister(MOTUFW_REG_ISOCTRL, isoctrl);
        break;

    default: // Invalid stream index
        return false;
    }

    return true;
}

bool
MotuDevice::stopStreamByIndex(int i) {

quadlet_t isoctrl = ReadRegister(MOTUFW_REG_ISOCTRL);

    // TODO: connection management: break connection
    // cfr the start function

    // NOTE: this assumes that you have two streams
    switch (i) {
    case 0:
        // Turn bit 22 off to disable iso send by the MOTU.  Turn
        // bit 23 on to enable changes to the MOTU's iso transmit
        // settings when the iso control register is written.
        isoctrl &= 0xffbfffff;
        isoctrl |= 0x00800000;
        WriteRegister(MOTUFW_REG_ISOCTRL, isoctrl);
        break;
    case 1:
        // Turn bit 30 off to disable iso receive by the MOTU.  Turn
        // bit 31 on to enable changes to the MOTU's iso receive
        // settings when the iso control register is written.
        isoctrl &= 0xbfffffff;
        isoctrl |= 0x80000000;
        WriteRegister(MOTUFW_REG_ISOCTRL, isoctrl);
        break;

    default: // Invalid stream index
        return false;
    }

    return true;
}

signed int MotuDevice::getIsoRecvChannel(void) {
    return m_iso_recv_channel;
}

signed int MotuDevice::getIsoSendChannel(void) {
    return m_iso_send_channel;
}

unsigned int MotuDevice::getOpticalMode(unsigned int dir) {
    unsigned int reg = ReadRegister(MOTUFW_REG_ROUTE_PORT_CONF);

debugOutput(DEBUG_LEVEL_VERBOSE, "optical mode: %x %x %x %x\n",dir, reg, reg & MOTUFW_OPTICAL_IN_MODE_MASK,
reg & MOTUFW_OPTICAL_OUT_MODE_MASK);

    if (dir == MOTUFW_DIR_IN)
        return (reg & MOTUFW_OPTICAL_IN_MODE_MASK) >> 8;
    else
        return (reg & MOTUFW_OPTICAL_OUT_MODE_MASK) >> 10;
}

signed int MotuDevice::setOpticalMode(unsigned int dir, unsigned int mode) {
    unsigned int reg = ReadRegister(MOTUFW_REG_ROUTE_PORT_CONF);
    unsigned int opt_ctrl = 0x0000002;

    // Set up the optical control register value according to the current
    // optical port modes.  At this stage it's not completely understood
    // what the "Optical control" register does, so the values it's set to
    // are more or less "magic" numbers.
    if (reg & MOTUFW_OPTICAL_IN_MODE_MASK != (MOTUFW_OPTICAL_MODE_ADAT<<8))
        opt_ctrl |= 0x00000080;
    if (reg & MOTUFW_OPTICAL_OUT_MODE_MASK != (MOTUFW_OPTICAL_MODE_ADAT<<10))
        opt_ctrl |= 0x00000040;

    if (mode & MOTUFW_DIR_IN) {
        reg &= ~MOTUFW_OPTICAL_IN_MODE_MASK;
        reg |= (mode << 8) & MOTUFW_OPTICAL_IN_MODE_MASK;
        if (mode != MOTUFW_OPTICAL_MODE_ADAT)
            opt_ctrl |= 0x00000080;
        else
            opt_ctrl &= ~0x00000080;
    }
    if (mode & MOTUFW_DIR_OUT) {
        reg &= ~MOTUFW_OPTICAL_OUT_MODE_MASK;
        reg |= (mode <<10) & MOTUFW_OPTICAL_OUT_MODE_MASK;
        if (mode != MOTUFW_OPTICAL_MODE_ADAT)
            opt_ctrl |= 0x00000040;
        else
            opt_ctrl &= ~0x00000040;
    }

    // FIXME: there seems to be more to it than this, but for
    // the moment at least this seems to work.
    WriteRegister(MOTUFW_REG_ROUTE_PORT_CONF, reg);
    return WriteRegister(MOTUFW_REG_OPTICAL_CTRL, opt_ctrl);
}

signed int MotuDevice::getEventSize(unsigned int direction) {
//
// Return the size in bytes of a single event sent to (dir==MOTUFW_OUT) or
// from (dir==MOTUFW_IN) the MOTU as part of an iso data packet.
//
// FIXME: for performance it may turn out best to calculate the event
// size in setOpticalMode and cache the result in a data field.  However,
// as it stands this will not adapt to dynamic changes in sample rate - we'd
// need a setFrameRate() for that.
//
// At the very least an event consists of the SPH (4 bytes) and the control/MIDI
// bytes (6 bytes).
// Note that all audio channels are sent using 3 bytes.
signed int sample_rate = getSamplingFrequency();
signed int optical_mode = getOpticalMode(direction);
signed int size = 4+6;

unsigned int i;
unsigned int dir = direction==Streaming::Port::E_Capture?MOTUFW_DIR_IN:MOTUFW_DIR_OUT;
unsigned int flags = (1 << ( optical_mode + 4 ));

    if ( sample_rate > 96000 )
        flags |= MOTUFW_PA_RATE_4x;
    else if ( sample_rate > 48000 )
        flags |= MOTUFW_PA_RATE_2x;
    else
        flags |= MOTUFW_PA_RATE_1x;

    for (i=0; i < ( DevicesProperty[m_motu_model-1].PortsListLength /sizeof( PortEntry ) ); i++) {
        if (( DevicesProperty[m_motu_model-1].PortsList[i].port_dir & dir ) &&
	   ( DevicesProperty[m_motu_model-1].PortsList[i].port_flags & MOTUFW_PA_RATE_MASK & flags ) &&
	   ( DevicesProperty[m_motu_model-1].PortsList[i].port_flags & MOTUFW_PA_OPTICAL_MASK & flags )) {
            size += 3;
        }
    }

    // Finally round size up to the next quadlet boundary
    return ((size+3)/4)*4;
}
/* ======================================================================= */

bool MotuDevice::addPort(Streaming::StreamProcessor *s_processor,
  char *name, enum Streaming::Port::E_Direction direction,
  int position, int size) {
/*
 * Internal helper function to add a MOTU port to a given stream processor.
 * This just saves the unnecessary replication of what is essentially
 * boilerplate code.  Note that the port name is freed by this function
 * prior to exit.
 */
Streaming::Port *p=NULL;

    p = new Streaming::MotuAudioPort(*s_processor, name, direction, position, size);

    if (!p) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n",name);
    }
    free(name);
    return true;
}
/* ======================================================================= */

bool MotuDevice::addDirPorts(
  enum Streaming::Port::E_Direction direction,
  unsigned int sample_rate, unsigned int optical_mode) {
/*
 * Internal helper method: adds all required ports for the given direction
 * based on the indicated sample rate and optical mode.
 *
 * Notes: currently ports are not created if they are disabled due to sample
 * rate or optical mode.  However, it might be better to unconditionally
 * create all ports and just disable those which are not active.
 */
const char *mode_str = direction==Streaming::Port::E_Capture?"cap":"pbk";
Streaming::StreamProcessor *s_processor;
unsigned int i;
char *buff;
unsigned int dir = direction==Streaming::Port::E_Capture?MOTUFW_DIR_IN:MOTUFW_DIR_OUT;
unsigned int flags = (1 << ( optical_mode + 4 ));

    if ( sample_rate > 96000 )
        flags |= MOTUFW_PA_RATE_4x;
    else if ( sample_rate > 48000 )
        flags |= MOTUFW_PA_RATE_2x;
    else
        flags |= MOTUFW_PA_RATE_1x;

    // retrieve the ID
    std::string id=std::string("dev?");
    if(!getOption("id", id)) {
        debugWarning("Could not retrieve id parameter, defauling to 'dev?'\n");
    }

    if (direction == Streaming::Port::E_Capture) {
        s_processor = m_receiveProcessor;
    } else {
        s_processor = m_transmitProcessor;
    }

    for (i=0; i < ( DevicesProperty[m_motu_model-1].PortsListLength /sizeof( PortEntry ) ); i++) {
        if (( DevicesProperty[m_motu_model-1].PortsList[i].port_dir & dir ) &&
	   ( DevicesProperty[m_motu_model-1].PortsList[i].port_flags & MOTUFW_PA_RATE_MASK & flags ) &&
	   ( DevicesProperty[m_motu_model-1].PortsList[i].port_flags & MOTUFW_PA_OPTICAL_MASK & flags )) {
	    asprintf(&buff,"%s_%s_%s" , id.c_str(), mode_str,
              DevicesProperty[m_motu_model-1].PortsList[i].port_name);
            if (!addPort(s_processor, buff, direction, DevicesProperty[m_motu_model-1].PortsList[i].port_offset, 0))
                return false;
        }
    }
    
    return true;
}
/* ======================================================================== */

unsigned int MotuDevice::ReadRegister(unsigned int reg) {
/*
 * Attempts to read the requested register from the MOTU.
 */

  quadlet_t quadlet;

  quadlet = 0;
  // Note: 1394Service::read() expects a physical ID, not the node id
  if (get1394Service().read(0xffc0 | getNodeId(), MOTUFW_BASE_ADDR+reg, 1, &quadlet) < 0) {
    debugError("Error doing motu read from register 0x%06x\n",reg);
  }

  return ntohl(quadlet);
}

signed int MotuDevice::WriteRegister(unsigned int reg, quadlet_t data) {
/*
 * Attempts to write the given data to the requested MOTU register.
 */

  unsigned int err = 0;
  data = htonl(data);

  // Note: 1394Service::write() expects a physical ID, not the node id
  if (get1394Service().write(0xffc0 | getNodeId(), MOTUFW_BASE_ADDR+reg, 1, &data) < 0) {
    err = 1;
    debugError("Error doing motu write to register 0x%06x\n",reg);
  }

  SleepRelativeUsec(100);
  return (err==0)?0:-1;
}

}
