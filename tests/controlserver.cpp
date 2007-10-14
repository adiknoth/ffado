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

DBus::String
Element::getLabel( )
{
    return DBus::String(m_Slave.getLabel());
}

DBus::String
Element::getDescription( )
{
    return DBus::String(m_Slave.getDescription());
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

DBus::Int32
Container::getNbElements( ) {
    return m_Slave.countElements();
}

DBus::String
Container::getElementName( const DBus::Int32& i ) {
    int nbElements=m_Slave.countElements();
    if (i<nbElements) {
        return m_Slave.getElements().at(i)->getName();
    } else return "";
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
    
    if (dynamic_cast<Control::Continuous *>(&e) != NULL) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "Source is a Control::Continuous\n");
        
        return new Continuous(conn(), std::string(path()+"/"+e.getName()),
            *dynamic_cast<Control::Continuous *>(&e));
    }
    
    if (dynamic_cast<Control::Discrete *>(&e) != NULL) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "Source is a Control::Discrete\n");
        
        return new Discrete(conn(), std::string(path()+"/"+e.getName()),
            *dynamic_cast<Control::Discrete *>(&e));
    }
    
    if (dynamic_cast<ConfigRom *>(&e) != NULL) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "Source is a ConfigRom\n");
        
        return new ConfigRomX(conn(), std::string(path()+"/"+e.getName()),
            *dynamic_cast<ConfigRom *>(&e));
    }
    
    debugOutput( DEBUG_LEVEL_VERBOSE, "Source is a Control::Element\n");
    return new Element(conn(), std::string(path()+"/"+e.getName()), e);
}

// --- Continuous

Continuous::Continuous( DBus::Connection& connection, std::string p, Control::Continuous &slave)
: Element(connection, p, slave)
, m_Slave(slave)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Continuous on '%s'\n",
                 path().c_str() );
}

DBus::Double
Continuous::setValue( const DBus::Double& value )
{
    m_Slave.setValue(value);
/*    
    usleep(1000*500);
    
    debugOutput( DEBUG_LEVEL_VERBOSE, "setValue(%lf) => %lf\n", value, m_Slave.getValue() );
    
    return m_Slave.getValue();*/
    return value;
}

DBus::Double
Continuous::getValue(  )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "getValue() => %lf\n", m_Slave.getValue() );
    return m_Slave.getValue();
}

// --- Discrete

Discrete::Discrete( DBus::Connection& connection, std::string p, Control::Discrete &slave)
: Element(connection, p, slave)
, m_Slave(slave)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Discrete on '%s'\n",
                 path().c_str() );
}

DBus::Int32
Discrete::setValue( const DBus::Int32& value )
{
    m_Slave.setValue(value);
    
/*    usleep(1000*500);
    debugOutput( DEBUG_LEVEL_VERBOSE, "setValue(%d) => %d\n", value, m_Slave.getValue() );
    
    return m_Slave.getValue();*/
    return value;
}

DBus::Int32
Discrete::getValue(  )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "getValue() => %d\n", m_Slave.getValue() );
    return m_Slave.getValue();
}

// --- ConfigRom

ConfigRomX::ConfigRomX( DBus::Connection& connection, std::string p, ConfigRom &slave)
: Element(connection, p, slave)
, m_Slave(slave)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created ConfigRomX on '%s'\n",
                 path().c_str() );
}

DBus::String
ConfigRomX::getGUID( )
{
    return m_Slave.getGuidString();
}

DBus::String
ConfigRomX::getVendorName( )
{
    return m_Slave.getVendorName();
}

DBus::String
ConfigRomX::getModelName( )
{
    return m_Slave.getModelName();
}

DBus::Int32
ConfigRomX::getVendorId( )
{
    return m_Slave.getNodeVendorId();
}

DBus::Int32
ConfigRomX::getModelId( )
{
    return m_Slave.getModelId();
}

} // end of namespace Control
