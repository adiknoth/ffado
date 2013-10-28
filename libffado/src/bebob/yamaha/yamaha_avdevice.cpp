/*
 * Copyright (C) 2013      by Takashi Sakamoto
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

#include "yamaha_avdevice.h"
#include "yamaha_cmd.h"

namespace BeBoB {
namespace Yamaha {

GoDevice::GoDevice(DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ))
    : BeBoB::Device( d, configRom)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created BeBoB::Yamaha::GoDevice (NodeID %d)\n",
                 getConfigRom().getNodeId() );
    updateClockSources();
}

GoDevice::~GoDevice()
{
}

void
GoDevice::showDevice()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "This is a BeBoB::Yamaha::GoDevice\n");
    BeBoB::Device::showDevice();
}

bool
GoDevice::updateClockSources()
{
    int err;

    m_internal_clocksource.type = FFADODevice::eCT_Internal;
    m_internal_clocksource.valid = true;
    m_internal_clocksource.active = false;
    m_internal_clocksource.locked = true;
    m_internal_clocksource.id = 0;
    m_internal_clocksource.slipping = false;
    m_internal_clocksource.description = "Internal";

    m_spdif_clocksource.type = FFADODevice::eCT_SPDIF;
    m_spdif_clocksource.valid = true;
    m_spdif_clocksource.active = false;
    m_spdif_clocksource.locked = false;
    m_spdif_clocksource.id = 1;
    m_spdif_clocksource.slipping = false;
    m_spdif_clocksource.description = "S/PDIF";

    /* detect digital input */
    YamahaDigInDetectCmd cmd ( get1394Service() );
    cmd.setCommandType( AVC::AVCCommand::eCT_Status );
    cmd.setNodeId( getConfigRom().getNodeId() );
    cmd.setVerbose( getDebugLevel() );
    if ( !cmd.fire() ) {
        debugError( "YamahaDigInDetectCmd failed\n" );
        return false;
    } else if (cmd.m_digin == 0) {
        m_spdif_clocksource.locked = true;
    }

    /* get current clock source */
    err = getSelectorFBValue(4);
    if (err < 0)
        return err;
    else if (err > 0) {
        m_active_clocksource = &m_spdif_clocksource;
        m_spdif_clocksource.active = true;
    } else {
        m_active_clocksource = &m_internal_clocksource;
        m_internal_clocksource.active = true;
    }

    return true;
}

FFADODevice::ClockSource
GoDevice::getActiveClockSource()
{
    if (!updateClockSources()) {
        ClockSource s;
        s.type = eCT_Invalid;
        return s;
    }
    return *m_active_clocksource;
}

bool
GoDevice::setActiveClockSource(ClockSource s)
{
    if (!updateClockSources())
        return false;

    if ((s.id > 0) || (!m_spdif_clocksource.locked))
        return false;
    return setSelectorFBValue(4, s.id);
}

FFADODevice::ClockSourceVector
GoDevice::getSupportedClockSources() {
    FFADODevice::ClockSourceVector r;
    r.push_back(m_internal_clocksource);
    r.push_back(m_spdif_clocksource);
    return r;
}
} // namespace Yamaha
} // namespace BeBoB
