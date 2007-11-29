/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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

#include "audiofire_device.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

namespace FireWorks {
namespace ECHO {

AudioFire::AudioFire(std::auto_ptr<ConfigRom>( configRom ))
    : FireWorks::Device(configRom)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created FireWorks::ECHO::AudioFire (NodeID %d)\n",
                 getConfigRom().getNodeId() );
}

AudioFire::~AudioFire()
{
}

void
AudioFire::showDevice()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "This is a FireWorks::ECHO::AudioFire\n");
    FireWorks::Device::showDevice();
}

} // ECHO
} // FireWorks
