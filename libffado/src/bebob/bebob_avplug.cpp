/* bebob_avplug.cpp
 * Copyright (C) 2005,06,07 by Daniel Wagner
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

#include "bebob/bebob_avplug.h"
#include "bebob/bebob_avdevice.h"
#include "libieee1394/configrom.h"

#include "libieee1394/ieee1394service.h"
#include "libavc/avc_serialize.h"

#include <sstream>

namespace BeBoB {

int AvPlug::m_globalIdCounter = 0;

IMPL_DEBUG_MODULE( AvPlug, AvPlug, DEBUG_LEVEL_NORMAL );
IMPL_DEBUG_MODULE( AvPlugManager, AvPlugManager, DEBUG_LEVEL_NORMAL );

AvPlug::AvPlug( Ieee1394Service& ieee1394Service,
                ConfigRom& configRom,
                AvPlugManager& plugManager,
                AVCCommand::ESubunitType subunitType,
                subunit_id_t subunitId,
                function_block_type_t functionBlockType,
                function_block_id_t functionBlockId,
                EAvPlugAddressType plugAddressType,
                EAvPlugDirection plugDirection,
                plug_id_t plugId,
                int verboseLevel )
    : m_p1394Service( &ieee1394Service )
    , m_pConfigRom( &configRom )
    , m_subunitType( subunitType )
    , m_subunitId( subunitId )
    , m_functionBlockType( functionBlockType )
    , m_functionBlockId( functionBlockId )
    , m_addressType( plugAddressType )
    , m_direction( plugDirection )
    , m_id( plugId )
    , m_infoPlugType( eAPT_Unknown )
    , m_nrOfChannels( 0 )
    , m_plugManager( &plugManager )
    , m_verboseLevel( verboseLevel )
    , m_globalId( m_globalIdCounter++ )
{
    setDebugLevel( m_verboseLevel );
    debugOutput( DEBUG_LEVEL_VERBOSE,
                 "nodeId = %d, subunitType = %d, "
                 "subunitId = %d, functionBlockType = %d, "
                 "functionBlockId = %d, addressType = %d, "
                 "direction = %d, id = %d\n",
                 m_pConfigRom->getNodeId(),
                 m_subunitType,
                 m_subunitId,
                 m_functionBlockType,
                 m_functionBlockId,
                 m_addressType,
                 m_direction,
                 m_id );
}

AvPlug::AvPlug( const AvPlug& rhs )
    : m_p1394Service( rhs.m_p1394Service )
    , m_pConfigRom( rhs.m_pConfigRom )
    , m_subunitType( rhs.m_subunitType )
    , m_subunitId( rhs.m_subunitId )
    , m_functionBlockType( rhs.m_functionBlockType )
    , m_functionBlockId( rhs.m_functionBlockId )
    , m_addressType( rhs.m_addressType )
    , m_direction( rhs.m_direction )
    , m_id( rhs.m_id )
    , m_infoPlugType( rhs.m_infoPlugType )
    , m_nrOfChannels( rhs.m_nrOfChannels )
    , m_name( rhs.m_name )
    , m_clusterInfos( rhs.m_clusterInfos )
    , m_formatInfos( rhs.m_formatInfos )
    , m_plugManager( rhs.m_plugManager )
    , m_verboseLevel( rhs.m_verboseLevel )
{
    if ( m_verboseLevel ) {
        setDebugLevel( DEBUG_LEVEL_VERBOSE );
     }
}

AvPlug::AvPlug()
    : m_p1394Service( 0 )
    , m_pConfigRom( 0 )
    , m_subunitType( AVCCommand::eST_Reserved ) // a good value for unknown/undefined?
    , m_subunitId( 0 )
    , m_functionBlockType( 0 )
    , m_functionBlockId( 0 )
    , m_addressType( eAPA_Undefined )
    , m_direction( eAPD_Unknown )
    , m_id( 0 )
    , m_infoPlugType( eAPT_Unknown )
    , m_nrOfChannels( 0 )
    , m_plugManager( 0 )
    , m_verboseLevel( 0 )
    , m_globalId( 0 )
{
}

AvPlug::~AvPlug()
{
    if ( m_plugManager ) {
        m_plugManager->remPlug( *this );
    }
}

bool
AvPlug::discover()
{
    if ( !discoverPlugType() ) {
        debugError( "discover: Could not discover plug type (%d,%d,%d,%d,%d)\n",
                    m_pConfigRom->getNodeId(), m_subunitType, m_subunitId, m_direction, m_id );
        return false;
    }

    if ( !discoverName() ) {
        debugError( "Could not discover name (%d,%d,%d,%d,%d)\n",
                    m_pConfigRom->getNodeId(), m_subunitType, m_subunitId, m_direction, m_id );
        return false;
    }

    if ( !discoverNoOfChannels() ) {
        debugError( "Could not discover number of channels "
                    "(%d,%d,%d,%d,%d)\n",
                    m_pConfigRom->getNodeId(), m_subunitType, m_subunitId, m_direction, m_id );
        return false;
    }

    if ( !discoverChannelPosition() ) {
        debugError( "Could not discover channel positions "
                    "(%d,%d,%d,%d,%d)\n",
                    m_pConfigRom->getNodeId(), m_subunitType, m_subunitId, m_direction, m_id );
        return false;
    }

    if ( !discoverChannelName() ) {
        debugError( "Could not discover channel name "
                    "(%d,%d,%d,%d,%d)\n",
                    m_pConfigRom->getNodeId(), m_subunitType, m_subunitId, m_direction, m_id );
        return false;
    }

    if ( !discoverClusterInfo() ) {
        debugError( "Could not discover channel name "
                    "(%d,%d,%d,%d,%d)\n",
                    m_pConfigRom->getNodeId(), m_subunitType, m_subunitId, m_direction, m_id );
        return false;
    }

    if ( !discoverStreamFormat() ) {
        debugError( "Could not discover stream format "
                    "(%d,%d,%d,%d,%d)\n",
                    m_pConfigRom->getNodeId(), m_subunitType, m_subunitId, m_direction, m_id );
        return false;
    }

    if ( !discoverSupportedStreamFormats() ) {
        debugError( "Could not discover supported stream formats "
                    "(%d,%d,%d,%d,%d)\n",
                    m_pConfigRom->getNodeId(), m_subunitType, m_subunitId, m_direction, m_id );
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
    signalSourceCmd.setCommandType( AVCCommand::eCT_SpecificInquiry );
    signalSourceCmd.setVerbose( m_verboseLevel );

    if ( !signalSourceCmd.fire() ) {
        debugError( "Could not inquire connection between '%s' and '%s'\n",
                    getName(), plug.getName() );
        return false;
    }

    if ( signalSourceCmd.getResponse() == AVCCommand::eR_Implemented ) {
        debugOutput( DEBUG_LEVEL_VERBOSE,
                     "Connection possible between '%s' and '%s'\n",
                     getName(),  plug.getName() );
        return true;
    }
    debugOutput( DEBUG_LEVEL_VERBOSE,
                 "Connection not possible between '%s' and '%s'\n",
                 getName(),  plug.getName() );
    return false;
}

bool
AvPlug::setConnection( AvPlug& plug )
{
    SignalSourceCmd signalSourceCmd = setSrcPlugAddrToSignalCmd();
    setDestPlugAddrToSignalCmd( signalSourceCmd, plug );
    signalSourceCmd.setCommandType( AVCCommand::eCT_Control );
    signalSourceCmd.setVerbose( m_verboseLevel );

    if ( !signalSourceCmd.fire() ) {
        debugError( "Could not set connection between '%s' and '%s'\n",
                    getName(), plug.getName() );
        return false;
    }

    if ( signalSourceCmd.getResponse() == AVCCommand::eR_Accepted ) {
        debugOutput( DEBUG_LEVEL_VERBOSE,
                     "Could set connection between '%s' and '%s'\n",
                     getName(), plug.getName() );
        return true;
    }
    debugOutput( DEBUG_LEVEL_VERBOSE,
                 "Could not set connection between '%s' and '%s'\n",
                 getName(),  plug.getName() );
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
    extPlugInfoCmd.setVerbose( m_verboseLevel );

    if ( !extPlugInfoCmd.fire() ) {
        debugError( "plug type command failed\n" );
        return false;
    }

    m_infoPlugType = eAPT_Unknown;

    if ( extPlugInfoCmd.getResponse() == AVCCommand::eR_Implemented ) {

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
    } else {
        debugError( "Plug does not implement extended plug info plug "
                    "type info command\n" );
        return false;
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
    extPlugInfoCmd.setVerbose( m_verboseLevel );

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
    extPlugInfoCmd.setVerbose( m_verboseLevel );

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
    extPlugInfoCmd.setVerbose( m_verboseLevel );

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
            extPlugInfoCmd.setVerbose( m_verboseLevel );

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
                     getName(),
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
        extPlugInfoCmd.setVerbose( m_verboseLevel );

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
                         getName(),
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
    extStreamFormatCmd.setVerbose( m_verboseLevel );

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

    if ( !extStreamFormatCmd.getFormatInformation() ) {
        debugWarning( "No stream format information for plug found -> skip\n" );
        return true;
    }

    if ( extStreamFormatCmd.getFormatInformation()->m_root
           != FormatInformation::eFHR_AudioMusic  )
    {
        debugWarning( "Format hierarchy root is not Audio&Music -> skip\n" );
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
                     getName(),
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
                              getName(),
                              nrOfChannels,
                              clusterInfo->m_name.c_str(),
                              streamFormatInfo->m_numberOfChannels);
            }
            clusterInfo->m_streamFormat = streamFormatInfo->m_streamFormat;

            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "%s plug %d cluster info %d ('%s'): "
                         "stream format %d\n",
                         getName(),
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
                     getName(),
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
        setPlugAddrToStreamFormatCmd(
            ExtendedStreamFormatCmd::eSF_ExtendedStreamFormatInformationCommandList);
    extStreamFormatCmd.setVerbose( m_verboseLevel );

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
            bool formatInfoIsValid = true;

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
                    case AVC1394_STREAM_FORMAT_AM824_IEC60968_3:
                        formatInfo.m_audioChannels +=
                            compoundStream->m_streamFormatInfos[j]->m_numberOfChannels;
                        break;
                    case AVC1394_STREAM_FORMAT_AM824_MULTI_BIT_LINEAR_AUDIO_RAW:
                        formatInfo.m_audioChannels +=
                            compoundStream->m_streamFormatInfos[j]->m_numberOfChannels;
                        break;
                    case AVC1394_STREAM_FORMAT_AM824_MIDI_CONFORMANT:
                        formatInfo.m_midiChannels +=
                            compoundStream->m_streamFormatInfos[j]->m_numberOfChannels;
                        break;
                    default:
                        formatInfoIsValid = false;
                        debugWarning("unknown stream format (0x%02x) for channel "
                                      "(%d)\n",
                                     compoundStream->m_streamFormatInfos[j]->m_streamFormat,
                                     j );
                    }
                }
            }

            if ( formatInfoIsValid ) {
                debugOutput( DEBUG_LEVEL_VERBOSE,
                             "[%s:%d] formatInfo[%d].m_samplingFrequency "
                             "= %d\n",
                             getName(), m_id,
                             i, formatInfo.m_samplingFrequency );
                debugOutput( DEBUG_LEVEL_VERBOSE,
                             "[%s:%d] formatInfo[%d].m_isSyncStream = %d\n",
                             getName(), m_id,
                             i, formatInfo.m_isSyncStream );
                debugOutput( DEBUG_LEVEL_VERBOSE,
                             "[%s:%d] formatInfo[%d].m_audioChannels = %d\n",
                             getName(), m_id,
                             i, formatInfo.m_audioChannels );
                debugOutput( DEBUG_LEVEL_VERBOSE,
                             "[%s:%d] formatInfo[%d].m_midiChannels = %d\n",
                             getName(), m_id,
                             i, formatInfo.m_midiChannels );
                m_formatInfos.push_back( formatInfo );
            }
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
    extPlugInfoCmd.setVerbose( m_verboseLevel );

    if ( !extPlugInfoCmd.fire() ) {
        debugError( "plug type command failed\n" );
        return false;
    }

    if ( extPlugInfoCmd.getResponse() == AVCCommand::eR_Rejected ) {
        // Plugs does not like to be asked about connections
        debugOutput( DEBUG_LEVEL_VERBOSE, "Plug '%s' rejects "
                     "connections command\n",
                     getName() );
        return true;
    }

    ExtendedPlugInfoInfoType* infoType = extPlugInfoCmd.getInfoType();
    if ( infoType
         && infoType->m_plugInput )
    {
        PlugAddressSpecificData* plugAddress
            = infoType->m_plugInput->m_plugAddress;

        if ( plugAddress->m_addressMode ==
             PlugAddressSpecificData::ePAM_Undefined )
        {
            // This plug has no input connection
            return true;
        }

        if ( !discoverConnectionsFromSpecificData( eAPD_Input,
                                                   plugAddress,
                                                   m_inputConnections ) )
        {
            debugWarning( "Could not discover connnections for plug '%s'\n",
                          getName() );
        }
    } else {
        debugError( "no valid info type for plug '%s'\n", getName() );
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
    extPlugInfoCmd.setVerbose( m_verboseLevel );

    if ( !extPlugInfoCmd.fire() ) {
        debugError( "plug type command failed\n" );
        return false;
    }

    if ( extPlugInfoCmd.getResponse() == AVCCommand::eR_Rejected ) {
        // Plugs does not like to be asked about connections
        debugOutput( DEBUG_LEVEL_VERBOSE, "Plug '%s' rejects "
                     "connections command\n",
                     getName() );
        return true;
    }

    ExtendedPlugInfoInfoType* infoType = extPlugInfoCmd.getInfoType();
    if ( infoType
         && infoType->m_plugOutput )
    {
        if ( infoType->m_plugOutput->m_nrOfOutputPlugs
             != infoType->m_plugOutput->m_outputPlugAddresses.size() )
        {
            debugError( "number of output plugs (%d) disagree with "
                        "number of elements in plug address vector (%d)\n",
                        infoType->m_plugOutput->m_nrOfOutputPlugs,
                        infoType->m_plugOutput->m_outputPlugAddresses.size());
        }

        if ( infoType->m_plugOutput->m_nrOfOutputPlugs == 0 ) {
            // This plug has no output connections
            return true;
        }

        for ( unsigned int i = 0;
              i < infoType->m_plugOutput->m_outputPlugAddresses.size();
              ++i )
        {
            PlugAddressSpecificData* plugAddress
                = infoType->m_plugOutput->m_outputPlugAddresses[i];

            if ( !discoverConnectionsFromSpecificData( eAPD_Output,
                                                       plugAddress,
                                                       m_outputConnections ) )
            {
                debugWarning( "Could not discover connnections for "
                              "plug '%s'\n", getName() );
            }
        }
    } else {
        debugError( "no valid info type for plug '%s'\n", getName() );
        return false;
    }

    return true;
}

bool
AvPlug::discoverConnectionsFromSpecificData(
    EAvPlugDirection discoverDirection,
    PlugAddressSpecificData* plugAddress,
    AvPlugVector& connections )
{
    UnitPlugSpecificDataPlugAddress* pUnitPlugAddress =
        dynamic_cast<UnitPlugSpecificDataPlugAddress*>
        ( plugAddress->m_plugAddressData );

    SubunitPlugSpecificDataPlugAddress* pSubunitPlugAddress =
        dynamic_cast<SubunitPlugSpecificDataPlugAddress*>
        ( plugAddress->m_plugAddressData );

    FunctionBlockPlugSpecificDataPlugAddress*
        pFunctionBlockPlugAddress =
        dynamic_cast<FunctionBlockPlugSpecificDataPlugAddress*>
        ( plugAddress->m_plugAddressData );

    AvPlug* plug = getPlugDefinedBySpecificData(
        pUnitPlugAddress,
        pSubunitPlugAddress,
        pFunctionBlockPlugAddress );

    if ( plug ) {
        debugOutput( DEBUG_LEVEL_VERBOSE,
                     "'(%d) %s' has a connection to '(%d) %s'\n",
                     getGlobalId(),
                     getName(),
                     plug->getGlobalId(),
                     plug->getName() );
        addPlugConnection( connections, *plug );
    } else {
        debugError( "no corresponding plug found for '(%d) %s'\n",
                    getGlobalId(),
                    getName() );
        return false;
    }

    return true;
}

bool
AvPlug::addPlugConnection( AvPlugVector& connections,
                           AvPlug& plug )

{
    for ( AvPlugVector::iterator it = connections.begin();
          it != connections.end();
          ++it )
    {
        AvPlug* cPlug = *it;
        if ( cPlug == &plug ) {
            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "plug '%s' already in connection list\n",
                         plug.getName() );
            return true;
        }
    }

    connections.push_back( &plug );
    return true;
}

ExtendedPlugInfoCmd
AvPlug::setPlugAddrToPlugInfoCmd()
{
    ExtendedPlugInfoCmd extPlugInfoCmd( *m_p1394Service );

    switch( m_subunitType ) {
    case AVCCommand::eST_Unit:
        {
            UnitPlugAddress::EPlugType ePlugType =
                UnitPlugAddress::ePT_Unknown;
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
            extPlugInfoCmd.setPlugAddress(
                PlugAddress( convertPlugDirection( getPlugDirection() ),
                             PlugAddress::ePAM_Unit,
                             unitPlugAddress ) );
        }
        break;
    case AVCCommand::eST_Music:
    case AVCCommand::eST_Audio:
        {
            switch( m_addressType ) {
            case eAPA_SubunitPlug:
            {
                SubunitPlugAddress subunitPlugAddress( m_id );
                extPlugInfoCmd.setPlugAddress(
                    PlugAddress(
                        convertPlugDirection( getPlugDirection() ),
                        PlugAddress::ePAM_Subunit,
                        subunitPlugAddress ) );
            }
            break;
            case eAPA_FunctionBlockPlug:
            {
                FunctionBlockPlugAddress functionBlockPlugAddress(
                    m_functionBlockType,
                    m_functionBlockId,
                    m_id );
                extPlugInfoCmd.setPlugAddress(
                    PlugAddress(
                        convertPlugDirection( getPlugDirection() ),
                        PlugAddress::ePAM_FunctionBlock,
                        functionBlockPlugAddress ) );
            }
            break;
            default:
                extPlugInfoCmd.setPlugAddress(PlugAddress());
            }
        }
        break;
    default:
        debugError( "Unknown subunit type\n" );
    }

    extPlugInfoCmd.setNodeId( m_pConfigRom->getNodeId() );
    extPlugInfoCmd.setCommandType( AVCCommand::eCT_Status );
    extPlugInfoCmd.setSubunitId( m_subunitId );
    extPlugInfoCmd.setSubunitType( m_subunitType );

    return extPlugInfoCmd;
}

ExtendedStreamFormatCmd
AvPlug::setPlugAddrToStreamFormatCmd(
    ExtendedStreamFormatCmd::ESubFunction subFunction)
{
    ExtendedStreamFormatCmd extStreamFormatInfoCmd(
        *m_p1394Service,
        subFunction );
    switch( m_subunitType ) {
    case AVCCommand::eST_Unit:
    {
            UnitPlugAddress::EPlugType ePlugType =
                UnitPlugAddress::ePT_Unknown;
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
        extStreamFormatInfoCmd.setPlugAddress(
            PlugAddress( convertPlugDirection( getPlugDirection() ),
                         PlugAddress::ePAM_Unit,
                         unitPlugAddress ) );
        }
        break;
    case AVCCommand::eST_Music:
    case AVCCommand::eST_Audio:
    {
        switch( m_addressType ) {
        case eAPA_SubunitPlug:
        {
            SubunitPlugAddress subunitPlugAddress( m_id );
            extStreamFormatInfoCmd.setPlugAddress(
                PlugAddress( convertPlugDirection( getPlugDirection() ),
                             PlugAddress::ePAM_Subunit,
                             subunitPlugAddress ) );
        }
        break;
        case eAPA_FunctionBlockPlug:
        {
            FunctionBlockPlugAddress functionBlockPlugAddress(
                m_functionBlockType,
                m_functionBlockId,
                m_id );
            extStreamFormatInfoCmd.setPlugAddress(
                PlugAddress( convertPlugDirection( getPlugDirection() ),
                             PlugAddress::ePAM_FunctionBlock,
                             functionBlockPlugAddress ) );
        }
        break;
        default:
            extStreamFormatInfoCmd.setPlugAddress(PlugAddress());
        }
    }
    break;
    default:
        debugError( "Unknown subunit type\n" );
    }

    extStreamFormatInfoCmd.setNodeId( m_pConfigRom->getNodeId() );
    extStreamFormatInfoCmd.setCommandType( AVCCommand::eCT_Status );
    extStreamFormatInfoCmd.setSubunitId( m_subunitId );
    extStreamFormatInfoCmd.setSubunitType( m_subunitType );

    return extStreamFormatInfoCmd;
}

SignalSourceCmd
AvPlug::setSrcPlugAddrToSignalCmd()
{
    SignalSourceCmd signalSourceCmd( *m_p1394Service );

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

    signalSourceCmd.setNodeId( m_pConfigRom->getNodeId() );
    signalSourceCmd.setSubunitType( AVCCommand::eST_Unit  );
    signalSourceCmd.setSubunitId( 0xff );

    return signalSourceCmd;
}

void
AvPlug::setDestPlugAddrToSignalCmd(SignalSourceCmd& signalSourceCmd,
                                   AvPlug& plug)
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
AvPlug::convertPlugDirection( EAvPlugDirection direction )
{
    PlugAddress::EPlugDirection dir;
    switch ( direction ) {
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
    debugOutput( DEBUG_LEVEL_VERBOSE, "\tName               = %s\n",
                 getName() );
    debugOutput( DEBUG_LEVEL_VERBOSE, "\tType               = %s\n",
                 extendedPlugInfoPlugTypeToString( getPlugType() ) );
    debugOutput( DEBUG_LEVEL_VERBOSE, "\tAddress Type       = %s\n",
                 avPlugAddressTypeToString( getPlugAddressType() ) );
    debugOutput( DEBUG_LEVEL_VERBOSE, "\tId                 = %d\n",
                 getPlugId() );
    debugOutput( DEBUG_LEVEL_VERBOSE, "\tSubunitType        = %d\n",
                 getSubunitType() );
    debugOutput( DEBUG_LEVEL_VERBOSE, "\tSubunitId          = %d\n",
                 getSubunitId() );
    debugOutput( DEBUG_LEVEL_VERBOSE, "\tDirection          = %s\n",
                 avPlugDirectionToString( getPlugDirection() ) );
    debugOutput( DEBUG_LEVEL_VERBOSE, "\tSampling Rate      = %d\n",
                 getSampleRate() );
    debugOutput( DEBUG_LEVEL_VERBOSE, "\tNumber of Channels = %d\n",
                 getNrOfChannels() );
    debugOutput( DEBUG_LEVEL_VERBOSE, "\tNumber of Streams  = %d\n",
                 getNrOfStreams() );
}


AvPlug*
AvPlug::getPlugDefinedBySpecificData(
    UnitPlugSpecificDataPlugAddress* pUnitPlugAddress,
    SubunitPlugSpecificDataPlugAddress* pSubunitPlugAddress,
    FunctionBlockPlugSpecificDataPlugAddress* pFunctionBlockPlugAddress )
{
    subunit_type_t        subunitType       = 0xff;
    subunit_id_t          subunitId         = 0xff;
    function_block_type_t functionBlockType = 0xff;
    function_block_id_t   functionBlockId   = 0xff;
    EAvPlugAddressType    addressType       = eAPA_Undefined;
    EAvPlugDirection      direction         = eAPD_Unknown;
    plug_id_t             plugId            = 0xff;

    if ( !pUnitPlugAddress
         && !pSubunitPlugAddress
         && !pFunctionBlockPlugAddress )
    {
        debugError( "No correct specific data found\n" );
        return 0;
    }

    if ( pUnitPlugAddress ) {
        subunitType = AVCCommand::eST_Unit;
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
        // unit plug have only connections to subunits
        if ( getPlugAddressType() == eAPA_SubunitPlug ) {
            direction = getDirection();
        } else {
            debugError( "Function block has connection from/to unknown "
                        "plug type\n" );
            direction = eAPD_Unknown;
        }
        plugId = pUnitPlugAddress->m_plugId;

        debugOutput( DEBUG_LEVEL_VERBOSE,
                     "'(%d) %s': Remote plug is a unit plug "
                     "(%s, %s, %d)\n",
                     getGlobalId(),
                     getName(),
                     avPlugAddressTypeToString( addressType ),
                     avPlugDirectionToString( direction ),
                     plugId );
    }

    if ( pSubunitPlugAddress ) {
        subunitType = pSubunitPlugAddress->m_subunitType;
        subunitId = pSubunitPlugAddress->m_subunitId;
        addressType = eAPA_SubunitPlug;

        if ( getPlugAddressType() == eAPA_FunctionBlockPlug ) {
            direction = getDirection();
        } else if ( getPlugAddressType() == eAPA_SubunitPlug ) {
            direction = toggleDirection( getDirection() );
        } else {
            // unit
            direction = getDirection();
        }

        plugId = pSubunitPlugAddress->m_plugId;
        debugOutput( DEBUG_LEVEL_VERBOSE,
                     "'(%d) %s': Remote plug is a subunit plug "
                     "(%d, %d, %s, %d)\n",
                     getGlobalId(),
                     getName(),
                     subunitType,
                     subunitId,
                     avPlugDirectionToString( direction ),
                     plugId );
    }

    if ( pFunctionBlockPlugAddress ) {
        subunitType = pFunctionBlockPlugAddress->m_subunitType;
        subunitId = pFunctionBlockPlugAddress->m_subunitId;
        functionBlockType = pFunctionBlockPlugAddress->m_functionBlockType;
        functionBlockId = pFunctionBlockPlugAddress->m_functionBlockId;
        addressType = eAPA_FunctionBlockPlug;

        if ( getPlugAddressType() == eAPA_FunctionBlockPlug ) {
            direction = toggleDirection( getDirection() );
        } else if ( getPlugAddressType() == eAPA_SubunitPlug ){
            direction = getDirection();
        } else {
            debugError( "Function block has connection from/to unknown "
                        "plug type\n" );
            direction = eAPD_Unknown;
        }

        plugId = pFunctionBlockPlugAddress->m_plugId;

        debugOutput( DEBUG_LEVEL_VERBOSE,
                     "'(%d) %s': Remote plug is a functionblock plug "
                     "(%d, %d, %d, %d, %s, %d)\n",
                     getGlobalId(),
                     getName(),
                     subunitType,
                     subunitId,
                     functionBlockType,
                     functionBlockId,
                     avPlugDirectionToString( direction ),
                     plugId );
    }

    AVCCommand::ESubunitType enumSubunitType =
        static_cast<AVCCommand::ESubunitType>( subunitType );

    return m_plugManager->getPlug(
        enumSubunitType,
        subunitId,
        functionBlockType,
        functionBlockId,
        addressType,
        direction,
        plugId );
}

AvPlug::EAvPlugDirection
AvPlug::toggleDirection( EAvPlugDirection direction ) const
{
    EAvPlugDirection newDirection;
    switch ( direction ) {
    case eAPD_Output:
        newDirection = eAPD_Input;
        break;
    case eAPD_Input:
        newDirection = eAPD_Output;
        break;
    default:
        newDirection = direction;
    }

    return newDirection;
}

bool
AvPlug::serializeChannelInfos( Glib::ustring basePath,
                               Util::IOSerialize& ser,
                               const ClusterInfo& clusterInfo ) const
{
    bool result = true;
    int i = 0;
    for ( ChannelInfoVector::const_iterator it = clusterInfo.m_channelInfos.begin();
          it != clusterInfo.m_channelInfos.end();
          ++it )
    {
        const ChannelInfo& info = *it;
        std::ostringstream strstrm;
        strstrm << basePath << i;

        result &= ser.write( strstrm.str() + "/m_streamPosition", info.m_streamPosition );
        result &= ser.write( strstrm.str() + "/m_location", info.m_location );
        result &= ser.write( strstrm.str() + "/m_name", info.m_name );
    }

    return result;
}

bool
AvPlug::deserializeChannelInfos( Glib::ustring basePath,
                                 Util::IODeserialize& deser,
                                 ClusterInfo& clusterInfo )
{
    int i = 0;
    bool bFinished = false;
    bool result = true;
    do {
        std::ostringstream strstrm;
        strstrm << basePath << i;

        // check for one element to exist. when one exist the other elements
        // must also be there. otherwise just return (last) result.
        if ( deser.isExisting( strstrm.str() + "/m_streamPosition" ) ) {
            ChannelInfo info;

            result &= deser.read( strstrm.str() + "/m_streamPosition", info.m_streamPosition );
            result &= deser.read( strstrm.str() + "/m_location", info.m_location );
            result &= deser.read( strstrm.str() + "/m_name", info.m_name );

            if ( result ) {
                clusterInfo.m_channelInfos.push_back( info );
                i++;
            } else {
                bFinished = true;
            }
        } else {
            bFinished = true;
        }
    } while ( !bFinished );

    return result;
}


bool
AvPlug::serializeClusterInfos( Glib::ustring basePath,
                               Util::IOSerialize& ser ) const
{
    bool result = true;
    int i = 0;
    for ( ClusterInfoVector::const_iterator it = m_clusterInfos.begin();
          it != m_clusterInfos.end();
          ++it )
    {
        const ClusterInfo& info = *it;
        std::ostringstream strstrm;
        strstrm << basePath << i;

        result &= ser.write( strstrm.str() + "/m_index", info.m_index );
        result &= ser.write( strstrm.str() + "/m_portType", info.m_portType );
        result &= ser.write( strstrm.str() + "/m_name", info.m_name );
        result &= ser.write( strstrm.str() + "/m_nrOfChannels", info.m_nrOfChannels );
        result &= serializeChannelInfos( strstrm.str() + "/m_channelInfo", ser, info );
        result &= ser.write( strstrm.str() + "/m_streamFormat", info.m_streamFormat );

    }

    return result;
}

bool
AvPlug::deserializeClusterInfos( Glib::ustring basePath,
                                 Util::IODeserialize& deser )
{
    int i = 0;
    bool bFinished = false;
    bool result = true;
    do {
        std::ostringstream strstrm;
        strstrm << basePath << i;

        // check for one element to exist. when one exist the other elements
        // must also be there. otherwise just return (last) result.
        if ( deser.isExisting( strstrm.str() + "/m_index" ) ) {
            ClusterInfo info;

            result &= deser.read( strstrm.str() + "/m_index", info.m_index );
            result &= deser.read( strstrm.str() + "/m_portType", info.m_portType );
            result &= deser.read( strstrm.str() + "/m_name", info.m_name );
            result &= deser.read( strstrm.str() + "/m_nrOfChannels", info.m_nrOfChannels );
            result &= deserializeChannelInfos( strstrm.str() + "/m_channelInfo", deser, info );
            result &= deser.read( strstrm.str() + "/m_streamFormat", info.m_streamFormat );

            if ( result ) {
                m_clusterInfos.push_back( info );
                i++;
            } else {
                bFinished = true;
            }
        } else {
            bFinished = true;
        }
    } while ( !bFinished );

    return result;
}


bool
AvPlug::serializeFormatInfos( Glib::ustring basePath,
                              Util::IOSerialize& ser ) const
{
    bool result = true;
    int i = 0;
    for ( FormatInfoVector::const_iterator it = m_formatInfos.begin();
          it != m_formatInfos.end();
          ++it )
    {
        const FormatInfo& info = *it;
        std::ostringstream strstrm;
        strstrm << basePath << i;

        result &= ser.write( strstrm.str() + "/m_samplingFrequency", info.m_samplingFrequency );
        result &= ser.write( strstrm.str() + "/m_isSyncStream", info.m_isSyncStream );
        result &= ser.write( strstrm.str() + "/m_audioChannels", info.m_audioChannels );
        result &= ser.write( strstrm.str() + "/m_midiChannels", info.m_midiChannels );
        result &= ser.write( strstrm.str() + "/m_index", info.m_index );
    }
    return result;
}

bool
AvPlug::deserializeFormatInfos( Glib::ustring basePath,
                                Util::IODeserialize& deser )
{
    int i = 0;
    bool bFinished = false;
    bool result = true;
    do {
        std::ostringstream strstrm;
        strstrm << basePath << i;

        // check for one element to exist. when one exist the other elements
        // must also be there. otherwise just return (last) result.
        if ( deser.isExisting( strstrm.str() + "/m_samplingFrequency" ) ) {
            FormatInfo info;

            result &= deser.read( strstrm.str() + "/m_samplingFrequency", info.m_samplingFrequency );
            result &= deser.read( strstrm.str() + "/m_isSyncStream", info.m_isSyncStream );
            result &= deser.read( strstrm.str() + "/m_audioChannels", info.m_audioChannels );
            result &= deser.read( strstrm.str() + "/m_midiChannels", info.m_midiChannels );
            result &= deser.read( strstrm.str() + "/m_index", info.m_index );

            if ( result ) {
                m_formatInfos.push_back( info );
                i++;
            } else {
                bFinished = true;
            }
        } else {
            bFinished = true;
        }
    } while ( !bFinished );

    return result;
}


bool
AvPlug::serializeAvPlugVector( Glib::ustring basePath,
                               Util::IOSerialize& ser,
                               const AvPlugVector& vec) const
{
    bool result = true;
    int i = 0;
    for ( AvPlugVector::const_iterator it = vec.begin();
          it != vec.end();
          ++it )
    {
        const AvPlug* pPlug = *it;
        std::ostringstream strstrm;
        strstrm << basePath << i;

        result &= ser.write( strstrm.str() + "/global_id", pPlug->getGlobalId() );
        i++;
    }
    return result;
}

bool
AvPlug::deserializeAvPlugVector( Glib::ustring basePath,
                                 Util::IODeserialize& deser,
                                 AvPlugVector& vec )
{
    int i = 0;
    bool bFinished = false;
    bool result = true;
    do {
        std::ostringstream strstrm;
        strstrm << basePath << i;

        // check for one element to exist. when one exist the other elements
        // must also be there. otherwise just return (last) result.
        if ( deser.isExisting( strstrm.str() + "/global_id" ) ) {
            unsigned int iPlugId;
            result &= deser.read( strstrm.str() + "/global_id", iPlugId );

            if ( result ) {
                AvPlug* pPlug = m_plugManager->getPlug( iPlugId );
                if ( pPlug ) {
                    vec.push_back( pPlug );
                } else {
                    result = false;
                    bFinished = true;
                }
                i++;
            } else {
                bFinished = true;
            }
        } else {
            bFinished = true;
        }
    } while ( !bFinished );

    return result;
}

bool
AvPlug::serialize( Glib::ustring basePath, Util::IOSerialize& ser ) const
{
    bool result;
    result  = ser.write( basePath + "m_subunitType", m_subunitType );
    result &= ser.write( basePath + "m_subunitId", m_subunitId );
    result &= ser.write( basePath + "m_functionBlockType", m_functionBlockType);
    result &= ser.write( basePath + "m_functionBlockId", m_functionBlockId);
    result &= ser.write( basePath + "m_addressType", m_addressType );
    result &= ser.write( basePath + "m_direction", m_direction);
    result &= ser.write( basePath + "m_id", m_id);
    result &= ser.write( basePath + "m_infoPlugType", m_infoPlugType);
    result &= ser.write( basePath + "m_nrOfChannels", m_nrOfChannels);
    result &= ser.write( basePath + "m_name", m_name);
    result &= serializeClusterInfos( basePath + "m_clusterInfos", ser );
    result &= ser.write( basePath + "m_samplingFrequency", m_samplingFrequency);
    result &= serializeFormatInfos( basePath + "m_formatInfo", ser );
    result &= serializeAvPlugVector( basePath + "m_inputConnections", ser, m_inputConnections );
    result &= serializeAvPlugVector( basePath + "m_outputConnections", ser, m_outputConnections );
    result &= ser.write( basePath + "m_verboseLevel", m_verboseLevel);
    result &= ser.write( basePath + "m_globalId", m_globalId);
    result &= ser.write( basePath + "m_globalIdCounter", m_globalIdCounter );

    return result;
}

AvPlug*
AvPlug::deserialize( Glib::ustring basePath,
                     Util::IODeserialize& deser,
                     AvDevice& avDevice,
                     AvPlugManager& plugManager )
{
    AvPlug* pPlug = new AvPlug;
    if ( !pPlug ) {
        return 0;
    }

    pPlug->m_p1394Service = &avDevice.get1394Service();
    pPlug->m_pConfigRom   = &avDevice.getConfigRom();
    pPlug->m_plugManager  = &plugManager;
    bool result;
    result  = deser.read( basePath + "m_subunitType", pPlug->m_subunitType );
    result &= deser.read( basePath + "m_subunitId", pPlug->m_subunitId );
    result &= deser.read( basePath + "m_functionBlockType", pPlug->m_functionBlockType );
    result &= deser.read( basePath + "m_functionBlockId", pPlug->m_functionBlockId );
    result &= deser.read( basePath + "m_addressType", pPlug->m_addressType );
    result &= deser.read( basePath + "m_direction", pPlug->m_direction );
    result &= deser.read( basePath + "m_id", pPlug->m_id );
    result &= deser.read( basePath + "m_infoPlugType", pPlug->m_infoPlugType );
    result &= deser.read( basePath + "m_nrOfChannels", pPlug->m_nrOfChannels );
    result &= deser.read( basePath + "m_name", pPlug->m_name );
    result &= pPlug->deserializeClusterInfos( basePath + "m_clusterInfos", deser );
    result &= deser.read( basePath + "m_samplingFrequency", pPlug->m_samplingFrequency );
    result &= pPlug->deserializeFormatInfos( basePath + "m_formatInfos", deser );
    // input and output connections can't be processed here because not all plugs might
    // deserialized at this point. so we do that in deserializeUpdate.
    result &= deser.read( basePath + "m_verboseLevel", pPlug->m_verboseLevel );
    result &= deser.read( basePath + "m_globalId", pPlug->m_globalId );
    result &= deser.read( basePath + "m_globalIdCounter", pPlug->m_globalIdCounter );

    if ( !result ) {
        delete pPlug;
        return 0;
    }

    return pPlug;
}

bool
AvPlug::deserializeUpdate( Glib::ustring basePath,
                           Util::IODeserialize& deser )
{
    bool result;

    result  = deserializeAvPlugVector( basePath + "m_inputConnections", deser, m_inputConnections );
    result &= deserializeAvPlugVector( basePath + "m_outputConnections", deser, m_outputConnections );

    return result;
}

/////////////////////////////////////////
/////////////////////////////////////////

const char* avPlugAddressTypeStrings[] =
{
    "PCR",
    "external",
    "asynchronous",
    "subunit",
    "functionblock",
    "undefined",
};

const char* avPlugAddressTypeToString( AvPlug::EAvPlugAddressType type )
{
    if ( type > ( int )( sizeof( avPlugAddressTypeStrings )
                         / sizeof( avPlugAddressTypeStrings[0] ) ) )
    {
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
    if ( type > ( int )( sizeof( avPlugTypeStrings )
                         / sizeof( avPlugTypeStrings[0] ) ) )
    {
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
    if ( type > ( int )( sizeof( avPlugDirectionStrings )
                         / sizeof( avPlugDirectionStrings[0] ) ) )
    {
        type = AvPlug::eAPD_Unknown;
    }
    return avPlugDirectionStrings[type];
}

/////////////////////////////////////


AvPlugManager::AvPlugManager( int verboseLevel )
    : m_verboseLevel( verboseLevel )
{
    setDebugLevel( m_verboseLevel );
}

AvPlugManager::AvPlugManager()
    : m_verboseLevel( 0 )
{
    setDebugLevel( 0 );
}

AvPlugManager::AvPlugManager( const AvPlugManager& rhs )
    : m_verboseLevel( rhs.m_verboseLevel )
{
    setDebugLevel( m_verboseLevel );
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

// helper function
static void addConnection( AvPlugConnectionOwnerVector& connections,
                           AvPlug& srcPlug,
                           AvPlug& destPlug )
{
    for ( AvPlugConnectionOwnerVector::iterator it = connections.begin();
          it != connections.end();
          ++it )
    {
        AvPlugConnection& con = *it;
        if ( ( &( con.getSrcPlug() ) == &srcPlug )
             && ( &( con.getDestPlug() ) == &destPlug ) )
        {
            return;
        }
    }
    connections.push_back( AvPlugConnection( srcPlug, destPlug ) );
}

void
AvPlugManager::showPlugs() const
{
    // \todo The information provided here could be better arranged. For a start it is ok, but
    // there is room for improvement. Something for a lazy sunday afternoon (tip: maybe drink some
    // beer to get into the mood)

    printf( "\nSummary\n" );
    printf( "-------\n\n" );
    printf( "Nr | AddressType     | Direction | SubUnitType | SubUnitId | FunctionBlockType | FunctionBlockId | Id   | Type         |Name\n" );
    printf( "---+-----------------+-----------+-------------+-----------+-------------------+-----------------+------+--------------+------\n" );

    for ( AvPlugVector::const_iterator it = m_plugs.begin();
          it !=  m_plugs.end();
          ++it )
    {
        AvPlug* plug = *it;

        printf( "%2d | %15s | %9s | %11s |      0x%02x |              0x%02x |            0x%02x | 0x%02x | %12s | %s\n",
                plug->getGlobalId(),
                avPlugAddressTypeToString( plug->getPlugAddressType() ),
                avPlugDirectionToString( plug->getDirection() ),
                subunitTypeToString( plug->getSubunitType() ),
                plug->getSubunitId(),
                plug->getFunctionBlockType(),
                plug->getFunctionBlockId(),
                plug->getPlugId(),
                avPlugTypeToString( plug->getPlugType() ),
                plug->getName() );
    }

    printf( "\nConnections\n" );
    printf( "-----------\n" );

    AvPlugConnectionOwnerVector connections;

    for ( AvPlugVector::const_iterator it = m_plugs.begin();
          it !=  m_plugs.end();
          ++it )
    {
        AvPlug* plug = *it;
        for ( AvPlugVector::const_iterator it =
                  plug->getInputConnections().begin();
            it != plug->getInputConnections().end();
            ++it )
        {
            addConnection( connections, *( *it ), *plug );
        }
        for ( AvPlugVector::const_iterator it =
                  plug->getOutputConnections().begin();
            it != plug->getOutputConnections().end();
            ++it )
        {
            addConnection( connections, *plug, *( *it ) );
        }
    }

    printf( "digraph avcconnections {\n" );
    for ( AvPlugConnectionOwnerVector::iterator it = connections.begin();
          it != connections.end();
          ++it )
    {
        AvPlugConnection& con = *it;
        printf( "\t\"(%d) %s\" -> \"(%d) %s\"\n",
                con.getSrcPlug().getGlobalId(),
                con.getSrcPlug().getName(),
                con.getDestPlug().getGlobalId(),
                con.getDestPlug().getName() );
    }
    for ( AvPlugVector::const_iterator it = m_plugs.begin();
          it != m_plugs.end();
          ++it )
    {
        AvPlug* plug = *it;
        if ( plug->getFunctionBlockType() != 0xff ) {
            if ( plug->getPlugDirection() == AvPlug::eAPD_Input ) {
                printf( "\t\"(%d) %s\" -> \"(0x%02x,%d)\"\n",
                        plug->getGlobalId(),
                        plug->getName(),
                        plug->getFunctionBlockType(),
                        plug->getFunctionBlockId() );
            } else {
                printf( "\t\"(0x%02x,%d)\" -> \t\"(%d) %s\"\n",
                        plug->getFunctionBlockType(),
                        plug->getFunctionBlockId(),
                        plug->getGlobalId(),
                        plug->getName() );
            }
        }
    }

    const char* colorStrings[] = {
        "coral",
        "slateblue",
        "white",
        "green",
        "yellow",
        "grey",
    };

    for ( AvPlugVector::const_iterator it = m_plugs.begin();
          it !=  m_plugs.end();
          ++it )
    {
        AvPlug* plug = *it;
        printf( "\t\"(%d) %s\" [color=%s,style=filled];\n",
                plug->getGlobalId(), plug->getName(),
                colorStrings[plug->getPlugAddressType() ] );
    }

    printf("}\n" );
    printf( "Use \"dot -Tps FILENAME.dot -o FILENAME.ps\" "
            "to generate graph\n");

    debugOutput( DEBUG_LEVEL_VERBOSE, "Plug details\n" );
    debugOutput( DEBUG_LEVEL_VERBOSE, "------------\n" );
    for ( AvPlugVector::const_iterator it = m_plugs.begin();
          it !=  m_plugs.end();
          ++it )
    {
        AvPlug* plug = *it;
        debugOutput( DEBUG_LEVEL_VERBOSE, "Plug %d:\n", plug->getGlobalId() );
        plug->showPlug();

    }
}

AvPlug*
AvPlugManager::getPlug( AVCCommand::ESubunitType subunitType,
                        subunit_id_t subunitId,
                        function_block_type_t functionBlockType,
                        function_block_id_t functionBlockId,
                        AvPlug::EAvPlugAddressType plugAddressType,
                        AvPlug::EAvPlugDirection plugDirection,
                        plug_id_t plugId ) const
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "SBT, SBID, FBT, FBID, AT, PD, ID = "
                 "(0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x)\n",
                 subunitType,
                 subunitId,
                 functionBlockType,
                 functionBlockId,
                 plugAddressType,
                 plugDirection,
                 plugId );

    for ( AvPlugVector::const_iterator it = m_plugs.begin();
          it !=  m_plugs.end();
          ++it )
    {
        AvPlug* plug = *it;

        if (    ( subunitType == plug->getSubunitType() )
             && ( subunitId == plug->getSubunitId() )
             && ( functionBlockType == plug->getFunctionBlockType() )
             && ( functionBlockId == plug->getFunctionBlockId() )
             && ( plugAddressType == plug->getPlugAddressType() )
             && ( plugDirection == plug->getPlugDirection() )
             && ( plugId == plug->getPlugId() ) )
        {
            return plug;
        }
    }

    return 0;
}

AvPlug*
AvPlugManager::getPlug( int iGlobalId ) const
{
    for ( AvPlugVector::const_iterator it = m_plugs.begin();
          it !=  m_plugs.end();
          ++it )
    {
        AvPlug* pPlug = *it;
        if ( pPlug->getGlobalId() == iGlobalId ) {
            return pPlug;
        }
    }

    return 0;
}

AvPlugVector
AvPlugManager::getPlugsByType( AVCCommand::ESubunitType subunitType,
                               subunit_id_t subunitId,
                               function_block_type_t functionBlockType,
                               function_block_id_t functionBlockId,
                               AvPlug::EAvPlugAddressType plugAddressType,
                               AvPlug::EAvPlugDirection plugDirection,
                               AvPlug::EAvPlugType type) const
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "SBT, SBID, FBT, FBID, AT, PD, T = "
                 "(0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x)\n",
                 subunitType,
                 subunitId,
                 functionBlockType,
                 functionBlockId,
                 plugAddressType,
                 plugDirection,
                 type );

    AvPlugVector plugVector;
    for ( AvPlugVector::const_iterator it = m_plugs.begin();
          it !=  m_plugs.end();
          ++it )
    {
        AvPlug* pPlug = *it;

        if (    ( subunitType == pPlug->getSubunitType() )
             && ( subunitId == pPlug->getSubunitId() )
             && ( functionBlockType == pPlug->getFunctionBlockType() )
             && ( functionBlockId == pPlug->getFunctionBlockId() )
             && ( plugAddressType == pPlug->getPlugAddressType() )
             && ( plugDirection == pPlug->getPlugDirection() )
             && ( type == pPlug->getPlugType() ) )
        {
            plugVector.push_back( pPlug );
        }
    }

    return plugVector;
}

bool
AvPlugManager::serialize( Glib::ustring basePath, Util::IOSerialize& ser ) const
{
    bool result = true;
    int i = 0;
    for ( AvPlugVector::const_iterator it = m_plugs.begin();
          it !=  m_plugs.end();
          ++it )
    {
        AvPlug* pPlug = *it;
        std::ostringstream strstrm;
        strstrm << basePath << i;
        result &= pPlug->serialize( strstrm.str() + "/", ser );
        i++;
    }

    return result;
}

AvPlugManager*
AvPlugManager::deserialize( Glib::ustring basePath,
                            Util::IODeserialize& deser,
                            AvDevice& avDevice )

{
    AvPlugManager* pMgr = new AvPlugManager;

    if ( !pMgr ) {
        return 0;
    }

    int i = 0;
    bool bFinished = false;
    do {
        std::ostringstream strstrm;
        strstrm << basePath << i;
        // avDevice still holds a null pointer for the plug manager
        // therefore we have to *this as additional argument
        AvPlug* pPlug = AvPlug::deserialize( strstrm.str() + "/",
                                             deser,
                                             avDevice,
                                             *pMgr );
        if ( pPlug ) {
            pMgr->m_plugs.push_back( pPlug );
            i++;
        } else {
            bFinished = true;
        }
    } while ( !bFinished );

    return pMgr;
}


////////////////////////////////////

AvPlugConnection::AvPlugConnection( AvPlug& srcPlug, AvPlug& destPlug )
    : m_srcPlug( &srcPlug )
    , m_destPlug( &destPlug )
{
}

AvPlugConnection::AvPlugConnection()
    : m_srcPlug( 0 )
    , m_destPlug( 0 )
{
}

bool
AvPlugConnection::serialize( Glib::ustring basePath, Util::IOSerialize& ser ) const
{
    bool result;
    result  = ser.write( basePath + "m_srcPlug", m_srcPlug->getGlobalId() );
    result &= ser.write( basePath + "m_destPlug", m_destPlug->getGlobalId() );
    return result;
}

AvPlugConnection*
AvPlugConnection::deserialize( Glib::ustring basePath,
                               Util::IODeserialize& deser,
                               AvDevice& avDevice )
{
    AvPlugConnection* pConnection = new AvPlugConnection;
    if ( !pConnection ) {
        return 0;
    }

    bool result;
    int iSrcPlugId;
    int iDestPlugId;
    result  = deser.read( basePath + "m_srcPlug", iSrcPlugId );
    result &= deser.read( basePath + "m_destPlug",  iDestPlugId );

    if ( !result ) {
        delete pConnection;
        return 0;
    }

    pConnection->m_srcPlug  = avDevice.getPlugManager().getPlug( iSrcPlugId );
    pConnection->m_destPlug = avDevice.getPlugManager().getPlug( iDestPlugId );

    if ( !pConnection->m_srcPlug || !pConnection->m_destPlug ) {
        delete pConnection;
        return 0;
    }

    return pConnection;
}

}
