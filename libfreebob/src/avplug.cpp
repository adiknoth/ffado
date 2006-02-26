/* avplug.cpp
 * Copyright (C) 2005,06 by Daniel Wagner
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

#include "avplug.h"

#include "libfreebobavc/ieee1394service.h"
#include "libfreebobavc/serialize.h"


IMPL_DEBUG_MODULE( AvPlug, AvPlug, DEBUG_LEVEL_NORMAL );

AvPlug::AvPlug( Ieee1394Service& ieee1394Service,
                int nodeId,
                AVCCommand::ESubunitType subunitType,
                subunit_id_t subunitId,
                EAvPlugType plugType,
                PlugAddress::EPlugDirection plugDirection,
                plug_id_t plugId )
    : m_1394Service( &ieee1394Service )
    , m_nodeId( nodeId )
    , m_subunitType( subunitType )
    , m_subunitId( subunitId )
    , m_type( plugType )
    , m_direction( plugDirection )
    , m_id( plugId )
    , m_infoPlugType( ExtendedPlugInfoPlugTypeSpecificData::eEPIPT_Unknown )
    , m_nrOfChannels( 0 )
{
}

AvPlug::AvPlug( const AvPlug& rhs )
    : m_1394Service( rhs.m_1394Service )
    , m_nodeId( rhs.m_nodeId )
    , m_subunitType( rhs.m_subunitType )
    , m_subunitId( rhs.m_subunitId )
    , m_type( rhs.m_type )
    , m_direction( rhs.m_direction )
    , m_id( rhs.m_id )
    , m_infoPlugType( rhs.m_infoPlugType )
    , m_nrOfChannels( rhs.m_nrOfChannels )
    , m_name( rhs.m_name )
    , m_clusterInfos( rhs.m_clusterInfos )
    , m_formatInfos( rhs.m_formatInfos )
{
}

AvPlug::~AvPlug()
{
}

bool
AvPlug::discover()
{
    if ( !discoverPlugType() ) {
        debugError( "discover: Could not discover plug type (%d,%d,%d,%d,%d)\n",
                    m_nodeId, m_subunitType, m_subunitId, m_direction, m_id );
        return false;
    }

    if ( !discoverName() ) {
        debugError( "discover: Could not discover name (%d,%d,%d,%d,%d)\n",
                    m_nodeId, m_subunitType, m_subunitId, m_direction, m_id );
        return false;
    }

    if ( !discoverNoOfChannels() ) {
        debugError( "discover: Could not discover number of channels "
                    "(%d,%d,%d,%d,%d)\n",
                    m_nodeId, m_subunitType, m_subunitId, m_direction, m_id );
        return false;
    }

    if ( !discoverChannelPosition() ) {
        debugError( "discover: Could not discover channel positions "
                    "(%d,%d,%d,%d,%d)\n",
                    m_nodeId, m_subunitType, m_subunitId, m_direction, m_id );
        return false;
    }

    if ( !discoverChannelName() ) {
        debugError( "discover: Could not discover channel name "
                    "(%d,%d,%d,%d,%d)\n",
                    m_nodeId, m_subunitType, m_subunitId, m_direction, m_id );
        return false;
    }

    if ( !discoverClusterInfo() ) {
        debugError( "discover: Could not discover channel name "
                    "(%d,%d,%d,%d,%d)\n",
                    m_nodeId, m_subunitType, m_subunitId, m_direction, m_id );
        return false;
    }

    if ( !discoverStreamFormat() ) {
        debugError( "discover: Could not discover stream format "
                    "(%d,%d,%d,%d,%d)\n",
                    m_nodeId, m_subunitType, m_subunitId, m_direction, m_id );
        return false;
    }

    if ( !discoverSupportedStreamFormats() ) {
        debugError( "discover: Could not discover supported stream formats "
                    "(%d,%d,%d,%d,%d)\n",
                    m_nodeId, m_subunitType, m_subunitId, m_direction, m_id );
        return false;
    }

    return true;
}

int
AvPlug::getNrOfStreams()
{
    int nrOfChannels = 0;
    for ( ClusterInfoVector::const_iterator it = m_clusterInfos.begin();
          it != m_clusterInfos.end();
          ++it )
    {
        const ClusterInfo* clusterInfo = &( *it );
        nrOfChannels += clusterInfo->m_nrOfChannels;
    }
    return nrOfChannels;
}

int
AvPlug::getNrOfChannels()
{
    return m_nrOfChannels;
}

int
AvPlug::getSampleRate()
{
    return convertESamplingFrequency( static_cast<ESamplingFrequency>( m_samplingFrequency ) );
}

bool
AvPlug::discoverPlugType()
{
    ExtendedPlugInfoCmd extPlugInfoCmd = setPlugAddrToPlugInfoCmd();
    ExtendedPlugInfoInfoType extendedPlugInfoInfoType(
        ExtendedPlugInfoInfoType::eIT_PlugType );
    extendedPlugInfoInfoType.initialize();
    extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );

    if ( !extPlugInfoCmd.fire() ) {
        debugError( "discoverPlugType: plug type command failed\n" );
        return false;
    }

    ExtendedPlugInfoInfoType* infoType = extPlugInfoCmd.getInfoType();
    if ( infoType
         && infoType->m_plugType )
    {
        plug_type_t plugType = infoType->m_plugType->m_plugType;

        debugOutput( DEBUG_LEVEL_VERBOSE,
                     "plug %d is of type %d (%s)\n",
                     m_id,
                     plugType,
                     extendedPlugInfoPlugTypeToString( plugType ) );
        m_infoPlugType = static_cast<ExtendedPlugInfoPlugTypeSpecificData::EExtendedPlugInfoPlugType>( plugType );
    }

   return true;
}

bool
AvPlug::discoverName()
{
    ExtendedPlugInfoCmd extPlugInfoCmd = setPlugAddrToPlugInfoCmd();
    ExtendedPlugInfoInfoType extendedPlugInfoInfoType(
        ExtendedPlugInfoInfoType::eIT_PlugName );
    extendedPlugInfoInfoType.initialize();
    extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );

    if ( !extPlugInfoCmd.fire() ) {
        debugError( "discoverName: name command failed\n" );
        return false;
    }

    ExtendedPlugInfoInfoType* infoType = extPlugInfoCmd.getInfoType();
    if ( infoType
         && infoType->m_plugName )
    {
        std::string name =
            infoType->m_plugName->m_name;

        debugOutput( DEBUG_LEVEL_VERBOSE,
                     "plug %d has name '%s'\n",
                     m_id,
                     name.c_str() );

        m_name = name;
    }
    return true;
}

bool
AvPlug::discoverNoOfChannels()
{
    ExtendedPlugInfoCmd extPlugInfoCmd = setPlugAddrToPlugInfoCmd();
    //extPlugInfoCmd.setVerbose( true );
    ExtendedPlugInfoInfoType extendedPlugInfoInfoType(
        ExtendedPlugInfoInfoType::eIT_NoOfChannels );
    extendedPlugInfoInfoType.initialize();
    extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );

    if ( !extPlugInfoCmd.fire() ) {
        debugError( "discoverNoOfChannels: number of channels command failed\n" );
        return false;
    }

    ExtendedPlugInfoInfoType* infoType = extPlugInfoCmd.getInfoType();
    if ( infoType
         && infoType->m_plugNrOfChns )
    {
        nr_of_channels_t nrOfChannels
            = infoType->m_plugNrOfChns->m_nrOfChannels;

        debugOutput( DEBUG_LEVEL_VERBOSE,
                     "plug %d has %d channels\n",
                     m_id,
                     nrOfChannels );

        m_nrOfChannels = nrOfChannels;
    }
    return true;
}

bool
AvPlug::discoverChannelPosition()
{
    ExtendedPlugInfoCmd extPlugInfoCmd = setPlugAddrToPlugInfoCmd();
    ExtendedPlugInfoInfoType extendedPlugInfoInfoType(
        ExtendedPlugInfoInfoType::eIT_ChannelPosition );
    extendedPlugInfoInfoType.initialize();
    extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );

    if ( !extPlugInfoCmd.fire() ) {
        debugError( "discoverNoOfChannels: channel position command failed\n" );
        return false;
    }

    ExtendedPlugInfoInfoType* infoType = extPlugInfoCmd.getInfoType();
    if ( infoType
         && infoType->m_plugChannelPosition )
    {
        if ( !copyClusterInfo( *( infoType->m_plugChannelPosition ) ) ) {
            debugError( "discoverChannelPosition: Could not copy channel position "
                        "information\n" );
            return false;
        }

        debugOutput( DEBUG_LEVEL_VERBOSE,
                     "plug %d: channel position information "
                     "retrieved\n",
                     m_id );

        debugOutputClusterInfos( DEBUG_LEVEL_VERBOSE );
    }

    return true;
}

bool
AvPlug::discoverChannelName()
{
    for ( ClusterInfoVector::iterator clit = m_clusterInfos.begin();
          clit != m_clusterInfos.end();
          ++clit )
    {
        ClusterInfo* clitInfo = &*clit;

        for ( ChannelInfoVector::iterator pit =  clitInfo->m_channelInfos.begin();
              pit != clitInfo->m_channelInfos.end();
              ++pit )
        {
            ChannelInfo* channelInfo = &*pit;

            ExtendedPlugInfoCmd extPlugInfoCmd = setPlugAddrToPlugInfoCmd();
            ExtendedPlugInfoInfoType extendedPlugInfoInfoType(
                ExtendedPlugInfoInfoType::eIT_ChannelName );
            extendedPlugInfoInfoType.initialize();
            extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );

            ExtendedPlugInfoInfoType* infoType =
                extPlugInfoCmd.getInfoType();
            if ( infoType ) {
                infoType->m_plugChannelName->m_streamPosition =
                    channelInfo->m_streamPosition;
            }
            if ( !extPlugInfoCmd.fire() ) {
                debugError( "discoverChannelName: channel name command failed\n" );
                return false;
            }
            infoType = extPlugInfoCmd.getInfoType();
            if ( infoType
                 && infoType->m_plugChannelName )
            {
                debugOutput( DEBUG_LEVEL_VERBOSE,
                             "plug %d stream "
                             "position %d: channel name = %s\n",
                             m_id,
                             channelInfo->m_streamPosition,
                             infoType->m_plugChannelName->m_plugChannelName.c_str() );
                channelInfo->m_name =
                    infoType->m_plugChannelName->m_plugChannelName;
            }

        }
    }

    return true;
}

bool
AvPlug::discoverClusterInfo()
{
    if ( m_infoPlugType == ExtendedPlugInfoPlugTypeSpecificData::eEPIPT_Sync )
    {
        // If the plug is of type sync it is either a normal 2 channel
        // stream (not compound stream) or it is a compound stream
        // with exactly one cluster. This depends on the
        // extended stream format command version which is used.
        // We are not interested in this plug so we skip it.
        debugOutput( DEBUG_LEVEL_VERBOSE,
                     "%s plug %d is of type sync -> skip\n",
                     m_name.c_str(),
                     m_id );
        return true;
    }

    for ( ClusterInfoVector::iterator clit = m_clusterInfos.begin();
          clit != m_clusterInfos.end();
          ++clit )
    {
        ClusterInfo* clusterInfo = &*clit;

        ExtendedPlugInfoCmd extPlugInfoCmd = setPlugAddrToPlugInfoCmd();
        ExtendedPlugInfoInfoType extendedPlugInfoInfoType(
            ExtendedPlugInfoInfoType::eIT_ClusterInfo );
        extendedPlugInfoInfoType.initialize();
        extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );

        extPlugInfoCmd.getInfoType()->m_plugClusterInfo->m_clusterIndex =
            clusterInfo->m_index;

        if ( !extPlugInfoCmd.fire() ) {
            debugError( "discoverClusterInfo: cluster info command failed\n" );
            return false;
        }

        ExtendedPlugInfoInfoType* infoType = extPlugInfoCmd.getInfoType();
        if ( infoType
             && infoType->m_plugClusterInfo )
        {
            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "%s plug %d: cluster index = %d, "
                         "portType %s, cluster name = %s\n",
                         m_name.c_str(),
                         m_id,
                         infoType->m_plugClusterInfo->m_clusterIndex,
                         extendedPlugInfoClusterInfoPortTypeToString(
                             infoType->m_plugClusterInfo->m_portType ),
                         infoType->m_plugClusterInfo->m_clusterName.c_str() );

            clusterInfo->m_portType = infoType->m_plugClusterInfo->m_portType;
            clusterInfo->m_name = infoType->m_plugClusterInfo->m_clusterName;
        }
    }

    return true;
}

bool
AvPlug::discoverStreamFormat()
{
    ExtendedStreamFormatCmd extStreamFormatCmd =
        setPlugAddrToStreamFormatCmd( ExtendedStreamFormatCmd::eSF_ExtendedStreamFormatInformationCommand );

    if ( !extStreamFormatCmd.fire() ) {
        debugError( "discoverStreamFormat: stream format command failed\n" );
        return false;
    }

    FormatInformation* formatInfo =
        extStreamFormatCmd.getFormatInformation();
    FormatInformationStreamsCompound* compoundStream
        = dynamic_cast< FormatInformationStreamsCompound* > (
            formatInfo->m_streams );
    if ( compoundStream ) {
        m_samplingFrequency =
            compoundStream->m_samplingFrequency;
        debugOutput( DEBUG_LEVEL_VERBOSE,
                     "discoverStreamFormat: %s plug %d uses "
                     "sampling frequency %d, nr of stream infos = %d\n",
                     m_name.c_str(),
                     m_id,
                     m_samplingFrequency,
                     compoundStream->m_numberOfStreamFormatInfos );

        for ( int i = 1;
              i <= compoundStream->m_numberOfStreamFormatInfos;
              ++i )
        {
            ClusterInfo* clusterInfo =
                getClusterInfoByIndex( i );

            if ( !clusterInfo ) {
                debugError( "discoverStreamFormat: No matching cluster "
                            "info found for index %d\n",  i );
                    return false;
            }
            StreamFormatInfo* streamFormatInfo =
                compoundStream->m_streamFormatInfos[ i - 1 ];

            debugOutput( DEBUG_LEVEL_VERBOSE, "discoverStreamFormat: "
                         "number of channels = %d, stream format = %d\n",
                         streamFormatInfo->m_numberOfChannels,
                         streamFormatInfo->m_streamFormat );

            int nrOfChannels = clusterInfo->m_nrOfChannels;
            if ( streamFormatInfo->m_streamFormat ==
                 FormatInformation::eFHL2_AM824_MIDI_CONFORMANT )
            {
                // 8 logical midi channels fit into 1 channel
                nrOfChannels = ( ( nrOfChannels + 7 ) / 8 );
            }
            // sanity checks
            if ( nrOfChannels != streamFormatInfo->m_numberOfChannels )
            {
                debugError( "discoverStreamFormat: Number of channels "
                            "mismatch: '%s' plug %d discovering reported "
                            "%d channels for cluster '%s', while stream "
                            "format reported %d\n",
                            m_name.c_str(),
                            m_id,
                            nrOfChannels,
                            clusterInfo->m_name.c_str(),
                            streamFormatInfo->m_numberOfChannels);
                return false;
            }
            clusterInfo->m_streamFormat = streamFormatInfo->m_streamFormat;

            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "%s plug %d cluster info %d ('%s'): "
                         "stream format %d\n",
                         m_name.c_str(),
                         m_id,
                         i,
                         clusterInfo->m_name.c_str(),
                         clusterInfo->m_streamFormat );
        }
    }

    FormatInformationStreamsSync* syncStream
        = dynamic_cast< FormatInformationStreamsSync* > (
            formatInfo->m_streams );
    if ( syncStream ) {
        m_samplingFrequency =
            syncStream->m_samplingFrequency;
        debugOutput( DEBUG_LEVEL_VERBOSE,
                     "%s plug %d is sync stream with sampling frequency %d\n",
                     m_name.c_str(),
                     m_id,
                     m_samplingFrequency );
    }

    if ( !compoundStream && !syncStream ) {
        debugError( "discoverStreamFormat: Unsupported stream format\n" );
        return false;
    }

    return true;
}

bool
AvPlug::discoverSupportedStreamFormats()
{
    ExtendedStreamFormatCmd extStreamFormatCmd =
        setPlugAddrToStreamFormatCmd(ExtendedStreamFormatCmd::eSF_ExtendedStreamFormatInformationCommandList);

    int i = 0;
    bool cmdSuccess = false;

    do {
        extStreamFormatCmd.setIndexInStreamFormat( i );
        extStreamFormatCmd.setCommandType( AVCCommand::eCT_Status );
        cmdSuccess = extStreamFormatCmd.fire();
        if ( cmdSuccess
             && ( extStreamFormatCmd.getResponse()
                  == AVCCommand::eR_Implemented ) )
        {
            FormatInfo formatInfo;
            formatInfo.m_index = i;

            FormatInformationStreamsSync* syncStream
                = dynamic_cast< FormatInformationStreamsSync* >
                ( extStreamFormatCmd.getFormatInformation()->m_streams );
            if ( syncStream ) {
                formatInfo.m_samplingFrequency =
                    syncStream->m_samplingFrequency;
                formatInfo.m_isSyncStream = true ;
            }

            FormatInformationStreamsCompound* compoundStream
                = dynamic_cast< FormatInformationStreamsCompound* >
                ( extStreamFormatCmd.getFormatInformation()->m_streams );
            if ( compoundStream ) {
                formatInfo.m_samplingFrequency =
                    compoundStream->m_samplingFrequency;
                formatInfo.m_isSyncStream = false;
                for ( int j = 0;
                      j < compoundStream->m_numberOfStreamFormatInfos;
                      ++j )
                {
                    switch ( compoundStream->m_streamFormatInfos[j]->m_streamFormat ) {
                    case AVC1394_STREAM_FORMAT_AM824_MULTI_BIT_LINEAR_AUDIO_RAW:
                        formatInfo.m_audioChannels +=
                            compoundStream->m_streamFormatInfos[j]->m_numberOfChannels;
                        break;
                    case AVC1394_STREAM_FORMAT_AM824_MIDI_CONFORMANT:
                        formatInfo.m_midiChannels +=
                            compoundStream->m_streamFormatInfos[j]->m_numberOfChannels;
                        break;
                    default:
                        debugWarning( "discoverSupportedStreamFormats: "
                                      "unknown stream format for channel "
                                      "(%d)\n", j );
                    }
                }
            }

            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "[%s:%d] formatInfo[%d].m_samplingFrequency = %d\n",
                         m_name.c_str(), m_id,
                         i, formatInfo.m_samplingFrequency );
            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "[%s:%d] formatInfo[%d].m_isSyncStream = %d\n",
                         m_name.c_str(), m_id,
                         i, formatInfo.m_isSyncStream );
            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "[%s:%d] formatInfo[%d].m_audioChannels = %d\n",
                         m_name.c_str(), m_id,
                         i, formatInfo.m_audioChannels );
            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "[%s:%d] formatInfo[%d].m_midiChannels = %d\n",
                         m_name.c_str(), m_id,
                         i, formatInfo.m_midiChannels );

            m_formatInfos.push_back( formatInfo );
        }

        ++i;
    } while ( cmdSuccess && ( extStreamFormatCmd.getResponse()
                              == ExtendedStreamFormatCmd::eR_Implemented ) );

    return true;
}


ExtendedPlugInfoCmd
AvPlug::setPlugAddrToPlugInfoCmd()
{
    ExtendedPlugInfoCmd extPlugInfoCmd( m_1394Service );

    switch( m_subunitType ) {
    case AVCCommand::eST_Unit:
        {
            UnitPlugAddress::EPlugType ePlugType = UnitPlugAddress::ePT_Unknown;
            switch ( m_type ) {
                case eAP_PCR:
                    ePlugType = UnitPlugAddress::ePT_PCR;
                    break;
                case eAP_ExternalPlug:
                    ePlugType = UnitPlugAddress::ePT_ExternalPlug;
                    break;
                case eAP_AsynchronousPlug:
                    ePlugType = UnitPlugAddress::ePT_AsynchronousPlug;
                    break;
                default:
                    ePlugType = UnitPlugAddress::ePT_Unknown;
            }
            UnitPlugAddress unitPlugAddress( ePlugType,
                                             m_id );
            extPlugInfoCmd.setPlugAddress( PlugAddress( m_direction,
                                                        PlugAddress::ePAM_Unit,
                                                        unitPlugAddress ) );
        }
        break;
    case AVCCommand::eST_Music:
    case AVCCommand::eST_Audio:
        {
            SubunitPlugAddress subunitPlugAddress( m_id );
            extPlugInfoCmd.setPlugAddress( PlugAddress( m_direction,
                                                        PlugAddress::ePAM_Subunit,
                                                        subunitPlugAddress ) );
        }
        break;
    default:
        debugError( "setPlugAddrToPlugInfoCmd: Unknown subunit type\n" );
    }

    extPlugInfoCmd.setNodeId( m_nodeId );
    extPlugInfoCmd.setCommandType( AVCCommand::eCT_Status );
    extPlugInfoCmd.setSubunitId( m_subunitId );
    extPlugInfoCmd.setSubunitType( m_subunitType );

    return extPlugInfoCmd;
}

ExtendedStreamFormatCmd
AvPlug::setPlugAddrToStreamFormatCmd(ExtendedStreamFormatCmd::ESubFunction subFunction)
{
    ExtendedStreamFormatCmd extStreamFormatInfoCmd( m_1394Service, subFunction );
    switch( m_subunitType ) {
    case AVCCommand::eST_Unit:
    {
            UnitPlugAddress::EPlugType ePlugType = UnitPlugAddress::ePT_Unknown;
            switch ( m_type ) {
                case eAP_PCR:
                    ePlugType = UnitPlugAddress::ePT_PCR;
                    break;
                case eAP_ExternalPlug:
                    ePlugType = UnitPlugAddress::ePT_ExternalPlug;
                    break;
                case eAP_AsynchronousPlug:
                    ePlugType = UnitPlugAddress::ePT_AsynchronousPlug;
                    break;
                default:
                    ePlugType = UnitPlugAddress::ePT_Unknown;
            }
        UnitPlugAddress unitPlugAddress( ePlugType,
                                         m_id );
        extStreamFormatInfoCmd.setPlugAddress( PlugAddress( m_direction,
                                                            PlugAddress::ePAM_Unit,
                                                            unitPlugAddress ) );
        }
        break;
    case AVCCommand::eST_Music:
    case AVCCommand::eST_Audio:
    {
        SubunitPlugAddress subunitPlugAddress( m_id );
        extStreamFormatInfoCmd.setPlugAddress( PlugAddress( m_direction,
                                                            PlugAddress::ePAM_Subunit,
                                                            subunitPlugAddress ) );
    }
    break;
    default:
        debugError( "discoverStreamFormat: Unknown subunit type\n" );
    }

    extStreamFormatInfoCmd.setNodeId( m_nodeId );
    extStreamFormatInfoCmd.setCommandType( AVCCommand::eCT_Status );
    extStreamFormatInfoCmd.setSubunitId( m_subunitId );
    extStreamFormatInfoCmd.setSubunitType( m_subunitType );

    return extStreamFormatInfoCmd;
}

bool
AvPlug::copyClusterInfo(ExtendedPlugInfoPlugChannelPositionSpecificData&
                        channelPositionData )
{
    int index = 1;
    for ( ExtendedPlugInfoPlugChannelPositionSpecificData::ClusterInfoVector::const_iterator it
              = channelPositionData.m_clusterInfos.begin();
          it != channelPositionData.m_clusterInfos.end();
          ++it )
    {
        const ExtendedPlugInfoPlugChannelPositionSpecificData::ClusterInfo*
            extPlugSpClusterInfo = &( *it );

        ClusterInfo clusterInfo;
        clusterInfo.m_nrOfChannels = extPlugSpClusterInfo->m_nrOfChannels;
        clusterInfo.m_index = index;
        index++;

        for (  ExtendedPlugInfoPlugChannelPositionSpecificData::ChannelInfoVector::const_iterator cit
                  = extPlugSpClusterInfo->m_channelInfos.begin();
              cit != extPlugSpClusterInfo->m_channelInfos.end();
              ++cit )
        {
            const ExtendedPlugInfoPlugChannelPositionSpecificData::ChannelInfo*
                extPlugSpChannelInfo = &( *cit );

            ChannelInfo channelInfo;
            channelInfo.m_streamPosition =
                extPlugSpChannelInfo->m_streamPosition;
            channelInfo.m_location =
                extPlugSpChannelInfo->m_location;

            clusterInfo.m_channelInfos.push_back( channelInfo );
        }
        m_clusterInfos.push_back( clusterInfo );
    }

    return true;
}

void
AvPlug::debugOutputClusterInfos( int debugLevel )
{
    for ( ClusterInfoVector::const_iterator it = m_clusterInfos.begin();
          it != m_clusterInfos.end();
          ++it )
    {
        const ClusterInfo* clusterInfo = &( *it );

        debugOutput( debugLevel, "number of channels: %d\n",
                     clusterInfo->m_nrOfChannels );

        for ( ChannelInfoVector::const_iterator cit
                  = clusterInfo->m_channelInfos.begin();
              cit != clusterInfo->m_channelInfos.end();
              ++cit )
        {
            const ChannelInfo* channelInfo = &( *cit );
            channelInfo = channelInfo;
            debugOutput( debugLevel,
                         "stream position: %d\n",
                         channelInfo->m_streamPosition );
            debugOutput( debugLevel,
                         "location: %d\n",
                         channelInfo->m_location );
        }
    }
}

AvPlug::ClusterInfo*
AvPlug::getClusterInfoByIndex(int index)
{
    for ( AvPlug::ClusterInfoVector::iterator clit =
              m_clusterInfos.begin();
          clit != m_clusterInfos.end();
          ++clit )
    {
        ClusterInfo* info = &*clit;
        if ( info->m_index == index ) {
            return info;
        }
    }
    return 0;
}

////////////////////////////////////

AvPlugCluster::AvPlugCluster()
{
}

AvPlugCluster::~AvPlugCluster()
{
}

////////////////////////////////////

AvPlugConnection::AvPlugConnection()
{
}

AvPlugConnection::~AvPlugConnection()
{
}
