/*
 * Copyright (C) 2005-2007 by Pieter Palmers
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

#include <lo/lo.h>

#include "OscClient.h"
#include "OscMessage.h"
#include "OscResponse.h"

#include <assert.h>

namespace OSC {

IMPL_DEBUG_MODULE( OscClient, OscClient, DEBUG_LEVEL_VERBOSE );

OscClient::OscClient(string target)
    : m_target(target)
{

}

OscClient::~OscClient() {

}

void
OscClient::setVerboseLevel(int l) {
    setDebugLevel(l);
}

bool
OscClient::init()
{

    return true;
}

// callbacks

void
OscClient::error_cb(int num, const char* msg, const char* path)
{
    debugError("liblo server error %d in path %s, message: %s\n", num, path, msg);
}

// Display incoming OSC messages (for debugging purposes)
int
OscClient::generic_cb(const char* path, const char* types, lo_arg** argv, int argc, lo_message msg, void* user_data)
{
    OscClient *client=reinterpret_cast<OscClient *>(user_data);
    assert(client);

    debugOutput(DEBUG_LEVEL_VERBOSE, "Message on: %s\n", path);

    return 0;
}


} // end of namespace OSC
