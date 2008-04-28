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

double
Continuous::getMinimum()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "%s getMinimum()=%lf\n",
        getName().c_str(), m_Value);
    return m_Value;
}

double
Continuous::getMaximum()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "%s getMaximum()=%lf\n",
        getName().c_str(), m_Value);
    return m_Value;
}

bool
Continuous::setValue(int idx, double v)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "%s setValue(%d, %lf)\n",
        getName().c_str(), idx, v);
    return setValue(v);
}

double
Continuous::getValue(int idx)
{
    double retval = getValue();
    debugOutput( DEBUG_LEVEL_VERBOSE, "%s getValue()=%lf\n",
        getName().c_str(), retval);
    return retval;
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
bool
Discrete::setValue(int idx, int v)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "%s setValue(%d, %d)\n",
        getName().c_str(), idx, v);
    return setValue(v);
}

int
Discrete::getValue(int idx)
{
    int retval = getValue();
    debugOutput( DEBUG_LEVEL_VERBOSE, "%s getValue()=%d\n",
        getName().c_str(), retval);
    return retval;
}

//// ---

Text::Text()
: Element()
, m_Value( "" )
{
}

Text::Text(std::string n)
: Element(n)
, m_Value( "" )
{
}

void
Text::show()
{
    debugOutput( DEBUG_LEVEL_NORMAL, "Text Element %s, value=%s\n",
                 getName().c_str(), m_Value.c_str());
}

bool
Text::setValue(std::string v)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "%s setValue(%s)\n",
                 getName().c_str(), v.c_str());
    m_Value=v;
    return true;
}

std::string
Text::getValue()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "%s getValue()=%s\n",
                 getName().c_str(), m_Value.c_str());
    return m_Value;
}

//// ---

Enum::Enum()
: Element()
, m_selected( -1 )
{
}

Enum::Enum(std::string n)
: Element(n)
, m_selected( -1 )
{
}

void
Enum::show()
{
    debugOutput( DEBUG_LEVEL_NORMAL, "Enum Element %s, selected=%d\n",
                 getName().c_str(), m_selected);
}

// NOTE: dummy implementation for tests
bool
Enum::select(int idx)
{
    if(idx <3) {
        m_selected=idx;
        return true;
    } else {
        return false;
    }
}

int
Enum::selected()
{
    return m_selected;
}

int
Enum::count()
{
    return 3;
}

std::string
Enum::getEnumLabel(int idx)
{
    switch(idx) {
        case 0: return "enum val 1";
        case 1: return "enum val 2";
        case 2: return "enum val 3";
        default: return "bad index";
    }
}


//// ---

AttributeEnum::AttributeEnum()
: Enum()
{
}

AttributeEnum::AttributeEnum(std::string n)
: Enum(n)
{
}

void
AttributeEnum::show()
{
    debugOutput( DEBUG_LEVEL_NORMAL, "AttributeEnum Element %s\n",
                 getName().c_str());
    Enum::show();
}

// NOTE: dummy implementation for tests
int
AttributeEnum::attributeCount()
{
    return 2;
}

std::string
AttributeEnum::getAttributeValue(int attridx)
{
    switch(attridx) {
        case 0: return "attr val 1";
        case 1: return "attr val 2";
        default: return "bad attr index";
    }
}

std::string
AttributeEnum::getAttributeName(int attridx)
{
    switch(attridx) {
        case 0: return "attr 1";
        case 1: return "attr 2";
        default: return "bad attr index";
    }
}

} // namespace Control
