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

#include "controlclient.h"

namespace DBusControl {

// ---

IMPL_DEBUG_MODULE( ContinuousClient, ContinuousClient, DEBUG_LEVEL_VERBOSE );

ContinuousClient::ContinuousClient( DBus::Connection& connection, const char* path, const char* name )
: DBus::ObjectProxy(connection, path, name)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created ContinuousClient '%s' on '%s'\n",
                 name, path );
}

} // end of namespace Control
