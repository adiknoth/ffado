/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 * Copyright (C) 2005-2008 by Daniel Wagner
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

#include "avc_audiosubunit.h"
#include "../general/avc_subunit.h"

#include "../audiosubunit/avc_function_block.h"
#include "../audiosubunit/avc_descriptor_audio.h"

#include <sstream>

namespace AVC {

////////////////////////////////////////////

SubunitAudio::SubunitAudio( Unit& unit, subunit_t id )
    : Subunit( unit, eST_Audio, id )
    , m_identifier_descriptor ( new AVCAudioIdentifierDescriptor( &unit, this ) )
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

    // load the descriptor (if not already loaded)
//     m_identifier_descriptor->setVerboseLevel(DEBUG_LEVEL_VERY_VERBOSE);
//     if (m_identifier_descriptor != NULL) {
//         if(!m_identifier_descriptor->load()) {
//             debugWarning("Could not load Audio Subunit Identifier descriptor\n");
//         }
//     }

    return true;
}


const char*
SubunitAudio::getName()
{
    return "AVC::AudioSubunit";
}


}
