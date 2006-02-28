/* avc_extended_plug_info.cpp
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

#include "avc_extended_plug_info.h"
#include "serialize.h"
#include "ieee1394service.h"

#include <netinet/in.h>
#include <iostream>

using namespace std;

/////////////////////////////////////////
/////////////////////////////////////////

ExtendedPlugInfoPlugTypeSpecificData::ExtendedPlugInfoPlugTypeSpecificData( EExtendedPlugInfoPlugType ePlugType )
    : IBusData()
    , m_plugType( ePlugType )
{
}

ExtendedPlugInfoPlugTypeSpecificData::~ExtendedPlugInfoPlugTypeSpecificData()
{
}

bool
ExtendedPlugInfoPlugTypeSpecificData::serialize( IOSSerialize& se )
{
    se.write( m_plugType, "ExtendedPlugInfoPlugTypeSpecificData plugType" );
    return true;
}


bool
ExtendedPlugInfoPlugTypeSpecificData::deserialize( IISDeserialize& de )
{
    de.read( &m_plugType );
    return true;
}

ExtendedPlugInfoPlugTypeSpecificData* ExtendedPlugInfoPlugTypeSpecificData::clone() const
{
    return new ExtendedPlugInfoPlugTypeSpecificData( *this );
}

const char* extendedPlugInfoPlugTypeStrings[] =
{
    "IsoStream",
    "AsyncStream",
    "Midi",
    "Sync",
    "Analog",
    "Digital",
    "Unknown",
};

const char* extendedPlugInfoPlugTypeToString( plug_type_t plugType )
{
    if ( plugType > sizeof( extendedPlugInfoPlugTypeStrings ) ) {
        return "Unknown";
    } else {
        return extendedPlugInfoPlugTypeStrings[plugType];
    }
}

/////////////////////////////////////////
/////////////////////////////////////////

ExtendedPlugInfoPlugNameSpecificData::ExtendedPlugInfoPlugNameSpecificData()
    : IBusData()
{
}

ExtendedPlugInfoPlugNameSpecificData::~ExtendedPlugInfoPlugNameSpecificData()
{
}

bool
ExtendedPlugInfoPlugNameSpecificData::serialize( IOSSerialize& se )
{
    byte_t length = strlen( m_name.c_str() );
    se.write( length,
              "ExtendedPlugInfoPlugNameSpecificData: string length" );
    for ( unsigned int i = 0; i < length; ++i ) {
        se.write( static_cast<byte_t>( m_name[i] ),
                  "ExtendedPlugInfoPlugNameSpecificData: char" );
    }

    return true;
}

bool
ExtendedPlugInfoPlugNameSpecificData::deserialize( IISDeserialize& de )
{
    byte_t length;
    de.read( &length );
    m_name.clear();
    for ( int i = 0; i < length; ++length ) {
        byte_t c;
        de.read( &c );
        // \todo do correct encoding
        if ( c == '&' ) {
            c = '+';
        }
        m_name += c;
    }
    return true;
}

ExtendedPlugInfoPlugNameSpecificData::ExtendedPlugInfoPlugNameSpecificData*
ExtendedPlugInfoPlugNameSpecificData::clone() const
{
    return new ExtendedPlugInfoPlugNameSpecificData( *this );
}


/////////////////////////////////////////
/////////////////////////////////////////

ExtendedPlugInfoPlugNumberOfChannelsSpecificData::ExtendedPlugInfoPlugNumberOfChannelsSpecificData()
    : IBusData()
    , m_nrOfChannels( 0 )
{
}

ExtendedPlugInfoPlugNumberOfChannelsSpecificData::~ExtendedPlugInfoPlugNumberOfChannelsSpecificData()
{
}

bool
ExtendedPlugInfoPlugNumberOfChannelsSpecificData::serialize( IOSSerialize& se )
{
    se.write( m_nrOfChannels,
              "ExtendedPlugInfoPlugNumberOfChannelsSpecificData: "
              "number of channels" );
    return true;
}

bool
ExtendedPlugInfoPlugNumberOfChannelsSpecificData::deserialize( IISDeserialize& de )
{
    de.read( &m_nrOfChannels );
    return true;
}

ExtendedPlugInfoPlugNumberOfChannelsSpecificData::ExtendedPlugInfoPlugNumberOfChannelsSpecificData*
ExtendedPlugInfoPlugNumberOfChannelsSpecificData::clone() const
{
    return new ExtendedPlugInfoPlugNumberOfChannelsSpecificData( *this );
}

/////////////////////////////////////////
/////////////////////////////////////////

ExtendedPlugInfoPlugChannelPositionSpecificData::ExtendedPlugInfoPlugChannelPositionSpecificData()
    : IBusData()
    , m_nrOfClusters( 0 )
{
}

ExtendedPlugInfoPlugChannelPositionSpecificData::~ExtendedPlugInfoPlugChannelPositionSpecificData()
{
}

bool
ExtendedPlugInfoPlugChannelPositionSpecificData::serialize( IOSSerialize& se )
{
    se.write( m_nrOfClusters,
              "ExtendedPlugInfoPlugChannelPositionSpecificData: "
              "number of clusters" );

    for ( ClusterInfoVector::const_iterator it = m_clusterInfos.begin();
          it != m_clusterInfos.end();
          ++it )
    {
        const ClusterInfo* clusterInfo = &( *it );

        se.write( clusterInfo->m_nrOfChannels,
                  "ExtendedPlugInfoPlugChannelPositionSpecificData: "
                  "number of channels" );
        for ( ChannelInfoVector::const_iterator cit
                  = clusterInfo->m_channelInfos.begin();
              cit != clusterInfo->m_channelInfos.end();
              ++cit )
        {
            const ChannelInfo* channelInfo = &( *cit );
            se.write( channelInfo->m_streamPosition,
                      "ExtendedPlugInfoPlugChannelPositionSpecificData: "
                      "stream position" );
            se.write( channelInfo->m_location,
                      "ExtendedPlugInfoPlugChannelPositionSpecificData: "
                      "location" );
        }
    }

    return true;
}

bool
ExtendedPlugInfoPlugChannelPositionSpecificData::deserialize( IISDeserialize& de )
{
    m_clusterInfos.clear();

    de.read( &m_nrOfClusters );
    for ( int i = 0; i < m_nrOfClusters; ++i ) {
        ClusterInfo clusterInfo;
        de.read ( &clusterInfo.m_nrOfChannels );
        for ( int j = 0; j < clusterInfo.m_nrOfChannels; ++j ) {
            ChannelInfo channelInfo;
            de.read( &channelInfo.m_streamPosition );
            de.read( &channelInfo.m_location );
            clusterInfo.m_channelInfos.push_back( channelInfo );
        }
        m_clusterInfos.push_back( clusterInfo );
    }
    return true;
}

ExtendedPlugInfoPlugChannelPositionSpecificData::ExtendedPlugInfoPlugChannelPositionSpecificData*
ExtendedPlugInfoPlugChannelPositionSpecificData::clone() const
{
    return new ExtendedPlugInfoPlugChannelPositionSpecificData( *this );
}

/////////////////////////////////////////
/////////////////////////////////////////

ExtendedPlugInfoPlugChannelNameSpecificData::ExtendedPlugInfoPlugChannelNameSpecificData()
    : IBusData()
    , m_streamPosition( 0 )
    , m_stringLength( 0xff )
{
}

ExtendedPlugInfoPlugChannelNameSpecificData::~ExtendedPlugInfoPlugChannelNameSpecificData()
{
}

bool
ExtendedPlugInfoPlugChannelNameSpecificData::serialize( IOSSerialize& se )
{
    se.write( m_streamPosition,
              "ExtendedPlugInfoPlugChannelNameSpecificData: stream position" );
    se.write( m_stringLength,
              "ExtendedPlugInfoPlugChannelNameSpecificData: string length" );
    for ( unsigned int i = 0; i < m_plugChannelName.size(); ++i ) {
        se.write( static_cast<byte_t>( m_plugChannelName[i] ),
                  "ExtendedPlugInfoPlugChannelNameSpecificData: char" );
    }

    return true;
}

bool
ExtendedPlugInfoPlugChannelNameSpecificData::deserialize( IISDeserialize& de )
{
    de.read( &m_streamPosition );
    de.read( &m_stringLength );

    char* name = new char[m_stringLength+1];
    for ( int i = 0; i < m_stringLength; ++i ) {
        byte_t c;
        de.read( &c );
        // \todo do correct encoding
        if ( c == '&' ) {
            c = '+';
        }
        name[i] = c;
    }
    name[m_stringLength] = '\0';
    m_plugChannelName = name;
    delete[] name;

    return true;
}

ExtendedPlugInfoPlugChannelNameSpecificData::ExtendedPlugInfoPlugChannelNameSpecificData*
ExtendedPlugInfoPlugChannelNameSpecificData::clone() const
{
    return new ExtendedPlugInfoPlugChannelNameSpecificData( *this );
}

/////////////////////////////////////////
/////////////////////////////////////////
ExtendedPlugInfoPlugInputSpecificData::ExtendedPlugInfoPlugInputSpecificData()
    : IBusData()
{
    UnitPlugSpecificDataPlugAddress
        unitPlug( UnitPlugSpecificDataPlugAddress::ePT_PCR,  0x00 );
    m_plugAddress
        = new PlugAddressSpecificData( PlugAddressSpecificData::ePD_Output,
                                       PlugAddressSpecificData::ePAM_Unit,
                                       unitPlug );
}

ExtendedPlugInfoPlugInputSpecificData::ExtendedPlugInfoPlugInputSpecificData(const ExtendedPlugInfoPlugInputSpecificData& rhs )
{
    m_plugAddress = rhs.m_plugAddress->clone();
}


ExtendedPlugInfoPlugInputSpecificData::~ExtendedPlugInfoPlugInputSpecificData()
{
    delete m_plugAddress;
}

bool
ExtendedPlugInfoPlugInputSpecificData::serialize( IOSSerialize& se )
{
    if ( m_plugAddress ) {
        return m_plugAddress->serialize( se );
    } else {
        return false;
    }
}

bool
ExtendedPlugInfoPlugInputSpecificData::deserialize( IISDeserialize& de )
{
    return m_plugAddress->deserialize( de );
}

ExtendedPlugInfoPlugInputSpecificData::ExtendedPlugInfoPlugInputSpecificData*
ExtendedPlugInfoPlugInputSpecificData::clone() const
{
    return new ExtendedPlugInfoPlugInputSpecificData( *this );
}

/////////////////////////////////////////
/////////////////////////////////////////

ExtendedPlugInfoPlugOutputSpecificData::ExtendedPlugInfoPlugOutputSpecificData()
    : IBusData()
    , m_nrOfOutputPlugs( 0 )
{
}

ExtendedPlugInfoPlugOutputSpecificData::ExtendedPlugInfoPlugOutputSpecificData( const ExtendedPlugInfoPlugOutputSpecificData& rhs)
    : IBusData()
    , m_nrOfOutputPlugs( rhs.m_nrOfOutputPlugs )
{
    for ( PlugAddressVector::const_iterator it = rhs.m_outputPlugAddresses.begin();
          it != rhs.m_outputPlugAddresses.end();
          ++it )
    {
        m_outputPlugAddresses.push_back( ( *it )->clone() );
    }

}


ExtendedPlugInfoPlugOutputSpecificData::~ExtendedPlugInfoPlugOutputSpecificData()
{
    for ( PlugAddressVector::iterator it = m_outputPlugAddresses.begin();
          it != m_outputPlugAddresses.end();
          ++it )
    {
        delete *it;
    }
}

bool
ExtendedPlugInfoPlugOutputSpecificData::serialize( IOSSerialize& se )
{
    se.write( m_nrOfOutputPlugs, "ExtendedPlugInfoPlugOutputSpecificData: number of output plugs" );
    for ( PlugAddressVector::const_iterator it = m_outputPlugAddresses.begin();
          it != m_outputPlugAddresses.end();
          ++it )
    {
        ( *it )->serialize( se );
    }

    return true;
}

bool
ExtendedPlugInfoPlugOutputSpecificData::deserialize( IISDeserialize& de )
{
    de.read( &m_nrOfOutputPlugs );

    for ( int i = 0; i < m_nrOfOutputPlugs; ++i )
    {
        UnitPlugSpecificDataPlugAddress
            unitPlug( UnitPlugSpecificDataPlugAddress::ePT_PCR,  0x00 );
        PlugAddressSpecificData* plugAddress
            = new PlugAddressSpecificData( PlugAddressSpecificData::ePD_Output,
                                           PlugAddressSpecificData::ePAM_Unit,
                                           unitPlug );
        if ( !plugAddress->deserialize( de ) ) {
            return false;
        }

        m_outputPlugAddresses.push_back( plugAddress );
    }

    return true;
}

ExtendedPlugInfoPlugOutputSpecificData::ExtendedPlugInfoPlugOutputSpecificData*
ExtendedPlugInfoPlugOutputSpecificData::clone() const
{
    return new ExtendedPlugInfoPlugOutputSpecificData( *this );
}

/////////////////////////////////////////
/////////////////////////////////////////
ExtendedPlugInfoClusterInfoSpecificData::ExtendedPlugInfoClusterInfoSpecificData()
    : IBusData()
    , m_clusterIndex( 0 )
    , m_portType( ePT_NoType )
    , m_stringLength( 0xff )
{
}

ExtendedPlugInfoClusterInfoSpecificData::~ExtendedPlugInfoClusterInfoSpecificData()
{
}

bool
ExtendedPlugInfoClusterInfoSpecificData::serialize( IOSSerialize& se )
{
    se.write( m_clusterIndex,
              "ExtendedPlugInfoClusterInfoSpecificData: cluster index" );
    se.write( m_portType,
              "ExtendedPlugInfoClusterInfoSpecificData: port type" );
    se.write( m_stringLength,
              "ExtendedPlugInfoClusterInfoSpecificData: string length" );
    for ( unsigned int i = 0; i < m_clusterName.length(); ++i ) {
        se.write( static_cast<byte_t>( m_clusterName[i] ),
                  "ExtendedPlugInfoClusterInfoSpecificData: char" );
    }

    return true;
}

bool
ExtendedPlugInfoClusterInfoSpecificData::deserialize( IISDeserialize& de )
{

    de.read( &m_clusterIndex );
    de.read( &m_portType );
    de.read( &m_stringLength );

    char* name = new char[m_stringLength+1];
    for ( int i = 0; i < m_stringLength; ++i ) {
        byte_t c;
        de.read( &c );
        // \todo do correct encoding
        if ( c == '&' ) {
            c = '+';
        }
        name[i] = c;
    }
    name[m_stringLength] = '\0';
    m_clusterName = name;
    delete[] name;

    return true;
}

ExtendedPlugInfoClusterInfoSpecificData::ExtendedPlugInfoClusterInfoSpecificData*
ExtendedPlugInfoClusterInfoSpecificData::clone() const
{
    return new ExtendedPlugInfoClusterInfoSpecificData( *this );
}

const char* extendedPlugInfoPortTypeStrings[] =
{
    "Speaker",
    "Headphone",
    "Microphone",
    "Line",
    "SPDIF",
    "ADAT",
    "TDIF",
    "MADI",
    "Analog",
    "Digital",
    "MIDI",
};

const char* extendedPlugInfoClusterInfoPortTypeToString( port_type_t portType )
{
    if ( portType > sizeof( extendedPlugInfoPortTypeStrings ) ) {
        return "Unknown";
    } else {
        return extendedPlugInfoPortTypeStrings[portType];
    }
}

/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////

ExtendedPlugInfoInfoType::ExtendedPlugInfoInfoType(EInfoType eInfoType)
    : IBusData()
    , m_infoType( eInfoType )
    , m_plugType( 0 )
    , m_plugName( 0 )
    , m_plugNrOfChns( 0 )
    , m_plugChannelPosition( 0 )
    , m_plugChannelName( 0 )
    , m_plugInput( 0 )
    , m_plugOutput( 0 )
    , m_plugClusterInfo( 0 )
{
}

ExtendedPlugInfoInfoType::~ExtendedPlugInfoInfoType()
{
    delete( m_plugType );
    delete( m_plugName );
    delete( m_plugNrOfChns );
    delete( m_plugChannelPosition );
    delete( m_plugChannelName );
    delete( m_plugInput );
    delete( m_plugOutput );
    delete( m_plugClusterInfo );
 }

bool
ExtendedPlugInfoInfoType::initialize()
{
    switch ( m_infoType ) {
    case eIT_PlugType:
        m_plugType = new ExtendedPlugInfoPlugTypeSpecificData;
        break;
    case eIT_PlugName:
        m_plugName = new ExtendedPlugInfoPlugNameSpecificData;
        break;
    case eIT_NoOfChannels:
        m_plugNrOfChns = new ExtendedPlugInfoPlugNumberOfChannelsSpecificData;
        break;
    case eIT_ChannelPosition:
        m_plugChannelPosition = new ExtendedPlugInfoPlugChannelPositionSpecificData;
        break;
    case eIT_ChannelName:
        m_plugChannelName = new ExtendedPlugInfoPlugChannelNameSpecificData;
        break;
    case eIT_PlugInput:
        m_plugInput = new ExtendedPlugInfoPlugInputSpecificData;
        break;
    case eIT_PlugOutput:
        m_plugOutput = new ExtendedPlugInfoPlugOutputSpecificData;
        break;
    case eIT_ClusterInfo:
        m_plugClusterInfo = new ExtendedPlugInfoClusterInfoSpecificData;
        break;
    default:
        return false;
    }

    return true;
}

bool
ExtendedPlugInfoInfoType::serialize( IOSSerialize& se )
{
    se.write( m_infoType, "ExtendedPlugInfoInfoType infoType" );

    switch ( m_infoType ) {
    case eIT_PlugType:
        if ( m_plugType ) {
            m_plugType->serialize( se );
        }
        break;
    case eIT_PlugName:
        if ( m_plugName ) {
            m_plugName->serialize( se );
        }
        break;
    case eIT_NoOfChannels:
        if ( m_plugNrOfChns ) {
            m_plugNrOfChns->serialize( se );
        }
        break;
    case eIT_ChannelPosition:
        if ( m_plugChannelPosition ) {
            m_plugChannelPosition->serialize( se );
        }
        break;
    case eIT_ChannelName:
        if ( m_plugChannelName ) {
            m_plugChannelName->serialize( se );
        }
        break;
    case eIT_PlugInput:
        if ( m_plugInput ) {
            m_plugInput->serialize( se );
        }
        break;
    case eIT_PlugOutput:
        if ( m_plugOutput ) {
            m_plugOutput->serialize( se );
        }
        break;
    case eIT_ClusterInfo:
        if ( m_plugClusterInfo ) {
            m_plugClusterInfo->serialize( se );
        }
        break;
    default:
        return false;
    }

    return true;
}

bool
ExtendedPlugInfoInfoType::deserialize( IISDeserialize& de )
{
    de.read( &m_infoType );

    switch ( m_infoType ) {
    case eIT_PlugType:
        if ( !m_plugType ) {
            m_plugType = new ExtendedPlugInfoPlugTypeSpecificData;
        }
        m_plugType->deserialize( de );
        break;
    case eIT_PlugName:
        if ( !m_plugName ) {
            m_plugName = new ExtendedPlugInfoPlugNameSpecificData;
        }
        m_plugName->deserialize( de );
        break;
    case eIT_NoOfChannels:
        if ( !m_plugNrOfChns ) {
            m_plugNrOfChns =
                new ExtendedPlugInfoPlugNumberOfChannelsSpecificData;
        }
        m_plugNrOfChns->deserialize( de );
        break;
    case eIT_ChannelPosition:
        if ( !m_plugChannelPosition ) {
            m_plugChannelPosition =
                new ExtendedPlugInfoPlugChannelPositionSpecificData;
        }
        m_plugChannelPosition->deserialize( de );

        break;
    case eIT_ChannelName:
        if ( !m_plugChannelName ) {
            m_plugChannelName =
                new ExtendedPlugInfoPlugChannelNameSpecificData;
        }
        m_plugChannelName->deserialize( de );
        break;
    case eIT_PlugInput:
        if ( !m_plugInput ) {
            m_plugInput = new ExtendedPlugInfoPlugInputSpecificData;
        }
        m_plugInput->deserialize( de );
        break;
    case eIT_PlugOutput:
        if ( !m_plugOutput ) {
            m_plugOutput = new ExtendedPlugInfoPlugOutputSpecificData;
        }
        m_plugOutput->deserialize( de );
        break;
    case eIT_ClusterInfo:
        if ( !m_plugClusterInfo ) {
            m_plugClusterInfo = new ExtendedPlugInfoClusterInfoSpecificData;
        }
        m_plugClusterInfo->deserialize( de );
        break;
    default:
        return false;
    }

    return true;
}

ExtendedPlugInfoInfoType*
ExtendedPlugInfoInfoType::clone() const
{
   ExtendedPlugInfoInfoType* extPlugInfoInfoType
       = new ExtendedPlugInfoInfoType( *this );
   extPlugInfoInfoType->initialize();
   return extPlugInfoInfoType;
}

//////////////////////////////////////////////

ExtendedPlugInfoCmd::ExtendedPlugInfoCmd( Ieee1394Service* ieee1394service,
                                          ESubFunction eSubFunction )
    : AVCCommand( ieee1394service, AVC1394_CMD_PLUG_INFO )
{
    setSubFunction( eSubFunction );
    UnitPlugAddress unitPlugAddress( UnitPlugAddress::ePT_PCR, 0x00 );
    m_plugAddress = new PlugAddress( PlugAddress::ePD_Output,
                                      PlugAddress::ePAM_Unit,
                                      unitPlugAddress );
    m_infoType =
        new ExtendedPlugInfoInfoType( ExtendedPlugInfoInfoType::eIT_PlugType );
    m_infoType->initialize();
}

ExtendedPlugInfoCmd::~ExtendedPlugInfoCmd()
{
    delete m_plugAddress;
    m_plugAddress = 0;
    delete m_infoType;
    m_infoType = 0;
}

bool
ExtendedPlugInfoCmd::serialize( IOSSerialize& se )
{
    AVCCommand::serialize( se );
    se.write( m_subFunction, "ExtendedPlugInfoCmd subFunction" );
    m_plugAddress->serialize( se );
    m_infoType->serialize( se );

    return true;
}

bool
ExtendedPlugInfoCmd::deserialize( IISDeserialize& de )
{
    AVCCommand::deserialize( de );
    de.read( &m_subFunction );
    m_plugAddress->deserialize( de );
    m_infoType->deserialize( de );

    return true;
}

bool
ExtendedPlugInfoCmd::fire()
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
        printf(  "ExtendedPlugInfoCmd::fire: Could not serialize\n" );
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
ExtendedPlugInfoCmd::setPlugAddress( const PlugAddress& plugAddress )
{
    delete m_plugAddress;
    m_plugAddress = plugAddress.clone();
    return true;
}

bool
ExtendedPlugInfoCmd::setSubFunction( ESubFunction subFunction )
{
    m_subFunction = subFunction;
    return true;
}

bool
ExtendedPlugInfoCmd::setInfoType( const ExtendedPlugInfoInfoType& infoType )
{
    delete m_infoType;
    m_infoType = infoType.clone();
    return true;
}
