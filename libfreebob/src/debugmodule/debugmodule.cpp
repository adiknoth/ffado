/* debugmodule.cpp
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

#include "debugmodule.h"

#include <stdarg.h>
#include <netinet/in.h>

#include <iostream>

using namespace std;

struct ColorEntry  {
    const char* preSequence;
    const char* postSequence;
};

ColorEntry colorTable[] = {
    { "\033[31mFatal",   "\033[0m" },
    { "\033[31mError",   "\033[0m" },
    { "\033[31mWarning", "\033[0m" },
    { "Debug",           ""        },
};


DebugModule::DebugModule( std::string name,  debug_level_t level )
    : m_name( name )
    , m_level( level )
{
    if ( !DebugModuleManager::instance()->registerModule( *this ) ) {
        cerr << "Could not register DebugModule (" << name
             << ") at DebugModuleManager"
             << endl;
    }
}

DebugModule::~DebugModule()
{
    if ( !DebugModuleManager::instance()->unregisterModule( *this ) ) {
        cerr << "Could not unregister DebugModule at DebugModuleManager"
             << endl;
    }
}

void
DebugModule::printShort( debug_level_t level,
                         const char* format,
                         ... ) const
{
    if ( level > m_level ) {
        return;
    }

    va_list arg;

    va_start( arg, format );
    if ( printf( "%s: ", getPreSequence( level ) ) < 0 ) {
        cerr << "Could not create debug string with printf" << endl;
        return;
    }
    if ( vprintf( format, arg ) < 0 ) {
        cerr << "Could not create debug string with printf" << endl;
        return;
    }
    if ( printf( "%s", getPostSequence( level ) ) < 0 ) {
        cerr << "Could not create debug string with printf" << endl;
        return;
    }
    va_end( arg );
}

void
DebugModule::print( debug_level_t level,
                    const char*   file,
                    const char*   function,
                    unsigned int  line,
                    const char*   format,
                    ... ) const
{
    if ( level > m_level ) {
        return;
    }

    va_list arg;
    va_start( arg, format );

    if ( printf( "%s (%s)[%d] %s: ", getPreSequence( level ),
                 file,  line,  function ) < 0 ) {
        cerr << "Could not create debug string with printf" << endl;
        return;
    }
    if ( vprintf( format, arg ) < 0 ) {
        cerr << "Could not create debug string with printf" << endl;
        return;
    }
    if ( printf( "%s", getPostSequence( level ) ) < 0 ) {
        cerr << "Could not create debug string with printf" << endl;
        return;
    }
    va_end( arg );}

const char*
DebugModule::getPreSequence( debug_level_t level ) const
{
    if ( ( level <= eDL_Normal ) && ( level >= eDL_Fatal ) ) {
        return colorTable[level].preSequence;
    }
    return colorTable[eDL_Normal].preSequence;
}

const char*
DebugModule::getPostSequence( debug_level_t level ) const
{
    if ( ( level <= eDL_Normal ) && ( level >= eDL_Fatal ) ) {
        return colorTable[level].postSequence;
    }
    return colorTable[eDL_Normal].postSequence;
}

//--------------------------------------

DebugModuleManager* DebugModuleManager::m_instance = 0;

DebugModuleManager::DebugModuleManager()
{
}

DebugModuleManager::~DebugModuleManager()
{
}

DebugModuleManager*
DebugModuleManager::instance()
{
    if ( !m_instance ) {
        m_instance = new DebugModuleManager;
        if ( !m_instance ) {
            cerr << "DebugModuleManager::instance Failed to create "
                 << "DebugModuleManager" << endl;
        }
    }
    return m_instance;
}

bool
DebugModuleManager::registerModule( DebugModule& debugModule )
{
    m_debugModules.push_back( &debugModule );
    return true;
}

bool
DebugModuleManager::unregisterModule( DebugModule& debugModule )
{
    for ( DebugModuleVectorIterator it = m_debugModules.begin();
          it != m_debugModules.end();
          ++it )
    {
        if ( *it == &debugModule ) {
            m_debugModules.erase( it );
            return true;
        }
    }

    cerr << "DebugModuleManager::unregisterModule: Could not unregister "
         << "DebugModule (" << debugModule.getName() << ")" << endl;
    return false;
}

bool
DebugModuleManager::setMgrDebugLevel( std::string name, debug_level_t level )
{
    for ( DebugModuleVectorIterator it = m_debugModules.begin();
          it != m_debugModules.end();
          ++it )
    {
        if ( (*it)->getName() == name ) {
            return (*it)->setLevel( level );
        }
    }

    cerr << "setDebugLevel: Did not find DebugModule ("
         << name << ")" << endl;
    return false;
}

//----------------------------------------

unsigned char
toAscii( unsigned char c )
{
    if ( ( c > 31 ) && ( c < 126) ) {
        return c;
    } else {
        return '.';
    }
}

/* converts a quadlet to a uchar * buffer
 * not implemented optimally, but clear
 */
void
quadlet2char( quadlet_t quadlet, unsigned char* buff )
{
    *(buff)   = (quadlet>>24)&0xFF;
    *(buff+1) = (quadlet>>16)&0xFF;
    *(buff+2) = (quadlet>> 8)&0xFF;
    *(buff+3) = (quadlet)    &0xFF;
}

void
hexDump( unsigned char *data_start, unsigned int length )
{
    unsigned int i=0;
    unsigned int byte_pos;
    unsigned int bytes_left;

    if ( length <= 0 ) {
        return;
    }
    if ( length >= 7 ) {
        for ( i = 0; i < (length-7); i += 8 ) {
            printf( "%04X: %02X %02X %02X %02X %02X %02X %02X %02X "
                    "- [%c%c%c%c%c%c%c%c]\n",

                    i,

                    *(data_start+i+0),
                    *(data_start+i+1),
                    *(data_start+i+2),
                    *(data_start+i+3),
                    *(data_start+i+4),
                    *(data_start+i+5),
                    *(data_start+i+6),
                    *(data_start+i+7),

                    toAscii( *(data_start+i+0) ),
                    toAscii( *(data_start+i+1) ),
                    toAscii( *(data_start+i+2) ),
                    toAscii( *(data_start+i+3) ),
                    toAscii( *(data_start+i+4) ),
                    toAscii( *(data_start+i+5) ),
                    toAscii( *(data_start+i+6) ),
                    toAscii( *(data_start+i+7) )
                );
        }
    }
    byte_pos = i;
    bytes_left = length - byte_pos;

    printf( "%04X:" ,i );
    for ( i = byte_pos; i < length; i += 1 ) {
        printf( " %02X", *(data_start+i) );
    }
    for ( i=0; i < 8-bytes_left; i+=1 ) {
        printf( "   " );
    }

    printf( " - [" );
    for ( i = byte_pos; i < length; i += 1) {
        printf( "%c", toAscii(*(data_start+i)));
    }
    for ( i = 0; i < 8-bytes_left; i += 1) {
        printf( " " );
    }

    printf( "]" );
    printf( "\n" );
}

void
hexDumpQuadlets( quadlet_t *data, unsigned int length )
{
    unsigned int i=0;

    if ( length <= 0 ) {
        return;
    }
    for (i = 0; i< length; i += 1) {
        printf( "%02d %04X: %08X (%08X)"
                "\n", i, i*4, data[i],ntohl(data[i]));
    }
}


