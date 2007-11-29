/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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
    setDebugLevel(l);
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting verbose level to %d...\n", l );
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
    if (e==NULL) {
        debugWarning("Cannot add NULL element\n");
        return false;
    }

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

bool
Container::clearElements(bool delete_pointers) 
{
    while(m_Children.size()) {
        Element *e=m_Children[0];
        deleteElement(e);
        if (delete_pointers) delete e;
    }
    return true;
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

void
Container::setVerboseLevel(int l)
{
    setDebugLevel(l);
    for ( ElementVectorIterator it = m_Children.begin();
      it != m_Children.end();
      ++it )
    {
        (*it)->setVerboseLevel(l);
    }
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting verbose level to %d...\n", l );
}

} // namespace Control
