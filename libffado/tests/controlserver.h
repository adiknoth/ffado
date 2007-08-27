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

#ifndef CONTROLSERVER_H
#define CONTROLSERVER_H

#include "debugmodule/debugmodule.h"

#include <dbus-c++/dbus.h>

#include "controlserver-glue.h"

#include "libcontrol/BasicElements.h"

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
private:
    Control::Container &m_Slave;
    ElementVector m_Children;
};

class Contignous
: public org::ffado::Control::Element::Fader
, public Element
{
public:
    Contignous( DBus::Connection& connection,
                  std::string p,
                  Control::Contignous &slave );
    
    DBus::Double setValue( const DBus::Double & value );
    DBus::Double getValue( );

private:
    Control::Contignous &m_Slave;
};


}

#endif // CONTROLSERVER_H
