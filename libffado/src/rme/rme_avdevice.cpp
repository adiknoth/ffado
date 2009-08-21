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
#include "rme/fireface_settings_ctrls.h"

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

Device::Device( DeviceManager& d,
                      std::auto_ptr<ConfigRom>( configRom ))
    : FFADODevice( d, configRom )
    , m_rme_model( RME_MODEL_NONE )
    , is_streaming( 0 )
    , m_dds_freq( -1 )
    , m_software_freq( -1 )
    , tco_present( 0 )
    , num_channels( 0 )
    , samples_per_packet( 0 )
    , speed800( 0 )
    , m_MixerContainer( NULL )
    , m_ControlContainer( NULL )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Rme::Device (NodeID %d)\n",
                 getConfigRom().getNodeId() );

    settings = &settings_localobj;
    tco_settings = &tco_settings_localobj;

    memset(&settings, 0, sizeof(settings));
    memset(&tco_settings, 0, sizeof(tco_settings));
}

Device::~Device()
{
    destroyMixer();
}

bool
Device::buildMixer() {
    signed int i;
    bool result = true;

    destroyMixer();
    debugOutput(DEBUG_LEVEL_VERBOSE, "Building an RME mixer...\n");


    // Non-mixer device controls
    m_ControlContainer = new Control::Container(this, "Control");
    if (!m_ControlContainer) {
        debugError("Could not create control container\n");
        destroyMixer();
        return false;
    }

    result &= m_ControlContainer->addElement(
        new RmeSettingsCtrl(*this, RME_CTRL_INFO_MODEL, 0,
            "Model", "Model ID", ""));
    result &= m_ControlContainer->addElement(
        new RmeSettingsCtrl(*this, RME_CTRL_INFO_TCO_PRESENT, 0,
            "TCO_present", "TCO is present", ""));

    result &= m_ControlContainer->addElement(
        new RmeSettingsCtrl(*this, RME_CTRL_PHANTOM_SW, 0,
            "Phantom", "Phantom switches", ""));
    result &= m_ControlContainer->addElement(
        new RmeSettingsCtrl(*this, RME_CTRL_INPUT_LEVEL, 0,
            "Input_level", "Input level", ""));
    result &= m_ControlContainer->addElement(
        new RmeSettingsCtrl(*this, RME_CTRL_OUTPUT_LEVEL, 0,
            "Output_level", "Output level", ""));
    result &= m_ControlContainer->addElement(
        new RmeSettingsCtrl(*this, RME_CTRL_PHONES_LEVEL, 0,
            "Phones_level", "Phones level", ""));

    if (m_rme_model == RME_MODEL_FIREFACE400) {
        // Instrument input options
        for (i=3; i<=4; i++) {
            char path[32], desc[64];
            snprintf(path, sizeof(path), "Chan%d_opt_instr", i);
            snprintf(desc, sizeof(desc), "Chan%d instrument option", i);
            result &= m_ControlContainer->addElement(
                new RmeSettingsCtrl(*this, RME_CTRL_FF400_INSTR_SW, i,
                    path, desc, ""));
            snprintf(path, sizeof(path), "Chan%d_opt_pad", i);
            snprintf(desc, sizeof(desc), "Chan%d pad option", i);
            result &= m_ControlContainer->addElement(
                new RmeSettingsCtrl(*this, RME_CTRL_FF400_PAD_SW, i,
                    path, desc, ""));
        }

        // Input/output gains
        result &= m_ControlContainer->addElement(
            new RmeSettingsMatrixCtrl(*this, RME_MATRIXCTRL_GAINS, "Gains"));
    }

    if (!result) {
        debugWarning("One or more device control elements could not be created\n");
        destroyMixer();
        return false;
    }

    if (!addElement(m_ControlContainer)) {
        debugWarning("Could not register mixer to device\n");
        // clean up
        destroyMixer();
        return false;
    }

    return true;
}

bool
Device::destroyMixer() {
    bool ret = true;
    debugOutput(DEBUG_LEVEL_VERBOSE, "destroy mixer...\n");

    if (m_MixerContainer == NULL) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "no mixer to destroy...\n");
    } else
    if (!deleteElement(m_MixerContainer)) {
        debugError("Mixer present but not registered to the avdevice\n");
        ret = false;
    } else {
        // remove and delete (as in free) child control elements
        m_MixerContainer->clearElements(true);
        delete m_MixerContainer;
        m_MixerContainer = NULL;
    }

    // remove control container
    if (m_ControlContainer == NULL) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "no controls to destroy...\n");
    } else
    if (!deleteElement(m_ControlContainer)) {
        debugError("Controls present but not registered to the avdevice\n");
        ret = false;
    } else {
        // remove and delete (as in free) child control elements
        m_ControlContainer->clearElements(true);
        delete m_ControlContainer;
        m_ControlContainer = NULL;
    }

    return false;
}

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
    debugOutput(DEBUG_LEVEL_VERBOSE, "TCO present: %s\n",
      tco_present?"yes":"no");

    // Find out the device's streaming status
    is_streaming = hardware_is_streaming();

    init_hardware();

    if (!buildMixer()) {
        debugWarning("Could not build mixer\n");
    }

    // This is just for testing
    read_device_flash_settings(NULL);

    return true;
}

int
Device::getSamplingFrequency( ) {

    // Retrieve the current sample rate.  For practical purposes this
    // is the software rate currently in use.
    return m_software_freq;
}

int
Device::getConfigurationId()
{
    return 0;
}

bool
Device::setDDSFrequency( int dds_freq )
{
    // Set a fixed DDS frequency.  If the device is the clock master this
    // will immediately be copied to the hardware DDS register.  Otherwise
    // it will take effect as required at the time the sampling rate is 
    // changed or streaming is started.

    // If the device is streaming, the new DDS rate must have the same
    // multiplier as the software sample rate
    if (hardware_is_streaming()) {
        if (multiplier_of_freq(dds_freq) != multiplier_of_freq(m_software_freq))
            return false;
    }

    m_dds_freq = dds_freq;
    if (settings->clock_mode == FF_STATE_CLOCKMODE_MASTER) {
        if (set_hardware_dds_freq(dds_freq) != 0)
            return false;
    }

    return true;
}

bool
Device::setSamplingFrequency( int samplingFrequency )
{
    // Request a sampling rate on behalf of software.  Software is limited
    // to sample rates of 32k, 44.1k, 48k and the 2x/4x multiples of these. 
    // The user may lock the device to a much wider range of frequencies via
    // the explicit DDS controls in the control panel.  If the explicit DDS
    // control is active the software is limited to the "standard" speeds
    // corresponding to the multiplier in use by the DDS.
    //
    // Similarly, if the device is externally clocked the software is 
    // limited to the external clock frequency.
    //
    // Otherwise the software has free choice of the software speeds noted
    // above.

    bool ret = -1;
    signed int i, j;
    signed int mult[3] = {1, 2, 4};
    signed int base_freq[3] = {32000, 44100, 48000};
    signed int freq = samplingFrequency;
    FF_state_t state;
    signed int fixed_freq = 0;

    get_hardware_state(&state);

    // If device is locked to a frequency via external clock, explicit
    // setting of the DDS or by virtue of streaming being active, get that
    // frequency.
    if (state.clock_mode == FF_STATE_CLOCKMODE_AUTOSYNC) {
        // FIXME: if synced to TCO, is autosync_freq valid?
        fixed_freq = state.autosync_freq;
    } else
    if (m_dds_freq > 0) {
        fixed_freq = m_dds_freq;
    } else
    if (hardware_is_streaming()) {
        fixed_freq = m_software_freq;
    }

    // If the device is running to a fixed frequency, software can only
    // request frequencies with the same multiplier.  Similarly, the
    // multiplier is locked in "master" clock mode if the device is
    // streaming.
    if (fixed_freq > 0) {
        signed int fixed_mult = multiplier_of_freq(fixed_freq);
        if (multiplier_of_freq(freq) != multiplier_of_freq(fixed_freq))
            return -1;
        for (j=0; j<3; j++) {
            if (freq == base_freq[j]*fixed_mult) {
                ret = 0;
                break;
            }
        }
    } else {
        for (i=0; i<3; i++) {
            for (j=0; j<3; j++) {
                if (freq == base_freq[j]*mult[i]) {
                    ret = 0;
                    break;
                }
            }
        }
    }
    // If requested frequency is unavailable, return -1
    if (ret == -1)
        return false;

    // If a DDS frequency has been explicitly requested this is always
    // used to programm the hardware DDS regardless of the rate requested
    // by the software.  Otherwise we use the requested sampling rate.
    if (m_dds_freq > 0)
        freq = m_dds_freq;
    if (set_hardware_dds_freq(freq) != 0)
        return false;

    m_software_freq = samplingFrequency;
    return true;
}

std::vector<int>
Device::getSupportedSamplingFrequencies()
{
    std::vector<int> frequencies;
    signed int i, j;
    signed int mult[3] = {1, 2, 4};
    signed int freq[3] = {32000, 44100, 48000};
    FF_state_t state;

    get_hardware_state(&state);

    // Generate the list of supported frequencies.  If the device is
    // externally clocked the frequency is limited to the external clock
    // frequency.  If the device is running the multiplier is fixed.
    if (state.clock_mode == FF_STATE_CLOCKMODE_AUTOSYNC) {
        // FIXME: if synced to TCO, is autosync_freq valid?
        frequencies.push_back(state.autosync_freq);
    } else
    if (state.is_streaming) {
        unsigned int fixed_mult = multiplier_of_freq(m_software_freq);
        for (j=0; j<3; j++) {
            frequencies.push_back(freq[j]*fixed_mult);
        }
    } else {
        for (i=0; i<3; i++) {
            for (j=0; j<3; j++) {
                frequencies.push_back(freq[j]*mult[i]);
            }
        }
    }
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
