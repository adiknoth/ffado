/*
 * Copyright (C) 2005-2009 by Jonathan Woithe
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

#warning RME support is currently useless (detection only)

#include "rme/rme_avdevice.h"
#include "rme/fireface_def.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "debugmodule/debugmodule.h"

#include <string>
#include <stdint.h>
#include <assert.h>
#include "libutil/ByteSwap.h"

#include <iostream>
#include <sstream>

#include <libraw1394/csr.h>

namespace Rme {

// Template for a RmeDevice object method which intelligently returns a
// register or value applicable to the connected model and warns if something
// isn't quite right.
#define MODEL_SELECTOR(_name,_ff400_arg,_ff800_arg) \
unsigned long long int \
RmeDevice::_name() { \
    switch (m_rme_model) { \
        case RME_MODEL_FIREFACE400: return _ff400_arg; \
        case RME_MODEL_FIREFACE800: return _ff800_arg; \
    default: \
      debugOutput( DEBUG_LEVEL_WARNING, "Bad RME model %d\n", m_rme_model ); \
  } \
  return 0xffffffffffffffffLL; \
}

// to define the supported devices
static VendorModelEntry supportedDeviceList[] =
{
//  {vendor_id, unit_version, model identifier, vendor name, model name,}
    {FW_VENDORID_RME, 0x0001, RME_MODEL_FIREFACE800, "RME", "Fireface-800",},
    {FW_VENDORID_RME, 0x0002, RME_MODEL_FIREFACE400, "RME", "Fireface-400",},
};

RmeDevice::RmeDevice( DeviceManager& d,
                      std::auto_ptr<ConfigRom>( configRom ))
    : FFADODevice( d, configRom )
    , m_model( NULL )
    , m_rme_model( RME_MODEL_NONE )
    , m_ddsFreq( -1 )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Rme::RmeDevice (NodeID %d)\n",
                 getConfigRom().getNodeId() );
}

RmeDevice::~RmeDevice()
{

}

MODEL_SELECTOR(cmd_buffer_addr, RME_FF400_CMD_BUFFER, RME_FF800_CMD_BUFFER)
MODEL_SELECTOR(stream_start_reg, RME_FF400_STREAM_START_REG, RME_FF800_STREAM_START_REG)
MODEL_SELECTOR(stream_end_reg, RME_FF400_STREAM_END_REG, RME_FF800_STREAM_END_REG)
MODEL_SELECTOR(flash_settings_addr, RME_FF400_FLASH_SETTINGS_ADDR, RME_FF800_FLASH_SETTINGS_ADDR)
MODEL_SELECTOR(flash_mixer_vol_addr, RME_FF400_FLASH_MIXER_VOLUME_ADDR, RME_FF800_FLASH_MIXER_VOLUME_ADDR)
MODEL_SELECTOR(flash_mixer_pan_addr, RME_FF400_FLASH_MIXER_PAN_ADDR, RME_FF800_FLASH_MIXER_PAN_ADDR)
MODEL_SELECTOR(flash_mixer_hw_addr, RME_FF400_FLASH_MIXER_HW_ADDR, RME_FF800_FLASH_MIXER_HW_ADDR)

bool
RmeDevice::probe( ConfigRom& configRom, bool generic )
{
    if (generic) return false;
    unsigned int vendorId = configRom.getNodeVendorId();
    unsigned int unitVersion = configRom.getUnitVersion();

    for ( unsigned int i = 0;
          i < ( sizeof( supportedDeviceList )/sizeof( VendorModelEntry ) );
          ++i )
    {
        if ( ( supportedDeviceList[i].vendor_id == vendorId )
             && ( supportedDeviceList[i].unit_version == unitVersion )
           )
        {
            return true;
        }
    }

    return false;
}

FFADODevice *
RmeDevice::createDevice(DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ))
{
    return new RmeDevice(d, configRom );
}

bool
RmeDevice::discover()
{
    unsigned int vendorId = getConfigRom().getNodeVendorId();
    unsigned int unitVersion = getConfigRom().getUnitVersion();

    for ( unsigned int i = 0;
          i < ( sizeof( supportedDeviceList )/sizeof( VendorModelEntry ) );
          ++i )
    {
        if ( ( supportedDeviceList[i].vendor_id == vendorId )
             && ( supportedDeviceList[i].unit_version == unitVersion )
           )
        {
            m_model = &(supportedDeviceList[i]);
            m_rme_model = supportedDeviceList[i].model;
        }
    }

    if (m_model == NULL)
        return false;

    debugOutput( DEBUG_LEVEL_VERBOSE, "found %s %s\n",
        m_model->vendor_name, m_model->model_name);

    init_hardware();
read_device_settings();

    return true;
}

int
RmeDevice::getSamplingFrequency( ) {
/*
 * Retrieve the current sample rate from the RME device.  At this stage it
 * seems that the "current rate" can't be retrieved from the device.  Other
 * drivers don't read the DDS control register and there isn't anywhere else
 * where the frequency is sent back to the PC.  Unless we test the DDS
 * control register for readabilty and find it can be read we'll assume it
 * can't and instead cache the DDS frequency.
 *
 * If the device frequency has not been set this function will return -1
 * (the default value of m_ddsFreq).
 */
    return m_ddsFreq;
}

int
RmeDevice::getConfigurationId()
{
    return 0;
}

bool
RmeDevice::setSamplingFrequency( int samplingFrequency )
{
/*
 * Set the RME device's samplerate.  The RME can do sampling frequencies of
 * 32k, 44.1k and 48k along with the corresponding 2x and 4x rates. 
 * However, it can also do +/- 4% from any of these "base" frequencies.
 * This makes it a little long-winded to work out whether a given frequency
 * is supported or not.
 */

    /* Work out whether the requested rate is supported */
    if (!((samplingFrequency >= 32000*0.96 && samplingFrequency <= 32000*1.04) ||
        (samplingFrequency >= 44100*0.96 && samplingFrequency <= 44100*1.04) ||
        (samplingFrequency >= 48000*0.96 && samplingFrequency <= 48000*1.04) ||
        (samplingFrequency >= 64000*0.96 && samplingFrequency <= 64000*1.04) ||
        (samplingFrequency >= 88200*0.96 && samplingFrequency <= 88200*1.04) ||
        (samplingFrequency >= 96000*0.96 && samplingFrequency <= 96000*1.04) ||
        (samplingFrequency >= 128000*0.96 && samplingFrequency <= 128000*1.04) ||
        (samplingFrequency >= 176000*0.96 && samplingFrequency <= 176000*1.04) ||
        (samplingFrequency >= 192000*0.96 && samplingFrequency <= 192000*1.04))) {
        return false;
    }
    
    /* Send the desired frequency to the RME */
    if (writeRegister(RME_REG_DDS_CONTROL, samplingFrequency) != 0)
      return false;

    m_ddsFreq = samplingFrequency;
    return true;
}

#define RME_CHECK_AND_ADD_SR(v, x) \
    { \
    if (((x >= 32000*0.96 && x <= 32000*1.04) || \
        (x >= 44100*0.96 && x <= 44100*1.04) || \
        (x >= 48000*0.96 && x <= 48000*1.04) || \
        (x >= 64000*0.96 && x <= 64000*1.04) || \
        (x >= 88200*0.96 && x <= 88200*1.04) || \
        (x >= 96000*0.96 && x <= 96000*1.04) || \
        (x >= 128000*0.96 && x <= 128000*1.04) || \
        (x >= 176000*0.96 && x <= 176000*1.04) || \
        (x >= 192000*0.96 && x <= 192000*1.04))) { \
        v.push_back(x); \
    };};

std::vector<int>
RmeDevice::getSupportedSamplingFrequencies()
{
    std::vector<int> frequencies;
    RME_CHECK_AND_ADD_SR(frequencies, 32000);
    RME_CHECK_AND_ADD_SR(frequencies, 44100);
    RME_CHECK_AND_ADD_SR(frequencies, 48000);
    RME_CHECK_AND_ADD_SR(frequencies, 64000);
    RME_CHECK_AND_ADD_SR(frequencies, 88200);
    RME_CHECK_AND_ADD_SR(frequencies, 96000);
    RME_CHECK_AND_ADD_SR(frequencies, 128000);
    RME_CHECK_AND_ADD_SR(frequencies, 176400);
    RME_CHECK_AND_ADD_SR(frequencies, 192000);
    return frequencies;
}

FFADODevice::ClockSourceVector
RmeDevice::getSupportedClockSources() {
    FFADODevice::ClockSourceVector r;
    return r;
}

bool
RmeDevice::setActiveClockSource(ClockSource s) {
    return false;
}

FFADODevice::ClockSource
RmeDevice::getActiveClockSource() {
    ClockSource s;
    return s;
}

bool
RmeDevice::lock() {

    return true;
}


bool
RmeDevice::unlock() {

    return true;
}

void
RmeDevice::showDevice()
{
	debugOutput(DEBUG_LEVEL_VERBOSE,
		"%s %s at node %d\n", m_model->vendor_name, m_model->model_name,
		getNodeId());
}

bool
RmeDevice::prepare() {

	debugOutput(DEBUG_LEVEL_NORMAL, "Preparing RmeDevice...\n" );

	return true;
}

int
RmeDevice::getStreamCount() {
 	return 0; // one receive, one transmit
}

Streaming::StreamProcessor *
RmeDevice::getStreamProcessorByIndex(int i) {
    return NULL;
}

bool
RmeDevice::startStreamByIndex(int i) {
    return false;
}

bool
RmeDevice::stopStreamByIndex(int i) {
    return false;

}

unsigned int 
RmeDevice::readRegister(fb_nodeaddr_t reg) {

    quadlet_t quadlet;
    
    quadlet = 0;
    if (get1394Service().read(0xffc0 | getNodeId(), reg, 1, &quadlet) <= 0) {
        debugError("Error doing RME read from register 0x%06x\n",reg);
    }
    return CondSwapFromBus32(quadlet);
}

signed int 
RmeDevice::readBlock(fb_nodeaddr_t reg, quadlet_t *buf, unsigned int n_quads) {

    unsigned int i;

    if (get1394Service().read(0xffc0 | getNodeId(), reg, n_quads, buf) <= 0) {
        debugError("Error doing RME block read of %d quadlets from register 0x%06x\n",
            n_quads, reg);
        return -1;
    }
    for (i=0; i<n_quads; i++) {
       buf[i] = CondSwapFromBus32(buf[i]);
    }

    return 0;
}

signed int
RmeDevice::writeRegister(fb_nodeaddr_t reg, quadlet_t data) {

    unsigned int err = 0;
    data = CondSwapToBus32(data);
    if (get1394Service().write(0xffc0 | getNodeId(), reg, 1, &data) <= 0) {
        err = 1;
        debugError("Error doing RME write to register 0x%06x\n",reg);
    }
    return (err==0)?0:-1;
}

signed int
RmeDevice::writeBlock(fb_nodeaddr_t reg, quadlet_t *data, unsigned int n_quads) {
//
// Write a block of data to the device starting at address "reg".  Note that
// the conditional byteswap is done "in place" on data, so the contents of
// data may be modified by calling this function.
//
    unsigned int err = 0;
    unsigned int i;

    for (i=0; i<n_quads; i++)
      data[i] = CondSwapToBus32(data[i]);
    if (get1394Service().write(0xffc0 | getNodeId(), reg, n_quads, data) <= 0) {
        err = 1;
        debugError("Error doing RME block write of %d quadlets to register 0x%06x\n",
          n_quads, reg);
    }
    return (err==0)?0:-1;
}
                  
}
