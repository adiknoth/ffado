/* bebob_light_avdevicesubunit.cpp
 * Copyright (C) 2006 by Daniel Wagner
 *
 * This file is part of FreeBoB.
 *
 * FreeBoB is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBoB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBoB; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */

#include "bebob_light/bebob_light_avdevicesubunit.h"
#include "bebob_light/bebob_light_avdevice.h"
#include "bebob_light/bebob_light_avplug.h"

namespace BeBoB_Light {

IMPL_DEBUG_MODULE( AvDeviceSubunit, AvDeviceSubunit, DEBUG_LEVEL_VERBOSE );

////////////////////////////////////////////

AvDeviceSubunit::AvDeviceSubunit( AvDevice* avDevice, subunit_type_t type, subunit_t id )
    : m_avDevice( avDevice )
    , m_sbType( type )
    , m_sbId( id )
{
}

AvDeviceSubunit::~AvDeviceSubunit()
{
    for ( AvPlugVector::iterator it = m_plugs.begin();
          it != m_plugs.end();
          ++it )
    {
        delete *it;
    }
}

bool
AvDeviceSubunit::addPlug( AvPlug& plug )
{
    m_plugs.push_back( &plug );
    return true;
}

////////////////////////////////////////////

AvDeviceSubunitAudio::AvDeviceSubunitAudio( AvDevice* avDevice, subunit_t id )
    : AvDeviceSubunit( avDevice, AVCCommand::eST_Audio, id )
{
}

AvDeviceSubunitAudio::~AvDeviceSubunitAudio()
{
}

bool
AvDeviceSubunitAudio::discover()
{
    return true;
}

const char*
AvDeviceSubunitAudio::getName()
{
    return "AudioSubunit";
}

////////////////////////////////////////////

AvDeviceSubunitMusic::AvDeviceSubunitMusic( AvDevice* avDevice, subunit_t id )
    : AvDeviceSubunit( avDevice, AVCCommand::eST_Music, id )
{
}

AvDeviceSubunitMusic::~AvDeviceSubunitMusic()
{
}

bool
AvDeviceSubunitMusic::discover()
{
    return true;
}

const char*
AvDeviceSubunitMusic::getName()
{
    return "MusicSubunit";
}

}
