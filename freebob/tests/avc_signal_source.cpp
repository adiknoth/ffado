/* avc_signal_source.cpp
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

#include "avc_signal_source.h"
#include "serialize.h"

#include <netinet/in.h>
#include <iostream>

using namespace std;

#define AVC1394_CMD_SIGNAL_SOURCE 0x1A

SignalUnitAddress::SignalUnitAddress()
    : m_plugId( ePI_Invalid )
{
}

bool
SignalUnitAddress::serialize( IOSSerialize& se )
{
    byte_t reserved = 0xff;
    se.write( reserved, "SignalUnitAddress" );
    se.write( m_plugId, "SignalUnitAddress plugId" );
    return true;
}

bool
SignalUnitAddress::deserialize( IISDeserialize& de )
{
    byte_t operand;
    de.read( &operand );
    de.read( &m_plugId );
    return true;
}

SignalUnitAddress*
SignalUnitAddress::clone() const
{
    return new SignalUnitAddress( *this );
}

////////////////////////////////////////

SignalSubunitAddress::SignalSubunitAddress()
    : m_subunitType( AVC1394_SUBUNIT_RESERVED )
    , m_subunitId( AVC1394_SUBUNIT_ID_RESERVED )
    , m_plugId( ePI_Invalid )
{
}

bool
SignalSubunitAddress::serialize( IOSSerialize& se )
{
    byte_t operand = ( m_subunitType << 3 ) | ( m_subunitId & 0x7 );
    se.write( operand,  "SignalSubunitAddress subunitType & subunitId" );
    se.write( m_plugId, "SignalSubunitAddress plugId" );
    return true;
}

bool
SignalSubunitAddress::deserialize( IISDeserialize& de )
{
    byte_t operand;
    de.read( &operand );
    m_subunitType = operand >> 3;
    m_subunitId = operand & 0x7;
    de.read( &m_plugId );
    return true;
}

SignalSubunitAddress*
SignalSubunitAddress::clone() const
{
    return new SignalSubunitAddress( *this );
}

////////////////////////////////////////


SignalSourceCmd::SignalSourceCmd()
    : AVCCommand( AVC1394_CMD_SIGNAL_SOURCE )
    , m_resultStatus( 0xff )
    , m_outputStatus( 0xff )
    , m_conv( 0xff )
    , m_signalStatus( 0xff )
    , m_signalSource( 0 )
    , m_signalDestination( 0 )
{
}

SignalSourceCmd::~SignalSourceCmd()
{
    delete m_signalSource;
    m_signalSource = 0;
    delete m_signalDestination;
    m_signalDestination = 0;
}

bool
SignalSourceCmd::serialize( IOSSerialize& se )
{
    AVCCommand::serialize( se );
    switch( getSubunitType() ) {
    case eST_Unit:
        {
            byte_t operand = m_resultStatus & 0xf;
            se.write( operand, "SignalSourceCmd resultStatus" );

            if ( m_signalSource ) {
                m_signalSource->serialize( se );
            } else {
                byte_t reserved = 0xff;
                se.write( reserved, "SignalSourceCmd" );
                se.write( reserved, "SignalSourceCmd" );
            }

            if ( m_signalDestination ) {
                m_signalDestination->serialize( se );
            } else {
                byte_t reserved = 0xff;
                se.write( reserved, "SignalSourceCmd" );
                se.write( reserved, "SignalSourceCmd" );
            }
        }
        break;
    case eST_Audio:
    case eST_Music:
        {
            byte_t operand = ( m_outputStatus << 5 ) | ( ( m_conv & 0x1 ) << 4 ) | ( m_signalStatus & 0x7 );
            se.write( operand, "SignalSourceCmd outputStatus & conv & signalStatus" );

            if ( m_signalSource ) {
                m_signalSource->serialize( se );
            } else {
                byte_t reserved = 0xff;
                se.write( reserved, "SignalSourceCmd" );
                se.write( reserved, "SignalSourceCmd" );
            }

            if ( m_signalDestination ) {
                m_signalDestination->serialize( se );
            } else {
                byte_t reserved = 0xff;
                se.write( reserved, "SignalSourceCmd" );
                se.write( reserved, "SignalSourceCmd" );
            }
        }
        break;
    default:
        cerr << "Can't handle subunit type " << getSubunitType() << endl;
        return false;
    }

    return true;
}

bool
SignalSourceCmd::deserialize( IISDeserialize& de )
{
    delete m_signalSource;
    m_signalSource = 0;
    delete m_signalDestination;
    m_signalDestination = 0;

    AVCCommand::deserialize( de );
    switch( getSubunitType() ) {
    case eST_Unit:
        {
            byte_t operand;

            de.read( &operand );
            m_resultStatus = operand & 0xf;

            de.peek( &operand );
            if ( operand == 0xff ) {
                m_signalSource = new SignalUnitAddress;
            } else {
                m_signalSource = new SignalSubunitAddress;
            }
            m_signalSource->deserialize( de );

            de.peek( &operand );
            if ( operand == 0xff ) {
                m_signalDestination = new SignalUnitAddress;
            } else {
                m_signalDestination = new SignalSubunitAddress;
            }
            m_signalDestination->deserialize( de );
        }
        break;
    case eST_Audio:
    case eST_Music:
        {
            byte_t operand;

            de.read( &operand );
            m_outputStatus = operand >> 5;
            m_conv = ( operand & 0x10 ) >> 4;
            m_signalStatus = operand & 0xf;

            de.peek( &operand );
            if ( operand == 0xff ) {
                m_signalSource = new SignalUnitAddress;
            } else {
                m_signalSource = new SignalSubunitAddress;
            }

            m_signalSource->deserialize( de );

            de.peek( &operand );
            if ( operand == 0xff ) {
                m_signalDestination = new SignalUnitAddress;
            } else {
                m_signalDestination = new SignalSubunitAddress;
            }
            m_signalDestination->deserialize( de );
        }
        break;
    default:
        cerr << "Can't handle subunit type " << getSubunitType() << endl;
        return false;
    }

    return true;
}

bool
SignalSourceCmd::fire( raw1394handle_t handle,
                       unsigned int node_id )
{
    bool result = false;

    #define STREAM_FORMAT_REQUEST_SIZE 10 // XXX random length
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
        printf(  "SignalSourceCmd::fire: Could not serialize\n" );
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

    resp = reinterpret_cast<packet_t*> ( avc1394_transaction_block( handle, node_id, req.quadlet, STREAM_FORMAT_REQUEST_SIZE,  1 ) );
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
        case eR_NotImplemented:
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
SignalSourceCmd::setSignalSource( SignalUnitAddress& signalAddress )
{
    if ( m_signalSource ) {
        delete m_signalSource;
    }
    m_signalSource = signalAddress.clone();
    return true;
}

bool
SignalSourceCmd::setSignalSource( SignalSubunitAddress& signalAddress )
{
    if ( m_signalSource ) {
        delete m_signalSource;
    }
    m_signalSource = signalAddress.clone();
    return true;
}

bool
SignalSourceCmd::setSignalDestination( SignalUnitAddress& signalAddress )
{
    if ( m_signalDestination ) {
        delete m_signalDestination;
    }
    m_signalDestination = signalAddress.clone();
    return true;
}

bool
SignalSourceCmd::setSignalDestination( SignalSubunitAddress& signalAddress )
{
    if ( m_signalDestination ) {
        delete m_signalDestination;
    }
    m_signalDestination = signalAddress.clone();
    return true;
}
