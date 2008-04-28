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

#ifndef CONTROLSERVER_H
#define CONTROLSERVER_H

#include "debugmodule/debugmodule.h"

#include <dbus-c++/dbus.h>

#include "controlserver-glue.h"

#include "libcontrol/BasicElements.h"
#include "libieee1394/configrom.h"

namespace Control {
    class MatrixMixer;
};

namespace DBusControl {

class Element
: public org::ffado::Control::Element::Element
, public DBus::IntrospectableAdaptor
, public DBus::ObjectAdaptor
{
public:

    Element( DBus::Connection& connection,
             std::string p,
             Control::Element &slave );

    DBus::UInt64 getId( );
    DBus::String getName( );
    DBus::String getLabel( );
    DBus::String getDescription( );

private:
    Control::Element &m_Slave;

protected:
    DECLARE_DEBUG_MODULE;
};
typedef std::vector<Element *> ElementVector;
typedef std::vector<Element *>::iterator ElementVectorIterator;
typedef std::vector<Element *>::const_iterator ConstElementVectorIterator;

class Container
: public org::ffado::Control::Element::Container
, public Element
{
public:
    Container( DBus::Connection& connection,
                  std::string p,
                  Control::Container &slave );
    virtual ~Container();
    
    Element *createHandler(Control::Element& e);

    DBus::Int32 getNbElements( );
    DBus::String getElementName( const DBus::Int32& );

private:
    Control::Container &m_Slave;
    ElementVector m_Children;
};

class Continuous
: public org::ffado::Control::Element::Continuous
, public Element
{
public:
    Continuous( DBus::Connection& connection,
                  std::string p,
                  Control::Continuous &slave );
    
    DBus::Double setValue( const DBus::Double & value );
    DBus::Double getValue( );
    DBus::Double getMinimum( );
    DBus::Double getMaximum( );
    DBus::Double setValueIdx( const DBus::Int32 & idx,
                              const DBus::Double & value );
    DBus::Double getValueIdx( const DBus::Int32 & idx );

private:
    Control::Continuous &m_Slave;
};

class Discrete
: public org::ffado::Control::Element::Discrete
, public Element
{
public:
    Discrete( DBus::Connection& connection,
              std::string p,
              Control::Discrete &slave );
    
    DBus::Int32 setValue( const DBus::Int32 & value );
    DBus::Int32 getValue( );
    DBus::Int32 setValueIdx( const DBus::Int32 & idx,
                             const DBus::Int32 & value );
    DBus::Int32 getValueIdx( const DBus::Int32 & idx );

private:
    Control::Discrete &m_Slave;
};

class Text
: public org::ffado::Control::Element::Text
, public Element
{
public:
    Text( DBus::Connection& connection,
          std::string p,
          Control::Text &slave );

    DBus::String setValue( const DBus::String & value );
    DBus::String getValue( );

private:
    Control::Text &m_Slave;
};

class Register
: public org::ffado::Control::Element::Register
, public Element
{
public:
    Register( DBus::Connection& connection,
              std::string p,
              Control::Register &slave );
    
    DBus::UInt64 setValue( const DBus::UInt64 & addr, const DBus::UInt64 & value );
    DBus::UInt64 getValue( const DBus::UInt64 & addr );

private:
    Control::Register &m_Slave;
};

class Enum
: public org::ffado::Control::Element::Enum
, public Element
{
public:
    Enum( DBus::Connection& connection,
          std::string p,
          Control::Enum &slave );
    
    DBus::Int32 select( const DBus::Int32 & idx );
    DBus::Int32 selected( );
    DBus::Int32 count( );
    DBus::String getEnumLabel( const DBus::Int32 & idx );

private:
    Control::Enum &m_Slave;
};

class AttributeEnum
: public org::ffado::Control::Element::AttributeEnum
, public Element
{
public:
    AttributeEnum( DBus::Connection& connection,
                   std::string p,
                   Control::AttributeEnum &slave );
    
    DBus::Int32 select( const DBus::Int32 & idx );
    DBus::Int32 selected( );
    DBus::Int32 count( );
    DBus::Int32 attributeCount();
    DBus::String getEnumLabel( const DBus::Int32 & idx );
    DBus::String getAttributeValue( const DBus::Int32 & idx );
    DBus::String getAttributeName( const DBus::Int32 & idx );

private:
    Control::AttributeEnum &m_Slave;
};

// FIXME: to change this to a normal ConfigRom class name we have to
// put the 1394 config rom class into a separate namespace.
class ConfigRomX
: public org::ffado::Control::Element::ConfigRomX
, public Element
{
public:
    ConfigRomX( DBus::Connection& connection,
                  std::string p,
                  ConfigRom &slave );

    DBus::String getGUID( );
    DBus::String getVendorName( );
    DBus::String getModelName( );
    DBus::Int32 getVendorId( );
    DBus::Int32 getModelId( );
    DBus::Int32 getUnitVersion( );

private:
    ConfigRom &m_Slave;
};

class MatrixMixer
: public org::ffado::Control::Element::MatrixMixer
, public Element
{
public:
    MatrixMixer(  DBus::Connection& connection,
                  std::string p,
                  Control::MatrixMixer &slave );

    DBus::String getRowName( const DBus::Int32& );
    DBus::String getColName( const DBus::Int32& );
    DBus::Int32 canWrite( const DBus::Int32&, const DBus::Int32& );
    DBus::Double setValue( const DBus::Int32&, const DBus::Int32&, const DBus::Double& );
    DBus::Double getValue( const DBus::Int32&, const DBus::Int32& );
    DBus::Int32 getRowCount( );
    DBus::Int32 getColCount( );

private:
    Control::MatrixMixer &m_Slave;
};

}

#endif // CONTROLSERVER_H
