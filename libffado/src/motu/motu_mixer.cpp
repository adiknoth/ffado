/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 * Copyright (C) 2005-2009 by Jonathan Woithe
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

/* This file collects together everything associated with the management
 * of mixer controls in the MOTU device object.
 */

#include "motu/motu_avdevice.h"
#include "motu/motu_mixerdefs.h"
#include "motu/motu_mark3_mixerdefs.h"

namespace Motu {

bool
MotuDevice::buildMixerAudioControls(void) {

    bool result = true;
    MotuMatrixMixer *fader_mmixer = NULL;
    MotuMatrixMixer *pan_mmixer = NULL;
    MotuMatrixMixer *solo_mmixer = NULL;
    MotuMatrixMixer *mute_mmixer = NULL;
    const struct MatrixMixBus *buses = NULL;
    const struct MatrixMixChannel *channels = NULL;
    unsigned int bus, ch, i;

    if (DevicesProperty[m_motu_model-1].mixer == NULL) {
        debugOutput(DEBUG_LEVEL_INFO, "No pre-Mark3 mixer controls defined for model %d\n", m_motu_model);
        result = false;
    } else {
        buses = DevicesProperty[m_motu_model-1].mixer->mixer_buses;
        if (buses == NULL) {
            debugOutput(DEBUG_LEVEL_WARNING, "No buses defined for model %d\n", m_motu_model);
            result = false;
        }
        channels = DevicesProperty[m_motu_model-1].mixer->mixer_channels;
        if (channels == NULL) {
            debugOutput(DEBUG_LEVEL_WARNING, "No channels defined for model %d\n", m_motu_model);
            result = false;
        }
    }
    if (result == false) {
        return true;
    }

    /* Create the top-level matrix mixers */
    fader_mmixer = new ChannelFaderMatrixMixer(*this, "fader");
    result &= m_MixerContainer->addElement(fader_mmixer);
    pan_mmixer = new ChannelPanMatrixMixer(*this, "pan");
    result &= m_MixerContainer->addElement(pan_mmixer);
    solo_mmixer = new ChannelBinSwMatrixMixer(*this, "solo", 
        MOTU_CTRL_MASK_SOLO_VALUE, MOTU_CTRL_MASK_SOLO_SETENABLE);
    result &= m_MixerContainer->addElement(solo_mmixer);
    mute_mmixer = new ChannelBinSwMatrixMixer(*this, "mute",
        MOTU_CTRL_MASK_MUTE_VALUE, MOTU_CTRL_MASK_MUTE_SETENABLE);
    result &= m_MixerContainer->addElement(mute_mmixer);

    for (bus=0; bus<DevicesProperty[m_motu_model-1].mixer->n_mixer_buses; bus++) {
        fader_mmixer->addRowInfo(buses[bus].name, 0, buses[bus].address);
        pan_mmixer->addRowInfo(buses[bus].name, 0, buses[bus].address);
        solo_mmixer->addRowInfo(buses[bus].name, 0, buses[bus].address);
        mute_mmixer->addRowInfo(buses[bus].name, 0, buses[bus].address);
    }

    for (ch=0; ch<DevicesProperty[m_motu_model-1].mixer->n_mixer_channels; ch++) {
        uint32_t flags = channels[ch].flags;
        if (flags & MOTU_CTRL_CHANNEL_FADER)
            fader_mmixer->addColInfo(channels[ch].name, 0, channels[ch].addr_ofs);
        if (flags & MOTU_CTRL_CHANNEL_PAN)
            pan_mmixer->addColInfo(channels[ch].name, 0, channels[ch].addr_ofs);
        if (flags & MOTU_CTRL_CHANNEL_SOLO)
            solo_mmixer->addColInfo(channels[ch].name, 0, channels[ch].addr_ofs);
        if (flags & MOTU_CTRL_CHANNEL_MUTE)
            mute_mmixer->addColInfo(channels[ch].name, 0, channels[ch].addr_ofs);
        flags &= ~(MOTU_CTRL_CHANNEL_FADER|MOTU_CTRL_CHANNEL_PAN|MOTU_CTRL_CHANNEL_SOLO|MOTU_CTRL_CHANNEL_MUTE);
        if (flags) {
            debugOutput(DEBUG_LEVEL_WARNING, "Control %s: unknown flag bits 0x%08x\n", channels[ch].name, flags);
        }
    }

    // Single non-matrixed mixer controls get added here.  Channel controls are supported
    // here, but usually these will be a part of a matrix mixer.
    for (i=0; i<DevicesProperty[m_motu_model-1].mixer->n_mixer_ctrls; i++) {
        const struct MixerCtrl *ctrl = &DevicesProperty[m_motu_model-1].mixer->mixer_ctrl[i];
        unsigned int type;
        char name[100];
        char label[100];

        if (ctrl == NULL) {
            debugOutput(DEBUG_LEVEL_WARNING, "NULL control at index %d for model %d\n", i, m_motu_model);
            continue;
        }
        type = ctrl->type;
        if (type & MOTU_CTRL_CHANNEL_FADER) {
            snprintf(name, 100, "%s%s", ctrl->name, "fader");
            snprintf(label,100, "%s%s", ctrl->label,"fader");
            result &= m_MixerContainer->addElement(
                new ChannelFader(*this, ctrl->dev_register, name, label, ctrl->desc));
            type &= ~MOTU_CTRL_CHANNEL_FADER;
        }
        if (type & MOTU_CTRL_CHANNEL_PAN) {
            snprintf(name, 100, "%s%s", ctrl->name, "pan");
            snprintf(label,100, "%s%s", ctrl->label,"pan");
            result &= m_MixerContainer->addElement(
                new ChannelPan(*this, 
                    ctrl->dev_register,
                    name, label,
                    ctrl->desc));
            type &= ~MOTU_CTRL_CHANNEL_PAN;
        }
        if (type & MOTU_CTRL_CHANNEL_MUTE) {
            snprintf(name, 100, "%s%s", ctrl->name, "mute");
            snprintf(label,100, "%s%s", ctrl->label,"mute");
            result &= m_MixerContainer->addElement(
                new MotuBinarySwitch(*this, ctrl->dev_register,
                    MOTU_CTRL_MASK_MUTE_VALUE, MOTU_CTRL_MASK_MUTE_SETENABLE,
                    name, label, ctrl->desc));
            type &= ~MOTU_CTRL_CHANNEL_MUTE;
        }
        if (type & MOTU_CTRL_CHANNEL_SOLO) {
            snprintf(name, 100, "%s%s", ctrl->name, "solo");
            snprintf(label,100, "%s%s", ctrl->label,"solo");
            result &= m_MixerContainer->addElement(
                new MotuBinarySwitch(*this, ctrl->dev_register,
                    MOTU_CTRL_MASK_SOLO_VALUE, MOTU_CTRL_MASK_SOLO_SETENABLE,
                    name, label, ctrl->desc));
            type &= ~MOTU_CTRL_CHANNEL_SOLO;
        }

        if (type & MOTU_CTRL_MIX_FADER) {
            snprintf(name, 100, "%s%s", ctrl->name, "fader");
            snprintf(label,100, "%s%s", ctrl->label,"fader");
            result &= m_MixerContainer->addElement(
                new MixFader(*this, ctrl->dev_register, name, label, ctrl->desc));
            type &= ~MOTU_CTRL_MIX_FADER;
        }
        if (type & MOTU_CTRL_MIX_MUTE) {
            snprintf(name, 100, "%s%s", ctrl->name, "mute");
            snprintf(label,100, "%s%s", ctrl->label,"mute");
            result &= m_MixerContainer->addElement(
                new MixMute(*this, ctrl->dev_register, name, label, ctrl->desc));
            type &= ~MOTU_CTRL_MIX_MUTE;
        }
        if (type & MOTU_CTRL_MIX_DEST) {
            snprintf(name, 100, "%s%s", ctrl->name, "dest");
            snprintf(label,100, "%s%s", ctrl->label,"dest");
            result &= m_MixerContainer->addElement(
                new MixDest(*this, ctrl->dev_register, name, label, ctrl->desc));
            type &= ~MOTU_CTRL_MIX_DEST;
        }

        if (type & MOTU_CTRL_INPUT_UL_GAIN) {
            snprintf(name, 100, "%s%s", ctrl->name, "trimgain");
            snprintf(label,100, "%s%s", ctrl->label,"trimgain");
            result &= m_MixerContainer->addElement(
                new InputGainPadInv(*this, ctrl->dev_register, MOTU_CTRL_MODE_UL_GAIN,
                    name, label, ctrl->desc));
            type &= ~MOTU_CTRL_INPUT_UL_GAIN;
        }
        if (type & MOTU_CTRL_INPUT_PHASE_INV) {
            snprintf(name, 100, "%s%s", ctrl->name, "invert");
            snprintf(label,100, "%s%s", ctrl->label,"invert");
            result &= m_MixerContainer->addElement(
                new InputGainPadInv(*this, ctrl->dev_register, MOTU_CTRL_MODE_PHASE_INV,
                    name, label, ctrl->desc));
            type &= ~MOTU_CTRL_INPUT_PHASE_INV;
        }
        if (type & MOTU_CTRL_INPUT_TRIMGAIN) {
            snprintf(name, 100, "%s%s", ctrl->name, "trimgain");
            snprintf(label,100, "%s%s", ctrl->label,"trimgain");
            result &= m_MixerContainer->addElement(
                new InputGainPadInv(*this, ctrl->dev_register, MOTU_CTRL_MODE_TRIMGAIN,
                    name, label, ctrl->desc));
            type &= ~MOTU_CTRL_INPUT_TRIMGAIN;
        }
        if (type & MOTU_CTRL_INPUT_PAD) {
            snprintf(name, 100, "%s%s", ctrl->name, "pad");
            snprintf(label,100, "%s%s", ctrl->label,"pad");
            result &= m_MixerContainer->addElement(
                new InputGainPadInv(*this, ctrl->dev_register, MOTU_CTRL_MODE_PAD,
                    name, label, ctrl->desc));
            type &= ~MOTU_CTRL_INPUT_PAD;
        }

        if (type & MOTU_CTRL_INPUT_LEVEL) {
            snprintf(name, 100, "%s%s", ctrl->name, "level");
            snprintf(label,100, "%s%s", ctrl->label,"level");
            result &= m_MixerContainer->addElement(
                new MotuBinarySwitch(*this, MOTU_REG_INPUT_LEVEL,
                    1<<ctrl->dev_register, 0, name, label, ctrl->desc));
            type &= ~MOTU_CTRL_INPUT_LEVEL;
        }
        if (type & MOTU_CTRL_INPUT_BOOST) {
            snprintf(name, 100, "%s%s", ctrl->name, "boost");
            snprintf(label,100, "%s%s", ctrl->label,"boost");
            result &= m_MixerContainer->addElement(
                new MotuBinarySwitch(*this, MOTU_REG_INPUT_BOOST,
                    1<<ctrl->dev_register, 0, name, label, ctrl->desc));
            type &= ~MOTU_CTRL_INPUT_BOOST;
        }
        if (type & MOTU_CTRL_PHONES_SRC) {
            snprintf(name, 100, "%s%s", ctrl->name, "src");
            snprintf(label,100, "%s%s", ctrl->label,"src");
            result &= m_MixerContainer->addElement(
                new PhonesSrc(*this, name, label, ctrl->desc));
            type &= ~MOTU_CTRL_PHONES_SRC;
        }
        if (type & MOTU_CTRL_OPTICAL_MODE) {
            result &= m_MixerContainer->addElement(
                new OpticalMode(*this, ctrl->dev_register,
                    ctrl->name, ctrl->label, ctrl->desc));
            type &= ~MOTU_CTRL_OPTICAL_MODE;
        }
        if (type & MOTU_CTRL_METER) {
            if (ctrl->dev_register & MOTU_CTRL_METER_PEAKHOLD) {
                snprintf(name, 100, "%s%s", ctrl->name, "peakhold_time");
                snprintf(label,100, "%s%s", ctrl->label,"peakhold time");
                result &= m_MixerContainer->addElement(
                    new MeterControl(*this, MOTU_METER_PEAKHOLD_MASK,
                        MOTU_METER_PEAKHOLD_SHIFT, name, label, ctrl->desc));
            }
            if (ctrl->dev_register & MOTU_CTRL_METER_CLIPHOLD) {
                snprintf(name, 100, "%s%s", ctrl->name, "cliphold_time");
                snprintf(label,100, "%s%s", ctrl->label,"cliphold time");
                result &= m_MixerContainer->addElement(
                    new MeterControl(*this, MOTU_METER_CLIPHOLD_MASK,
                        MOTU_METER_CLIPHOLD_SHIFT, name, label, ctrl->desc));
            }
            if (ctrl->dev_register & MOTU_CTRL_METER_AESEBU_SRC) {
                snprintf(name, 100, "%s%s", ctrl->name, "aesebu_src");
                snprintf(label,100, "%s%s", ctrl->label,"AESEBU source");
                result &= m_MixerContainer->addElement(
                    new MeterControl(*this, MOTU_METER_AESEBU_SRC_MASK,
                        MOTU_METER_AESEBU_SRC_SHIFT, name, label, ctrl->desc));
            }
            if (ctrl->dev_register & MOTU_CTRL_METER_PROG_SRC) {
                snprintf(name, 100, "%s%s", ctrl->name, "src");
                snprintf(label,100, "%s%s", ctrl->label,"source");
                result &= m_MixerContainer->addElement(
                    new MeterControl(*this, MOTU_METER_PROG_SRC_MASK,
                        MOTU_METER_PROG_SRC_SHIFT, name, label, ctrl->desc));
            }
            type &= ~MOTU_CTRL_METER;
        }

        if (type) {
            debugOutput(DEBUG_LEVEL_WARNING, "Unknown mixer control type flag bits 0x%08x\n", ctrl->type);
        }
    }
    return result;
}

bool
MotuDevice::buildMark3MixerAudioControls(void) {

    bool result = true;

    if (DevicesProperty[m_motu_model-1].m3mixer == NULL) {
        debugOutput(DEBUG_LEVEL_INFO, "No Mark3 mixer controls defined for model %d\n", m_motu_model);
        return false;
    }

    // FIXME: Details to come
    result = false;

    return result;
}

bool
MotuDevice::buildMixer() {
    bool result = true;
    debugOutput(DEBUG_LEVEL_VERBOSE, "Building a MOTU mixer...\n");

    destroyMixer();
        
    // create the mixer object container
    m_MixerContainer = new Control::Container(this, "Mixer");
    if (!m_MixerContainer) {
        debugError("Could not create mixer container...\n");
        return false;
    }

    if (DevicesProperty[m_motu_model-1].mixer != NULL &&
        DevicesProperty[m_motu_model-1].m3mixer != NULL) {
        debugError("MOTU model %d has pre-Mark3 and Mark3 mixer descriptions\n", m_motu_model);
        return false;
    }

    // Create and populate the top-level matrix mixers
    result = buildMixerAudioControls() || buildMark3MixerAudioControls();

    /* Now add some general device information controls.  These may yet
     * become device-specific if it turns out to be easier that way.
     */
    result &= m_MixerContainer->addElement(
        new InfoElement(*this, MOTU_INFO_MODEL, "Info/Model", "Model identifier", ""));
    result &= m_MixerContainer->addElement(
        new InfoElement(*this, MOTU_INFO_IS_STREAMING, "Info/IsStreaming", "Is device streaming", ""));
    result &= m_MixerContainer->addElement(
        new InfoElement(*this, MOTU_INFO_SAMPLE_RATE, "Info/SampleRate", "Device sample rate", ""));

    if (!addElement(m_MixerContainer)) {
        debugWarning("Could not register mixer to device\n");
        // clean up
        destroyMixer();
        return false;
    }

    // Special controls
    m_ControlContainer = new Control::Container(this, "Control");
    if (!m_ControlContainer) {
        debugError("Could not create control container...\n");
        return false;
    }

    // Special controls get added here

    if (!result) {
        debugWarning("One or more device control elements could not be created.");
        // clean up those that couldn't be created
        destroyMixer();
        return false;
    }
    if (!addElement(m_ControlContainer)) {
        debugWarning("Could not register controls to device\n");
        // clean up
        destroyMixer();
        return false;
    }

    return true;
}


bool
MotuDevice::destroyMixer() {
    debugOutput(DEBUG_LEVEL_VERBOSE, "destroy mixer...\n");

    if (m_MixerContainer == NULL) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "no mixer to destroy...\n");
        return true;
    }
    
    if (!deleteElement(m_MixerContainer)) {
        debugError("Mixer present but not registered to the avdevice\n");
        return false;
    }

    // remove and delete (as in free) child control elements
    m_MixerContainer->clearElements(true);
    delete m_MixerContainer;
    m_MixerContainer = NULL;

    // remove control container
    if (m_ControlContainer == NULL) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "no controls to destroy...\n");
        return true;
    }
    
    if (!deleteElement(m_ControlContainer)) {
        debugError("Controls present but not registered to the avdevice\n");
        return false;
    }
    
    // remove and delete (as in free) child control elements
    m_ControlContainer->clearElements(true);
    delete m_ControlContainer;
    m_ControlContainer = NULL;

    return true;
}

}
