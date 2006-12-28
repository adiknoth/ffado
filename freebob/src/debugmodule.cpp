/* debugmodule.cpp
 * Copyright (C) 2004,05 by Pieter Palmers, Daniel Wagner
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
#include <stdarg.h>
#include "debugmodule.h"
#include <netinet/in.h>

#undef setDebugLevel
#undef debugError
#undef debugPrint
#undef debugPrintShort

DebugAnsiColor m_debug;

////////////////////////////////////////////////////////////////
DebugBase::DebugBase()
  : m_level( 0 )
{}

DebugBase::~DebugBase()
{}

////////////////////////////////////////////////////////////////
DebugStandard::DebugStandard()
    : DebugBase()
{}

DebugStandard::~DebugStandard()
{}

void
DebugStandard::error( const char* file,
                           const char* function,
                           unsigned int line,
                           const char* format, ... ) const
{
    va_list arg;

    va_start( arg, format );
    fprintf( stderr, "ERROR: %s %s %d: ", file, function, line );
    vfprintf( stderr, format, arg );
    va_end( arg );
}

void
DebugStandard::print( int level,
                           const char* file,
                           const char* function,
                           unsigned int line,
                           const char* format, ... ) const
{
    if (level & m_level) {
        va_list arg;

        va_start( arg,  format );
        printf( "DEBUG %s %s %d : ", file, function, line );
        vprintf( format, arg );
        va_end( arg );
    }
}

void
DebugStandard::printShort( int level,
                                const char* format, ... ) const
{
    if (level & m_level) {
        va_list arg;

        va_start( arg,  format );
        vprintf ( format, arg );
        va_end( arg );
    }
}

////////////////////////////////////////////////////////////////
DebugAnsiColor::DebugAnsiColor()
    : DebugBase()
{}

DebugAnsiColor::~DebugAnsiColor()
{}

void
DebugAnsiColor::error( const char* file,
                       const char* function,
                       unsigned int line,
                       const char* format, ... ) const
{
    va_list arg;

    va_start( arg,  format );
    fprintf( stderr, "\033[37m\033[41mERROR: %s %s %d: ",
             file, function, line );
    vfprintf( stderr, format, arg );
    fprintf( stderr,  "\033[0m\033[49m" );
    va_end( arg );
}

void
DebugAnsiColor::print( int level,
                       const char* file,
                       const char* function,
                       unsigned int line,
                       const char* format, ... ) const
{
    if (level & m_level) {
        va_list arg;

        va_start( arg, format );
        printf( "\033[37mDEBUG %s %s %d: %s" ,
                file,  function,  line, getLevelColor( level ) );
        vprintf( format, arg );
        printf( "\033[0m" );
        va_end( arg );
    }
}

void
DebugAnsiColor::printShort( int level,
                            const char* format, ... ) const
{
    if (level & m_level) {
        va_list arg;

        va_start( arg, format );
        printf( "%s", getLevelColor( level ) );
        vprintf( format,  arg );
        printf( "\033[0m" );
        va_end( arg );
    }
}

const char*
DebugAnsiColor::getLevelColor( int level ) const
{
    switch(level) {
    case DEBUG_LEVEL_INFO:
        return "\033[31m";
        break;
    case DEBUG_LEVEL_DEVICE:
        return "\033[32m";
        break;
    case DEBUG_LEVEL_SUBUNIT:
        return "\033[33m";
        break;
    case DEBUG_LEVEL_DESCRIPTOR:
        return "\033[34m"; // blue
        break;
    case DEBUG_LEVEL_INFOBLOCK: // purple
        return "\033[35m";
        break;
    case DEBUG_LEVEL_TRANSFERS:
        return "\033[36m";
        break;
    default:
        return "\033[0m";
        break;

    }
    return "";
}

////////////////////////////////////////////////////////////////
DebugHtml::DebugHtml()
    : DebugBase()
{}

DebugHtml::~DebugHtml()
{}

void
DebugHtml::error( const char* file,
                  const char* function,
                  unsigned int line,
                  const char* format, ... ) const
{
    va_list arg;

    va_start( arg, format );
    fprintf( stdout, "<P style='background: red;'><FONT color='white'>"
             "ERROR: %s %s %d: ", file, function, line );
    vfprintf( stdout, format, arg );
    fprintf( stdout, "</FONT></P>" );
    va_end( arg );
}

void
DebugHtml::print( int level,
                  const char* file,
                  const char* function,
                  unsigned int line,
                  const char* format, ... ) const
{
    if (level & m_level) {
        va_list arg;

        va_start( arg, format );
        printf( "<FONT color='darkGray'>" "DEBUG %s %s %d: %s",
                file, function, line, getLevelColor( level ) );
        vprintf( format,  arg );
        printf( "</FONT></FONT>" );
        va_end( arg );
    }
}

void
DebugHtml::printShort( int level, const char* format, ... ) const
{
    if (level & m_level) {
        va_list( arg );

        va_start( arg, format );
        printf ( "%s", getLevelColor( level ) );
        vprintf( format, arg );
        printf( "</FONT>" );
        va_end( arg );
    }
}

const char*
DebugHtml::getLevelColor( int level ) const
{
    switch( level ) {
    case DEBUG_LEVEL_INFO:
        return "<FONT color='darkRed'>";
        break;
    case DEBUG_LEVEL_DEVICE:
        return "<FONT color='darkGreen'>";
        break;
    case DEBUG_LEVEL_SUBUNIT:
        return "<FONT color='darkYellow'>";
        break;
    case DEBUG_LEVEL_DESCRIPTOR:
        return "<FONT color='darkBlue'>"; // blue
        break;
    case DEBUG_LEVEL_INFOBLOCK: // purple
        return "<FONT color='darkMagenta'>";
        break;
    case DEBUG_LEVEL_TRANSFERS:
        return "<FONT color='darkCyan'>";
        break;
    default:
        return "<FONT color='black'>";
        break;
    }
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

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
    //printf("hexdump: %p %d\n",data_start,length);

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
    for (i=0;i<length;i+=1) {
            printf( "%02d %04X: %08X (%08X)"
                    "\n", i, i*4, data[i],ntohl(data[i]));
    }
}
