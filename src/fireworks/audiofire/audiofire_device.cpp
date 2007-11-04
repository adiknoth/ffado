/*
 * Copyright (C) 2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software Foundation;
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#include "audiofire_device.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

namespace FireWorks {
namespace ECHO {

AudioFire::AudioFire( Ieee1394Service& ieee1394Service,
                            std::auto_ptr<ConfigRom>( configRom ))
    : FireWorks::Device( ieee1394Service, configRom)
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
