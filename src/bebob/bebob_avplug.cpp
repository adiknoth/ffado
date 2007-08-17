/*
 * Copyright (C) 2005-2007 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software Foundation;
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#include "bebob/bebob_avplug.h"
#include "bebob/bebob_avdevice.h"
#include "libieee1394/configrom.h"

#include "libieee1394/ieee1394service.h"
#include "libavc/util/avc_serialize.h"

#include <sstream>

using namespace AVC;

namespace BeBoB {

Plug::Plug( AVC::Unit* unit,
            AVC::Subunit* subunit,
            AVC::function_block_type_t functionBlockType,
            AVC::function_block_type_t functionBlockId,
            AVC::Plug::EPlugAddressType plugAddressType,
            AVC::Plug::EPlugDirection plugDirection,
            AVC::plug_id_t plugId )
    : AVC::Plug( unit,
                 subunit,
                 functionBlockType,
                 functionBlockId,
                 plugAddressType,
                 plugDirection,
                 plugId )
{
    debugOutput( DEBUG_LEVEL_VERBOSE,
                 "nodeId = %d, subunitType = %d, "
                 "subunitId = %d, functionBlockType = %d, "
                 "functionBlockId = %d, addressType = %d, "
                 "direction = %d, id = %d\n",
                 unit->getConfigRom().getNodeId(),
                 getSubunitType(),
                 getSubunitId(),
                 functionBlockType,
                 functionBlockId,
                 plugAddressType,
                 plugDirection,
                 plugId );
}

Plug::Plug( const Plug& rhs )
    : AVC::Plug( rhs )
{

}

Plug::Plug()
    : AVC::Plug()
{
}

Plug::~Plug()
{

}

bool
Plug::discover()
{
    if ( !discoverPlugType() ) {
        debugError( "discover: Could not discover plug type (%d,%d,%d,%d,%d)\n",
                    m_unit->getConfigRom().getNodeId(), getSubunitType(), getSubunitId(), m_direction, m_id );
        return false;
    }

    if ( !discoverName() ) {
        debugError( "Could not discover name (%d,%d,%d,%d,%d)\n",
                    m_unit->getConfigRom().getNodeId(), getSubunitType(), getSubunitId(), m_direction, m_id );
        return false;
    }

    if ( !discoverNoOfChannels() ) {
        debugError( "Could not discover number of channels "
                    "(%d,%d,%d,%d,%d)\n",
                    m_unit->getConfigRom().getNodeId(), getSubunitType(), getSubunitId(), m_direction, m_id );
        return false;
    }

    if ( !discoverChannelPosition() ) {
        debugError( "Could not discover channel positions "
                    "(%d,%d,%d,%d,%d)\n",
                    m_unit->getConfigRom().getNodeId(), getSubunitType(), getSubunitId(), m_direction, m_id );
        return false;
    }

    if ( !discoverChannelName() ) {
        debugError( "Could not discover channel name "
                    "(%d,%d,%d,%d,%d)\n",
                    m_unit->getConfigRom().getNodeId(), getSubunitType(), getSubunitId(), m_direction, m_id );
        return false;
    }

    if ( !discoverClusterInfo() ) {
        debugError( "Could not discover channel name "
                    "(%d,%d,%d,%d,%d)\n",
                    m_unit->getConfigRom().getNodeId(), getSubunitType(), getSubunitId(), m_direction, m_id );
        return false;
    }

    if ( !discoverStreamFormat() ) {
        debugError( "Could not discover stream format "
                    "(%d,%d,%d,%d,%d)\n",
                    m_unit->getConfigRom().getNodeId(), getSubunitType(), getSubunitId(), m_direction, m_id );
        return false;
    }

    if ( !discoverSupportedStreamFormats() ) {
        debugError( "Could not discover supported stream formats "
                    "(%d,%d,%d,%d,%d)\n",
                    m_unit->getConfigRom().getNodeId(), getSubunitType(), getSubunitId(), m_direction, m_id );
        return false;
    }

    return m_unit->getPlugManager().addPlug( *this );
}

bool
Plug::discoverConnections()
{
    return discoverConnectionsInput() && discoverConnectionsOutput();
}

bool
Plug::discoverPlugType()
{
    ExtendedPlugInfoCmd extPlugInfoCmd = setPlugAddrToPlugInfoCmd();
    ExtendedPlugInfoInfoType extendedPlugInfoInfoType(
        ExtendedPlugInfoInfoType::eIT_PlugType );
    extendedPlugInfoInfoType.initialize();
    extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );
    extPlugInfoCmd.setVerbose( getDebugLevel() );

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
Plug::discoverName()
{
    ExtendedPlugInfoCmd extPlugInfoCmd = setPlugAddrToPlugInfoCmd();
    ExtendedPlugInfoInfoType extendedPlugInfoInfoType(
        ExtendedPlugInfoInfoType::eIT_PlugName );
    extendedPlugInfoInfoType.initialize();
    extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );
    extPlugInfoCmd.setVerbose( getDebugLevel() );

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
Plug::discoverNoOfChannels()
{
    ExtendedPlugInfoCmd extPlugInfoCmd = setPlugAddrToPlugInfoCmd();
    //extPlugInfoCmd.setVerbose( true );
    ExtendedPlugInfoInfoType extendedPlugInfoInfoType(
        ExtendedPlugInfoInfoType::eIT_NoOfChannels );
    extendedPlugInfoInfoType.initialize();
    extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );
    extPlugInfoCmd.setVerbose( getDebugLevel() );

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
Plug::discoverChannelPosition()
{
    ExtendedPlugInfoCmd extPlugInfoCmd = setPlugAddrToPlugInfoCmd();
    ExtendedPlugInfoInfoType extendedPlugInfoInfoType(
        ExtendedPlugInfoInfoType::eIT_ChannelPosition );
    extendedPlugInfoInfoType.initialize();
    extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );
    extPlugInfoCmd.setVerbose( getDebugLevel() );

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
Plug::discoverChannelName()
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
            extPlugInfoCmd.setVerbose( getDebugLevel() );

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
Plug::discoverClusterInfo()
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
        extPlugInfoCmd.setVerbose( getDebugLevel() );

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

// NOTE: currently in the generic AV/C unit
// bool
// Plug::discoverStreamFormat()
// {
//     ExtendedStreamFormatCmd extStreamFormatCmd =
//         setPlugAddrToStreamFormatCmd( ExtendedStreamFormatCmd::eSF_ExtendedStreamFormatInformationCommand );
//     extStreamFormatCmd.setVerbose( getDebugLevel() );
// 
//     if ( !extStreamFormatCmd.fire() ) {
//         debugError( "stream format command failed\n" );
//         return false;
//     }
// 
//     if ( ( extStreamFormatCmd.getStatus() ==  ExtendedStreamFormatCmd::eS_NoStreamFormat )
//          || ( extStreamFormatCmd.getStatus() ==  ExtendedStreamFormatCmd::eS_NotUsed ) )
//     {
//         debugOutput( DEBUG_LEVEL_VERBOSE,
//                      "No stream format information available\n" );
//         return true;
//     }
// 
//     if ( !extStreamFormatCmd.getFormatInformation() ) {
//         debugWarning( "No stream format information for plug found -> skip\n" );
//         return true;
//     }
// 
//     if ( extStreamFormatCmd.getFormatInformation()->m_root
//            != FormatInformation::eFHR_AudioMusic  )
//     {
//         debugWarning( "Format hierarchy root is not Audio&Music -> skip\n" );
//         return true;
//     }
// 
//     FormatInformation* formatInfo =
//         extStreamFormatCmd.getFormatInformation();
//     FormatInformationStreamsCompound* compoundStream
//         = dynamic_cast< FormatInformationStreamsCompound* > (
//             formatInfo->m_streams );
//     if ( compoundStream ) {
//         m_samplingFrequency =
//             compoundStream->m_samplingFrequency;
//         debugOutput( DEBUG_LEVEL_VERBOSE,
//                      "%s plug %d uses "
//                      "sampling frequency %d, nr of stream infos = %d\n",
//                      getName(),
//                      m_id,
//                      m_samplingFrequency,
//                      compoundStream->m_numberOfStreamFormatInfos );
// 
//         for ( int i = 1;
//               i <= compoundStream->m_numberOfStreamFormatInfos;
//               ++i )
//         {
//             ClusterInfo* clusterInfo =
//                 const_cast<ClusterInfo*>( getClusterInfoByIndex( i ) );
// 
//             if ( !clusterInfo ) {
//                 debugError( "No matching cluster "
//                             "info found for index %d\n",  i );
//                     return false;
//             }
//             StreamFormatInfo* streamFormatInfo =
//                 compoundStream->m_streamFormatInfos[ i - 1 ];
// 
//             debugOutput( DEBUG_LEVEL_VERBOSE,
//                          "number of channels = %d, stream format = %d\n",
//                          streamFormatInfo->m_numberOfChannels,
//                          streamFormatInfo->m_streamFormat );
// 
//             int nrOfChannels = clusterInfo->m_nrOfChannels;
//             if ( streamFormatInfo->m_streamFormat ==
//                  FormatInformation::eFHL2_AM824_MIDI_CONFORMANT )
//             {
//                 // 8 logical midi channels fit into 1 channel
//                 nrOfChannels = ( ( nrOfChannels + 7 ) / 8 );
//             }
//             // sanity check
//             if ( nrOfChannels != streamFormatInfo->m_numberOfChannels )
//             {
//                 debugWarning( "Number of channels "
//                               "mismatch: '%s' plug discovering reported "
//                               "%d channels for cluster '%s', while stream "
//                               "format reported %d\n",
//                               getName(),
//                               nrOfChannels,
//                               clusterInfo->m_name.c_str(),
//                               streamFormatInfo->m_numberOfChannels);
//             }
//             clusterInfo->m_streamFormat = streamFormatInfo->m_streamFormat;
// 
//             debugOutput( DEBUG_LEVEL_VERBOSE,
//                          "%s plug %d cluster info %d ('%s'): "
//                          "stream format %d\n",
//                          getName(),
//                          m_id,
//                          i,
//                          clusterInfo->m_name.c_str(),
//                          clusterInfo->m_streamFormat );
//         }
//     }
// 
//     FormatInformationStreamsSync* syncStream
//         = dynamic_cast< FormatInformationStreamsSync* > (
//             formatInfo->m_streams );
//     if ( syncStream ) {
//         m_samplingFrequency =
//             syncStream->m_samplingFrequency;
//         debugOutput( DEBUG_LEVEL_VERBOSE,
//                      "%s plug %d is sync stream with sampling frequency %d\n",
//                      getName(),
//                      m_id,
//                      m_samplingFrequency );
//     }
// 
// 
//     if ( !compoundStream && !syncStream )
//     {
//         debugError( "Unsupported stream format\n" );
//         return false;
//     }
// 
//     return true;
// }
// 
// bool
// Plug::discoverSupportedStreamFormats()
// {
//     ExtendedStreamFormatCmd extStreamFormatCmd =
//         setPlugAddrToStreamFormatCmd(
//             ExtendedStreamFormatCmd::eSF_ExtendedStreamFormatInformationCommandList);
//     extStreamFormatCmd.setVerbose( getDebugLevel() );
// 
//     int i = 0;
//     bool cmdSuccess = false;
// 
//     do {
//         extStreamFormatCmd.setIndexInStreamFormat( i );
//         extStreamFormatCmd.setCommandType( AVCCommand::eCT_Status );
//         cmdSuccess = extStreamFormatCmd.fire();
//         if ( cmdSuccess
//              && ( extStreamFormatCmd.getResponse()
//                   == AVCCommand::eR_Implemented ) )
//         {
//             FormatInfo formatInfo;
//             formatInfo.m_index = i;
//             bool formatInfoIsValid = true;
// 
//             FormatInformationStreamsSync* syncStream
//                 = dynamic_cast< FormatInformationStreamsSync* >
//                 ( extStreamFormatCmd.getFormatInformation()->m_streams );
//             if ( syncStream ) {
//                 formatInfo.m_samplingFrequency =
//                     syncStream->m_samplingFrequency;
//                 formatInfo.m_isSyncStream = true ;
//             }
// 
//             FormatInformationStreamsCompound* compoundStream
//                 = dynamic_cast< FormatInformationStreamsCompound* >
//                 ( extStreamFormatCmd.getFormatInformation()->m_streams );
//             if ( compoundStream ) {
//                 formatInfo.m_samplingFrequency =
//                     compoundStream->m_samplingFrequency;
//                 formatInfo.m_isSyncStream = false;
//                 for ( int j = 0;
//                       j < compoundStream->m_numberOfStreamFormatInfos;
//                       ++j )
//                 {
//                     switch ( compoundStream->m_streamFormatInfos[j]->m_streamFormat ) {
//                     case AVC1394_STREAM_FORMAT_AM824_IEC60968_3:
//                         formatInfo.m_audioChannels +=
//                             compoundStream->m_streamFormatInfos[j]->m_numberOfChannels;
//                         break;
//                     case AVC1394_STREAM_FORMAT_AM824_MULTI_BIT_LINEAR_AUDIO_RAW:
//                         formatInfo.m_audioChannels +=
//                             compoundStream->m_streamFormatInfos[j]->m_numberOfChannels;
//                         break;
//                     case AVC1394_STREAM_FORMAT_AM824_MIDI_CONFORMANT:
//                         formatInfo.m_midiChannels +=
//                             compoundStream->m_streamFormatInfos[j]->m_numberOfChannels;
//                         break;
//                     default:
//                         formatInfoIsValid = false;
//                         debugWarning("unknown stream format (0x%02x) for channel "
//                                       "(%d)\n",
//                                      compoundStream->m_streamFormatInfos[j]->m_streamFormat,
//                                      j );
//                     }
//                 }
//             }
// 
//             if ( formatInfoIsValid ) {
//                 flushDebugOutput();
//                 debugOutput( DEBUG_LEVEL_VERBOSE,
//                              "[%s:%d] formatInfo[%d].m_samplingFrequency "
//                              "= %d\n",
//                              getName(), m_id,
//                              i, formatInfo.m_samplingFrequency );
//                 debugOutput( DEBUG_LEVEL_VERBOSE,
//                              "[%s:%d] formatInfo[%d].m_isSyncStream = %d\n",
//                              getName(), m_id,
//                              i, formatInfo.m_isSyncStream );
//                 debugOutput( DEBUG_LEVEL_VERBOSE,
//                              "[%s:%d] formatInfo[%d].m_audioChannels = %d\n",
//                              getName(), m_id,
//                              i, formatInfo.m_audioChannels );
//                 debugOutput( DEBUG_LEVEL_VERBOSE,
//                              "[%s:%d] formatInfo[%d].m_midiChannels = %d\n",
//                              getName(), m_id,
//                              i, formatInfo.m_midiChannels );
//                 m_formatInfos.push_back( formatInfo );
//                 flushDebugOutput();
//             }
//         }
// 
//         ++i;
//     } while ( cmdSuccess && ( extStreamFormatCmd.getResponse()
//                               == ExtendedStreamFormatCmd::eR_Implemented ) );
// 
//     return true;
// }


bool
Plug::discoverConnectionsInput()
{
    ExtendedPlugInfoCmd extPlugInfoCmd = setPlugAddrToPlugInfoCmd();
    ExtendedPlugInfoInfoType extendedPlugInfoInfoType(
        ExtendedPlugInfoInfoType::eIT_PlugInput );
    extendedPlugInfoInfoType.initialize();
    extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );
    extPlugInfoCmd.setVerbose( getDebugLevel() );

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
Plug::discoverConnectionsOutput()
{
    ExtendedPlugInfoCmd extPlugInfoCmd = setPlugAddrToPlugInfoCmd();
    ExtendedPlugInfoInfoType extendedPlugInfoInfoType(
        ExtendedPlugInfoInfoType::eIT_PlugOutput );
    extendedPlugInfoInfoType.initialize();
    extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );
    extPlugInfoCmd.setVerbose( getDebugLevel() );

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

}
