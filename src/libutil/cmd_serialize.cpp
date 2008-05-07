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

#include "cmd_serialize.h"

#include <iostream>
#include <iomanip>

#include <byteswap.h>


namespace Util {
  namespace Cmd {


IMPL_DEBUG_MODULE( CoutSerializer, CoutSerializer, DEBUG_LEVEL_NORMAL );
IMPL_DEBUG_MODULE( StringSerializer, StringSerializer, DEBUG_LEVEL_NORMAL );
IMPL_DEBUG_MODULE( BufferSerialize, BufferSerialize, DEBUG_LEVEL_NORMAL );
IMPL_DEBUG_MODULE( BufferDeserialize, BufferDeserialize, DEBUG_LEVEL_NORMAL );

bool
CoutSerializer::write( byte_t d, const char* name )
{
    debugOutput( DEBUG_LEVEL_NORMAL, "  %3d:        0x%02x %-40.40s\n", m_cnt, d, name );
    m_cnt += sizeof( byte_t );

    return true;
}

bool
CoutSerializer::write( uint16_t d, const char* name )
{
    debugOutput( DEBUG_LEVEL_NORMAL, "  %3d:    0x%04x %-40.40s\n", m_cnt, d, name );
    m_cnt += sizeof( uint16_t );

    return true;
}

bool
CoutSerializer::write( quadlet_t d, const char* name )
{
    debugOutput( DEBUG_LEVEL_NORMAL, "  %3d: 0x%08x %-40.40s\n", m_cnt, d, name );
    m_cnt += sizeof( quadlet_t );
    return true;
}

bool
CoutSerializer::write( const char * v, size_t len, const char* name )
{
    debugOutput( DEBUG_LEVEL_NORMAL, "  %3d: %s %-40.40s\n", m_cnt, v, name );
    m_cnt += len;
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
StringSerializer::write( uint16_t d, const char* name )
{
    char* result;
    asprintf( &result, "  %3d:\t0x%04x\t%s\n", m_cnt, d, name );

    m_string += result;
    free( result );

    m_cnt += sizeof( uint16_t );

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

bool
StringSerializer::write( const char * v, size_t len, const char* name )
{
    char* result;
    asprintf( &result, "  %3d:\t%s\t%s\n", m_cnt, v, name );

    m_string += result;
    free( result );

    m_cnt += len;
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
BufferSerialize::write( uint16_t value, const char* name )
{
    byte_t hi = (value & 0xFF00) >> 8;
    byte_t lo = value & 0xFF;

    bool result = false;
    if ( isCurPosValid() ) {
        *m_curPos = hi;
        m_curPos += sizeof( byte_t );
        if ( isCurPosValid() ) {
            *m_curPos = lo;
            m_curPos += sizeof( byte_t );
            result = true;
        }
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
BufferSerialize::write( const char * v, size_t len, const char* name )
{
    bool result = false;
    if ( isCurPosValid() ) {
        m_curPos += len;
        // avoid write beyond buffer
        if ( isCurPosValid() ) {
            m_curPos -= len;
            memcpy(m_curPos, v, len);
            m_curPos += len;
            result = true;
        }
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
BufferDeserialize::read( uint16_t* value )
{
    byte_t hi;
    byte_t lo;
    bool result = false;
    if ( isCurPosValid() ) {
        hi = *((byte_t *)m_curPos);
        m_curPos += sizeof( byte_t );
        if ( isCurPosValid() ) {
            lo = *((byte_t *)m_curPos);
            m_curPos += sizeof( byte_t );
            *value=(hi << 8) | lo;
            result = true;
        }
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

        m_curPos += length-1;
        if ( !isCurPosValid() ) {
            debugError("Read past end of response\n");
            result=false;
        } else {
            m_curPos++;
            result = true;
        }
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
BufferDeserialize::peek( uint16_t* value, size_t offset )
{
    byte_t hi;
    byte_t lo;
    bool result = false;
    m_curPos+=offset;
    if ( isCurPosValid() ) {
        hi = *((byte_t *)m_curPos);
        m_curPos += sizeof( byte_t );
        if ( isCurPosValid() ) {
            lo = *((byte_t *)m_curPos);
            *value=(hi << 8) | lo;
            result = true;
        }
        m_curPos -= sizeof( byte_t );
    }
    m_curPos-=offset;
    return result;
}

bool
BufferDeserialize::skip( size_t length ) {
    m_curPos+=length;
    return true;
}

bool
BufferDeserialize::isCurPosValid() const
{
    if ( static_cast<size_t>( ( m_curPos - m_buffer ) ) >= m_length ) {
        return false;
    }
    return true;
}

  }
}
