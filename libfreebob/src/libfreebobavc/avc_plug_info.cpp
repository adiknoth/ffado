/* avc_plug_info.cpp
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

#include "avc_plug_info.h"
#include "serialize.h"
#include "ieee1394service.h"

#include <netinet/in.h>
#include <iostream>

using namespace std;

PlugInfoCmd::PlugInfoCmd( Ieee1394Service* ieee1394service,
                          ESubFunction eSubFunction )
    : AVCCommand( ieee1394service, AVC1394_CMD_PLUG_INFO )
    , m_serialBusIsochronousInputPlugs( 0xff )
    , m_serialBusIsochronousOutputPlugs( 0xff )
    , m_externalInputPlugs( 0xff )
    , m_externalOutputPlugs( 0xff )
    , m_serialBusAsynchronousInputPlugs( 0xff )
    , m_serialBusAsynchronousOuputPlugs( 0xff )
    , m_destinationPlugs( 0xff )
    , m_sourcePlugs( 0xff )
    , m_subFunction( eSubFunction )
{
}

PlugInfoCmd::~PlugInfoCmd()
{
}

bool
PlugInfoCmd::serialize( IOSSerialize& se )
{
    byte_t reserved = 0xff;

    AVCCommand::serialize( se );
    se.write( m_subFunction, "PlugInfoCmd subFunction" );
    switch( getSubunitType() ) {
    case eST_Unit:
        switch( m_subFunction ) {
        case eSF_SerialBusIsochronousAndExternalPlug:
            se.write( m_serialBusIsochronousInputPlugs, "PlugInfoCmd serialBusIsochronousInputPlugs" );
            se.write( m_serialBusIsochronousOutputPlugs, "PlugInfoCmd serialBusIsochronousOutputPlugs" );
            se.write( m_externalInputPlugs, "PlugInfoCmd externalInputPlugs" );
            se.write( m_externalOutputPlugs, "PlugInfoCmd externalOutputPlugs" );
            break;
        case eSF_SerialBusAsynchonousPlug:
            se.write( m_serialBusAsynchronousInputPlugs, "PlugInfoCmd serialBusAsynchronousInputPlugs" );
            se.write( m_serialBusAsynchronousOuputPlugs, "PlugInfoCmd serialBusAsynchronousOuputPlugs" );
            se.write( reserved, "PlugInfoCmd" );
            se.write( reserved, "PlugInfoCmd" );
            break;
        default:
            cerr << "Could not serialize with subfucntion " << m_subFunction << endl;
            return false;
        }
        break;
    default:
        se.write( m_destinationPlugs, "PlugInfoCmd destinationPlugs" );
        se.write( m_sourcePlugs, "PlugInfoCmd sourcePlugs" );
        se.write( reserved, "PlugInfoCmd" );
        se.write( reserved, "PlugInfoCmd" );
    }
    return true;
}

bool
PlugInfoCmd::deserialize( IISDeserialize& de )
{
    byte_t reserved;

    AVCCommand::deserialize( de );
    de.read( &m_subFunction );
    switch ( getSubunitType() ) {
    case eST_Unit:
        switch ( m_subFunction ) {
        case eSF_SerialBusIsochronousAndExternalPlug:
            de.read( &m_serialBusIsochronousInputPlugs );
            de.read( &m_serialBusIsochronousOutputPlugs );
            de.read( &m_externalInputPlugs );
            de.read( &m_externalOutputPlugs );
            break;
        case eSF_SerialBusAsynchonousPlug:
            de.read( &m_serialBusAsynchronousInputPlugs );
            de.read( &m_serialBusAsynchronousOuputPlugs );
            de.read( &reserved );
            de.read( &reserved );
            break;
        default:
            cerr << "Could not deserialize with subfunction " << m_subFunction << endl;
            return false;
        }
        break;
    default:
        de.read( &m_destinationPlugs );
        de.read( &m_sourcePlugs );
        de.read( &reserved );
        de.read( &reserved );
    }
    return true;
}

bool
PlugInfoCmd::fire()
{
    bool result = false;

    #define STREAM_FORMAT_REQUEST_SIZE 5 // XXX random length
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
        printf(  "PlugInfoCmd::fire: Could not serialize\n" );
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

bool
PlugInfoCmd::setSubFunction( ESubFunction subFunction )
{
    m_subFunction = subFunction;
    return true;
}
