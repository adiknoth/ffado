/* avc_generic.cpp
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

#include "avc_generic.h"
#include "serialize.h"

AVCCommand::AVCCommand( opcode_t opcode )
    : m_ctype( eCT_Unknown )
    , m_subunit( 0xff )
    , m_opcode( opcode )
    , m_eResponse( eR_Unknown )
    , m_verbose( false )
{
}

bool
AVCCommand::serialize( IOSSerialize& se )
{
    se.write( m_ctype, "AVCCommand ctype" );
    se.write( m_subunit, "AVCCommand subunit" );
    se.write( m_opcode, "AVCCommand opcode" );
    return true;
}

bool
AVCCommand::deserialize( IISDeserialize& de )
{
    de.read( &m_ctype );
    de.read( &m_subunit );
    de.read( &m_opcode );
    return true;
}

bool
AVCCommand::setCommandType( ECommandType commandType )
{
    m_ctype = commandType;
    return true;
}

AVCCommand::EResponse
AVCCommand::getResponse()
{
    return m_eResponse;
}

bool
AVCCommand::parseResponse( byte_t response )
{
    m_eResponse = static_cast<EResponse>( response );
    return true;
}

bool
AVCCommand::setSubunitType(subunit_type_t subunitType)
{
    m_subunit = ( subunitType << 3 ) | ( m_subunit & 0x7 );
    return true;
}

bool
AVCCommand::setSubunitId(subunit_id_t subunitId)
{
    m_subunit = ( subunitId & 0x7 ) | ( m_subunit & 0xf8 );
    return true;
}

bool
AVCCommand::setVerbose( bool enable )
{
    m_verbose = enable;
    return true;
}

bool
AVCCommand::isVerbose()
{
    return m_verbose;
}
