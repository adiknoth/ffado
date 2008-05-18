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

#include "terratec_device.h"

namespace BeBoB {
namespace Terratec {

Phase88Device::Phase88Device(DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ))
    : BeBoB::AvDevice( d, configRom)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created BeBoB::Terratec::Phase88Device (NodeID %d)\n",
                 getConfigRom().getNodeId() );
    updateClockSources();
}

Phase88Device::~Phase88Device()
{
}

void
Phase88Device::showDevice()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "This is a BeBoB::Terratec::Phase88Device\n");
    BeBoB::AvDevice::showDevice();
}
/*                'externalsync': ['/Mixer/Selector_8', self.comboExtSync], 
                'syncsource':   ['/Mixer/Selector_9', self.comboSyncSource], */
void
Phase88Device::updateClockSources() {
    m_internal_clocksource.type = FFADODevice::eCT_Internal;
    m_internal_clocksource.valid = true;
    m_internal_clocksource.locked = true;
    m_internal_clocksource.id = 0;
    m_internal_clocksource.slipping = false;
    m_internal_clocksource.description = "Internal";

    m_spdif_clocksource.type = FFADODevice::eCT_SPDIF;
    m_spdif_clocksource.valid = true;
    m_spdif_clocksource.locked = false;
    m_spdif_clocksource.id = 1;
    m_spdif_clocksource.slipping = false;
    m_spdif_clocksource.description = "S/PDIF";

    m_wordclock_clocksource.type = FFADODevice::eCT_WordClock;
    m_wordclock_clocksource.valid = true;
    m_wordclock_clocksource.locked = false;
    m_wordclock_clocksource.id = 2;
    m_wordclock_clocksource.slipping = false;
    m_wordclock_clocksource.description = "WordClock";
}

FFADODevice::ClockSource
Phase88Device::getActiveClockSource() {
    int fb_extsync_value = getSelectorFBValue(8);
    int fb_syncsource_value = getSelectorFBValue(9);

    debugOutput(DEBUG_LEVEL_VERBOSE,
                "Selectors: 0x%02X 0x%02X\n",
                fb_extsync_value, fb_syncsource_value);


    if(fb_syncsource_value == 0) {
        return m_internal_clocksource;
    } else {
        if(fb_extsync_value == 0) {
            return m_spdif_clocksource;
        } else {
            return m_wordclock_clocksource;
        }
    }
}

bool
Phase88Device::setActiveClockSource(ClockSource s) {
    if(s.id == m_internal_clocksource.id) {
        return setSelectorFBValue(9, 0);
    }
    if(s.id == m_spdif_clocksource.id) {
        bool retval = true;
        retval &= setSelectorFBValue(8, 0);
        retval &= setSelectorFBValue(9, 1);
        return retval;
    }
    if(s.id == m_wordclock_clocksource.id) {
        bool retval = true;
        retval &= setSelectorFBValue(8, 1);
        retval &= setSelectorFBValue(9, 1);
        return retval;
    }
    return false;
}

FFADODevice::ClockSourceVector
Phase88Device::getSupportedClockSources() {
    FFADODevice::ClockSourceVector r;
    r.push_back(m_internal_clocksource);
    r.push_back(m_spdif_clocksource);
    r.push_back(m_wordclock_clocksource);
    return r;
}

uint8_t
Phase88Device::getConfigurationIdSyncMode()
{
    uint8_t fb_extsync_value = getSelectorFBValue(8);
    uint8_t fb_syncsource_value = getSelectorFBValue(9);
    return fb_extsync_value & 0x01 | (fb_syncsource_value << 1) & 0x01;
}

} // namespace Terratec
} // namespace BeBoB
