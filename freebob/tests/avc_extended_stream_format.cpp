/* avc_extended_stream_format.cpp
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

#include "avc_extended_stream_format.h"
#include "serialize.h"

#include <netinet/in.h>

UnitPlugAddress::UnitPlugAddress( EPlugType plugType,  plug_type_t plugId )
    : m_plugType( plugType )
    , m_plugId( plugId )
    , m_reserved( 0xff )
{
}

bool
UnitPlugAddress::serialize( IOSSerialize& se )
{
    se.write( m_plugType, "UnitPlugAddress plugType" );
    se.write( m_plugId, "UnitPlugAddress plugId" );
    se.write( m_reserved, "UnitPlugAddress reserved" );
    return true;
}

bool
UnitPlugAddress::deserialize( IISDeserialize& de )
{
    de.read( &m_plugType );
    de.read( &m_plugId );
    de.read( &m_reserved );
    return true;
}

UnitPlugAddress*
UnitPlugAddress::clone() const
{
    return new UnitPlugAddress( *this );
}

////////////////////////////////////////////////////////////

SubunitPlugAddress::SubunitPlugAddress( plug_id_t plugId )
    : m_plugId( plugId )
    , m_reserved0( 0xff )
    , m_reserved1( 0xff )
{
}

bool
SubunitPlugAddress::serialize( IOSSerialize& se )
{
    se.write( m_plugId, "SubunitPlugAddress plugId" );
    se.write( m_reserved0, "SubunitPlugAddress reserved0" );
    se.write( m_reserved1, "SubunitPlugAddress reserved1" );
    return true;
}

bool
SubunitPlugAddress::deserialize( IISDeserialize& de )
{
    de.read( &m_plugId );
    de.read( &m_reserved0 );
    de.read( &m_reserved1 );
    return true;
}

SubunitPlugAddress*
SubunitPlugAddress::clone() const
{
    return new SubunitPlugAddress( *this );
}

////////////////////////////////////////////////////////////


FunctionBlockPlugAddress::FunctionBlockPlugAddress(  function_block_type_t functionBlockType,
                                                     function_block_id_t functionBlockId,
                                                     plug_id_t plugId )
    : m_functionBlockType( functionBlockType )
    , m_functionBlockId( functionBlockId )
    , m_plugId( plugId )
{
}

bool
FunctionBlockPlugAddress::serialize( IOSSerialize& se )
{
    se.write( m_functionBlockType, "FunctionBlockPlugAddress functionBlockType" );
    se.write( m_functionBlockId, "FunctionBlockPlugAddress functionBlockId" );
    se.write( m_plugId, "FunctionBlockPlugAddress plugId" );
    return true;
}

bool
FunctionBlockPlugAddress::deserialize( IISDeserialize& de )
{
    de.read( &m_functionBlockType );
    de.read( &m_functionBlockId );
    de.read( &m_plugId );
    return true;
}

FunctionBlockPlugAddress*
FunctionBlockPlugAddress:: clone() const
{
    return new FunctionBlockPlugAddress( *this );
}

////////////////////////////////////////////////////////////

PlugAddress::PlugAddress( plug_direction_t plugDirection,
                          plug_address_mode_t plugAddressMode,
                          UnitPlugAddress& unitPlugAddress )
    : m_plugDirection( plugDirection )
    , m_addressMode( plugAddressMode )
    , m_plugAddressData( new UnitPlugAddress( unitPlugAddress ) )
{
}

PlugAddress::PlugAddress( plug_direction_t plugDirection,
                          plug_address_mode_t plugAddressMode,
                          SubunitPlugAddress& subUnitPlugAddress )
    : m_plugDirection( plugDirection )
    , m_addressMode( plugAddressMode )
    , m_plugAddressData( new SubunitPlugAddress( subUnitPlugAddress ) )
{
}

PlugAddress::PlugAddress( plug_direction_t plugDirection,
                          plug_address_mode_t plugAddressMode,
                          FunctionBlockPlugAddress& functionBlockPlugAddress )
    : m_plugDirection( plugDirection )
    , m_addressMode( plugAddressMode )
    , m_plugAddressData( new FunctionBlockPlugAddress( functionBlockPlugAddress ) )
{
}

PlugAddress::PlugAddress( const PlugAddress& pa )
    : m_plugDirection( pa.m_plugDirection )
    , m_addressMode( pa.m_addressMode )
    , m_plugAddressData( dynamic_cast<PlugAddressData*>( pa.m_plugAddressData->clone() ) )
{
}

PlugAddress::~PlugAddress()
{
    delete m_plugAddressData;
    m_plugAddressData = 0;
}

bool
PlugAddress::serialize( IOSSerialize& se )
{
    se.write( m_plugDirection, "PlugAddress plugDirection" );
    se.write( m_addressMode, "PlugAddress addressMode" );
    return m_plugAddressData->serialize( se );
}

bool
PlugAddress::deserialize( IISDeserialize& de )
{
    de.read( &m_plugDirection );
    de.read( &m_addressMode );
    return m_plugAddressData->deserialize( de );
}

PlugAddress*
PlugAddress::clone() const
{
    return new PlugAddress( *this );
}

////////////////////////////////////////////////////////////

StreamFormatInfo::StreamFormatInfo()
    : IBusData()
{
}

bool
StreamFormatInfo::serialize( IOSSerialize& se )
{
    se.write( m_numberOfChannels, "StreamFormatInfo numberOfChannels" );
    se.write( m_streamFormat, "StreamFormatInfo streamFormat" );
    return true;
}

bool
StreamFormatInfo::deserialize( IISDeserialize& de )
{
    de.read( &m_numberOfChannels );
    de.read( &m_streamFormat );
    return true;
}

StreamFormatInfo*
StreamFormatInfo::clone() const
{
    return new StreamFormatInfo( *this );
}

////////////////////////////////////////////////////////////

FormatInformationStreamsSync::FormatInformationStreamsSync()
    : FormatInformationStreams()
    , m_reserved0( 0xff )
    , m_samplingFrequency( eSF_DontCare )
    , m_rateControl( eRC_DontCare )
    , m_reserved1( 0xff )
{
}

bool
FormatInformationStreamsSync::serialize( IOSSerialize& se )
{
    se.write( m_reserved0, "FormatInformationStreamsSync reserved" );

    // we have to clobber some bits
    byte_t operand = ( m_samplingFrequency << 4 ) | 0x0e;
    if ( m_rateControl == eRC_DontCare) {
        operand |= 0x1;
    }
    se.write( operand, "FormatInformationStreamsSync sampling frequency and rate control" );

    se.write( m_reserved1, "FormatInformationStreamsSync reserved" );
    return true;
}

bool
FormatInformationStreamsSync::deserialize( IISDeserialize& de )
{
    de.read( &m_reserved0 );

    byte_t operand;
    de.read( &operand );
    m_samplingFrequency = operand >> 4;
    m_rateControl = operand & 0x01;

    de.read( &m_reserved1 );
    return true;
}

FormatInformationStreamsSync*
FormatInformationStreamsSync::clone() const
{
    return new FormatInformationStreamsSync( *this );
}

////////////////////////////////////////////////////////////

FormatInformationStreamsCompound::FormatInformationStreamsCompound()
    : FormatInformationStreams()
    , m_samplingFrequency( eSF_DontCare )
    , m_rateControl( eRC_DontCare )
    , m_numberOfStreamFormatInfos( 0 )
{
}

FormatInformationStreamsCompound::~FormatInformationStreamsCompound()
{
    for ( StreamFormatInfoVector::iterator it = m_streamFormatInfos.begin();
          it != m_streamFormatInfos.end();
          ++it )
    {
        delete *it;
    }
}

bool
FormatInformationStreamsCompound::serialize( IOSSerialize& se )
{
    se.write( m_samplingFrequency, "FormatInformationStreamsCompound samplingFrequency" );
    se.write( m_rateControl, "FormatInformationStreamsCompound rateControl" );
    se.write( m_numberOfStreamFormatInfos, "FormatInformationStreamsCompound numberOfStreamFormatInfos" );
    for ( StreamFormatInfoVector::iterator it = m_streamFormatInfos.begin();
          it != m_streamFormatInfos.end();
          ++it )
    {
        ( *it )->serialize( se );
    }
    return true;
}

bool
FormatInformationStreamsCompound::deserialize( IISDeserialize& de )
{
    de.read( &m_samplingFrequency );
    de.read( &m_rateControl );
    de.read( &m_numberOfStreamFormatInfos );
    for ( int i = 0; i < m_numberOfStreamFormatInfos; ++i ) {
        StreamFormatInfo* streamFormatInfo = new StreamFormatInfo;
        if ( !streamFormatInfo->deserialize( de ) ) {
            return false;
        }
        m_streamFormatInfos.push_back( streamFormatInfo );
    }
    return true;
}

FormatInformationStreamsCompound*
FormatInformationStreamsCompound::clone() const
{
    return new FormatInformationStreamsCompound( *this );
}

////////////////////////////////////////////////////////////

FormatInformation::FormatInformation()
    : IBusData()
    , m_root( eFHR_Invalid )
    , m_level1( eFHL1_AUDIOMUSIC_DONT_CARE )
    , m_level2( eFHL2_AM824_DONT_CARE )
    , m_streams( 0 )
{
}

FormatInformation::~FormatInformation()
{
    delete m_streams;
    m_streams = 0;
}

bool
FormatInformation::serialize( IOSSerialize& se )
{
    if ( m_root != eFHR_Invalid ) {
        se.write( m_root, "FormatInformation hierarchy root" );
        if ( m_level1 != eFHL1_AUDIOMUSIC_DONT_CARE ) {
            se.write( m_level1, "FormatInformation hierarchy level 1" );
            if ( m_level2 != eFHL2_AM824_DONT_CARE ) {
                se.write( m_level2, "FormatInformation hierarchy level 2" );
            }
        }
    }
    if ( m_streams ) {
        return m_streams->serialize( se );
    }
    return true;
}

bool
FormatInformation::deserialize( IISDeserialize& de )
{
    bool result = false;

    delete m_streams;
    m_streams = 0;

    // this code just parses BeBoB replies.
    de.read( &m_root );

    // just parse an audio and music hierarchy
    if ( m_root == eFHR_AudioMusic ) {
        de.read( &m_level1 );

        switch ( m_level1 ) {
        case eFHL1_AUDIOMUSIC_AM824:
        {
            // note: compound streams don't have a second level
            de.read( &m_level2 );

            if (m_level2 == eFHL2_AM824_SYNC_STREAM ) {
                m_streams = new FormatInformationStreamsSync();
                result = m_streams->deserialize( de );
            } else {
                // anything but the sync stream workds currently.
                printf( "could not parse format information. (format hierarchy level 2 not recognized)\n" );
            }
        }
        break;
        case  eFHL1_AUDIOMUSIC_AM824_COMPOUND:
        {
            m_streams = new FormatInformationStreamsCompound();
            result = m_streams->deserialize( de );
        }
        break;
        default:
            printf( "could not parse format information. (format hierarchy level 1 not recognized)\n" );
        }
    }

    return result;
}

FormatInformation*
FormatInformation::clone() const
{
    return new FormatInformation( *this );
}

////////////////////////////////////////////////////////////

ExtendedStreamFormatCmd::ExtendedStreamFormatCmd( ESubFunction eSubFunction )
    : AVCCommand( AVC1394_STREAM_FORMAT_SUPPORT )
    , m_subFunction( eSubFunction )
    , m_status( eS_NotUsed )
    , m_indexInStreamFormat( 0 )
    , m_formatInformation( new FormatInformation )
{
    UnitPlugAddress unitPlugAddress( UnitPlugAddress::ePT_PCR, 0x00 );
    m_plugAddress = new PlugAddress(  PlugAddress::ePD_Output, PlugAddress::eM_Subunit, unitPlugAddress );
}

ExtendedStreamFormatCmd::~ExtendedStreamFormatCmd()
{
    delete m_plugAddress;
    m_plugAddress = 0;
    delete m_formatInformation;
    m_formatInformation = 0;
}

bool
ExtendedStreamFormatCmd::setPlugAddress( const PlugAddress& plugAddress )
{
    delete m_plugAddress;
    m_plugAddress = plugAddress.clone();
    return true;
}

bool
ExtendedStreamFormatCmd::setIndexInStreamFormat( const int index )
{
    m_indexInStreamFormat = index;
    return true;
}

bool
ExtendedStreamFormatCmd::serialize( IOSSerialize& se )
{
    AVCCommand::serialize( se );
    se.write( m_subFunction, "ExtendedStreamFormatCmd subFunction" );
    m_plugAddress->serialize( se );
    se.write( m_status, "ExtendedStreamFormatCmd status" );
    if ( m_subFunction == eSF_ExtendedStreamFormatInformationCommandList ) {
        se.write( m_indexInStreamFormat, "indexInStreamFormat" );
    }
    m_formatInformation->serialize( se );
    return true;
}

bool
ExtendedStreamFormatCmd::deserialize( IISDeserialize& de )
{
    AVCCommand::deserialize( de );
    de.read( &m_subFunction );
    m_plugAddress->deserialize( de );
    de.read( &m_status );
    if ( m_subFunction == eSF_ExtendedStreamFormatInformationCommandList ) {
        de.read( &m_indexInStreamFormat );
    }
    m_formatInformation->deserialize( de );
    return true;
}

bool
ExtendedStreamFormatCmd::fire( raw1394handle_t handle,
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
        printf(  "ExtendedStreamFormatCmd::fire: Could not serialize\n" );
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
            {
                BufferDeserialize de( resp->byte, sizeof( req ) );
                deserialize( de );
                result = true;
            }
            break;
            case eR_Rejected:
                if ( m_subFunction == eSF_ExtendedStreamFormatInformationCommandList ) {
                    if ( isVerbose() ) {
                        printf( "no futher stream formats defined\n" );
                    }
                    result = true;
                } else {
                    printf( "request rejected\n" );
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

status_t
ExtendedStreamFormatCmd::getStatus()
{
    return m_status;
}

FormatInformation*
ExtendedStreamFormatCmd::getFormatInformation()
{
    return m_formatInformation;
}

ExtendedStreamFormatCmd::index_in_stream_format_t
ExtendedStreamFormatCmd::getIndex()
{
    return m_indexInStreamFormat;
}

bool
ExtendedStreamFormatCmd::setSubFunction( ESubFunction subFunction )
{
    m_subFunction = subFunction;
    return true;
}
