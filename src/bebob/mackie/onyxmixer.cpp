/*
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

#include "onyxmixer.h"

#include "debugmodule/debugmodule.h"

namespace BeBoB {
namespace Mackie {

OnyxMixerDevice::OnyxMixerDevice( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ))
    : BeBoB::AvDevice( d, configRom)
{
    m_fixed_clocksource.type = FFADODevice::eCT_Internal;
    m_fixed_clocksource.valid = true;
    m_fixed_clocksource.locked = true;
    m_fixed_clocksource.id = 0;
    m_fixed_clocksource.slipping = false;
    m_fixed_clocksource.description = "Internal";

    debugOutput( DEBUG_LEVEL_VERBOSE, "Created BeBoB::Mackie::OnyxMixerDevice (NodeID %d)\n",
                 getConfigRom().getNodeId() );
}

OnyxMixerDevice::~OnyxMixerDevice()
{
}

void
OnyxMixerDevice::showDevice()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "This is a BeBoB::Mackie::OnyxMixerDevice\n");
    BeBoB::AvDevice::showDevice();
}

FFADODevice::ClockSource
OnyxMixerDevice::getActiveClockSource() {
    return m_fixed_clocksource;
}

bool
OnyxMixerDevice::setActiveClockSource(ClockSource s) {
    // can't change, hence only succeed when identical
    return s.id == m_fixed_clocksource.id;
}

FFADODevice::ClockSourceVector
OnyxMixerDevice::getSupportedClockSources() {
    FFADODevice::ClockSourceVector r;
    r.push_back(m_fixed_clocksource);
    return r;
}

} // Mackie
} // BeBoB
