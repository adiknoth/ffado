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

#warning RME support is at an early development stage and is not functional

#include "rme/rme_avdevice.h"
#include "rme/fireface_def.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "debugmodule/debugmodule.h"

#include "devicemanager.h"

#include <string>
#include <stdint.h>
#include <assert.h>
#include "libutil/ByteSwap.h"

#include <iostream>
#include <sstream>

#include <libraw1394/csr.h>

// Known values for the unit version of RME devices
#define RME_UNITVERSION_FF800  0x0001
#define RME_UNITVERSION_FF400  0x0002

namespace Rme {

// The RME devices expect async packet data in little endian format (as
// opposed to bus order, which is big endian).  Therefore define our own
// 32-bit byteswap function to do this.
#if __BYTE_ORDER == __BIG_ENDIAN
static inline uint32_t
ByteSwapToDevice32(uint32_t d)
{
    return byteswap_32(d);
}
ByteSwapFromDevice32(uint32_t d)
{
    return byteswap_32(d);
}
#else
static inline uint32_t
ByteSwapToDevice32(uint32_t d)
{
    return d;
}
static inline uint32_t
ByteSwapFromDevice32(uint32_t d)
{
    return d;
}
#endif

// Template for an RME Device object method which intelligently returns a
// register or value applicable to the connected model and warns if something
// isn't quite right.
#define MODEL_SELECTOR(_name,_ff400_arg,_ff800_arg) \
unsigned long long int \
Device::_name() { \
    switch (m_rme_model) { \
        case RME_MODEL_FIREFACE400: return _ff400_arg; \
        case RME_MODEL_FIREFACE800: return _ff800_arg; \
    default: \
      debugOutput( DEBUG_LEVEL_WARNING, "Bad RME model %d\n", m_rme_model ); \
  } \
  return 0xffffffffffffffffLL; \
}

Device::Device( DeviceManager& d,
                      std::auto_ptr<ConfigRom>( configRom ))
    : FFADODevice( d, configRom )
    , m_rme_model( RME_MODEL_NONE )
    , m_ddsFreq( -1 )
    , tco_present( 0 )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Rme::Device (NodeID %d)\n",
                 getConfigRom().getNodeId() );
}

Device::~Device()
{

}

MODEL_SELECTOR(cmd_buffer_addr, RME_FF400_CMD_BUFFER, RME_FF800_CMD_BUFFER)
MODEL_SELECTOR(stream_init_reg, RME_FF400_STREAM_INIT_REG, RME_FF800_STREAM_INIT_REG)
MODEL_SELECTOR(stream_start_reg, RME_FF400_STREAM_START_REG, RME_FF800_STREAM_START_REG)
MODEL_SELECTOR(stream_end_reg, RME_FF400_STREAM_END_REG, RME_FF800_STREAM_END_REG)
MODEL_SELECTOR(flash_settings_addr, RME_FF400_FLASH_SETTINGS_ADDR, RME_FF800_FLASH_SETTINGS_ADDR)
MODEL_SELECTOR(flash_mixer_vol_addr, RME_FF400_FLASH_MIXER_VOLUME_ADDR, RME_FF800_FLASH_MIXER_VOLUME_ADDR)
MODEL_SELECTOR(flash_mixer_pan_addr, RME_FF400_FLASH_MIXER_PAN_ADDR, RME_FF800_FLASH_MIXER_PAN_ADDR)
MODEL_SELECTOR(flash_mixer_hw_addr, RME_FF400_FLASH_MIXER_HW_ADDR, RME_FF800_FLASH_MIXER_HW_ADDR)

bool
Device::probe( Util::Configuration& c, ConfigRom& configRom, bool generic )
{
    if (generic) {
        return false;
    } else {
        // check if device is in supported devices list.  Note that the RME
        // devices use the unit version to identify the individual devices. 
        // To avoid having to extend the configuration file syntax to
        // include this at this point, we'll use the configuration file
        // model ID to test against the device unit version.  This can be
        // tidied up if the configuration file is extended at some point to
        // include the unit version.
        unsigned int vendorId = configRom.getNodeVendorId();
        unsigned int unitVersion = configRom.getUnitVersion();

        Util::Configuration::VendorModelEntry vme = c.findDeviceVME( vendorId, unitVersion );
        return c.isValid(vme) && vme.driver == Util::Configuration::eD_RME;
    }
}

FFADODevice *
Device::createDevice(DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ))
{
    return new Device(d, configRom );
}

bool
Device::discover()
{
    unsigned int vendorId = getConfigRom().getNodeVendorId();
    // See note in Device::probe() about why we use the unit version here.
    unsigned int unitVersion = getConfigRom().getUnitVersion();

    Util::Configuration &c = getDeviceManager().getConfiguration();
    Util::Configuration::VendorModelEntry vme = c.findDeviceVME( vendorId, unitVersion );

    if (c.isValid(vme) && vme.driver == Util::Configuration::eD_RME) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "found %s %s\n",
                     vme.vendor_name.c_str(),
                     vme.model_name.c_str());
    } else {
        debugWarning("Device '%s %s' unsupported by RME driver (no generic RME support)\n",
                     getConfigRom().getVendorName().c_str(), getConfigRom().getModelName().c_str());
    }

    if (unitVersion == RME_UNITVERSION_FF800) {
        m_rme_model = RME_MODEL_FIREFACE800;
    } else
    if (unitVersion == RME_MODEL_FIREFACE400) {
        m_rme_model = RME_MODEL_FIREFACE400;
    } else {
        debugError("Unsupported model\n");
        return false;
    }

    // If device is FF800, check to see if the TCO is fitted
    if (m_rme_model == RME_MODEL_FIREFACE800) {
        tco_present = (read_tco(NULL, 0) == 0);
    }

    init_hardware();

    // This is just for testing
    read_device_settings();

    return true;
}

int
Device::getSamplingFrequency( ) {
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
Device::getConfigurationId()
{
    return 0;
}

bool
Device::setSamplingFrequency( int samplingFrequency )
{
/*
 * Set the RME device's samplerate.  The RME can do sampling frequencies of
 * 32k, 44.1k and 48k along with the corresponding 2x and 4x rates. 
 * However, it can also do +/- 4% from any of these "base" frequencies using
 * its DDS.  This makes it a little long-winded to work out whether a given
 * frequency is supported or not.
 *
 * This function is concerned with setting the device up for streaming, so the
 * register we want to write to is in fact the streaming sample rate portion of
 * the streaming initialisation function (as opposed to the DDS frequency register
 * which is distinct on the FF800.  How the FF800's DDS register will ultimately
 * be controlled is yet to be determined.
 */

    /* Work out whether the requested rate is supported */
    /* FIXME: the +/- 4% range is only doable if the DDS is engaged */
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
    if (writeRegister(stream_init_reg(), samplingFrequency) != 0)
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
Device::getSupportedSamplingFrequencies()
{
    std::vector<int> frequencies;
    /* FIXME: the +/- 4% frequency range is only doable if the DDS is
     * engaged.
     */
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

	debugOutput(DEBUG_LEVEL_NORMAL, "Preparing Device...\n" );

	return true;
}

int
Device::getStreamCount() {
 	return 0; // one receive, one transmit
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

unsigned int 
Device::readRegister(fb_nodeaddr_t reg) {

    quadlet_t quadlet;
    
    quadlet = 0;
    if (get1394Service().read(0xffc0 | getNodeId(), reg, 1, &quadlet) <= 0) {
        debugError("Error doing RME read from register 0x%06x\n",reg);
    }
    return ByteSwapFromDevice32(quadlet);
}

signed int 
Device::readBlock(fb_nodeaddr_t reg, quadlet_t *buf, unsigned int n_quads) {

    unsigned int i;

    if (get1394Service().read(0xffc0 | getNodeId(), reg, n_quads, buf) <= 0) {
        debugError("Error doing RME block read of %d quadlets from register 0x%06x\n",
            n_quads, reg);
        return -1;
    }
    for (i=0; i<n_quads; i++) {
       buf[i] = ByteSwapFromDevice32(buf[i]);
    }

    return 0;
}

signed int
Device::writeRegister(fb_nodeaddr_t reg, quadlet_t data) {

    unsigned int err = 0;
    data = ByteSwapToDevice32(data);
    if (get1394Service().write(0xffc0 | getNodeId(), reg, 1, &data) <= 0) {
        err = 1;
        debugError("Error doing RME write to register 0x%06x\n",reg);
    }
    return (err==0)?0:-1;
}

signed int
Device::writeBlock(fb_nodeaddr_t reg, quadlet_t *data, unsigned int n_quads) {
//
// Write a block of data to the device starting at address "reg".  Note that
// the conditional byteswap is done "in place" on data, so the contents of
// data may be modified by calling this function.
//
    unsigned int err = 0;
    unsigned int i;

    for (i=0; i<n_quads; i++) {
      data[i] = ByteSwapToDevice32(data[i]);
    }
    if (get1394Service().write(0xffc0 | getNodeId(), reg, n_quads, data) <= 0) {
        err = 1;
        debugError("Error doing RME block write of %d quadlets to register 0x%06x\n",
          n_quads, reg);
    }
    return (err==0)?0:-1;
}
                  
}
