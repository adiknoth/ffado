/*
 * Copyright (C) 2005-2008 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) version 3 of the License.
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

#include "bebob/bebob_avplug.h"
#include "bebob/bebob_avdevice.h"
#include "libieee1394/configrom.h"

#include "libieee1394/ieee1394service.h"
#include "libutil/cmd_serialize.h"

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
Plug::copyClusterInfo(ExtendedPlugInfoPlugChannelPositionSpecificData&
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
                extPlugSpChannelInfo->m_streamPosition-1;
            // FIXME: this can only become a mess with the two meanings
            //        of the location parameter. the audio style meaning
            //        starts from 1, the midi style meaning from 0
            //        lucky for us we recalculate this for the midi channels
            //        and don't use this value.
            channelInfo.m_location =
                extPlugSpChannelInfo->m_location;

            clusterInfo.m_channelInfos.push_back( channelInfo );
        }
        m_clusterInfos.push_back( clusterInfo );
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
                    channelInfo->m_streamPosition + 1;
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

ExtendedPlugInfoCmd
Plug::setPlugAddrToPlugInfoCmd()
{
    ExtendedPlugInfoCmd extPlugInfoCmd( m_unit->get1394Service() );

    switch( getSubunitType() ) {
    case eST_Unit:
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
    case eST_Music:
    case eST_Audio:
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

    extPlugInfoCmd.setNodeId( m_unit->getConfigRom().getNodeId() );
    extPlugInfoCmd.setCommandType( AVCCommand::eCT_Status );
    extPlugInfoCmd.setSubunitId( getSubunitId() );
    extPlugInfoCmd.setSubunitType( getSubunitType() );

    return extPlugInfoCmd;
}

}
