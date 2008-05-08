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

#include "avc_subunit_info.h"
#include "libutil/cmd_serialize.h"
#include "libieee1394/ieee1394service.h"

#include "libutil/ByteSwap.h"
#include <iostream>

using namespace std;

namespace AVC {


SubUnitInfoCmd::SubUnitInfoCmd( Ieee1394Service& ieee1349service )
    : AVCCommand( ieee1349service, AVC1394_CMD_SUBUNIT_INFO )
{
    clear();
}

bool
SubUnitInfoCmd::clear()
{
    m_page = 0xff;
    m_extension_code = 0x7;
    for ( int i = 0; i < eMaxSubunitsPerPage; ++i ) {
        m_table[i].m_subunit_type = 0xff;
        m_table[i].m_max_subunit_id = 0xff;
    }
    m_nrOfValidEntries = 0;
    return true;
}


SubUnitInfoCmd::~SubUnitInfoCmd()
{
}

bool
SubUnitInfoCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    AVCCommand::serialize( se );

    byte_t operand = 0;
    operand = ((  m_page & 0x7 ) << 4 ) | ( m_extension_code & 0x7 );
    se.write( operand, "SubUnitInfoCmd page and extension_code" );

    for ( int i = 0; i < eMaxSubunitsPerPage; ++i ) {
        operand =  ( m_table[i].m_subunit_type << 3 )
                   | ( m_table[i].m_max_subunit_id & 0x7 );
        se.write( operand, "SubUnitInfoCmd subunit_type and max_subunit_ID" );
    }

    return true;
}

bool
SubUnitInfoCmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    AVCCommand::deserialize( de );

    byte_t operand;
    de.read( &operand );
    m_page = ( operand >> 4 ) & 0x7;
    m_extension_code = operand & 0x7;

    m_nrOfValidEntries = 0;
    for ( int i = 0; i < eMaxSubunitsPerPage; ++i ) {
        de.read( &operand );
        m_table[i].m_subunit_type = operand >> 3;
        m_table[i].m_max_subunit_id = operand & 0x7;

        if ( operand != 0xff ) {
            m_nrOfValidEntries++;
        }
    }

    return true;
}

}
