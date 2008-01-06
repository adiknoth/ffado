/*
 * Copyright (C) 2005-2007 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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

#include "avc_extended_plug_info.h"
#include "libutil/cmd_serialize.h"
#include "libieee1394/ieee1394service.h"

#include <netinet/in.h>
#include <iostream>

using namespace std;

namespace AVC {

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
ExtendedPlugInfoPlugTypeSpecificData::serialize( Util::Cmd::IOSSerialize& se )
{
    se.write( m_plugType, "ExtendedPlugInfoPlugTypeSpecificData plugType" );
    return true;
}


bool
ExtendedPlugInfoPlugTypeSpecificData::deserialize( Util::Cmd::IISDeserialize& de )
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
ExtendedPlugInfoPlugNameSpecificData::serialize( Util::Cmd::IOSSerialize& se )
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
ExtendedPlugInfoPlugNameSpecificData::deserialize( Util::Cmd::IISDeserialize& de )
{
    byte_t length;
    de.read( &length );
    m_name.clear();
    char* name;
    // note that the pointer returned by de.read is not valid outside this function
    // but since we assign it to m_name it's not a problem since the contents are copied
    de.read( &name, length );
    m_name = name;

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
ExtendedPlugInfoPlugNumberOfChannelsSpecificData::serialize( Util::Cmd::IOSSerialize& se )
{
    se.write( m_nrOfChannels,
              "ExtendedPlugInfoPlugNumberOfChannelsSpecificData: "
              "number of channels" );
    return true;
}

bool
ExtendedPlugInfoPlugNumberOfChannelsSpecificData::deserialize( Util::Cmd::IISDeserialize& de )
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
ExtendedPlugInfoPlugChannelPositionSpecificData::serialize( Util::Cmd::IOSSerialize& se )
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
ExtendedPlugInfoPlugChannelPositionSpecificData::deserialize( Util::Cmd::IISDeserialize& de )
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
ExtendedPlugInfoPlugChannelNameSpecificData::serialize( Util::Cmd::IOSSerialize& se )
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
ExtendedPlugInfoPlugChannelNameSpecificData::deserialize( Util::Cmd::IISDeserialize& de )
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
    m_plugAddress = 0;
}

bool
ExtendedPlugInfoPlugInputSpecificData::serialize( Util::Cmd::IOSSerialize& se )
{
    if ( m_plugAddress ) {
        return m_plugAddress->serialize( se );
    } else {
        return false;
    }
}

bool
ExtendedPlugInfoPlugInputSpecificData::deserialize( Util::Cmd::IISDeserialize& de )
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
ExtendedPlugInfoPlugOutputSpecificData::serialize( Util::Cmd::IOSSerialize& se )
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
ExtendedPlugInfoPlugOutputSpecificData::deserialize( Util::Cmd::IISDeserialize& de )
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
ExtendedPlugInfoClusterInfoSpecificData::serialize( Util::Cmd::IOSSerialize& se )
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
ExtendedPlugInfoClusterInfoSpecificData::deserialize( Util::Cmd::IISDeserialize& de )
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
    if ( portType > ( ( sizeof( extendedPlugInfoPortTypeStrings ) )
                      / ( sizeof( extendedPlugInfoPortTypeStrings[0] ) ) ) ) {
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

ExtendedPlugInfoInfoType::ExtendedPlugInfoInfoType( const ExtendedPlugInfoInfoType& rhs )
    : IBusData()
    , m_infoType( rhs.m_infoType )
    , m_plugType( 0 )
    , m_plugName( 0 )
    , m_plugNrOfChns( 0 )
    , m_plugChannelPosition( 0 )
    , m_plugChannelName( 0 )
    , m_plugInput( 0 )
    , m_plugOutput( 0 )
    , m_plugClusterInfo( 0 )
{
    switch( m_infoType ) {
    case eIT_PlugType:
        m_plugType =
            new ExtendedPlugInfoPlugTypeSpecificData( *rhs.m_plugType );
        break;
    case eIT_PlugName:
        m_plugName =
            new ExtendedPlugInfoPlugNameSpecificData( *rhs.m_plugName );
        break;
    case eIT_NoOfChannels:
        m_plugNrOfChns =
            new ExtendedPlugInfoPlugNumberOfChannelsSpecificData( *rhs.m_plugNrOfChns );
        break;
    case eIT_ChannelPosition:
        m_plugChannelPosition =
            new ExtendedPlugInfoPlugChannelPositionSpecificData( *rhs.m_plugChannelPosition );
        break;
    case eIT_ChannelName:
        m_plugChannelName =
            new ExtendedPlugInfoPlugChannelNameSpecificData( *rhs.m_plugChannelName );
        break;
    case eIT_PlugInput:
        m_plugInput =
            new ExtendedPlugInfoPlugInputSpecificData( *rhs.m_plugInput );
        break;
    case eIT_PlugOutput:
        m_plugOutput =
            new ExtendedPlugInfoPlugOutputSpecificData( *rhs.m_plugOutput );
        break;
    case eIT_ClusterInfo:
        m_plugClusterInfo =
            new ExtendedPlugInfoClusterInfoSpecificData( *rhs.m_plugClusterInfo );
        break;
    }
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
ExtendedPlugInfoInfoType::serialize( Util::Cmd::IOSSerialize& se )
{
    // XXX \todo improve Util::Cmd::IOSSerialize::write interface
    char* buf;
    asprintf( &buf, "ExtendedPlugInfoInfoType infoType (%s)",
              extendedPlugInfoInfoTypeToString( m_infoType ) );
    se.write( m_infoType, buf );

    free(buf);

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
ExtendedPlugInfoInfoType::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool status = false;

    de.read( &m_infoType );

    switch ( m_infoType ) {
    case eIT_PlugType:
        if ( !m_plugType ) {
            m_plugType = new ExtendedPlugInfoPlugTypeSpecificData;
        }
        status = m_plugType->deserialize( de );
        break;
    case eIT_PlugName:
        if ( !m_plugName ) {
            m_plugName = new ExtendedPlugInfoPlugNameSpecificData;
        }
        status = m_plugName->deserialize( de );
        break;
    case eIT_NoOfChannels:
        if ( !m_plugNrOfChns ) {
            m_plugNrOfChns =
                new ExtendedPlugInfoPlugNumberOfChannelsSpecificData;
        }
        status = m_plugNrOfChns->deserialize( de );
        break;
    case eIT_ChannelPosition:
        if ( !m_plugChannelPosition ) {
            m_plugChannelPosition =
                new ExtendedPlugInfoPlugChannelPositionSpecificData;
        }
        status = m_plugChannelPosition->deserialize( de );
        break;
    case eIT_ChannelName:
        if ( !m_plugChannelName ) {
            m_plugChannelName =
                new ExtendedPlugInfoPlugChannelNameSpecificData;
        }
        status = m_plugChannelName->deserialize( de );
        break;
    case eIT_PlugInput:
        if ( !m_plugInput ) {
            m_plugInput = new ExtendedPlugInfoPlugInputSpecificData;
        }
        status = m_plugInput->deserialize( de );
        break;
    case eIT_PlugOutput:
        if ( !m_plugOutput ) {
            m_plugOutput = new ExtendedPlugInfoPlugOutputSpecificData;
        }
        status = m_plugOutput->deserialize( de );
        break;
    case eIT_ClusterInfo:
        if ( !m_plugClusterInfo ) {
            m_plugClusterInfo = new ExtendedPlugInfoClusterInfoSpecificData;
        }
        status =m_plugClusterInfo->deserialize( de );
        break;
    default:
        return false;
    }

    return status;
}

ExtendedPlugInfoInfoType*
ExtendedPlugInfoInfoType::clone() const
{
   ExtendedPlugInfoInfoType* extPlugInfoInfoType
       = new ExtendedPlugInfoInfoType( *this );
   extPlugInfoInfoType->initialize();
   return extPlugInfoInfoType;
}

const char* extendedPlugInfoInfoTypeStrings[] =
{
    "PlugType",
    "PlugName",
    "NoOfChannels",
    "ChannelPosition",
    "ChannelName",
    "PlugInput",
    "PlugOutput",
    "ClusterInfo",
};

const char* extendedPlugInfoInfoTypeToString( info_type_t infoType )
{
    if ( infoType > ( ( sizeof( extendedPlugInfoInfoTypeStrings ) )
                      / ( sizeof( extendedPlugInfoInfoTypeStrings[0] ) ) ) )  {
        return "Unknown";
    } else {
        return extendedPlugInfoInfoTypeStrings[infoType];
    }
}


//////////////////////////////////////////////

ExtendedPlugInfoCmd::ExtendedPlugInfoCmd( Ieee1394Service& ieee1394service,
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

ExtendedPlugInfoCmd::ExtendedPlugInfoCmd( const ExtendedPlugInfoCmd& rhs )
    : AVCCommand( rhs )
{
    m_subFunction = rhs.m_subFunction;
    m_plugAddress = new PlugAddress( *rhs.m_plugAddress );
    m_infoType = new ExtendedPlugInfoInfoType( *rhs.m_infoType );
}

ExtendedPlugInfoCmd::~ExtendedPlugInfoCmd()
{
    delete m_plugAddress;
    m_plugAddress = 0;
    delete m_infoType;
    m_infoType = 0;
}

bool
ExtendedPlugInfoCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    bool status = false;
    AVCCommand::serialize( se );
    se.write( m_subFunction, "ExtendedPlugInfoCmd subFunction" );
    status = m_plugAddress->serialize( se );
    status &= m_infoType->serialize( se );

    return status;
}

bool
ExtendedPlugInfoCmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool status = false;
    AVCCommand::deserialize( de );
    de.read( &m_subFunction );
    status = m_plugAddress->deserialize( de );
    status &= m_infoType->deserialize( de );

    return status;
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

}
