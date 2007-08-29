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

#include "Element.h"

namespace Control {

IMPL_DEBUG_MODULE( Element, Element, DEBUG_LEVEL_NORMAL );

// This serves as an ID
// incremented on every Element creation
// ID's are unique as long as we don't create 
// more than 2^64 elements.
// if we would create 1000 Elements per second
// we'd still need >500000 years to wrap.
// I guess we're safe.
static uint64_t GlobalElementCounter=0;

Element::Element()
: m_Name ( "NoName" )
, m_Label ( "No Label" )
, m_Description ( "No Description" )
, m_id(GlobalElementCounter++)
{
}

Element::Element(std::string n)
: m_Name( n )
, m_Label ( "No Label" )
, m_Description ( "No Description" )
, m_id(GlobalElementCounter++)
{
}

void
Element::show()
{
    debugOutput( DEBUG_LEVEL_NORMAL, "Element %s\n",
        getName().c_str());
}

void
Element::setVerboseLevel(int l)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting verbose level to %d...\n", l );
    setDebugLevel(l);
}

//// --- Container --- ////
Container::Container()
: Element()
{
}

Container::Container(std::string n)
: Element(n)
{
}

bool
Container::addElement(Element *e)
{
    assert(e);

    debugOutput( DEBUG_LEVEL_VERBOSE, "Adding Element %s to %s\n",
        e->getName().c_str(), getName().c_str());

    // don't allow duplicates, only makes life hard
    for ( ElementVectorIterator it = m_Children.begin();
      it != m_Children.end();
      ++it )
    {
        if(*it == e) {
            debugOutput( DEBUG_LEVEL_VERBOSE, "Not adding Element %s, already present\n",
                e->getName().c_str());
            return false;
        }
    }

    m_Children.push_back(e);
    return true;
}

bool
Container::deleteElement(Element *e)
{
    assert(e);
    debugOutput( DEBUG_LEVEL_VERBOSE, "Deleting Element %s from %s\n",
        e->getName().c_str(), getName().c_str());

    for ( ElementVectorIterator it = m_Children.begin();
      it != m_Children.end();
      ++it )
    {
        if(*it == e) {
            m_Children.erase(it);
            return true;
        }
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, "Element %s not found \n",e->getName().c_str());
    return false; //not found
}

void
Container::show()
{
    debugOutput( DEBUG_LEVEL_NORMAL, "Container %s (%d Elements)\n",
        getName().c_str(), m_Children.size());

    for ( ElementVectorIterator it = m_Children.begin();
      it != m_Children.end();
      ++it )
    {
        (*it)->show();
    }
}

} // namespace Control
