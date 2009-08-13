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

    return settings.mic_phantom[channel] != 0;
}

signed int
Device::setPhantom(unsigned int channel, unsigned int status) {

    if (channel > 3) {
        debugOutput(DEBUG_LEVEL_WARNING, "Channel %d phantom power not supported\n", channel);
        return -1;
    }

    settings.mic_phantom[channel] = (status != 0);
    set_hardware_params();

    return 0;
}

signed int 
Device::getInputPadOpt(unsigned int channel) {
    if (m_rme_model!=RME_MODEL_FIREFACE400 || channel<3 || channel>4) {
        debugOutput(DEBUG_LEVEL_WARNING, "Channel %d input pad option not supported for model %d\n", channel, m_rme_model);
        return -1;
    }
    return settings.ff400_input_pad[channel-3] != 0;
}

signed int 
Device::setInputPadOpt(unsigned int channel, unsigned int status) {
    if (m_rme_model!=RME_MODEL_FIREFACE400 || channel<3 || channel>4) {
        debugOutput(DEBUG_LEVEL_WARNING, "Channel %d input pad option not supported for model %d\n", channel, m_rme_model);
        return -1;
    }
    settings.ff400_input_pad[channel-3] = (status != 0);
    set_hardware_params();
    return 0;
}

signed int 
Device::getInputInstrOpt(unsigned int channel) {
    if (m_rme_model!=RME_MODEL_FIREFACE400 || channel<3 || channel>4) {
        debugOutput(DEBUG_LEVEL_WARNING, "Channel %d input instrument option not supported for model %d\n", channel, m_rme_model);
        return -1;
    }
    return settings.ff400_instr_input[channel-3] != 0;
}

signed int 
Device::setInputInstrOpt(unsigned int channel, unsigned int status) {
    if (m_rme_model!=RME_MODEL_FIREFACE400 || channel<3 || channel>4) {
        debugOutput(DEBUG_LEVEL_WARNING, "Channel %d input instrument option not supported for model %d\n", channel, m_rme_model);
        return -1;
    }
    settings.ff400_instr_input[channel-3] = (status != 0);
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
    return settings.amp_gains[index];
}

signed int
Device::setAmpGain(unsigned int index, signed int val) {
    quadlet_t regval = 0;

    if (m_rme_model != RME_MODEL_FIREFACE400) {
        debugOutput(DEBUG_LEVEL_WARNING, "Amp gains only supported on FF400\n");
        return -1;
    }
    if (index > 21) {
        debugOutput(DEBUG_LEVEL_WARNING, "Amp gain index %d invalid\n", index);
         return -1;
    }
    settings.amp_gains[index] = val & 0xff;
    return set_hardware_ampgain(index, val);
}

}
