/*
 * Copyright (C)      2007 by Pieter Palmers
 * Copyright (C) 2005-2007 by Daniel Wagner
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

#warning this header should go
#include "bebob/bebob_avplug.h"
#include "libieee1394/configrom.h"

#include "../general/avc_subunit.h"
#include "../general/avc_unit.h"

#include "../general/avc_plug_info.h"
#include "../streamformat/avc_extended_stream_format.h"
#include "../util/avc_serialize.h"

#include <sstream>

namespace AVC {

////////////////////////////////////////////

SubunitMusic::SubunitMusic( Unit& unit, subunit_t id )
    : Subunit( unit, eST_Music, id )
{
}

SubunitMusic::SubunitMusic()
    : Subunit()
{
}

SubunitMusic::~SubunitMusic()
{
}

const char*
SubunitMusic::getName()
{
    return "MusicSubunit";
}

bool
SubunitMusic::serializeChild( Glib::ustring basePath,
                                             Util::IOSerialize& ser ) const
{
    return true;
}

bool
SubunitMusic::deserializeChild( Glib::ustring basePath,
                                               Util::IODeserialize& deser,
                                               Unit& unit )
{
    return true;
}

}
