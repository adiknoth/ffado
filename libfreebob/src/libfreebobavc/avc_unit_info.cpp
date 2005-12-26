/* avc_unit_info.cpp
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

#include "avc_unit_info.h"
#include "serialize.h"
#include "ieee1394service.h"

#include <netinet/in.h>
#include <iostream>

using namespace std;

UnitInfoCmd::UnitInfoCmd( Ieee1394Service* ieee1349service )
    : AVCCommand( ieee1349service, AVC1394_CMD_UNIT_INFO )
    , m_reserved( 0xff )
    , m_unit_type( 0xff )
    , m_unit( 0xff )
    , m_company_id( 0xffffffff )
{
}

UnitInfoCmd::~UnitInfoCmd()
{
}

bool
UnitInfoCmd::serialize( IOSSerialize& se )
{
    AVCCommand::serialize( se );

    se.write( m_reserved, "UnitInfoCmd reserved" );
    byte_t operand = ( m_unit_type << 3 ) | ( m_unit & 0x7 );
    se.write( operand, "UnitInfoCmd unit_type and unit" );
    operand = ( m_company_id >> 16 ) & 0xff;
    se.write( operand, "UnitInfoCmd company_ID (2)" );
    operand = ( m_company_id >> 8 ) & 0xff;
    se.write( operand, "UnitInfoCmd company_ID (1)" );
    operand = ( m_company_id >> 0 ) & 0xff;
    se.write( operand, "UnitInfoCmd company_ID (0)" );

    return true;
}

bool
UnitInfoCmd::deserialize( IISDeserialize& de )
{
    AVCCommand::deserialize( de );

    de.read( &m_reserved );

    byte_t operand;
    de.read( &operand );
    m_unit_type = ( operand >> 3 );
    m_unit = ( operand & 0x7 );

    de.read( &operand );
    m_company_id = 0;
    m_company_id |= operand << 16;
    de.read( &operand );
    m_company_id |= operand << 8;
    de.read( &operand );
    m_company_id |= operand;

    return true;
}

bool
UnitInfoCmd::fire()
{
    bool result = false;

    #define STREAM_FORMAT_REQUEST_SIZE 2
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
        printf(  "UnitInoCmd::fire: Could not serialize\n" );
        return false;
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
                deserialize( de );
                result = true;
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
