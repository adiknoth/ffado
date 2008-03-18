/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
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

#include "ClockSelect.h"
#include "ffadodevice.h"

namespace Control {

//// --- ClockSelect --- ////
ClockSelect::ClockSelect(FFADODevice &d)
: AttributeEnum()
, m_Device( d )
{
    setName("ClockSelect");
    setLabel("Clock Source");
    setDescription("Select the device clock source");
}

bool
ClockSelect::select(int idx)
{
    FFADODevice::ClockSourceVector v = m_Device.getSupportedClockSources();
    if(idx > (int)v.size()) {
        debugError("index out of range\n");
        return false;
    }
    if(!m_Device.setActiveClockSource(v.at(idx))) {
        debugWarning("could not set active clocksource\n");
        return false;
    }
    return true;
}

int
ClockSelect::selected()
{
    FFADODevice::ClockSourceVector v = m_Device.getSupportedClockSources();
    FFADODevice::ClockSource active = m_Device.getActiveClockSource();
    int i=0;
    for (i=0; i < (int)v.size(); i++) {
        FFADODevice::ClockSource c = v.at(i);
        if(c == active) {
            return i;
        }
    }
    debugError("No active clock source found!\n");
    return -1;
}

int
ClockSelect::count()
{
    return m_Device.getSupportedClockSources().size();
}

std::string
ClockSelect::getEnumLabel(int idx)
{
    FFADODevice::ClockSourceVector v = m_Device.getSupportedClockSources();
    if(idx > (int)v.size()) {
        debugError("index out of range\n");
        return false;
    }
    return v.at(idx).description;
}

int
ClockSelect::attributeCount()
{
/*
        /// indicates the type of the clock source (e.g. eCT_ADAT)
        enum eClockSourceType type;
        /// indicated the id of the clock source (e.g. id=1 => clocksource is ADAT_1)
        unsigned int id;
        /// is the clock source valid (i.e. can be selected) at this moment?
        bool valid;
        /// is the clock source active at this moment?
        bool active;
        /// is the clock source locked?
        bool locked;
        /// is the clock source slipping?
        bool slipping;
        /// description of the clock struct (optional)
        std::string description;
*/
    return 7;
}

std::string
ClockSelect::getAttributeValue(int attridx)
{
    char tmp[16];
    std::string retval = "bad attr index";
    FFADODevice::ClockSource active = m_Device.getActiveClockSource();
    
    switch(attridx) {
        case 0: 
            retval = FFADODevice::ClockSourceTypeToString(active.type);
            break;
        case 1:
            snprintf(tmp, 16, "%u", active.id);
            retval = tmp;
            break;
        case 2:
            snprintf(tmp, 16, "%u", active.valid);
            retval = tmp;
            break;
        case 3:
            snprintf(tmp, 16, "%u", active.active);
            retval = tmp;
            break;
        case 4:
            snprintf(tmp, 16, "%u", active.locked);
            retval = tmp;
            break;
        case 5:
            snprintf(tmp, 16, "%u", active.slipping);
            retval = tmp;
            break;
        case 6:
            retval = active.description;
    }
    return retval;
}

std::string
ClockSelect::getAttributeName(int attridx)
{
    switch(attridx) {
        case 0: return "type";
        case 1: return "id";
        case 2: return "valid";
        case 3: return "active";
        case 4: return "locked";
        case 5: return "slipping";
        case 6: return "description";
        default: return "bad attr index";
    }
}

void
ClockSelect::show()
{
    debugOutput( DEBUG_LEVEL_NORMAL, "ClockSelect Element %s, active: %s\n",
        getName().c_str(), m_Device.getActiveClockSource().description.c_str());
}

} // namespace Control
