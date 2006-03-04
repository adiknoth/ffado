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
IMPL_DEBUG_MODULE( AvPlugManager, AvPlugManager, DEBUG_LEVEL_NORMAL );

AvPlug::AvPlug( Ieee1394Service& ieee1394Service,
                int nodeId,
                AvPlugManager& plugManager,
                AVCCommand::ESubunitType subunitType,
                subunit_id_t subunitId,
                EAvPlugAddressType plugAddressType,
                EAvPlugDirection plugDirection,
                plug_id_t plugId,
                bool verbose )
    : m_1394Service( &ieee1394Service )
    , m_nodeId( nodeId )
    , m_subunitType( subunitType )
    , m_subunitId( subunitId )
    , m_addressType( plugAddressType )
    , m_direction( plugDirection )
    , m_id( plugId )
    , m_infoPlugType( eAPT_Unknown )
    , m_nrOfChannels( 0 )
    , m_plugManager( &plugManager )
    , m_verbose( verbose )
{
    if ( m_verbose ) {
        setDebugLevel( DEBUG_LEVEL_VERBOSE );
    }
}

AvPlug::AvPlug( const AvPlug& rhs )
    : m_1394Service( rhs.m_1394Service )
    , m_nodeId( rhs.m_nodeId )
    , m_subunitType( rhs.m_subunitType )
    , m_subunitId( rhs.m_subunitId )
    , m_addressType( rhs.m_addressType )
    , m_direction( rhs.m_direction )
    , m_id( rhs.m_id )
    , m_infoPlugType( rhs.m_infoPlugType )
    , m_nrOfChannels( rhs.m_nrOfChannels )
    , m_name( rhs.m_name )
    , m_clusterInfos( rhs.m_clusterInfos )
    , m_formatInfos( rhs.m_formatInfos )
    , m_plugManager( rhs.m_plugManager )
    , m_verbose( rhs.m_verbose )
{
    if ( m_verbose ) {
        setDebugLevel( DEBUG_LEVEL_VERBOSE );
    }
}

AvPlug::~AvPlug()
{
    m_plugManager->remPlug( *this );
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
        debugError( "Could not discover name (%d,%d,%d,%d,%d)\n",
                    m_nodeId, m_subunitType, m_subunitId, m_direction, m_id );
        return false;
    }

    if ( !discoverNoOfChannels() ) {
        debugError( "Could not discover number of channels "
                    "(%d,%d,%d,%d,%d)\n",
                    m_nodeId, m_subunitType, m_subunitId, m_direction, m_id );
        return false;
    }

    if ( !discoverChannelPosition() ) {
        debugError( "Could not discover channel positions "
                    "(%d,%d,%d,%d,%d)\n",
                    m_nodeId, m_subunitType, m_subunitId, m_direction, m_id );
        return false;
    }

    if ( !discoverChannelName() ) {
        debugError( "Could not discover channel name "
                    "(%d,%d,%d,%d,%d)\n",
                    m_nodeId, m_subunitType, m_subunitId, m_direction, m_id );
        return false;
    }

    if ( !discoverClusterInfo() ) {
        debugError( "Could not discover channel name "
                    "(%d,%d,%d,%d,%d)\n",
                    m_nodeId, m_subunitType, m_subunitId, m_direction, m_id );
        return false;
    }

    if ( !discoverStreamFormat() ) {
        debugError( "Could not discover stream format "
                    "(%d,%d,%d,%d,%d)\n",
                    m_nodeId, m_subunitType, m_subunitId, m_direction, m_id );
        return false;
    }

    if ( !discoverSupportedStreamFormats() ) {
        debugError( "Could not discover supported stream formats "
                    "(%d,%d,%d,%d,%d)\n",
                    m_nodeId, m_subunitType, m_subunitId, m_direction, m_id );
        return false;
    }

    return m_plugManager->addPlug( *this );
}

bool
AvPlug::discoverConnections()
{
    return discoverConnectionsInput() && discoverConnectionsOutput();
}

bool
AvPlug::inquireConnnection( AvPlug& plug )
{
    SignalSourceCmd signalSourceCmd = setSrcPlugAddrToSignalCmd();
    setDestPlugAddrToSignalCmd( signalSourceCmd, plug );

    if ( !signalSourceCmd.fire() ) {
        debugError( "Could not inquire connection between '%s' and '%s'\n",
                    m_name.c_str(), plug.getName() );
        return false;
    }

    if ( signalSourceCmd.getResponse() == AVCCommand::eR_Implemented ) {
        debugOutput( DEBUG_LEVEL_VERBOSE,
                     "Connection possible between '%s' and '%s'\n",
                     m_name.c_str(),  plug.getName() );
        return true;
    }
    debugOutput( DEBUG_LEVEL_VERBOSE,
                 "Connection not possible between '%s' and '%s'\n",
                 m_name.c_str(),  plug.getName() );
    return false;
}

int
AvPlug::getNrOfStreams() const
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
AvPlug::getNrOfChannels() const
{
    return m_nrOfChannels;
}

int
AvPlug::getSampleRate() const
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
        debugError( "plug type command failed\n" );
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
        switch ( plugType ) {
        case ExtendedPlugInfoPlugTypeSpecificData::eEPIPT_IsoStream:
            m_infoPlugType = eAPT_IsoStream;
            break;
        case ExtendedPlugInfoPlugTypeSpecificData::eEPIPT_AsyncStream:
            m_infoPlugType = eAPT_AsyncStream;
            break;
        case ExtendedPlugInfoPlugTypeSpecificData::eEPIPT_Midi:
            m_infoPlugType = eAPT_Midi;
            break;
        case ExtendedPlugInfoPlugTypeSpecificData::eEPIPT_Sync:
            m_infoPlugType = eAPT_Sync;
            break;
        case ExtendedPlugInfoPlugTypeSpecificData::eEPIPT_Analog:
            m_infoPlugType = eAPT_Analog;
            break;
        case ExtendedPlugInfoPlugTypeSpecificData::eEPIPT_Digital:
            m_infoPlugType = eAPT_Digital;
            break;
        default:
            m_infoPlugType = eAPT_Unknown;

        }
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
        debugError( "name command failed\n" );
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
        debugError( "number of channels command failed\n" );
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
        debugError( "channel position command failed\n" );
        return false;
    }

    ExtendedPlugInfoInfoType* infoType = extPlugInfoCmd.getInfoType();
    if ( infoType
         && infoType->m_plugChannelPosition )
    {
        if ( !copyClusterInfo( *( infoType->m_plugChannelPosition ) ) ) {
            debugError( "Could not copy channel position "
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
                debugError( "channel name command failed\n" );
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
    if ( m_infoPlugType == eAPT_Sync )
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
            debugError( "cluster info command failed\n" );
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
        debugError( "stream format command failed\n" );
        return false;
    }

    if ( ( extStreamFormatCmd.getStatus() ==  ExtendedStreamFormatCmd::eS_NoStreamFormat )
         || ( extStreamFormatCmd.getStatus() ==  ExtendedStreamFormatCmd::eS_NotUsed ) )
    {
        debugOutput( DEBUG_LEVEL_VERBOSE,
                     "No stream format information available\n" );
        return true;
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
                     "%s plug %d uses "
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
                const_cast<ClusterInfo*>( getClusterInfoByIndex( i ) );

            if ( !clusterInfo ) {
                debugError( "No matching cluster "
                            "info found for index %d\n",  i );
                    return false;
            }
            StreamFormatInfo* streamFormatInfo =
                compoundStream->m_streamFormatInfos[ i - 1 ];

            debugOutput( DEBUG_LEVEL_VERBOSE,
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
            // sanity check
            if ( nrOfChannels != streamFormatInfo->m_numberOfChannels )
            {
                debugWarning( "Number of channels "
                              "mismatch: '%s' plug discovering reported "
                              "%d channels for cluster '%s', while stream "
                              "format reported %d\n",
                              m_name.c_str(),
                              nrOfChannels,
                              clusterInfo->m_name.c_str(),
                              streamFormatInfo->m_numberOfChannels);
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


    if ( !compoundStream && !syncStream )
    {
        debugError( "Unsupported stream format\n" );
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
                        debugWarning("unknown stream format for channel "
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


bool
AvPlug::discoverConnectionsInput()
{
    ExtendedPlugInfoCmd extPlugInfoCmd = setPlugAddrToPlugInfoCmd();
    ExtendedPlugInfoInfoType extendedPlugInfoInfoType(
        ExtendedPlugInfoInfoType::eIT_PlugInput );
    extendedPlugInfoInfoType.initialize();
    extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );

    if ( !extPlugInfoCmd.fire() ) {
        debugError( "plug type command failed\n" );
        return false;
    }

    ExtendedPlugInfoInfoType* infoType = extPlugInfoCmd.getInfoType();
    if ( infoType
         && infoType->m_plugInput )
    {
        PlugAddressSpecificData* plugAddress
            = infoType->m_plugInput->m_plugAddress;

        UnitPlugSpecificDataPlugAddress* pUnitPlugAddress =
            dynamic_cast<UnitPlugSpecificDataPlugAddress*>
            ( plugAddress->m_plugAddressData );

        if ( pUnitPlugAddress ) {
            EAvPlugAddressType addressType = eAPA_PCR;
            switch ( pUnitPlugAddress->m_plugType ) {
                case UnitPlugSpecificDataPlugAddress::ePT_PCR:
                    addressType = eAPA_PCR;
                    break;
                case UnitPlugSpecificDataPlugAddress::ePT_ExternalPlug:
                    addressType = eAPA_ExternalPlug;
                    break;
                case UnitPlugSpecificDataPlugAddress::ePT_AsynchronousPlug:
                    addressType = eAPA_AsynchronousPlug;
                    break;
            }

            AvPlug* outputPlug = m_plugManager->getPlug(
                AVCCommand::eST_Unit,
                0xff,
                addressType,
                AvPlug::eAPD_Output,
                pUnitPlugAddress->m_plugId );

            if ( outputPlug ) {
                debugOutput( DEBUG_LEVEL_NORMAL,
                             "'%s' has a connection from '%s'\n",
                             m_name.c_str(),
                             outputPlug->getName() );
                m_inputConnections.push_back( outputPlug );
            } else {
                debugError( "no corresponding output plug found\n" );
                return false;
            }
        }

        SubunitPlugSpecificDataPlugAddress* pSubunitPlugAddress =
            dynamic_cast<SubunitPlugSpecificDataPlugAddress*>
            ( plugAddress->m_plugAddressData );

        if ( pSubunitPlugAddress ) {
            AVCCommand::ESubunitType subunitType =
                static_cast<AVCCommand::ESubunitType>( pSubunitPlugAddress->m_subunitType );

            AvPlug* outputPlug = m_plugManager->getPlug(
                subunitType,
                pSubunitPlugAddress->m_subunitId,
                eAPA_SubunitPlug,
                AvPlug::eAPD_Output,
                pSubunitPlugAddress->m_plugId );
           if ( outputPlug ) {
                debugOutput( DEBUG_LEVEL_NORMAL,
                             "'%s' has a connection from '%s'\n",
                             m_name.c_str(),
                             outputPlug->getName() );
                m_inputConnections.push_back( outputPlug );
            } else {
                debugError( "no corresponding output plug found\n" );
                return false;
            }
        }
        FunctionBlockPlugSpecificDataPlugAddress*
            pFunctionBlockPlugAddress =
            dynamic_cast<FunctionBlockPlugSpecificDataPlugAddress*>
            ( plugAddress->m_plugAddressData );

        if (  pFunctionBlockPlugAddress  ) {
            debugWarning( "function block address is not handled\n" );
        }
    } else {
        debugError( "no valid info type for plug '%s'\n", m_name.c_str() );
        return false;
    }

    return true;
}

bool
AvPlug::discoverConnectionsOutput()
{
    ExtendedPlugInfoCmd extPlugInfoCmd = setPlugAddrToPlugInfoCmd();
    ExtendedPlugInfoInfoType extendedPlugInfoInfoType(
        ExtendedPlugInfoInfoType::eIT_PlugOutput );
    extendedPlugInfoInfoType.initialize();
    extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );

    if ( !extPlugInfoCmd.fire() ) {
        debugError( "plug type command failed\n" );
        return false;
    }

    ExtendedPlugInfoInfoType* infoType = extPlugInfoCmd.getInfoType();
    if ( infoType
         && infoType->m_plugOutput )
    {
        debugOutput( DEBUG_LEVEL_VERBOSE,
                     "number of output plugs is %d for plug '%s'\n",
                     infoType->m_plugOutput->m_nrOfOutputPlugs,
                     m_name.c_str() );

        if ( infoType->m_plugOutput->m_nrOfOutputPlugs
             != infoType->m_plugOutput->m_outputPlugAddresses.size() )
        {
            debugError( "number of output plugs (%d) disagree with "
                        "number of elements in plug address vector (%d)\n",
                        infoType->m_plugOutput->m_nrOfOutputPlugs,
                        infoType->m_plugOutput->m_outputPlugAddresses.size());
        }
        for ( unsigned int i = 0;
              i < infoType->m_plugOutput->m_outputPlugAddresses.size();
              ++i )
        {
            PlugAddressSpecificData* plugAddress
                = infoType->m_plugOutput->m_outputPlugAddresses[i];

            UnitPlugSpecificDataPlugAddress* pUnitPlugAddress =
                dynamic_cast<UnitPlugSpecificDataPlugAddress*>
                ( plugAddress->m_plugAddressData );

            if ( pUnitPlugAddress ) {
                EAvPlugAddressType addressType = eAPA_PCR;
                switch ( pUnitPlugAddress->m_plugType ) {
                case UnitPlugSpecificDataPlugAddress::ePT_PCR:
                    addressType = eAPA_PCR;
                    break;
                case UnitPlugSpecificDataPlugAddress::ePT_ExternalPlug:
                    addressType = eAPA_ExternalPlug;
                    break;
                case UnitPlugSpecificDataPlugAddress::ePT_AsynchronousPlug:
                    addressType = eAPA_AsynchronousPlug;
                    break;
                }

                AvPlug* inputPlug = m_plugManager->getPlug(
                    AVCCommand::eST_Unit,
                    0xff,
                    addressType,
                    AvPlug::eAPD_Input,
                    pUnitPlugAddress->m_plugId );

                if ( inputPlug ) {
                    debugOutput( DEBUG_LEVEL_NORMAL,
                                 "'%s' has a connection to '%s'\n",
                                 m_name.c_str(),
                                 inputPlug->getName() );
                    m_outputConnections.push_back( inputPlug );
                } else {
                    debugError( "no corresponding input plug found\n" );
                    return false;
                }
            }

            SubunitPlugSpecificDataPlugAddress* pSubunitPlugAddress =
                dynamic_cast<SubunitPlugSpecificDataPlugAddress*>
                ( plugAddress->m_plugAddressData );

            if ( pSubunitPlugAddress ) {
                AVCCommand::ESubunitType subunitType =
                    static_cast<AVCCommand::ESubunitType>( pSubunitPlugAddress->m_subunitType );

                AvPlug* inputPlug = m_plugManager->getPlug(
                    subunitType,
                    pSubunitPlugAddress->m_subunitId,
                    eAPA_SubunitPlug,
                    AvPlug::eAPD_Input,
                    pSubunitPlugAddress->m_plugId );
                if ( inputPlug ) {
                    debugOutput( DEBUG_LEVEL_NORMAL,
                                 "'%s' has a connection to '%s'\n",
                                 m_name.c_str(),
                                 inputPlug->getName() );
                    m_outputConnections.push_back( inputPlug );
                } else {
                    debugError( "no corresponding input plug found\n" );
                    return false;
                }
            }

            FunctionBlockPlugSpecificDataPlugAddress*
                pFunctionBlockPlugAddress =
                dynamic_cast<FunctionBlockPlugSpecificDataPlugAddress*>
                ( plugAddress->m_plugAddressData );

            if (  pFunctionBlockPlugAddress  ) {
                debugWarning( "function block address is not handled\n" );
            }
        }
    } else {
        debugError( "no valid info type for plug '%s'\n", m_name.c_str() );
        return false;
    }

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
            switch ( m_addressType ) {
                case eAPA_PCR:
                    ePlugType = UnitPlugAddress::ePT_PCR;
                    break;
                case eAPA_ExternalPlug:
                    ePlugType = UnitPlugAddress::ePT_ExternalPlug;
                    break;
                case eAPA_AsynchronousPlug:
                    ePlugType = UnitPlugAddress::ePT_AsynchronousPlug;
                    break;
                default:
                    ePlugType = UnitPlugAddress::ePT_Unknown;
            }
            UnitPlugAddress unitPlugAddress( ePlugType,
                                             m_id );
            extPlugInfoCmd.setPlugAddress( PlugAddress( convertPlugDirection(),
                                                        PlugAddress::ePAM_Unit,
                                                        unitPlugAddress ) );
        }
        break;
    case AVCCommand::eST_Music:
    case AVCCommand::eST_Audio:
        {
            SubunitPlugAddress subunitPlugAddress( m_id );
            extPlugInfoCmd.setPlugAddress( PlugAddress( convertPlugDirection(),
                                                        PlugAddress::ePAM_Subunit,
                                                        subunitPlugAddress ) );
        }
        break;
    default:
        debugError( "Unknown subunit type\n" );
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
            switch ( m_addressType ) {
                case eAPA_PCR:
                    ePlugType = UnitPlugAddress::ePT_PCR;
                    break;
                case eAPA_ExternalPlug:
                    ePlugType = UnitPlugAddress::ePT_ExternalPlug;
                    break;
                case eAPA_AsynchronousPlug:
                    ePlugType = UnitPlugAddress::ePT_AsynchronousPlug;
                    break;
                default:
                    ePlugType = UnitPlugAddress::ePT_Unknown;
            }
        UnitPlugAddress unitPlugAddress( ePlugType,
                                         m_id );
        extStreamFormatInfoCmd.setPlugAddress( PlugAddress( convertPlugDirection(),
                                                            PlugAddress::ePAM_Unit,
                                                            unitPlugAddress ) );
        }
        break;
    case AVCCommand::eST_Music:
    case AVCCommand::eST_Audio:
    {
        SubunitPlugAddress subunitPlugAddress( m_id );
        extStreamFormatInfoCmd.setPlugAddress( PlugAddress( convertPlugDirection(),
                                                            PlugAddress::ePAM_Subunit,
                                                            subunitPlugAddress ) );
    }
    break;
    default:
        debugError( "Unknown subunit type\n" );
    }

    extStreamFormatInfoCmd.setNodeId( m_nodeId );
    extStreamFormatInfoCmd.setCommandType( AVCCommand::eCT_Status );
    extStreamFormatInfoCmd.setSubunitId( m_subunitId );
    extStreamFormatInfoCmd.setSubunitType( m_subunitType );

    return extStreamFormatInfoCmd;
}

SignalSourceCmd
AvPlug::setSrcPlugAddrToSignalCmd()
{
    SignalSourceCmd signalSourceCmd( m_1394Service );

    switch( m_subunitType ) {
    case AVCCommand::eST_Unit:
    {
        SignalUnitAddress signalUnitAddr;
        if ( getPlugAddressType() == eAPA_ExternalPlug ) {
            signalUnitAddr.m_plugId = m_id + 0x80;
        } else {
            signalUnitAddr.m_plugId = m_id;
        }
        signalSourceCmd.setSignalSource( signalUnitAddr );
    }
    break;
    case AVCCommand::eST_Music:
    case AVCCommand::eST_Audio:
    {
        SignalSubunitAddress signalSubunitAddr;
        signalSubunitAddr.m_subunitType = m_subunitType;
        signalSubunitAddr.m_subunitId = m_subunitId;
        signalSubunitAddr.m_plugId = m_id;
        signalSourceCmd.setSignalSource( signalSubunitAddr );
    }
    break;
    default:
        debugError( "Unknown subunit type\n" );
    }

    signalSourceCmd.setNodeId( m_nodeId );
    signalSourceCmd.setCommandType( AVCCommand::eCT_SpecificInquiry );
    signalSourceCmd.setSubunitType( m_subunitType );
    signalSourceCmd.setSubunitId( m_subunitId );

    return signalSourceCmd;
}

void
AvPlug::setDestPlugAddrToSignalCmd(SignalSourceCmd& signalSourceCmd, AvPlug& plug)
{
    switch( plug.m_subunitType ) {
    case AVCCommand::eST_Unit:
    {
        SignalUnitAddress signalUnitAddr;
        if ( plug.getPlugAddressType() == eAPA_ExternalPlug ) {
            signalUnitAddr.m_plugId = plug.m_id + 0x80;
        } else {
            signalUnitAddr.m_plugId = plug.m_id;
        }
        signalSourceCmd.setSignalDestination( signalUnitAddr );
    }
    break;
    case AVCCommand::eST_Music:
    case AVCCommand::eST_Audio:
    {
        SignalSubunitAddress signalSubunitAddr;
        signalSubunitAddr.m_subunitType = plug.m_subunitType;
        signalSubunitAddr.m_subunitId = plug.m_subunitId;
        signalSubunitAddr.m_plugId = plug.m_id;
        signalSourceCmd.setSignalDestination( signalSubunitAddr );
    }
    break;
    default:
        debugError( "Unknown subunit type\n" );
    }
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

const AvPlug::ClusterInfo*
AvPlug::getClusterInfoByIndex(int index) const
{
    for ( AvPlug::ClusterInfoVector::const_iterator clit =
              m_clusterInfos.begin();
          clit != m_clusterInfos.end();
          ++clit )
    {
        const ClusterInfo* info = &*clit;
        if ( info->m_index == index ) {
            return info;
        }
    }
    return 0;
}

PlugAddress::EPlugDirection
AvPlug::convertPlugDirection() const
{
    PlugAddress::EPlugDirection dir;
    switch ( m_direction ) {
    case AvPlug::eAPD_Input:
        dir = PlugAddress::ePD_Input;
        break;
    case AvPlug::eAPD_Output:
        dir = PlugAddress::ePD_Output;
        break;
    default:
        dir = PlugAddress::ePD_Undefined;
    }
    return dir;
}

void
AvPlug::showPlug() const
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "\tName               = %s\n", getName() );
    debugOutput( DEBUG_LEVEL_VERBOSE, "\tType               = %s\n", extendedPlugInfoPlugTypeToString( getPlugType() ) );
    debugOutput( DEBUG_LEVEL_VERBOSE, "\tAddress Type       = %s\n", avPlugAddressTypeToString( getPlugAddressType() ) );
    debugOutput( DEBUG_LEVEL_VERBOSE, "\tId                 = %d\n", getPlugId() );
    debugOutput( DEBUG_LEVEL_VERBOSE, "\tSubunitType        = %d\n",  getSubunitType() );
    debugOutput( DEBUG_LEVEL_VERBOSE, "\tSubunitId          = %d\n",  getSubunitId() );
    debugOutput( DEBUG_LEVEL_VERBOSE, "\tDirection          = %s\n", avPlugDirectionToString( getPlugDirection() ) );
    debugOutput( DEBUG_LEVEL_VERBOSE, "\tSampling Rate      = %d\n", getSampleRate() );
    debugOutput( DEBUG_LEVEL_VERBOSE, "\tNumber of Channels = %d\n",  getNrOfChannels() );
    debugOutput( DEBUG_LEVEL_VERBOSE, "\tNumber of Streams  = %d\n",  getNrOfStreams() );
}

/////////////////////////////////////////
/////////////////////////////////////////

const char* avPlugAddressTypeStrings[] =
{
    "PCR",
    "external",
    "asynchronous",
    "subunit",
    "undefined",
};

const char* avPlugAddressTypeToString( AvPlug::EAvPlugAddressType type )
{
    if ( type > AvPlug::eAPA_SubunitPlug ) {
        type = AvPlug::eAPA_Undefined;
    }
    return avPlugAddressTypeStrings[type];
}

const char* avPlugTypeStrings[] =
{
    "IsoStream",
    "AsyncStream",
    "MIDI",
    "Sync",
    "Analog",
    "Digital",
    "Unknown",
};

const char* avPlugTypeToString( AvPlug::EAvPlugType type )
{
    if ( type > AvPlug::eAPT_Digital ) {
        type = AvPlug::eAPT_Unknown;
    }
    return avPlugTypeStrings[type];
}

const char* avPlugDirectionStrings[] =
{
    "Input",
    "Output",
    "Unknown",
};

const char* avPlugDirectionToString( AvPlug::EAvPlugDirection type )
{
    if ( type > AvPlug::eAPD_Output ) {
        type = AvPlug::eAPD_Unknown;
    }
    return avPlugDirectionStrings[type];
}

/////////////////////////////////////


AvPlugManager::AvPlugManager( bool verbose )
    : m_verbose( verbose )
{
    if ( m_verbose ) {
        setDebugLevel( DEBUG_LEVEL_VERBOSE );
    }
}

AvPlugManager::AvPlugManager( const AvPlugManager& rhs )
    : m_verbose( rhs.m_verbose )
{
    if ( m_verbose ) {
        setDebugLevel( DEBUG_LEVEL_VERBOSE );
    }
}

AvPlugManager::~AvPlugManager()
{
}

bool
AvPlugManager::addPlug( AvPlug& plug )
{
    m_plugs.push_back( &plug );
    return true;
}

bool
AvPlugManager::remPlug( AvPlug& plug )
{
    for ( AvPlugVector::iterator it = m_plugs.begin();
          it !=  m_plugs.end();
          ++it )
    {
        AvPlug* plugIt = *it;
        if ( plugIt == &plug ) {
            m_plugs.erase( it );
            return true;
        }
    }
    return false;
}

void
AvPlugManager::showPlugs() const
{
    // \todo The information provided here could be better arranged. For a start it is ok, but
    // there is room for improvement. Something for a lazy sunday afternoon (tip: maybe drink some
    // beer to get into the mood)

    printf( "\nSummery\n" );
    printf( "-------\n\n" );
    printf( "Plug       | Id | Dir | Type     | SubUnitType | SubUnitId | Name\n" );
    printf( "-----------+----+-----+----------+-------------+-----------+-----\n" );

    for ( AvPlugVector::const_iterator it = m_plugs.begin();
          it !=  m_plugs.end();
          ++it )
    {
        AvPlug* plug = *it;

        printf( "0x%08x | %2d |   %1d | %8s |          %2d |       %3d | %s\n",
                ( unsigned int )plug,
                plug->getPlugId(),
                plug->getDirection(),
                avPlugAddressTypeToString( plug->getPlugAddressType() ),
                plug->getSubunitType(),
                plug->getSubunitId(),
                plug->getName() );
    }

    printf( "\nConnections\n" );
    printf( "-----------\n" );
    for ( AvPlugVector::const_iterator it = m_plugs.begin();
          it !=  m_plugs.end();
          ++it )
    {
        AvPlug* plug = *it;
        for ( AvPlugVector::const_iterator it = plug->getInputConnections().begin();
            it != plug->getInputConnections().end();
            ++it )
        {
            printf( "%30s <- %s\n", plug->getName(), ( *it )->getName() );
        }
        for ( AvPlugVector::const_iterator it = plug->getOutputConnections().begin();
            it != plug->getOutputConnections().end();
            ++it )
        {
            printf( "%30s -> %s\n", plug->getName(), ( *it )->getName() );
        }
    }

    /*
    debugOutput( DEBUG_LEVEL_VERBOSE, "Plug details\n" );
    debugOutput( DEBUG_LEVEL_VERBOSE, "------------\n" );
    int i = 0;
    for ( AvPlugVector::const_iterator it = m_plugs.begin();
          it !=  m_plugs.end();
          ++it, ++i )
    {
        AvPlug* plug = *it;
        debugOutput( DEBUG_LEVEL_VERBOSE, "Plug %d:\n", i );
        plug->showPlug();

    }
    */
}

AvPlug*
AvPlugManager::getPlug( AVCCommand::ESubunitType subunitType,
                        subunit_id_t subunitId,
                        AvPlug::EAvPlugAddressType plugAddressType,
                        AvPlug::EAvPlugDirection plugDirection,
                        plug_id_t plugId ) const
{
    for ( AvPlugVector::const_iterator it = m_plugs.begin();
          it !=  m_plugs.end();
          ++it )
    {
        AvPlug* plug = *it;

        if (    ( subunitType == plug->getSubunitType() )
             && ( subunitId == plug->getSubunitId() )
             && ( plugAddressType == plug->getPlugAddressType() )
             && ( plugDirection == plug->getPlugDirection() )
             && ( plugId == plug->getPlugId() ) )
        {
            return plug;
        }
    }

    return 0;
}

AvPlugVector
AvPlugManager::getPlugsByType( AVCCommand::ESubunitType subunitType,
                               subunit_id_t subunitId,
                               AvPlug::EAvPlugAddressType plugAddressType,
                               AvPlug::EAvPlugDirection plugDirection,
                               AvPlug::EAvPlugType type) const
{
    AvPlugVector plugVector;
    for ( AvPlugVector::const_iterator it = m_plugs.begin();
          it !=  m_plugs.end();
          ++it )
    {
        AvPlug* plug = *it;

        if (    ( subunitType == plug->getSubunitType() )
             && ( subunitId == plug->getSubunitId() )
             && ( plugAddressType == plug->getPlugAddressType() )
             && ( plugDirection == plug->getPlugDirection() )
             && ( type == plug->getPlugType() ) )
        {
            plugVector.push_back( plug );
        }
    }

    return plugVector;
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
