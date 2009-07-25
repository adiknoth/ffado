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


}
