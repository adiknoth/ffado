/* avc_extended_subunit_info.cpp
 * Copyright (C) 2006 by Daniel Wagner
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

#include "avc_extended_subunit_info.h"
#include "serialize.h"
#include "ieee1394service.h"

#include <netinet/in.h>

#define NR_OF_PAGE_DATA 5
#define SIZE_OF_PAGE_ENTRY 5

ExtendedSubunitInfoPageData::ExtendedSubunitInfoPageData()
    : IBusData()
    , m_functionBlockType( 0xff )
    , m_functionBlockId( 0xff )
    , m_functionBlockSpecialPupose( eSP_NoSpecialPupose )
    , m_noOfInputPlugs( 0xff )
    , m_noOfOutputPlugs( 0xff )
{
}

ExtendedSubunitInfoPageData::~ExtendedSubunitInfoPageData()
{
}

bool
ExtendedSubunitInfoPageData::serialize( IOSSerialize& se )
{
    se.write( m_functionBlockType, "ExtendedSubunitInfoPageData: function block type" );
    se.write( m_functionBlockId, "ExtendedSubunitInfoPageData: function block id" );
    se.write( m_functionBlockSpecialPupose, "ExtendedSubunitInfoPageData: function block special purpose" );
    se.write( m_noOfInputPlugs, "ExtendedSubunitInfoPageData: number of input plugs" );
    se.write( m_noOfOutputPlugs, "ExtendedSubunitInfoPageData: number of output plugs" );

    return true;
}

bool
ExtendedSubunitInfoPageData::deserialize( IISDeserialize& de )
{
    de.read( &m_functionBlockType );
    de.read( &m_functionBlockId );
    de.read( &m_functionBlockSpecialPupose );
    de.read( &m_noOfInputPlugs );
    de.read( &m_noOfOutputPlugs );
    return true;
}

ExtendedSubunitInfoPageData*
ExtendedSubunitInfoPageData::clone() const
{
    return new ExtendedSubunitInfoPageData( *this );
}

//////////////////////////////////////////////

ExtendedSubunitInfoCmd::ExtendedSubunitInfoCmd( Ieee1394Service* ieee1394service )
    : AVCCommand( ieee1394service, AVC1394_CMD_SUBUNIT_INFO )
    , m_page( 0xff )
    , m_fbType( eFBT_AllFunctinBlockType )
{
}

ExtendedSubunitInfoCmd::ExtendedSubunitInfoCmd( const ExtendedSubunitInfoCmd& rhs )
    : AVCCommand( rhs )
    , m_page( rhs.m_page )
    , m_fbType( rhs.m_fbType )
{
    for ( ExtendedSubunitInfoPageDataVector::const_iterator it =
              rhs.m_infoPageDatas.begin();
          it != rhs.m_infoPageDatas.end();
          ++it )
    {
        m_infoPageDatas.push_back( ( *it )->clone() );
    }
}

ExtendedSubunitInfoCmd::~ExtendedSubunitInfoCmd()
{
    for ( ExtendedSubunitInfoPageDataVector::iterator it =
              m_infoPageDatas.begin();
          it != m_infoPageDatas.end();
          ++it )
    {
        delete *it;
    }
}

bool
ExtendedSubunitInfoCmd::serialize( IOSSerialize& se )
{
    bool status = false;
    status = AVCCommand::serialize( se );
    status &= se.write( m_page, "ExtendedSubunitInfoCmd: page" );
    status &= se.write( m_fbType, "ExtendedSubunitInfoCmd: function block type" );
    for ( ExtendedSubunitInfoPageDataVector::const_iterator it =
              m_infoPageDatas.begin();
          it != m_infoPageDatas.end();
          ++it )
    {
        status &= ( *it )->serialize( se );
    }

    int startIndex = m_infoPageDatas.size() * SIZE_OF_PAGE_ENTRY;
    int endIndex = SIZE_OF_PAGE_ENTRY * NR_OF_PAGE_DATA;
    for ( int i = startIndex; i < endIndex; ++i ) {
        byte_t dummy = 0xff;
        se.write( dummy, "ExtendedSubunitInfoCmd: space fill" );
    }
    return status;
}

bool
ExtendedSubunitInfoCmd::deserialize( IISDeserialize& de )
{
    bool status = false;
    status = AVCCommand::deserialize( de );
    status &= de.read( &m_page );
    status &= de.read( &m_fbType );
    for ( int i = 0; i < 5; ++i ) {
        byte_t next;
        de.peek( &next );
        if ( next != 0xff ) {
            ExtendedSubunitInfoPageData* infoPageData =
                new ExtendedSubunitInfoPageData();
            if ( !infoPageData->deserialize( de ) ) {
                return false;
            }
            m_infoPageDatas.push_back( infoPageData );
        } else {
            return status;
        }
    }

    return status;
}

bool
ExtendedSubunitInfoCmd::fire()
{
    bool result = false;

    #define STREAM_FORMAT_REQUEST_SIZE 20 // XXX random length
    union UPacket {
        quadlet_t     quadlet[STREAM_FORMAT_REQUEST_SIZE];
        unsigned char byte[STREAM_FORMAT_REQUEST_SIZE*4];
    };
    typedef union UPacket packet_t;

    packet_t  req;
    packet_t* resp;

    // initialize complete packet
    memset( &req,  0xff,  sizeof( req ) );

    BufferSerialize se( req.byte, sizeof( req ) );
    if ( !serialize( se ) ) {
        printf(  "ExtendedSubunitInfoCmd::fire: Could not serialize\n" );
        return false;
    }

    if ( isVerbose() ) {
        printf( "\n" );
        printf( " idx type                       value\n" );
        printf( "-------------------------------------\n" );
        printf( "  %02d                     ctype: 0x%02x\n", 0, req.byte[0] );
        printf( "  %02d subunit_type + subunit_id: 0x%02x\n", 1, req.byte[1] );
        printf( "  %02d                    opcode: 0x%02x\n", 2, req.byte[2] );

        for ( int i = 3; i < STREAM_FORMAT_REQUEST_SIZE * 4; ++i ) {
            printf( "  %02d                operand %2d: 0x%02x\n", i, i-3, req.byte[i] );
        }
    }

    // reorder the bytes to the correct layout
    for (int i = 0; i < STREAM_FORMAT_REQUEST_SIZE; ++i) {
        req.quadlet[i] = ntohl( req.quadlet[i] );
    }

    if ( isVerbose() ) {
        // debug output
        puts("request:");
        for (int i = 0; i < STREAM_FORMAT_REQUEST_SIZE; ++i) {
            printf("  %2d: 0x%08x\n", i, req.quadlet[i]);
        }
    }

    resp = reinterpret_cast<packet_t*>(
        m_1394Service->transactionBlock( m_nodeId,
                                         req.quadlet,
                                         STREAM_FORMAT_REQUEST_SIZE ) );
    if ( resp ) {
        if ( isVerbose() ) {
            // debug output
            puts("response:");
            for ( int i = 0; i < STREAM_FORMAT_REQUEST_SIZE; ++i ) {
                printf( "  %2d: 0x%08x\n", i, resp->quadlet[i] );
            }
        }

        // reorder the bytes to the correct layout
        for ( int i = 0; i < STREAM_FORMAT_REQUEST_SIZE; ++i ) {
            resp->quadlet[i] = htonl( resp->quadlet[i] );
        }

        if ( isVerbose() ) {
            // a more detailed debug output
            printf( "\n" );
            printf( " idx type                       value\n" );
            printf( "-------------------------------------\n" );
            printf( "  %02d                     ctype: 0x%02x\n", 0, resp->byte[0] );
            printf( "  %02d subunit_type + subunit_id: 0x%02x\n", 1, resp->byte[1] );
            printf( "  %02d                    opcode: 0x%02x\n", 2, resp->byte[2] );

            for ( int i = 3; i < STREAM_FORMAT_REQUEST_SIZE * 4; ++i ) {
                printf( "  %02d                operand %2d: 0x%02x\n", i, i-3, resp->byte[i] );
            }
        }

        // parse output
        parseResponse( resp->byte[0] );
        switch ( getResponse() )
        {
            case eR_Implemented:
            {
                BufferDeserialize de( resp->byte, sizeof( req ) );
                result = deserialize( de );
                break;
            }
            case eR_NotImplemented:
            {
                BufferDeserialize de( resp->byte, sizeof( req ) );
                result = deserialize( de );
            }
            break;
            default:
                printf( "unexpected response received (0x%x)\n", getResponse() );
        }
    } else {
	printf( "no response\n" );
    }

    return result;
}
