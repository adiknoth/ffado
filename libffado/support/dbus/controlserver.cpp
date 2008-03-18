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

#include "controlserver.h"
#include "libcontrol/Element.h"
#include "libcontrol/BasicElements.h"
#include "libcontrol/MatrixMixer.h"
#include "libutil/Time.h"

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
    
    if (dynamic_cast<Control::Text *>(&e) != NULL) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "Source is a Control::Text\n");
        
        return new Text(conn(), std::string(path()+"/"+e.getName()),
            *dynamic_cast<Control::Text *>(&e));
    }

    // note that we have to check this before checking the Enum,
    // since Enum is a base class
    if (dynamic_cast<Control::AttributeEnum *>(&e) != NULL) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "Source is a Control::AttributeEnum\n");
        
        return new AttributeEnum(conn(), std::string(path()+"/"+e.getName()),
            *dynamic_cast<Control::AttributeEnum *>(&e));
    }
    
    if (dynamic_cast<Control::Enum *>(&e) != NULL) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "Source is a Control::Enum\n");
        
        return new Enum(conn(), std::string(path()+"/"+e.getName()),
            *dynamic_cast<Control::Enum *>(&e));
    }
    
    if (dynamic_cast<ConfigRom *>(&e) != NULL) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "Source is a ConfigRom\n");
        
        return new ConfigRomX(conn(), std::string(path()+"/"+e.getName()),
            *dynamic_cast<ConfigRom *>(&e));
    }
    
    if (dynamic_cast<Control::MatrixMixer *>(&e) != NULL) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "Source is a Control::MatrixMixer\n");
        
        return new MatrixMixer(conn(), std::string(path()+"/"+e.getName()),
            *dynamic_cast<Control::MatrixMixer *>(&e));
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
    SleepRelativeUsec(1000*500);
    
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
    
/*    SleepRelativeUsec(1000*500);
    debugOutput( DEBUG_LEVEL_VERBOSE, "setValue(%d) => %d\n", value, m_Slave.getValue() );
    
    return m_Slave.getValue();*/
    return value;
}

DBus::Int32
Discrete::getValue()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "getValue() => %d\n", m_Slave.getValue() );
    return m_Slave.getValue();
}

// --- Text

Text::Text( DBus::Connection& connection, std::string p, Control::Text &slave)
: Element(connection, p, slave)
, m_Slave(slave)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Text on '%s'\n",
                 path().c_str() );
}

DBus::String
Text::setValue( const DBus::String& value )
{
    m_Slave.setValue(value);
    
/*    SleepRelativeUsec(1000*500);
    debugOutput( DEBUG_LEVEL_VERBOSE, "setValue(%d) => %d\n", value, m_Slave.getValue() );
    
    return m_Slave.getValue();*/
    return value;
}

DBus::String
Text::getValue()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "getValue() => %s\n", m_Slave.getValue().c_str() );
    return m_Slave.getValue();
}


// --- Enum

Enum::Enum( DBus::Connection& connection, std::string p, Control::Enum &slave)
: Element(connection, p, slave)
, m_Slave(slave)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Enum on '%s'\n",
                 path().c_str() );
}

DBus::Int32
Enum::select( const DBus::Int32& idx )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "select(%d)\n", idx );
    return  m_Slave.select(idx);
}

DBus::Int32
Enum::selected()
{
    int retval = m_Slave.selected();
    debugOutput( DEBUG_LEVEL_VERBOSE, "selected() => %d\n", retval );
    return retval;
}

DBus::Int32
Enum::count()
{
    int retval = m_Slave.count();
    debugOutput( DEBUG_LEVEL_VERBOSE, "count() => %d\n", retval );
    return retval;
}

DBus::String
Enum::getEnumLabel( const DBus::Int32 & idx )
{
    std::string retval = m_Slave.getEnumLabel(idx);
    debugOutput( DEBUG_LEVEL_VERBOSE, "getEnumLabel(%d) => %s\n", idx, retval.c_str() );
    return retval;
}

// --- AttributeEnum
AttributeEnum::AttributeEnum( DBus::Connection& connection, std::string p, Control::AttributeEnum &slave)
: Element(connection, p, slave)
, m_Slave(slave)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Enum on '%s'\n",
                 path().c_str() );
}

DBus::Int32
AttributeEnum::select( const DBus::Int32& idx )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "select(%d)\n", idx );
    return  m_Slave.select(idx);
}

DBus::Int32
AttributeEnum::selected()
{
    int retval = m_Slave.selected();
    debugOutput( DEBUG_LEVEL_VERBOSE, "selected() => %d\n", retval );
    return retval;
}

DBus::Int32
AttributeEnum::count()
{
    int retval = m_Slave.count();
    debugOutput( DEBUG_LEVEL_VERBOSE, "count() => %d\n", retval );
    return retval;
}

DBus::Int32
AttributeEnum::attributeCount()
{
    int retval = m_Slave.attributeCount();
    debugOutput( DEBUG_LEVEL_VERBOSE, "attributeCount() => %d\n", retval );
    return retval;
}

DBus::String
AttributeEnum::getEnumLabel( const DBus::Int32 & idx )
{
    std::string retval = m_Slave.getEnumLabel(idx);
    debugOutput( DEBUG_LEVEL_VERBOSE, "getEnumLabel(%d) => %s\n", idx, retval.c_str() );
    return retval;
}

DBus::String
AttributeEnum::getAttributeValue( const DBus::Int32 & idx )
{
    std::string retval = m_Slave.getAttributeValue(idx);
    debugOutput( DEBUG_LEVEL_VERBOSE, "getAttributeValue(%d) => %s\n", idx, retval.c_str() );
    return retval;
}

DBus::String
AttributeEnum::getAttributeName( const DBus::Int32 & idx )
{
    std::string retval = m_Slave.getAttributeName(idx);
    debugOutput( DEBUG_LEVEL_VERBOSE, "getAttributeName(%d) => %s\n", idx, retval.c_str() );
    return retval;
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

// --- MatrixMixer

MatrixMixer::MatrixMixer( DBus::Connection& connection, std::string p, Control::MatrixMixer &slave)
: Element(connection, p, slave)
, m_Slave(slave)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created MatrixMixer on '%s'\n",
                 path().c_str() );
}

DBus::String
MatrixMixer::getRowName( const DBus::Int32& row) {
    return m_Slave.getRowName(row);
}

DBus::String
MatrixMixer::getColName( const DBus::Int32& col) {
    return m_Slave.getColName(col);
}

DBus::Int32
MatrixMixer::canWrite( const DBus::Int32& row, const DBus::Int32& col) {
    return m_Slave.canWrite(row,col);
}

DBus::Double
MatrixMixer::setValue( const DBus::Int32& row, const DBus::Int32& col, const DBus::Double& val ) {
    return m_Slave.setValue(row,col,val);
}

DBus::Double
MatrixMixer::getValue( const DBus::Int32& row, const DBus::Int32& col) {
    return m_Slave.getValue(row,col);
}

DBus::Int32
MatrixMixer::getRowCount( ) {
    return m_Slave.getRowCount();
}

DBus::Int32
MatrixMixer::getColCount( ) {
    return m_Slave.getColCount();
}

} // end of namespace Control
