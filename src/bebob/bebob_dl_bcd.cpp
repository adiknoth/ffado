/* bebob_dl_bcd.cpp
 * Copyright (C) 2006 by Daniel Wagner
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

#include "bebob_dl_bcd.h"

#include <cstdio>

namespace BeBoB {
    enum {
        BCDMagic = 0x446f4362,
    };

    // XXX not very nice tool box function?
    std::string makeString( fb_octlet_t v )
    {
        std::string s;

        for ( unsigned int i=0; i<sizeof( v ); ++i ) {
            s += reinterpret_cast<char*> ( &v )[i];
        }

        return s;
    }

    std::string makeDate( fb_octlet_t v )
    {
        std::string s;
        char* vc = reinterpret_cast<char*> ( &v );

        s += vc[6];
        s += vc[7];
        s += '.';
        s += vc[4];
        s += vc[5];
        s += '.';
        s += vc[0];
        s += vc[1];
        s += vc[2];
        s += vc[3];

        return s;
    }

    std::string makeTime( fb_octlet_t v )
    {
        std::string s;
        char* vc = reinterpret_cast<char*>( &v );

        s += vc[0];
        s += vc[1];
        s += ':';
        s += vc[2];
        s += vc[3];
        s += ':';
        s += vc[4];
        s += vc[5];
        s += vc[6];
        s += vc[7];

        return s;
    }

    enum {
        BCDFileVersionOffset = 0x28,

        V0HeaderCRCOffset    = 0x2c,
        V0HeaderSize         = 0x60,

        V1HeaderCRC0ffset    = 0x2c,
        V1HeaderSize         = 0x70,
    };

    IMPL_DEBUG_MODULE( BCD, BCD, DEBUG_LEVEL_NORMAL );
};

BeBoB::BCD::BCD( std::string filename )
    : m_file( 0 )
    , m_filename( filename )
    , m_bcd_version( -1 )
    , m_softwareDate( 0 )
    , m_softwareTime( 0 )
    , m_softwareId( 0 )
    , m_softwareVersion( 0 )
    , m_hardwareId( 0 )
    , m_vendorOUI( 0 )
    , m_imageBaseAddress( 0 )
    , m_imageLength( 0 )
    , m_imageOffset( 0 )
    , m_imageCRC( 0 )
    , m_cneLength( 0 )
    , m_cneOffset( 0 )
    , m_cneCRC( 0 )
{
    initCRC32Table();
}

BeBoB::BCD::~BCD()
{
    if ( m_file ) {
        fclose( m_file );
    }
}

bool
BeBoB::BCD::parse()
{
    using namespace std;

    m_file = fopen( m_filename.c_str(), "r" );
    if ( !m_file ) {
        debugError( "parse: Could not open file '%s'\n",
                    m_filename.c_str() );
        return false;
    }

    fb_quadlet_t identifier;
    size_t bytes_read = fread( &identifier, 1, sizeof( identifier ), m_file );
    if ( bytes_read  != sizeof( identifier ) ) {
        debugError( "parse: 4 bytes read failed at position 0\n" );
        return false;
    }

    if ( identifier != BCDMagic ) {
        debugError( "parse: File has not BCD header magic, 0x%08x expected, "
                    "0x%08x found\n", BCDMagic, identifier );
        return false;
    }

    if ( fseek( m_file, BCDFileVersionOffset, SEEK_SET ) == -1 ) {
        debugError( "parse: fseek failed\n" );
        return false;
    }

    bytes_read = fread( &m_bcd_version, 1, sizeof( fb_quadlet_t ), m_file );
    if ( bytes_read != sizeof( fb_quadlet_t ) ) {
        debugError( "parse: %d bytes read at position %d failed\n",
                    sizeof( fb_quadlet_t ),
                    BCDFileVersionOffset );
        return false;
    }

    unsigned int headerSize = 0;
    unsigned int crcOffset = 0;
    switch( m_bcd_version ) {
    case 0:
        headerSize = V0HeaderSize;
        crcOffset = V0HeaderCRCOffset;
        break;
    case 1:
        headerSize = V1HeaderSize;
        crcOffset = V1HeaderCRC0ffset;
        break;
    default:
        debugError( "parse: Unknown BCD file version %d found\n",
                    m_bcd_version );
        return false;
    }

    if ( !checkHeaderCRC( crcOffset, headerSize ) ) {
        debugError( "parse: Header CRC check failed\n" );
        return false;
    }

    if ( !readHeaderInfo() ) {
        debugError( "parse: Could not read all header info\n" );
        return false;
    }

    return true;
}

bool
BeBoB::BCD::readHeaderInfo()
{
    if ( !read( 0x08, &m_softwareDate ) ) {
        return false;
    }
    if ( !read( 0x10, &m_softwareTime ) ) {
        return false;
    }
    if ( !read( 0x18, &m_softwareId ) ) {
        return false;
    }
    if ( !read( 0x1c, &m_softwareVersion ) ) {
        return false;
    }
    if ( !read( 0x20, &m_hardwareId ) ) {
        return false;
    }
    if ( !read( 0x24, &m_vendorOUI ) ) {
        return false;
    }
    if ( !read( 0x30, &m_imageOffset ) ) {
        return false;
    }
    if ( !read( 0x34, &m_imageBaseAddress ) ) {
        return false;
    }
    if ( !read( 0x38, &m_imageLength ) ) {
        return false;
    }
    if ( !read( 0x3c, &m_imageCRC ) ) {
        return false;
    }
    if ( !read( 0x50, &m_cneOffset ) ) {
        return false;
    }
    if ( !read( 0x58, &m_cneLength ) ) {
        return false;
    }
    if ( !read( 0x5c, &m_cneCRC ) ) {
        return false;
    }
    return true;
}

bool
BeBoB::BCD::read( int addr, fb_quadlet_t* q )
{
    if ( std::fseek( m_file, addr, SEEK_SET ) == -1 ) {
        debugError( "read: seek to position 0x%08x failed\n", addr );
        return false;
    }

    size_t bytes_read = std::fread( q, 1, sizeof( *q ), m_file );
    if ( bytes_read  != sizeof( *q ) ) {
        debugError( "read: %d byte read failed at position 0x%08x\n",
                    sizeof( *q ),  addr );
        return false;
    }

    return true;
}

bool
BeBoB::BCD::read( int addr, fb_octlet_t* o )
{
    if ( std::fseek( m_file, addr, SEEK_SET ) == -1 ) {
        debugError( "read: seek to position 0x%08x failed\n", addr );
        return false;
    }

    size_t bytes_read = std::fread( o, 1, sizeof( *o ), m_file );
    if ( bytes_read  != sizeof( *o ) ) {
        debugError( "read: %d byte read failed at position 0x%08x\n",
                    sizeof( *o ), addr );
        return false;
    }

    return true;
}

bool
BeBoB::BCD::read( int addr, unsigned char* b, size_t len )
{
    if ( std::fseek( m_file, addr, SEEK_SET ) == -1 ) {
        debugError( "read: seek to position 0x%08x failed\n", addr );
        return false;
    }

    size_t bytes_read = std::fread( b, 1, len, m_file );
    if ( bytes_read  != len ) {
        debugError( "read: %d byte read failed at position 0x%08x\n",
                    len, addr );
        return false;
    }

    return true;
}

void
BeBoB::BCD::initCRC32Table()
{
    unsigned long polynomial = 0x04c11db7;

    for ( int i = 0; i <= 0xff; ++i ) {
        crc32_table[i] = reflect( i, 8 ) << 24;
        for ( int j = 0; j < 8; ++j ) {
            crc32_table[i] =
                (crc32_table[i] << 1)
                ^ (crc32_table[i] & (1 << 31) ? polynomial : 0);
        }
        crc32_table[i] = reflect( crc32_table[i], 32 );
    }
}

unsigned long
BeBoB::BCD::reflect( unsigned long ref, char ch )
{
    unsigned long value = 0;

    for ( int i = 1; i < (ch + 1); ++i ) {
        if(ref & 1) {
            value |= 1 << (ch - i);
        }
        ref >>= 1;
    }
    return value;
}

unsigned int
BeBoB::BCD::getCRC( unsigned char* text, size_t len )
{
    unsigned long crc = 0xffffffff;
    unsigned char* buffer;

    buffer = text;
    while ( len-- ) {
        crc = (crc >> 8) ^ crc32_table[(crc & 0xff) ^ *buffer++];
    }

    return crc ^ 0xffffffff;
}

bool
BeBoB::BCD::checkHeaderCRC( unsigned int crcOffset, unsigned int headerSize )
{
    fb_quadlet_t headerCRC;
    if ( !read( crcOffset, &headerCRC ) ) {
        debugError( "checkHeaderCRC: Could not read header CRC\n" );
        return false;
    }

    const int headerLength = headerSize;
    unsigned char buf[headerLength];
    if ( !read( 0x00, buf, headerLength ) ) {
        debugError( "checkHeaderCRC: Could not read complete header from file\n" );
        return false;
    }
    buf[crcOffset+0] = 0x00;
    buf[crcOffset+1] = 0x00;
    buf[crcOffset+2] = 0x00;
    buf[crcOffset+3] = 0x00;

    fb_quadlet_t calcCRC = getCRC( buf, headerLength );
    if ( headerCRC != calcCRC ) {
        debugError( "checkHeaderCRC: CRC check failed, 0x%08x expected, "
                    "0x%08x calculated\n", headerCRC, calcCRC );
        return false;
    }

    return true;
}

void
BeBoB::BCD::displayInfo()
{
    using namespace std;

    printf( "BCD Info\n" );
    printf( "\tBCD File Version\t%d\n", m_bcd_version );
    printf( "\tSoftware Date:\t\t%s, %s\n",
            makeDate( m_softwareDate ).c_str(),
            makeTime( m_softwareTime ).c_str() );
    printf( "\tSoftware Version:\t0x%08x\n", m_softwareVersion );
    printf( "\tSoftware Id:\t\t0x%08x\n", m_softwareId );
    printf( "\tHardware ID:\t\t0x%08x\n", m_hardwareId );
    printf( "\tVendor OUI:\t\t0x%08x\n", m_vendorOUI );
    printf( "\tImage Offset:\t\t0x%08x\n", m_imageOffset );
    printf( "\tImage Base Address:\t0x%08x\n", m_imageBaseAddress );
    printf( "\tImage Length:\t\t0x%08x\n", m_imageLength );
    printf( "\tImage CRC:\t\t0x%08x\n", m_imageCRC );
    printf( "\tCNE Length:\t\t0x%08x\n", m_cneLength );
    printf( "\tCNE Offset:\t\t0x%08x\n", m_cneOffset );
    printf( "\tCNE CRC:\t\t0x%08x\n", m_cneCRC );
}
