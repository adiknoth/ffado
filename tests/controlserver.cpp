/*
 * Copyright (C) 2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * FFADO is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FFADO is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FFADO; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 *
 */

#include "controlserver.h"
#include "libcontrol/Element.h"
#include "libcontrol/BasicElements.h"

namespace DBusControl {

IMPL_DEBUG_MODULE( Element, Element, DEBUG_LEVEL_VERBOSE );

// --- Element
Element::Element( DBus::Connection& connection, std::string p, Control::Element &slave)
: DBus::ObjectAdaptor(connection, p)
, m_Slave(slave)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Element on '%s'\n",
                 path().c_str() );
}

DBus::UInt64
Element::getId( )
{
    return m_Slave.getId();
}

DBus::String
Element::getName( )
{
    return DBus::String(m_Slave.getName());
}

// --- Container
Container::Container( DBus::Connection& connection, std::string p, Control::Container &slave)
: Element(connection, p, slave)
, m_Slave(slave)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Container on '%s'\n",
                 path().c_str() );

    // add children for the slave container
    for ( Control::ConstElementVectorIterator it = slave.getElements().begin();
      it != slave.getElements().end();
      ++it )
    {
        Element *e=createHandler(*(*it));
        if (e) {
            m_Children.push_back(e);
        } else {
            debugWarning("Failed to create handler for Control::Element %s\n",
                (*it)->getName().c_str());
        }
    }
}

Container::~Container() {
    for ( ElementVectorIterator it = m_Children.begin();
      it != m_Children.end();
      ++it )
    {
        delete (*it);
    }
}

/**
 * \brief create a correct DBusControl counterpart for a given Control::Element
 */
Element *
Container::createHandler(Control::Element& e) {
    debugOutput( DEBUG_LEVEL_VERBOSE, "Creating handler for '%s'\n",
                 e.getName().c_str() );
                 
    if (dynamic_cast<Control::Container *>(&e) != NULL) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "Source is a Control::Container\n");
        
        return new Container(conn(), std::string(path()+"/"+e.getName()), 
            *dynamic_cast<Control::Container *>(&e));
    }
    if (dynamic_cast<Control::Contignous *>(&e) != NULL) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "Source is a Control::Contignous\n");
        
        return new Contignous(conn(), std::string(path()+"/"+e.getName()),
            *dynamic_cast<Control::Contignous *>(&e));
    }
    debugOutput( DEBUG_LEVEL_VERBOSE, "Source is a Control::Element\n");
    return new Element(conn(), std::string(path()+"/"+e.getName()), e);
}

// --- Contignous

Contignous::Contignous( DBus::Connection& connection, std::string p, Control::Contignous &slave)
: Element(connection, p, slave)
, m_Slave(slave)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Contignous on '%s'\n",
                 path().c_str() );
}

DBus::Double
Contignous::setValue( const DBus::Double& value )
{
    m_Slave.setValue(value);
    debugOutput( DEBUG_LEVEL_VERBOSE, "setValue(%lf) => %lf\n", value, m_Slave.getValue() );
    
    return m_Slave.getValue();
}

DBus::Double
Contignous::getValue(  )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "getValue() => %lf\n", m_Slave.getValue() );
    return m_Slave.getValue();
}

} // end of namespace Control
