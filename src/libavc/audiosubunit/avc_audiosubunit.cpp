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

#include "avc_audiosubunit.h"
#include "../general/avc_subunit.h"

#warning merge with bebob functionblock
#include "bebob/bebob_functionblock.h"
#include "../audiosubunit/avc_function_block.h"


#include <sstream>

namespace AVC {

////////////////////////////////////////////

SubunitAudio::SubunitAudio( Unit& unit, subunit_t id )
    : Subunit( unit, eST_Audio, id )
{
}

SubunitAudio::SubunitAudio()
    : Subunit()
{
}

SubunitAudio::~SubunitAudio()
{

}

bool
SubunitAudio::discover()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "Discovering %s...\n", getName());
    
    if ( !Subunit::discover() ) {
        return false;
    }

    return true;
}


const char*
SubunitAudio::getName()
{
    return "AVC::AudioSubunit";
}


}
