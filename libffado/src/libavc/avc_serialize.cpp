/* avc_serialize.cpp
 * Copyright (C) 2005,07 by Daniel Wagner
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

#include "avc_serialize.h"

#include <iostream>
#include <iomanip>

#include <netinet/in.h>

bool
CoutSerializer::write( byte_t d, const char* name )
{
    printf( "  %3d:\t0x%02x\t%s\n", m_cnt, d, name );
    m_cnt += sizeof( byte_t );

    return true;
}

bool
CoutSerializer::write( quadlet_t d, const char* name )
{
    printf( "  %3d:\t0x%08x\t%s\n", m_cnt, d, name );
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

