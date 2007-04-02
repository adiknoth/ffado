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

#include "avc_serialize.h"

#include <iostream>
#include <iomanip>

#include <netinet/in.h>

IMPL_DEBUG_MODULE( CoutSerializer, CoutSerializer, DEBUG_LEVEL_NORMAL );

bool
CoutSerializer::write( byte_t d, const char* name )
{
    debugOutput( DEBUG_LEVEL_NORMAL, "  %3d:\t0x%02x\t%s\n", m_cnt, d, name );
    m_cnt += sizeof( byte_t );

    return true;
}

bool
CoutSerializer::write( quadlet_t d, const char* name )
{
    debugOutput( DEBUG_LEVEL_NORMAL, "  %3d:\t0x%08x\t%s\n", m_cnt, d, name );
    m_cnt += sizeof( quadlet_t );
    return true;
}

//////////////////////////////////////////////////

bool
StringSerializer::write( byte_t d, const char* name )
{
    char* result;
    asprintf( &result, "  %3d:\t0x%02x\t%s\n", m_cnt, d, name );

    m_string += result;
    free( result );

    m_cnt += sizeof( byte_t );

    return true;
}

bool
StringSerializer::write( quadlet_t d, const char* name )
{
    char* result;
    asprintf( &result, "  %3d:\t0x%08x\t%s\n", m_cnt, d, name );

    m_string += result;
    free( result );

    m_cnt += sizeof( quadlet_t );
    return true;
}

//////////////////////////////////////////////////

bool
BufferSerialize::write( byte_t value, const char* name )
{
    bool result = false;
    if ( isCurPosValid() ) {
        *m_curPos = value;
        m_curPos += sizeof( byte_t );
        result = true;
    }
    return result;
}

bool
BufferSerialize::write( quadlet_t value,  const char* name )
{
    bool result = false;
    if ( isCurPosValid() ) {
        * ( quadlet_t* )m_curPos = value;
        m_curPos += sizeof( quadlet_t );
        result = true;
    }
    return result;
}

bool
BufferSerialize::isCurPosValid() const
{
    if ( static_cast<size_t>( ( m_curPos - m_buffer ) ) >= m_length ) {
        return false;
    }
    return true;
}

//////////////////////////////////////////////////

bool
BufferDeserialize::read( byte_t* value )
{
    bool result = false;
    if ( isCurPosValid() ) {
        *value = *m_curPos;
        m_curPos += sizeof( byte_t );
        result = true;
    }
    return result;
}

bool
BufferDeserialize::read( quadlet_t* value )
{
    bool result = false;
    if ( isCurPosValid() ) {
        *value = *( ( quadlet_t* )m_curPos );
        m_curPos += sizeof( quadlet_t );
        result = true;
    }
    return result;
}

bool
BufferDeserialize::read( char** value, size_t length )
{
    bool result = false;
    if ( isCurPosValid() ) {
        *value = ( char* )m_curPos;
        m_curPos += length;
        result = true;
    }
    return result;
}

bool
BufferDeserialize::peek( byte_t* value )
{
    bool result = false;
    if ( isCurPosValid() ) {
        *value = *m_curPos;
        result = true;
    }
    return result;
}

bool
BufferDeserialize::isCurPosValid() const
{
    if ( static_cast<size_t>( ( m_curPos - m_buffer ) ) >= m_length ) {
        return false;
    }
    return true;
}

