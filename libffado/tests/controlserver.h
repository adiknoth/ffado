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

static const char* SERVER_NAME = "org.ffado.Control.Test";
static const char* SERVER_PATH = "/org/ffado/Control/Test";

namespace Control {

class Element {
public:
    Element() {};
};

class ControlServer
: public org::ffado::Control::Test,
  public DBus::IntrospectableAdaptor,
  public DBus::ObjectAdaptor
{
public:

    ControlServer( DBus::Connection& connection );
    
    DBus::Int32 Echo( const DBus::Int32 & value );
    std::map< DBus::String, DBus::String > Info();

private:
    DECLARE_DEBUG_MODULE;
};


}

#endif // CONTROLSERVER_H
