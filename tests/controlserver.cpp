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

namespace Control {

IMPL_DEBUG_MODULE( ControlServer, ControlServer, DEBUG_LEVEL_VERBOSE );

ControlServer::ControlServer( DBus::Connection& connection )
: DBus::ObjectAdaptor(connection, SERVER_PATH)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created ControlServer on '%s'\n",
                 SERVER_PATH );
}

DBus::Int32 ControlServer::Echo( const DBus::Int32& value )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Echo(%d)\n", value );
    return value;
}

std::map< DBus::String, DBus::String > ControlServer::Info()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Info()\n" );
    
    std::map< DBus::String, DBus::String > info;

    info["testset1"] = "set1";
    info["testset2"] = "set2";

    return info;
}


} // end of namespace Control
