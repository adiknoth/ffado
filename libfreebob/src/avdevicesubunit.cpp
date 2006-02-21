/* avdevicesubunit.cpp
 * Copyright (C) 2005 by Daniel Wagner
 *
 * This file is part of FreeBob.
 *
 * FreeBob is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBob is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBob; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */

#include "avdevicesubunit.h"
#include "avdevice.h"
#include "avplug.h"

#include "libfreebobavc/avc_plug_info.h"
#include "libfreebobavc/avc_extended_stream_format.h"
#include "libfreebobavc/serialize.h"




IMPL_DEBUG_MODULE( AvDeviceSubunit, AvDeviceSubunit, DEBUG_LEVEL_VERBOSE );

////////////////////////////////////////////

AvDeviceSubunit::AvDeviceSubunit( AvDevice& avDevice, AVCCommand::ESubunitType type, subunit_t id )
    : m_avDevice( &avDevice )
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
AvDeviceSubunit::discover()
{
    PlugInfoCmd plugInfoCmd( m_avDevice->get1394Service(),
                             PlugInfoCmd::eSF_SerialBusIsochronousAndExternalPlug );
    plugInfoCmd.setNodeId( m_avDevice->getNodeId() );
    plugInfoCmd.setCommandType( AVCCommand::eCT_Status );
    plugInfoCmd.setSubunitType( m_sbType );
    plugInfoCmd.setSubunitId( m_sbId );

    if ( !plugInfoCmd.fire() ) {
        debugError( "discover: plug info command failed\n" );
        return false;
    }

    for ( int plugIdx = 0;
          plugIdx < plugInfoCmd.m_destinationPlugs;
          ++plugIdx )
    {
        AVCCommand::ESubunitType subunitType =
            static_cast<AVCCommand::ESubunitType>( getSubunitType() );
        AvPlug* plug = new AvPlug( *m_avDevice->get1394Service(),
                                   m_avDevice->getNodeId(),
                                   subunitType,
                                   getSubunitId(),
                                   PlugAddress::ePD_Input,
                                   plugIdx );
        if ( !plug || !plug->discover() ) {
            debugError( "discover: plug discover failed\n" );
            return false;
        }

        m_plugs.push_back( plug );
    }

    for ( int plugIdx = 0;
          plugIdx < plugInfoCmd.m_sourcePlugs;
          ++plugIdx )
    {
        AVCCommand::ESubunitType subunitType =
            static_cast<AVCCommand::ESubunitType>( getSubunitType() );
        AvPlug* plug = new AvPlug( *m_avDevice->get1394Service(),
                                   m_avDevice->getNodeId(),
                                   subunitType,
                                   getSubunitId(),
                                   PlugAddress::ePD_Output,
                                   plugIdx );
        if ( !plug || !plug->discover() ) {
            debugError( "discover: plug discover failed\n" );
            return false;
        }

        m_plugs.push_back( plug );
    }

    return true;
}


bool
AvDeviceSubunit::addPlug( AvPlug& plug )
{
    m_plugs.push_back( &plug );
    return true;
}

////////////////////////////////////////////

AvDeviceSubunitAudio::AvDeviceSubunitAudio( AvDevice& avDevice, subunit_t id )
    : AvDeviceSubunit( avDevice, AVCCommand::eST_Audio, id )
{
}

AvDeviceSubunitAudio::~AvDeviceSubunitAudio()
{
}

const char*
AvDeviceSubunitAudio::getName()
{
    return "AudioSubunit";
}

////////////////////////////////////////////

AvDeviceSubunitMusic::AvDeviceSubunitMusic( AvDevice& avDevice, subunit_t id )
    : AvDeviceSubunit( avDevice, AVCCommand::eST_Music, id )
{
}

AvDeviceSubunitMusic::~AvDeviceSubunitMusic()
{
}

const char*
AvDeviceSubunitMusic::getName()
{
    return "MusicSubunit";
}
