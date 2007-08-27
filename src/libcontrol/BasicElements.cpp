/*
 * Copyright (C) 2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
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

#include "BasicElements.h"

namespace Control {

Contignous::Contignous()
: Element()
, m_Value( 0.0 )
{
}

Contignous::Contignous(std::string n)
: Element(n)
, m_Value( 0.0 )
{
}

void
Contignous::show()
{
    debugOutput( DEBUG_LEVEL_NORMAL, "Contignous Element %s, value=%lf\n",
        getName().c_str(), m_Value);
}

bool
Contignous::setValue(double v)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "%s setValue(%lf)\n",
        getName().c_str(), v);
    
    m_Value=v;
    return true;
}

double
Contignous::getValue()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "%s getValue()=%lf\n",
        getName().c_str(), m_Value);
    
    return m_Value;
}

} // namespace Control
