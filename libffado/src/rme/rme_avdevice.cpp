/*
 * Copyright (C) 2005-2012 by Jonathan Woithe
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

#include "config.h"

#include "rme/rme_avdevice.h"
#include "rme/fireface_def.h"
#include "rme/fireface_settings_ctrls.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"
#include "libieee1394/IsoHandlerManager.h"

#include "debugmodule/debugmodule.h"

#include "libstreaming/rme/RmePort.h"

#include "devicemanager.h"

#include <string>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include "libutil/ByteSwap.h"

#include <iostream>
#include <sstream>

#include <libraw1394/csr.h>

// Known values for the unit version of RME devices
#define RME_UNITVERSION_FF800  0x0001
#define RME_UNITVERSION_FF400  0x0002
#define RME_UNITVERSION_UFX    0x0003
#define RME_UNITVERSION_UCX    0x0004

namespace Rme {

// The RME devices expect packet data in little endian format (as
// opposed to bus order, which is big endian).  Therefore define our own
// 32-bit byteswap function to account for this.
#if __BYTE_ORDER == __BIG_ENDIAN
#define RME_BYTESWAP32(x)       ByteSwap32(x)
#else
#define RME_BYTESWAP32(x)       (x)
#endif

static inline uint32_t
ByteSwapToDevice32(uint32_t d)
{
    return RME_BYTESWAP32(d);
}
static inline uint32_t
ByteSwapFromDevice32(uint32_t d)
{
    return RME_BYTESWAP32(d);
}

Device::Device( DeviceManager& d,
                      std::auto_ptr<ConfigRom>( configRom ))
    : FFADODevice( d, configRom )
    , m_rme_model( RME_MODEL_NONE )
    , settings( NULL )
    , tco_settings( NULL )
    , dev_config ( NULL )
    , num_channels( 0 )
    , frames_per_packet( 0 )
    , speed800( 0 )
    , provide_midi( 0 )
    , iso_tx_channel( -1 )
    , iso_rx_channel( -1 )
    , m_receiveProcessor( NULL )
    , m_transmitProcessor( NULL )
    , m_MixerContainer( NULL )
    , m_ControlContainer( NULL )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Rme::Device (NodeID %d)\n",
                 getConfigRom().getNodeId() );
}

Device::~Device()
{
    delete m_receiveProcessor;
    delete m_transmitProcessor;

    if (iso_tx_channel>=0 && !get1394Service().freeIsoChannel(iso_tx_channel)) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Could not free tx iso channel %d\n", iso_tx_channel);
    }
    if (iso_rx_channel>=0 && m_rme_model==RME_MODEL_FIREFACE400 &&
            !get1394Service().freeIsoChannel(iso_rx_channel)) {
        // FF800 handles the rx channel itself
        debugOutput(DEBUG_LEVEL_VERBOSE, "Could not free rx iso channel %d\n", iso_rx_channel);
    }

    destroyMixer();

    if (dev_config != NULL) {
        switch (rme_shm_close(dev_config)) {
            case RSO_CLOSE:
                debugOutput( DEBUG_LEVEL_VERBOSE, "Configuration shared data object closed\n");
                break;
            case RSO_CLOSE_DELETE:
                debugOutput( DEBUG_LEVEL_VERBOSE, "Configuration shared data object closed and deleted (no other users)\n");
                break;
        }
    }
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
        new RmeSettingsCtrl(*this, RME_CTRL_INFO_SYSCLOCK_MODE, 0,
            "sysclock_mode", "System clock mode", ""));
    result &= m_ControlContainer->addElement(
        new RmeSettingsCtrl(*this, RME_CTRL_INFO_SYSCLOCK_FREQ, 0,
            "sysclock_freq", "System clock frequency", ""));
    result &= m_ControlContainer->addElement(
        new RmeSettingsCtrl(*this, RME_CTRL_INFO_AUTOSYNC_FREQ, 0,
            "autosync_freq", "Autosync frequency", ""));
    result &= m_ControlContainer->addElement(
        new RmeSettingsCtrl(*this, RME_CTRL_INFO_AUTOSYNC_SRC, 0,
            "autosync_src", "Autosync source", ""));
    result &= m_ControlContainer->addElement(
        new RmeSettingsCtrl(*this, RME_CTRL_INFO_SYNC_STATUS, 0,
            "sync_status", "Sync status", ""));
    result &= m_ControlContainer->addElement(
        new RmeSettingsCtrl(*this, RME_CTRL_INFO_SPDIF_FREQ, 0,
            "spdif_freq", "SPDIF frequency", ""));

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
        new RmeSettingsCtrl(*this, RME_CTRL_SPDIF_INPUT_MODE, 0,
            "SPDIF_input_mode", "SPDIF input mode", ""));
    result &= m_ControlContainer->addElement(
        new RmeSettingsCtrl(*this, RME_CTRL_SPDIF_OUTPUT_OPTICAL, 0,
            "SPDIF_output_optical", "SPDIF output optical", ""));
    result &= m_ControlContainer->addElement(
        new RmeSettingsCtrl(*this, RME_CTRL_SPDIF_OUTPUT_EMPHASIS, 0,
            "SPDIF_output_emphasis", "SPDIF output emphasis", ""));
    result &= m_ControlContainer->addElement(
        new RmeSettingsCtrl(*this, RME_CTRL_SPDIF_OUTPUT_PRO, 0,
            "SPDIF_output_pro", "SPDIF output pro", ""));
    result &= m_ControlContainer->addElement(
        new RmeSettingsCtrl(*this, RME_CTRL_SPDIF_OUTPUT_NONAUDIO, 0,
            "SPDIF_output_nonaudio", "SPDIF output non-audio", ""));
    result &= m_ControlContainer->addElement(
        new RmeSettingsCtrl(*this, RME_CTRL_PHONES_LEVEL, 0,
            "Phones_level", "Phones level", ""));

    result &= m_ControlContainer->addElement(
        new RmeSettingsCtrl(*this, RME_CTRL_CLOCK_MODE, 0,
            "Clock_mode", "Clock mode", ""));
    result &= m_ControlContainer->addElement(
        new RmeSettingsCtrl(*this, RME_CTRL_SYNC_REF, 0,
            "Sync_ref", "Preferred sync ref", ""));
    result &= m_ControlContainer->addElement(
        new RmeSettingsCtrl(*this, RME_CTRL_LIMIT_BANDWIDTH, 0,
            "Bandwidth_limit", "Bandwidth limit", ""));

    result &= m_ControlContainer->addElement(
        new RmeSettingsCtrl(*this, RME_CTRL_FLASH, 0,
            "Flash_control", "Flash control", ""));
    result &= m_ControlContainer->addElement(
        new RmeSettingsCtrl(*this, RME_CTRL_MIXER_PRESET, 0,
            "Mixer_preset", "Mixer preset", ""));

    if (m_rme_model == RME_MODEL_FIREFACE800) {
        result &= m_ControlContainer->addElement(
            new RmeSettingsCtrl(*this, RME_CTRL_INPUT_SOURCE, 1,
                "Chan1_source", "Channel 1 source", ""));
        result &= m_ControlContainer->addElement(
            new RmeSettingsCtrl(*this, RME_CTRL_INPUT_SOURCE, 7,
                "Chan7_source", "Channel 7 source", ""));
        result &= m_ControlContainer->addElement(
            new RmeSettingsCtrl(*this, RME_CTRL_INPUT_SOURCE, 8,
                "Chan8_source", "Channel 8 source", ""));
        result &= m_ControlContainer->addElement(
            new RmeSettingsCtrl(*this, RME_CTRL_INSTRUMENT_OPTIONS, 1,
                "Chan1_instr_opts", "Input instrument options channel 1", ""));
    }

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

    /* Mixer controls */
    m_MixerContainer = new Control::Container(this, "Mixer");
    if (!m_MixerContainer) {
        debugError("Could not create mixer container\n");
        destroyMixer();
        return false;
    }

    result &= m_MixerContainer->addElement(
        new RmeSettingsMatrixCtrl(*this, RME_MATRIXCTRL_INPUT_FADER, "InputFaders"));
    result &= m_MixerContainer->addElement(
        new RmeSettingsMatrixCtrl(*this, RME_MATRIXCTRL_PLAYBACK_FADER, "PlaybackFaders"));
    result &= m_MixerContainer->addElement(
        new RmeSettingsMatrixCtrl(*this, RME_MATRIXCTRL_OUTPUT_FADER, "OutputFaders"));
    result &= m_MixerContainer->addElement(
        new RmeSettingsMatrixCtrl(*this, RME_MATRIXCTRL_INPUT_MUTE, "InputMutes"));
    result &= m_MixerContainer->addElement(
        new RmeSettingsMatrixCtrl(*this, RME_MATRIXCTRL_PLAYBACK_MUTE, "PlaybackMutes"));
    result &= m_MixerContainer->addElement(
        new RmeSettingsMatrixCtrl(*this, RME_MATRIXCTRL_OUTPUT_MUTE, "OutputMutes"));
    result &= m_MixerContainer->addElement(
        new RmeSettingsMatrixCtrl(*this, RME_MATRIXCTRL_INPUT_INVERT, "InputInverts"));
    result &= m_MixerContainer->addElement(
        new RmeSettingsMatrixCtrl(*this, RME_MATRIXCTRL_PLAYBACK_INVERT, "PlaybackInverts"));

    if (!result) {
        debugWarning("One or more device control/mixer elements could not be created\n");
        destroyMixer();
        return false;
    }

    if (!addElement(m_ControlContainer) || !addElement(m_MixerContainer)) {
        debugWarning("Could not register controls/mixer to device\n");
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

    return ret;
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
    signed int i;
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

    switch (unitVersion) {
      case RME_UNITVERSION_FF800: m_rme_model = RME_MODEL_FIREFACE800; break;
      case RME_UNITVERSION_FF400: m_rme_model = RME_MODEL_FIREFACE400; break;
      case RME_UNITVERSION_UFX: m_rme_model = RME_MODEL_FIREFACE_UFX; break;
      case RME_UNITVERSION_UCX: m_rme_model = RME_MODEL_FIREFACE_UCX; break;
      default:
        debugError("Unsupported model\n");
        return false;
    }

    if (m_rme_model==RME_MODEL_FIREFACE_UFX || m_rme_model==RME_MODEL_FIREFACE_UCX) {
      debugError("Fireface UFX/UCX are not currently supported\n");
      return false;
    }

    // Set up the shared data object for configuration data
    i = rme_shm_open(&dev_config);
    if (i == RSO_OPEN_CREATED) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "New configuration shared data object created\n");
    } else
    if (i == RSO_OPEN_ATTACHED) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "Attached to existing configuration shared data object\n");
    }
    if (dev_config == NULL) {
        debugOutput( DEBUG_LEVEL_WARNING, "Could not create/access shared configuration memory object, using process-local storage\n");
        memset(&local_dev_config_obj, 0, sizeof(local_dev_config_obj));
        dev_config = &local_dev_config_obj;
    }
    settings = &dev_config->settings;
    tco_settings = &dev_config->tco_settings;

    // If device is FF800, check to see if the TCO is fitted
    if (m_rme_model == RME_MODEL_FIREFACE800) {
        dev_config->tco_present = (read_tco(NULL, 0) == 0);
    }
    debugOutput(DEBUG_LEVEL_VERBOSE, "TCO present: %s\n",
      dev_config->tco_present?"yes":"no");

    init_hardware();

    if (!buildMixer()) {
        debugWarning("Could not build mixer\n");
    }

    return true;
}

int
Device::getSamplingFrequency( ) {

    // Retrieve the current sample rate.  For practical purposes this
    // is the software rate currently in use if in master clock mode, or
    // the external clock if in slave mode.
    //
    // If dds_freq functionality is pursued, some thinking will be required
    // here because the streaming engine will take its timings from the
    // value returned by this function.  If the DDS is not running at
    // software_freq, returning software_freq won't work for the streaming
    // engine.  User software, on the other hand, would require the
    // software_freq value.  Ultimately the streaming engine will probably
    // have to be changed to obtain the "real" sample rate through other
    // means.

    // The kernel (as of 3.10 at least) seems to crash with an out-of-memory
    // condition if this function calls get_hardware_state() too frequently
    // (for example, several times per iso cycle).  The code of the RME
    // driver should be structured in such a way as to prevent such calls
    // from the fast path, but it's always possible that other components
    // will call into this function when streaming is active (ffado-mixer
    // for instance.  In such cases return the software frequency as a proxy
    // for the true rate.

    if (hardware_is_streaming()) {
        return dev_config->software_freq;
    }

    FF_state_t state;
    if (get_hardware_state(&state) != 0) {
        debugOutput(DEBUG_LEVEL_ERROR, "failed to read device state\n");
        return 0;
    }
    if (state.clock_mode == FF_STATE_CLOCKMODE_AUTOSYNC) {
        // Note: this could return 0 if there is no valid external clock
        return state.autosync_freq;
    }

    return dev_config->software_freq;
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
    // multiplier as the software sample rate.
    //
    // Since FFADO doesn't make use of the dds_freq functionality at present
    // (there being no user access provided for this) the effect of changing
    // the hardware DDS while streaming is active has not been tested.  A
    // new DDS value will change the timestamp intervals applicable to the
    // streaming engine, so an alteration here without at least a restart of
    // the streaming will almost certainly cause trouble.  Initially it may
    // be easiest to disallow such changes when streaming is active.
    if (hardware_is_streaming()) {
        if (multiplier_of_freq(dds_freq) != multiplier_of_freq(dev_config->software_freq))
            return false;
    }

    dev_config->dds_freq = dds_freq;
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

    bool ret = false;
    signed int i, j;
    signed int mult[3] = {1, 2, 4};
    unsigned int base_freq[3] = {32000, 44100, 48000};
    unsigned int freq = samplingFrequency;
    FF_state_t state;
    signed int fixed_freq = 0;

    if (get_hardware_state(&state) != 0) {
          debugOutput(DEBUG_LEVEL_ERROR, "failed to read device state\n");
          return false;
    }

    // If device is locked to a frequency via external clock, explicit
    // setting of the DDS or by virtue of streaming being active, get that
    // frequency.
    if (state.clock_mode == FF_STATE_CLOCKMODE_AUTOSYNC) {
        // The autosync frequency can be retrieved from state.autosync_freq. 
        // An autosync_freq value of 0 indicates the absence of a valid
        // external clock.  Allow sampling frequencies which match the
        // sync rate and reject all others.
        //
        // A further note: if synced to TCO, is autosync_freq valid?
        if (state.autosync_freq == 0) {
            debugOutput(DEBUG_LEVEL_ERROR, "slave clock mode active but no valid external clock present\n");
        }
        if (state.autosync_freq==0 || (int)state.autosync_freq!=samplingFrequency)
            return false;
        dev_config->software_freq = samplingFrequency;
        return true;
    } else
    if (dev_config->dds_freq > 0) {
        fixed_freq = dev_config->dds_freq;
    } else
    if (hardware_is_streaming()) {
        // See comments in getSamplingFrequency() as to why this may not
        // be successful in the long run.
        fixed_freq = dev_config->software_freq;
    }

    // If the device is running to a fixed frequency, software can only
    // request frequencies with the same multiplier.  Similarly, the
    // multiplier is locked in "master" clock mode if the device is
    // streaming.
    if (fixed_freq > 0) {
        unsigned int fixed_mult = multiplier_of_freq(fixed_freq);
        if (multiplier_of_freq(freq) != fixed_mult) {
            debugOutput(DEBUG_LEVEL_ERROR, "DDS currently set to %d Hz, new sampling rate %d does not have the same multiplier\n",
              fixed_freq, freq);
            return false;
        }
        for (j=0; j<3; j++) {
            if (freq == base_freq[j]*fixed_mult) {
                ret = true;
                break;
            }
        }
    } else {
        for (i=0; i<3; i++) {
            for (j=0; j<3; j++) {
                if (freq == base_freq[j]*mult[i]) {
                    ret = true;
                    break;
                }
            }
        }
    }
    // If requested frequency is unavailable, return false
    if (ret == false) {
        debugOutput(DEBUG_LEVEL_ERROR, "requested sampling rate %d Hz not available\n", freq);
        return false;
    }

    // If a DDS frequency has been explicitly requested this is always used
    // to program the hardware DDS regardless of the rate requested by the
    // software (such use of the DDS is only possible if the Fireface is
    // operating in master clock mode).  Otherwise we use the requested
    // sampling rate.
    if (dev_config->dds_freq>0 && state.clock_mode==FF_STATE_CLOCKMODE_MASTER)
        freq = dev_config->dds_freq;
    if (set_hardware_dds_freq(freq) != 0) {
        debugOutput(DEBUG_LEVEL_ERROR, "failed to set hardware sample rate to %d Hz\n", freq);
        return false;
    }

    debugOutput(DEBUG_LEVEL_VERBOSE, "hardware set to sampling frequency %d Hz\n", samplingFrequency);
    dev_config->software_freq = samplingFrequency;
    settings->sample_rate = samplingFrequency;
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

    if (get_hardware_state(&state) != 0) {
        debugOutput(DEBUG_LEVEL_ERROR, "failed to read device state\n");
        return frequencies;
    }

    // Generate the list of supported frequencies.  If the device is
    // externally clocked the frequency is limited to the external clock
    // frequency.
    if (state.clock_mode == FF_STATE_CLOCKMODE_AUTOSYNC) {
        // FIXME: if synced to TCO, is autosync_freq valid?
        // The autosync frequency will be zero if no valid clock is available
        frequencies.push_back(state.autosync_freq);
    } else
    // If the device is running the multiplier is fixed.
    if (state.is_streaming) {
        // It's not certain that permitting rate changes while streaming
        // is active will work.  See comments in setSamplingFrequency() and
        // elsewhere.
        unsigned int fixed_mult = multiplier_of_freq(dev_config->software_freq);
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

// The RME clock source selection logic is a little more complex than a
// simple list can cater for.  Therefore we just put in a placeholder and
// rely on the extended controls in ffado-mixer to deal with the details.
//
FFADODevice::ClockSource
Device::dummyClockSource(void) {
    ClockSource s;
    s.id = 0;
    s.type = eCT_Internal;
    s.description = "Selected via device controls";
    s.valid = s.active = s.locked = true;
    s.slipping = false;
    return s;
}
FFADODevice::ClockSourceVector
Device::getSupportedClockSources() {
    FFADODevice::ClockSourceVector r;
    ClockSource s;
    s = dummyClockSource();
    r.push_back(s);
    return r;
}
bool
Device::setActiveClockSource(ClockSource s) {
    return true;
}
FFADODevice::ClockSource
Device::getActiveClockSource() {
    return dummyClockSource();
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
Device::resetForStreaming() {
    signed int err;

    signed int iso_rx;
    unsigned int stat[4];
    signed int i;

    // Ensure the transmit processor is ready to start streaming.  When
    // this function is called from prepare() the transmit processor
    // won't be allocated.
    if (m_transmitProcessor != NULL)
        m_transmitProcessor->resetForStreaming();

    // Whenever streaming is restarted hardware_init_streaming() needs to be
    // called.  Otherwise the device won't start sending data when data is
    // sent to it and the rx stream will fail to start.
    err = hardware_init_streaming(dev_config->hardware_freq, iso_tx_channel) != 0;
    if (err) {
        debugFatal("Could not intialise device streaming system\n");
        return false;
    }

    i = 0;
    while (i < 100) {
        err = (get_hardware_streaming_status(stat, 4) != 0);
        if (err) {
            debugFatal("error reading status register\n");
            break;
        }

        debugOutput(DEBUG_LEVEL_VERBOSE, "rme init stat: %08x %08x %08x %08x\n",
            stat[0], stat[1], stat[2], stat[3]);

        if (m_rme_model == RME_MODEL_FIREFACE400) {
            break;
        }

        // The Fireface-800 chooses its tx channel (our rx channel).  Wait
        // for the device busy flag to clear, then confirm that the rx iso
        // channel hasn't changed (it shouldn't across a restart).
        if (stat[2] == 0xffffffff) {
            // Device not ready; wait 5 ms and try again
            usleep(5000);
            i++;
        } else {
            iso_rx = stat[2] & 63;
            if (iso_rx!=iso_rx_channel && iso_rx_channel!=-1)
                debugOutput(DEBUG_LEVEL_WARNING, "rx iso: now %d, was %d\n",
                    iso_rx, iso_rx_channel);
            iso_rx_channel = iso_rx;

            // Even if the rx channel has changed, the device takes care of
            // registering the channel itself, so we don't have to (neither
            // do we have to release the old one).  If we try to call
            // raw1394_channel_modify() on the returned channel we'll get an
            // error.
            //   iso_rx_channel = get1394Service().allocateFixedIsoChannelGeneric(iso_rx_channel, bandwidth);
            break;
        }
    }
    if (i==100 || err) {
        if (i == 100)
            debugFatal("timeout waiting for device not busy\n");
        return false;
    } else {
        signed int init_samplerate;
        if ((stat[1] & SR1_CLOCK_MODE_MASTER) ||
            (stat[0] & SR0_AUTOSYNC_FREQ_MASK)==0 ||
            (stat[0] & SR0_AUTOSYNC_SRC_MASK)==SR0_AUTOSYNC_SRC_NONE) {
            init_samplerate = dev_config->hardware_freq;
        } else {
            init_samplerate = (stat[0] & SR0_STREAMING_FREQ_MASK) * 250;
        }
        debugOutput(DEBUG_LEVEL_VERBOSE, "sample rate on start: %d\n",
            init_samplerate);
    }

    return FFADODevice::resetForStreaming();
}

bool
Device::prepare() {

    signed int mult, bandwidth;
    signed int freq;
    signed int err = 0;

    debugOutput(DEBUG_LEVEL_NORMAL, "Preparing Device...\n" );

    // If there is no iso data to send in a given cycle the RMEs simply
    // don't send anything.  This is in contrast to most other interfaces
    // which at least send an empty packet.  As a result the IsoHandler
    // contains code which detects missing packets as dropped packets.
    // For RME devices we must turn this test off since missing packets
    // are in fact to be expected.
    get1394Service().getIsoHandlerManager().setMissedCyclesOK(true);

    freq = getSamplingFrequency();
    if (freq <= 0) {
        debugOutput(DEBUG_LEVEL_ERROR, "Can't continue: sampling frequency not set\n");
        return false;
    }
    mult = freq<68100?1:(freq<136200?2:4);

    frames_per_packet = getFramesPerPacket();

    // The number of active channels depends on sample rate and whether
    // bandwidth limitation is active.  First set up the number of analog
    // channels (which differs between devices), then add SPDIF channels if
    // relevant.  Finally, the number of channels available from each ADAT
    // interface depends on sample rate: 0 at 4x, 4 at 2x and 8 at 1x.
    //  Note that "analog only" bandwidth limit mode means analog 1-8
    // regardless of the fireface model in use.
    if (m_rme_model==RME_MODEL_FIREFACE800 && settings->limit_bandwidth!=FF_SWPARAM_BWLIMIT_ANALOG_ONLY)
        num_channels = 10;
    else
        num_channels = 8;
    if (settings->limit_bandwidth != FF_SWPARAM_BWLIMIT_ANALOG_ONLY)
        num_channels += 2;
    if (settings->limit_bandwidth==FF_SWPARAM_BWLIMIT_SEND_ALL_CHANNELS ||
        settings->limit_bandwidth==FF_DEV_FLASH_BWLIMIT_NO_ADAT2)
        num_channels += (mult==4?0:(mult==2?4:8));
    if (m_rme_model==RME_MODEL_FIREFACE800 &&
        settings->limit_bandwidth==FF_SWPARAM_BWLIMIT_SEND_ALL_CHANNELS)
        num_channels += (mult==4?0:(mult==2?4:8));

    // Bandwidth is calculated here.  For the moment we assume the device 
    // is connected at S400, so 1 allocation unit is 1 transmitted byte.
    // There is 25 allocation units of protocol overhead per packet.  Each
    // channel of audio data is sent/received as a 32 bit integer.
    bandwidth = 25 + num_channels*4*frames_per_packet;

    // Both the FF400 and FF800 require we allocate a tx iso channel and
    // then initialise the device.  Device status is then read at least once
    // regardless of which interface is in use.  The rx channel is then
    // allocated for the FF400 or acquired from the device in the case of
    // the FF800.  Even though the FF800 chooses the rx channel it does not
    // handle the bus-level channel/bandwidth allocation so we must do that 
    // here.
    if (iso_tx_channel < 0) {
        iso_tx_channel = get1394Service().allocateIsoChannelGeneric(bandwidth);
    }
    if (iso_tx_channel < 0) {
        debugFatal("Could not allocate iso tx channel\n");
        return false;
    } else {
      debugOutput(DEBUG_LEVEL_NORMAL, "iso tx channel: %d\n", iso_tx_channel);
    }

    // Call this to initialise the device's streaming system and, in the
    // case of the FF800, obtain the rx iso channel to use.  Having that
    // functionality in resetForStreaming() means it's effectively done
    // twice when FFADO is first started, but this does no harm.
    if (resetForStreaming() == false)
        return false;
  
    if (err) {
        if (iso_tx_channel >= 0) 
            get1394Service().freeIsoChannel(iso_tx_channel);
        if (iso_rx_channel>=0 && m_rme_model==RME_MODEL_FIREFACE400)
            // The FF800 manages this channel itself.
            get1394Service().freeIsoChannel(iso_rx_channel);
        return false;
    }

    /* We need to manage the FF400's iso rx channel */
    if (m_rme_model == RME_MODEL_FIREFACE400) {
        iso_rx_channel = get1394Service().allocateIsoChannelGeneric(bandwidth);
    }

    // get the device specific and/or global SP configuration
    Util::Configuration &config = getDeviceManager().getConfiguration();
    // base value is the config.h value
    float recv_sp_dll_bw = STREAMPROCESSOR_DLL_BW_HZ;
    float xmit_sp_dll_bw = STREAMPROCESSOR_DLL_BW_HZ;

    // we can override that globally
    config.getValueForSetting("streaming.spm.recv_sp_dll_bw", recv_sp_dll_bw);
    config.getValueForSetting("streaming.spm.xmit_sp_dll_bw", xmit_sp_dll_bw);

    // or override in the device section
    config.getValueForDeviceSetting(getConfigRom().getNodeVendorId(), getConfigRom().getModelId(), "recv_sp_dll_bw", recv_sp_dll_bw);
    config.getValueForDeviceSetting(getConfigRom().getNodeVendorId(), getConfigRom().getModelId(), "xmit_sp_dll_bw", xmit_sp_dll_bw);

    // Calculate the event size.  Each audio channel is allocated 4 bytes in
    // the data stream.
    /* FIXME: this will still require fine-tuning, but it's a start */
    signed int event_size = num_channels * 4;

    // Set up receive stream processor, initialise it and set DLL bw
    m_receiveProcessor = new Streaming::RmeReceiveStreamProcessor(*this, 
      m_rme_model, event_size);
    m_receiveProcessor->setVerboseLevel(getDebugLevel());
    if (!m_receiveProcessor->init()) {
        debugFatal("Could not initialize receive processor!\n");
        return false;
    }
    if (!m_receiveProcessor->setDllBandwidth(recv_sp_dll_bw)) {
        debugFatal("Could not set DLL bandwidth\n");
        delete m_receiveProcessor;
        m_receiveProcessor = NULL;
        return false;
    }

    // Add ports to the processor - TODO
    std::string id=std::string("dev?");
    if (!getOption("id", id)) {
        debugWarning("Could not retrieve id parameter, defaulting to 'dev?'\n");
    }
    addDirPorts(Streaming::Port::E_Capture);

    /* Now set up the transmit stream processor */
    m_transmitProcessor = new Streaming::RmeTransmitStreamProcessor(*this,
      m_rme_model, event_size);
    m_transmitProcessor->setVerboseLevel(getDebugLevel());
    if (!m_transmitProcessor->init()) {
        debugFatal("Could not initialise receive processor!\n");
        return false;
    }
    if (!m_transmitProcessor->setDllBandwidth(xmit_sp_dll_bw)) {
        debugFatal("Could not set DLL bandwidth\n");
        delete m_transmitProcessor;
        m_transmitProcessor = NULL;
        return false;
    }

    // Other things to be done:
    //  * add ports to transmit stream processor
    addDirPorts(Streaming::Port::E_Playback);

    return true;
}

int
Device::getStreamCount() {
    return 2; // one receive, one transmit
}

Streaming::StreamProcessor *
Device::getStreamProcessorByIndex(int i) {
    switch (i) {
        case 0:
            return m_receiveProcessor;
        case 1:
            return m_transmitProcessor;
        default:
            debugWarning("Invalid stream index %d\n", i);
    }
    return NULL;
}

enum FFADODevice::eStreamingState
Device::getStreamingState() {
  if (hardware_is_streaming())
    return eSS_Both;
  return eSS_Idle;
}

bool
Device::startStreamByIndex(int i) {
    // The RME does not allow separate enabling of the transmit and receive
    // streams.  Therefore we start all streaming when index 0 is referenced
    // and silently ignore the start requests for other streams
    // (unconditionally flagging them as being successful).
    if (i == 0) {
        m_receiveProcessor->setChannel(iso_rx_channel);
        m_transmitProcessor->setChannel(iso_tx_channel);
        if (hardware_start_streaming(iso_rx_channel) != 0)
            return false;
    }
    return true;
}

bool
Device::stopStreamByIndex(int i) {
    // See comments in startStreamByIndex() as to why we act only when stream
    // 0 is requested.
    if (i == 0) {
        if (hardware_stop_streaming() != 0)
            return false;
    }
    return true;
}

signed int
Device::getFramesPerPacket(void) {
    // The number of frames transmitted in a single packet is solely
    // determined by the sample rate.  This function is called several times
    // per iso cycle by the tx stream processor, so use the software rate as
    // a proxy for the hardware sample rate.  Calling getSamplingFrequency()
    // is best avoided because otherwise the kernel tends to crash having
    // run out of memory (something about the timing of async commands in
    // getSamplingFrequency() and the iso tx handler seems to be tripping it
    // up).
    //
    // If streaming is active the software sampling rate should be set up.
    // If the dds_freq functionality is implemented the software rate can
    // probably still be used because the hardware dictates that both
    // must share the same multiplier, and the only reason for obtaining
    // the sampling frequency is to determine the multiplier.
    signed int freq = dev_config->software_freq;
    signed int mult = multiplier_of_freq(freq);
    switch (mult) {
        case 2: return 15;
        case 4: return 25;
    default:
        return 7;
    }
    return 7;
}

bool 
Device::addPort(Streaming::StreamProcessor *s_processor,
    char *name, enum Streaming::Port::E_Direction direction,
    int position, int size) {

    Streaming::Port *p;
    p = new Streaming::RmeAudioPort(*s_processor, name, direction, position, size);
    if (p == NULL) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n",name);
    }
    return true;
}

bool 
Device::addDirPorts(enum Streaming::Port::E_Direction direction) {

    const char *mode_str = direction==Streaming::Port::E_Capture?"cap":"pbk";
    Streaming::StreamProcessor *s_processor;
    std::string id;
    char name[128];
    signed int i;
    signed int n_analog, n_phones, n_adat, n_spdif;
    signed int sample_rate = getSamplingFrequency();

    /* Apply bandwidth limit if selected.  This effectively sets up the
     * number of adat and spdif channels assuming single-rate speed.
     * The total number of expected analog channels is also set here.
     */
    n_spdif = 2;
    n_analog = (m_rme_model==RME_MODEL_FIREFACE800)?10:8;
    switch (dev_config->settings.limit_bandwidth) {
      case FF_SWPARAM_BWLIMIT_ANALOG_ONLY:
        n_adat = n_spdif = 0;
        // "Analog only" means "Analog 1-8" regardless of the interface model
        n_analog = 8;
        break;
      case FF_SWPARAM_BWLIMIT_ANALOG_SPDIF_ONLY:
        n_adat = 0;
        break;
      case FF_SWPARAM_BWLIMIT_NO_ADAT2:
        /* FF800 only */
        n_adat = 8;
        break;
      default:
        /* Send all channels */
        n_adat = (m_rme_model==RME_MODEL_FIREFACE800)?16:8;
    }

    /* Adjust the spdif and ADAT channels according to the current sample
     * rate.
     */
    if (sample_rate>=MIN_DOUBLE_SPEED && sample_rate<MIN_QUAD_SPEED) {
      n_adat /= 2;
    } else
    if (sample_rate >= MIN_QUAD_SPEED) {
      n_adat = 0;
    }

    n_phones = 0;
    if (direction == Streaming::Port::E_Capture) {
        s_processor = m_receiveProcessor;
    } else {
        s_processor = m_transmitProcessor;
        /* Phones generally count as two of the analog outputs.  For
         * the FF800 in "Analog 1-8" bandwidth limit mode this is
         * not the case and the phones are inactive.
         */
        if (m_rme_model==RME_MODEL_FIREFACE400 || dev_config->settings.limit_bandwidth!=FF_SWPARAM_BWLIMIT_ANALOG_ONLY) {
            n_analog -= 2;
            n_phones = 2;
        }
    }

    id = std::string("dev?");
    if (!getOption("id", id)) {
        debugWarning("Could not retrieve id parameter, defaulting to 'dev?'\n");
    }

    for (i=0; i<n_analog; i++) {
      snprintf(name, sizeof(name), "%s_%s_analog-%d", id.c_str(), mode_str, i+1);
      addPort(s_processor, name, direction, i*4, 0);
    }
    for (i=0; i<n_phones; i++) {
      snprintf(name, sizeof(name), "%s_%s_phones-%c", id.c_str(), mode_str, 
        i==0?'L':'R');
      /* The headphone channels follow the straight analog lines */
      addPort(s_processor, name, direction, n_analog*4+i*4, 0);
    }
    for (i=0; i<n_spdif; i++) {
      snprintf(name, sizeof(name), "%s_%s_SPDIF-%d", id.c_str(), mode_str, i+1);
      /* The SPDIF channels start after all analog lines */
      addPort(s_processor, name, direction, (n_analog+n_phones)*4+i*4, 0);
    }
    for (i=0; i<n_adat; i++) {
      snprintf(name, sizeof(name), "%s_%s_adat-%d", id.c_str(), mode_str, i+1);
      /* ADAT ports follow all other ports */
      addPort(s_processor, name, direction, (n_analog+n_phones+n_spdif)*4+i*4, 0);
    }

    return true;
}

unsigned int 
Device::readRegister(fb_nodeaddr_t reg) {

    quadlet_t quadlet;
    
    quadlet = 0;
    if (get1394Service().read(0xffc0 | getNodeId(), reg, 1, &quadlet) <= 0) {
        debugError("Error doing RME read from register 0x%06"PRIx64"\n",reg);
    }
    return ByteSwapFromDevice32(quadlet);
}

signed int 
Device::readBlock(fb_nodeaddr_t reg, quadlet_t *buf, unsigned int n_quads) {

    unsigned int i;

    if (get1394Service().read(0xffc0 | getNodeId(), reg, n_quads, buf) <= 0) {
        debugError("Error doing RME block read of %d quadlets from register 0x%06"PRIx64"\n",
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
        debugError("Error doing RME write to register 0x%06"PRIx64"\n",reg);
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
        debugError("Error doing RME block write of %d quadlets to register 0x%06"PRIx64"\n",
          n_quads, reg);
    }

    return (err==0)?0:-1;
}
                  
}
