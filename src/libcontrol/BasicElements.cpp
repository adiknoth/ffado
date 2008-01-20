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

#include "BasicElements.h"

namespace Control {

Continuous::Continuous()
: Element()
, m_Value( 0.0 )
{
}

Continuous::Continuous(std::string n)
: Element(n)
, m_Value( 0.0 )
{
}

void
Continuous::show()
{
    debugOutput( DEBUG_LEVEL_NORMAL, "Continuous Element %s, value=%lf\n",
        getName().c_str(), m_Value);
}

bool
Continuous::setValue(double v)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "%s setValue(%lf)\n",
        getName().c_str(), v);
    
    m_Value=v;
    return true;
}

double
Continuous::getValue()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "%s getValue()=%lf\n",
        getName().c_str(), m_Value);
    
    return m_Value;
}

//// ---

Discrete::Discrete()
: Element()
, m_Value( 0 )
{
}

Discrete::Discrete(std::string n)
: Element(n)
, m_Value( 0 )
{
}

void
Discrete::show()
{
    debugOutput( DEBUG_LEVEL_NORMAL, "Discrete Element %s, value=%d\n",
        getName().c_str(), m_Value);
}

bool
Discrete::setValue(int v)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "%s setValue(%d)\n",
        getName().c_str(), v);
    
    m_Value=v;
    return true;
}

int
Discrete::getValue()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "%s getValue()=%d\n",
        getName().c_str(), m_Value);
    
    return m_Value;
}

} // namespace Control
