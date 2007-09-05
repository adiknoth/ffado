/*
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

#include "avc_subunit_info.h"
#include "libutil/cmd_serialize.h"
#include "libieee1394/ieee1394service.h"

#include <netinet/in.h>
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
SubUnitInfoCmd::serialize( Util::IOSSerialize& se )
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
SubUnitInfoCmd::deserialize( Util::IISDeserialize& de )
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
