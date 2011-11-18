/*
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
 */

#include "rme/rme_avdevice.h"
#include "rme/fireface_def.h"

#include "debugmodule/debugmodule.h"

namespace Rme {

signed int
Device::getPhantom(unsigned int channel) {

    if (channel > 3) {
        debugOutput(DEBUG_LEVEL_WARNING, "Channel %d phantom power not supported\n", channel);
        return -1;
    }

    return settings->mic_phantom[channel] != 0;
}

signed int
Device::setPhantom(unsigned int channel, unsigned int status) {

    if (channel > 3) {
        debugOutput(DEBUG_LEVEL_WARNING, "Channel %d phantom power not supported\n", channel);
        return -1;
    }

    settings->mic_phantom[channel] = (status != 0);
    set_hardware_params();

    return 0;
}

signed int
Device::getInputLevel(void) {
    return settings->input_level;
}

signed int
Device::setInputLevel(unsigned int level) {

    if (level<FF_SWPARAM_ILEVEL_LOGAIN || level>FF_SWPARAM_ILEVEL_m10dBV) {
        debugOutput(DEBUG_LEVEL_WARNING, "Invalid input level ID %d\n", level);
        return -1;
    }
    settings->input_level = level;
    set_hardware_params();

    return 0;
}

signed int
Device::getOutputLevel(void) {
    return settings->output_level;
}

signed int
Device::setOutputLevel(unsigned int level) {

    if (level<FF_SWPARAM_OLEVEL_HIGAIN || level>FF_SWPARAM_OLEVEL_m10dBV) {
        debugOutput(DEBUG_LEVEL_WARNING, "Invalid output level ID %d\n", level);
        return -1;
    }
    settings->output_level = level;
    set_hardware_params();

    return 0;
}

signed int
Device::getPhonesLevel(void) {
    return settings->phones_level;
}

signed int
Device::setPhonesLevel(unsigned int level) {

    if (level<FF_SWPARAM_PHONESLEVEL_HIGAIN || level>FF_SWPARAM_PHONESLEVEL_m10dBV) {
        debugOutput(DEBUG_LEVEL_WARNING, "Invalid phones level ID %d\n", level);
        return -1;
    }
    settings->phones_level = level;
    set_hardware_params();

    return 0;
}

signed int 
Device::getInputPadOpt(unsigned int channel) {
    if (m_rme_model!=RME_MODEL_FIREFACE400 || channel<3 || channel>4) {
        debugOutput(DEBUG_LEVEL_WARNING, "Channel %d input pad option not supported for model %d\n", channel, m_rme_model);
        return -1;
    }
    return settings->ff400_input_pad[channel-3] != 0;
}

signed int 
Device::setInputPadOpt(unsigned int channel, unsigned int status) {
    if (m_rme_model!=RME_MODEL_FIREFACE400 || channel<3 || channel>4) {
        debugOutput(DEBUG_LEVEL_WARNING, "Channel %d input pad option not supported for model %d\n", channel, m_rme_model);
        return -1;
    }
    settings->ff400_input_pad[channel-3] = (status != 0);
    set_hardware_params();
    return 0;
}

signed int 
Device::getInputInstrOpt(unsigned int channel) {
    if (m_rme_model!=RME_MODEL_FIREFACE400 || channel<3 || channel>4) {
        debugOutput(DEBUG_LEVEL_WARNING, "Channel %d input instrument option not supported for model %d\n", channel, m_rme_model);
        return -1;
    }
    return settings->ff400_instr_input[channel-3] != 0;
}

signed int 
Device::setInputInstrOpt(unsigned int channel, unsigned int status) {
    if (m_rme_model!=RME_MODEL_FIREFACE400 || channel<3 || channel>4) {
        debugOutput(DEBUG_LEVEL_WARNING, "Channel %d input instrument option not supported for model %d\n", channel, m_rme_model);
        return -1;
    }
    settings->ff400_instr_input[channel-3] = (status != 0);
    set_hardware_params();
    return 0;
}

signed int
Device::getAmpGain(unsigned int index) {
    if (m_rme_model != RME_MODEL_FIREFACE400) {
        debugOutput(DEBUG_LEVEL_WARNING, "Amp gains only supported on FF400\n");
        return -1;
    }
    if (index > 21) {
        debugOutput(DEBUG_LEVEL_WARNING, "Amp gain index %d invalid\n", index);
         return -1;
    }
    return settings->amp_gains[index];
}

signed int
Device::setAmpGain(unsigned int index, signed int val) {

    if (m_rme_model != RME_MODEL_FIREFACE400) {
        debugOutput(DEBUG_LEVEL_WARNING, "Amp gains only supported on FF400\n");
        return -1;
    }
    if (index > 21) {
        debugOutput(DEBUG_LEVEL_WARNING, "Amp gain index %d invalid\n", index);
         return -1;
    }
    settings->amp_gains[index] = val & 0xff;
    return set_hardware_ampgain(index, val);
}

signed int
Device::getMixerGainIndex(unsigned int src_channel, unsigned int dest_channel) {
    return dest_channel*RME_FF800_MAX_CHANNELS + src_channel;
}

signed int
Device::getMixerGain(unsigned int ctype,
    unsigned int src_channel, unsigned int dest_channel) {

    signed int idx = getMixerGainIndex(src_channel, dest_channel);
    switch (ctype) {
        case RME_FF_MM_INPUT:
            return settings->input_faders[idx];
            break;
        case RME_FF_MM_PLAYBACK:
            return settings->playback_faders[idx];
            break;
        case RME_FF_MM_OUTPUT:
            return settings->output_faders[src_channel];
            break;
    }
    return 0;
}

signed int
Device::setMixerGain(unsigned int ctype, 
    unsigned int src_channel, unsigned int dest_channel, signed int val) {

    signed int idx = getMixerGainIndex(src_channel, dest_channel);
    switch (ctype) {
        case RME_FF_MM_INPUT:
            settings->input_faders[idx] = val;
            break;
        case RME_FF_MM_PLAYBACK:
            settings->playback_faders[idx] = val;
            break;
        case RME_FF_MM_OUTPUT:
            settings->output_faders[src_channel] = val;
            break;
    }
    return set_hardware_mixergain(ctype, src_channel, dest_channel, val);
}

}
