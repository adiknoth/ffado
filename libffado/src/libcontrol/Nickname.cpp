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

#include "Nickname.h"
#include "ffadodevice.h"

namespace Control {

//// --- Nickname --- ////
Nickname::Nickname(FFADODevice &d)
: Text()
, m_Device( d )
{
    setName("Nickname");
    setLabel("Nickname");
    setDescription("Get/Set device nickname");
}

bool
Nickname::setValue(std::string v)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "%s setValue(%s)\n",
                 getName().c_str(), v.c_str());
    return m_Device.setNickname(v);
}

std::string
Nickname::getValue()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "%s getValue()=%s\n",
                 getName().c_str(), m_Device.getNickname().c_str());
    return m_Device.getNickname();
}


void
Nickname::show()
{
    debugOutput( DEBUG_LEVEL_NORMAL, "Nickname Element %s, %s\n",
        getName().c_str(), m_Device.getNickname().c_str());
}

} // namespace Control
