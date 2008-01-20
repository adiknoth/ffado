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

#include "avc_unit_info.h"
#include "libutil/cmd_serialize.h"
#include "libieee1394/ieee1394service.h"

#include <netinet/in.h>
#include <iostream>

using namespace std;

namespace AVC {


UnitInfoCmd::UnitInfoCmd( Ieee1394Service& ieee1349service )
    : AVCCommand( ieee1349service, AVC1394_CMD_UNIT_INFO )
    , m_reserved( 0xff )
    , m_unit_type( 0xff )
    , m_unit( 0xff )
    , m_company_id( 0xffffffff )
{
}

UnitInfoCmd::~UnitInfoCmd()
{
}

bool
UnitInfoCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    AVCCommand::serialize( se );

    se.write( m_reserved, "UnitInfoCmd reserved" );
    byte_t operand = ( m_unit_type << 3 ) | ( m_unit & 0x7 );
    se.write( operand, "UnitInfoCmd unit_type and unit" );
    operand = ( m_company_id >> 16 ) & 0xff;
    se.write( operand, "UnitInfoCmd company_ID (2)" );
    operand = ( m_company_id >> 8 ) & 0xff;
    se.write( operand, "UnitInfoCmd company_ID (1)" );
    operand = ( m_company_id >> 0 ) & 0xff;
    se.write( operand, "UnitInfoCmd company_ID (0)" );

    return true;
}

bool
UnitInfoCmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    AVCCommand::deserialize( de );

    de.read( &m_reserved );

    byte_t operand;
    de.read( &operand );
    m_unit_type = ( operand >> 3 );
    m_unit = ( operand & 0x7 );

    de.read( &operand );
    m_company_id = 0;
    m_company_id |= operand << 16;
    de.read( &operand );
    m_company_id |= operand << 8;
    de.read( &operand );
    m_company_id |= operand;

    return true;
}

}
