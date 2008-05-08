/*
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

#include "avc_plug_info.h"
#include "libutil/cmd_serialize.h"
#include "libieee1394/ieee1394service.h"

#include "libutil/ByteSwap.h"
#include <iostream>

using namespace std;

namespace AVC {

PlugInfoCmd::PlugInfoCmd( Ieee1394Service& ieee1394service,
                          ESubFunction eSubFunction )
    : AVCCommand( ieee1394service, AVC1394_CMD_PLUG_INFO )
    , m_serialBusIsochronousInputPlugs( 0xff )
    , m_serialBusIsochronousOutputPlugs( 0xff )
    , m_externalInputPlugs( 0xff )
    , m_externalOutputPlugs( 0xff )
    , m_serialBusAsynchronousInputPlugs( 0xff )
    , m_serialBusAsynchronousOuputPlugs( 0xff )
    , m_destinationPlugs( 0xff )
    , m_sourcePlugs( 0xff )
    , m_subFunction( eSubFunction )
{
}

PlugInfoCmd::PlugInfoCmd( const PlugInfoCmd& rhs )
    : AVCCommand( rhs )
    , m_serialBusIsochronousInputPlugs( rhs.m_serialBusIsochronousInputPlugs )
    , m_serialBusIsochronousOutputPlugs( rhs.m_serialBusIsochronousOutputPlugs )
    , m_externalInputPlugs( rhs.m_externalInputPlugs )
    , m_externalOutputPlugs( rhs.m_externalOutputPlugs )
    , m_serialBusAsynchronousInputPlugs( rhs.m_serialBusAsynchronousInputPlugs )
    , m_serialBusAsynchronousOuputPlugs( rhs.m_serialBusAsynchronousOuputPlugs )
    , m_destinationPlugs( rhs.m_destinationPlugs )
    , m_sourcePlugs( rhs.m_sourcePlugs )
    , m_subFunction( rhs.m_subFunction )
{
}

PlugInfoCmd::~PlugInfoCmd()
{
}

void
PlugInfoCmd::clear()
{
    m_serialBusIsochronousInputPlugs=0xff;
    m_serialBusIsochronousOutputPlugs=0xff;
    m_externalInputPlugs=0xff;
    m_externalOutputPlugs=0xff;
    m_serialBusAsynchronousInputPlugs=0xff;
    m_serialBusAsynchronousOuputPlugs=0xff;
    m_destinationPlugs=0xff;
    m_sourcePlugs=0xff;
}

bool
PlugInfoCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    byte_t reserved = 0xff;

    AVCCommand::serialize( se );
    se.write( m_subFunction, "PlugInfoCmd subFunction" );
    switch( getSubunitType() ) {
    case eST_Unit:
        switch( m_subFunction ) {
        case eSF_SerialBusIsochronousAndExternalPlug:
            se.write( m_serialBusIsochronousInputPlugs, "PlugInfoCmd serialBusIsochronousInputPlugs" );
            se.write( m_serialBusIsochronousOutputPlugs, "PlugInfoCmd serialBusIsochronousOutputPlugs" );
            se.write( m_externalInputPlugs, "PlugInfoCmd externalInputPlugs" );
            se.write( m_externalOutputPlugs, "PlugInfoCmd externalOutputPlugs" );
            break;
        case eSF_SerialBusAsynchonousPlug:
            se.write( m_serialBusAsynchronousInputPlugs, "PlugInfoCmd serialBusAsynchronousInputPlugs" );
            se.write( m_serialBusAsynchronousOuputPlugs, "PlugInfoCmd serialBusAsynchronousOuputPlugs" );
            se.write( reserved, "PlugInfoCmd" );
            se.write( reserved, "PlugInfoCmd" );
            break;
        default:
            cerr << "Could not serialize with subfucntion " << m_subFunction << endl;
            return false;
        }
        break;
    default:
        se.write( m_destinationPlugs, "PlugInfoCmd destinationPlugs" );
        se.write( m_sourcePlugs, "PlugInfoCmd sourcePlugs" );
        se.write( reserved, "PlugInfoCmd" );
        se.write( reserved, "PlugInfoCmd" );
    }
    return true;
}

bool
PlugInfoCmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    byte_t reserved;

    AVCCommand::deserialize( de );
    de.read( &m_subFunction );
    switch ( getSubunitType() ) {
    case eST_Unit:
        switch ( m_subFunction ) {
        case eSF_SerialBusIsochronousAndExternalPlug:
            de.read( &m_serialBusIsochronousInputPlugs );
            de.read( &m_serialBusIsochronousOutputPlugs );
            de.read( &m_externalInputPlugs );
            de.read( &m_externalOutputPlugs );
            break;
        case eSF_SerialBusAsynchonousPlug:
            de.read( &m_serialBusAsynchronousInputPlugs );
            de.read( &m_serialBusAsynchronousOuputPlugs );
            de.read( &reserved );
            de.read( &reserved );
            break;
        default:
            cerr << "Could not deserialize with subfunction " << m_subFunction << endl;
            return false;
        }
        break;
    default:
        de.read( &m_destinationPlugs );
        de.read( &m_sourcePlugs );
        de.read( &reserved );
        de.read( &reserved );
    }
    return true;
}

bool
PlugInfoCmd::setSubFunction( ESubFunction subFunction )
{
    m_subFunction = subFunction;
    return true;
}

}
