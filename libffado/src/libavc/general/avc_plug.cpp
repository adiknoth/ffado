/*
 * Copyright (C) 2005-2008 by Pieter Palmers
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

#include "avc_plug.h"
#include "avc_unit.h"
#include "avc_signal_format.h"

#include "avc_extended_plug_info.h"

#include "libieee1394/configrom.h"

#include "libieee1394/ieee1394service.h"
#include "libutil/cmd_serialize.h"

#include <cstdio>
#include <sstream>

namespace AVC {

IMPL_DEBUG_MODULE( Plug, Plug, DEBUG_LEVEL_NORMAL );
IMPL_DEBUG_MODULE( PlugManager, PlugManager, DEBUG_LEVEL_NORMAL );

Plug::Plug( Unit* unit,
            Subunit* subunit,
            function_block_type_t functionBlockType,
            function_block_id_t functionBlockId,
            EPlugAddressType plugAddressType,
            EPlugDirection plugDirection,
            plug_id_t plugId )
    : m_unit( unit )
    , m_subunit( subunit )
    , m_functionBlockType( functionBlockType )
    , m_functionBlockId( functionBlockId )
    , m_addressType( plugAddressType )
    , m_direction( plugDirection )
    , m_id( plugId )
    , m_infoPlugType( eAPT_Unknown )
    , m_nrOfChannels( 0 )
    , m_globalId( unit->getPlugManager().requestNewGlobalId() )
{
    debugOutput( DEBUG_LEVEL_VERBOSE,
                 "nodeId = %d, subunitType = %d, "
                 "subunitId = %d, functionBlockType = %d, "
                 "functionBlockId = %d, addressType = %d, "
                 "direction = %d, id = %d\n",
                 m_unit->getConfigRom().getNodeId(),
                 getSubunitType(),
                 getSubunitId(),
                 m_functionBlockType,
                 m_functionBlockId,
                 m_addressType,
                 m_direction,
                 m_id );
}

Plug::Plug( Unit* unit,
            Subunit* subunit,
            function_block_type_t functionBlockType,
            function_block_id_t functionBlockId,
            EPlugAddressType plugAddressType,
            EPlugDirection plugDirection,
            plug_id_t plugId,
            int globalId )
    : m_unit( unit )
    , m_subunit( subunit )
    , m_functionBlockType( functionBlockType )
    , m_functionBlockId( functionBlockId )
    , m_addressType( plugAddressType )
    , m_direction( plugDirection )
    , m_id( plugId )
    , m_infoPlugType( eAPT_Unknown )
    , m_nrOfChannels( 0 )
{
    if(globalId < 0) {
        m_globalId = unit->getPlugManager().requestNewGlobalId();
    } else {
        m_globalId = globalId;
    }
    debugOutput( DEBUG_LEVEL_VERBOSE,
                 "nodeId = %d, subunitType = %d, "
                 "subunitId = %d, functionBlockType = %d, "
                 "functionBlockId = %d, addressType = %d, "
                 "direction = %d, id = %d\n",
                 m_unit->getConfigRom().getNodeId(),
                 getSubunitType(),
                 getSubunitId(),
                 m_functionBlockType,
                 m_functionBlockId,
                 m_addressType,
                 m_direction,
                 m_id );
}

Plug::Plug( const Plug& rhs )
    : m_unit ( rhs.m_unit )
    , m_subunit ( rhs.m_subunit )
    , m_subunitType ( rhs.m_subunitType )
    , m_subunitId ( rhs.m_subunitId )
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
    , m_globalId( rhs.m_globalId )
{
    if ( getDebugLevel() ) {
        setDebugLevel( DEBUG_LEVEL_VERBOSE );
     }
}

Plug::Plug()
    : m_unit( NULL )
    , m_subunit( NULL )
    , m_functionBlockType( 0 )
    , m_functionBlockId( 0 )
    , m_addressType( eAPA_Undefined )
    , m_direction( eAPD_Unknown )
    , m_id( 0 )
    , m_infoPlugType( eAPT_Unknown )
    , m_nrOfChannels( 0 )
    , m_globalId( 0 )
{
}

Plug::~Plug()
{
    m_unit->getPlugManager().remPlug( *this );
}

void
Plug::setVerboseLevel(int l)
{
    setDebugLevel(l);
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Setting verbose level to %d...\n", l );
}

ESubunitType
Plug::getSubunitType() const
{
    return (m_subunit==NULL?eST_Unit:m_subunit->getSubunitType());
}

subunit_id_t
Plug::getSubunitId() const
{
    return (m_subunit==NULL?0xFF:m_subunit->getSubunitId());
}

bool
Plug::discover()
{

    if ( !initFromDescriptor() ) {
        debugOutput(DEBUG_LEVEL_NORMAL,
                    "discover: Could not init plug from descriptor (%d,%d,%d,%d,%d)\n",
                    m_unit->getConfigRom().getNodeId(), getSubunitType(), getSubunitId(), m_direction, m_id );
//         return false;
    }

    if ( !discoverPlugType() ) {
        debugOutput(DEBUG_LEVEL_NORMAL, 
                    "discover: Could not discover plug type (%d,%d,%d,%d,%d)\n",
                    m_unit->getConfigRom().getNodeId(), getSubunitType(), getSubunitId(), m_direction, m_id );
        return false;
    }

    if ( !discoverName() ) {
        debugOutput(DEBUG_LEVEL_NORMAL, 
                    "Could not discover name (%d,%d,%d,%d,%d)\n",
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
        debugOutput(DEBUG_LEVEL_NORMAL, 
                    "Could not discover channel positions "
                    "(%d,%d,%d,%d,%d)\n",
                    m_unit->getConfigRom().getNodeId(), getSubunitType(), getSubunitId(), m_direction, m_id );
        return false;
    }

    if ( !discoverChannelName() ) {
        debugOutput(DEBUG_LEVEL_NORMAL, 
                    "Could not discover channel name "
                    "(%d,%d,%d,%d,%d)\n",
                    m_unit->getConfigRom().getNodeId(), getSubunitType(), getSubunitId(), m_direction, m_id );
        return false;
    }

    if ( !discoverClusterInfo() ) {
        debugOutput(DEBUG_LEVEL_NORMAL, 
                    "Could not discover cluster info "
                    "(%d,%d,%d,%d,%d)\n",
                    m_unit->getConfigRom().getNodeId(), getSubunitType(), getSubunitId(), m_direction, m_id );
        return false;
    }

    if ( !discoverStreamFormat() ) {
        debugOutput(DEBUG_LEVEL_NORMAL, 
                    "Could not discover stream format "
                    "(%d,%d,%d,%d,%d)\n",
                    m_unit->getConfigRom().getNodeId(), getSubunitType(), getSubunitId(), m_direction, m_id );
//         return false;
    }

    // adi@2011-1-14: short-circuit left-to-right evaluation, so only try to
    // discover the supported stream formats when not on a sync channel.
    // According to Holger Dehnhardt, this is required to make his Mackie
    // Onyx work.
    if ( (m_infoPlugType != AVC::Plug::eAPT_Sync) && !discoverSupportedStreamFormats() ) {
        debugOutput(DEBUG_LEVEL_NORMAL, 
                    "Could not discover supported stream formats "
                    "(%d,%d,%d,%d,%d)\n",
                    m_unit->getConfigRom().getNodeId(), getSubunitType(), getSubunitId(), m_direction, m_id );
//         return false;
    }

    return m_unit->getPlugManager().addPlug( *this );
}

bool
Plug::initFromDescriptor()
{
    if(getSubunitType()==eST_Unit) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Not loading unit plug from descriptor.\n");
        return true;
    } else {
        return m_subunit->initPlugFromDescriptor(*this);
    }
}

bool
Plug::discoverConnections()
{
    return discoverConnectionsInput() && discoverConnectionsOutput();
}

bool
Plug::discoverPlugType()
{
    return true;
}

bool
Plug::discoverName()
{
    // name already set
    if (m_name != "") return true;

    m_name = plugAddressTypeToString(getPlugAddressType());
    m_name += " ";
    m_name += plugTypeToString(getPlugType());
    m_name += " ";
    m_name += plugDirectionToString(getPlugDirection());

    return true;
}

bool
Plug::discoverNoOfChannels()
{
    if (m_nrOfChannels == 0) {
        // not discovered yet, get from ext stream format
        ExtendedStreamFormatCmd extStreamFormatCmd =
            setPlugAddrToStreamFormatCmd( ExtendedStreamFormatCmd::eSF_ExtendedStreamFormatInformationCommand );
        extStreamFormatCmd.setVerbose( getDebugLevel() );
    
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
            debugOutput(DEBUG_LEVEL_NORMAL, "No stream format information for plug found -> skip\n" );
            return true;
        }
    
        if ( extStreamFormatCmd.getFormatInformation()->m_root
            != FormatInformation::eFHR_AudioMusic  )
        {
            debugOutput(DEBUG_LEVEL_NORMAL, "Format hierarchy root is not Audio&Music -> skip\n" );
            return true;
        }
    
        FormatInformation* formatInfo =
            extStreamFormatCmd.getFormatInformation();
        FormatInformationStreamsCompound* compoundStream
            = dynamic_cast< FormatInformationStreamsCompound* > (
                formatInfo->m_streams );
        if ( compoundStream ) {
            unsigned int nb_channels = 0;
            for ( int i = 1;
                i <= compoundStream->m_numberOfStreamFormatInfos;
                ++i )
            {
                StreamFormatInfo* streamFormatInfo =
                    compoundStream->m_streamFormatInfos[ i - 1 ];
    
                debugOutput( DEBUG_LEVEL_VERBOSE,
                            "number of channels = %d, stream format = %d\n",
                            streamFormatInfo->m_numberOfChannels,
                            streamFormatInfo->m_streamFormat );
                // FIXME: might not be correct to sum all
                nb_channels += streamFormatInfo->m_numberOfChannels;
            }
            m_nrOfChannels = nb_channels;
        }
        return true;
    } else {
        // already got the nb channels from somewhere else
        return true;
    }
}

bool
Plug::discoverChannelPosition()
{

    return true;
}

bool
Plug::discoverChannelName()
{

    return true;
}

bool
Plug::discoverClusterInfo()
{
    // if there are no cluster info's, we'll have to come up with some
    if(m_clusterInfos.size() == 0) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "fixing up cluster infos\n");
        // we figure out how many channels we have, and build one cluster
        struct ClusterInfo c;
        c.m_index = 1;
        c.m_portType = 0;
        c.m_name = "Unknown";
        c.m_buildSource = -1; // Flag this as a temp build

        c.m_nrOfChannels = m_nrOfChannels;
        for(int i=0; i<m_nrOfChannels; i++) {
            struct ChannelInfo ci;
            ci.m_streamPosition = i;
            ci.m_location = 0xFF;
            ci.m_name = "Unknown";
            c.m_channelInfos.push_back(ci);
        }
        c.m_streamFormat = -1; // filled in later
        
        m_clusterInfos.push_back(c);
    }
    
    return true;
}

bool
Plug::discoverStreamFormat()
{
    ExtendedStreamFormatCmd extStreamFormatCmd =
        setPlugAddrToStreamFormatCmd( ExtendedStreamFormatCmd::eSF_ExtendedStreamFormatInformationCommand );
    extStreamFormatCmd.setVerbose( getDebugLevel() );

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
        debugOutput(DEBUG_LEVEL_NORMAL, "No stream format information for plug found -> skip\n" );
        return true;
    }

    if ( extStreamFormatCmd.getFormatInformation()->m_root
           != FormatInformation::eFHR_AudioMusic  )
    {
        debugOutput(DEBUG_LEVEL_NORMAL, "Format hierarchy root is not Audio&Music -> skip\n" );
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
                debugOutput(DEBUG_LEVEL_NORMAL, 
                            "No matching cluster "
                            "info found for index %d\n",  i );
                    //return false;
            }
            StreamFormatInfo* streamFormatInfo =
                compoundStream->m_streamFormatInfos[ i - 1 ];

            debugOutput( DEBUG_LEVEL_VERBOSE,
                         "number of channels = %d, stream format = %d\n",
                         streamFormatInfo->m_numberOfChannels,
                         streamFormatInfo->m_streamFormat );

            if ( clusterInfo ) {
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
                    debugOutput(DEBUG_LEVEL_NORMAL, 
                                "Number of channels "
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
Plug::discoverSupportedStreamFormats()
{
    ExtendedStreamFormatCmd extStreamFormatCmd =
        setPlugAddrToStreamFormatCmd(
            ExtendedStreamFormatCmd::eSF_ExtendedStreamFormatInformationCommandList);
    extStreamFormatCmd.setVerbose( getDebugLevel() );

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
                    case AVC1394_STREAM_FORMAT_AM824_IEC60958_3:
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
                flushDebugOutput();
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
                flushDebugOutput();
            }
        }

        ++i;
    } while ( cmdSuccess && ( extStreamFormatCmd.getResponse()
                              == ExtendedStreamFormatCmd::eR_Implemented ) );

    return true;
}

bool
Plug::discoverConnectionsInput()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Discovering incoming connections...\n");

    m_inputConnections.clear();
    int sourcePlugGlobalId=getSignalSource();

    if(sourcePlugGlobalId >= 0) {
        Plug *p=m_unit->getPlugManager().getPlug(sourcePlugGlobalId);
        if (p==NULL) {
            debugError( "Plug with global id %d not found\n",sourcePlugGlobalId );
            return false;
        }
        // connected to another plug
        debugOutput( DEBUG_LEVEL_VERBOSE, "Plug '%s' gets signal from '%s'...\n",
            getName(), p->getName() );



        if ( p ) {
            debugOutput( DEBUG_LEVEL_VERBOSE,
                        "'(%d) %s' has a connection to '(%d) %s'\n",
                        getGlobalId(),
                        getName(),
                        p->getGlobalId(),
                        p->getName() );
            addPlugConnection( m_inputConnections, *p );
        } else {
            debugError( "no corresponding plug found for '(%d) %s'\n",
                        getGlobalId(),
                        getName() );
            return false;
        }

    }

    return true;
}

bool
Plug::discoverConnectionsOutput()
{
    return true;
}

bool
Plug::inquireConnnection( Plug& plug )
{
    SignalSourceCmd signalSourceCmd = setSrcPlugAddrToSignalCmd();
    setDestPlugAddrToSignalCmd( signalSourceCmd, plug );
    signalSourceCmd.setCommandType( AVCCommand::eCT_SpecificInquiry );
    signalSourceCmd.setVerbose( getDebugLevel() );

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

int
Plug::getSignalSource()
{
    if((getPlugAddressType() == eAPA_PCR) ||
       (getPlugAddressType() == eAPA_ExternalPlug)) {
        if (getPlugDirection() != eAPD_Output) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Signal Source command not valid for non-output unit plugs...\n");
            return -1;
        }
    }

    if(getPlugAddressType() == eAPA_SubunitPlug) {
        if (getPlugDirection() != eAPD_Input) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Signal Source command not valid for non-input subunit plugs...\n");
            return -1;
        }
    }

    SignalSourceCmd signalSourceCmd( m_unit->get1394Service() );

    signalSourceCmd.setNodeId( m_unit->getConfigRom().getNodeId() );
    signalSourceCmd.setSubunitType( eST_Unit  );
    signalSourceCmd.setSubunitId( 0xff );

    SignalSubunitAddress signalSubunitAddr;
    signalSubunitAddr.m_subunitType = 0xFF;
    signalSubunitAddr.m_subunitId = 0xFF;
    signalSubunitAddr.m_plugId = 0xFE;
    signalSourceCmd.setSignalSource( signalSubunitAddr );

    setDestPlugAddrToSignalCmd( signalSourceCmd, *this );

    signalSourceCmd.setCommandType( AVCCommand::eCT_Status );
    signalSourceCmd.setVerbose( getDebugLevel() );

    if ( !signalSourceCmd.fire() ) {
        debugError( "Could not get signal source for '%s'\n",
                    getName() );
        return -1;
    }

    if ( signalSourceCmd.getResponse() == AVCCommand::eR_Implemented ) {
        SignalAddress* src=signalSourceCmd.getSignalSource();
        Plug* p=NULL;
        if(dynamic_cast<SignalUnitAddress *>(src)) {
            SignalUnitAddress *usrc=dynamic_cast<SignalUnitAddress *>(src);
            if(usrc->m_plugId == 0xFE) {
                debugOutput(DEBUG_LEVEL_VERBOSE, "No/Invalid connection...\n");
                return -1;
            } else if (usrc->m_plugId & 0x80) {
                p=m_unit->getPlugManager().getPlug( eST_Unit, 0xFF,
                        0xFF, 0xFF, eAPA_ExternalPlug, eAPD_Input,
                        usrc->m_plugId & 0x7F );
            } else {
                p=m_unit->getPlugManager().getPlug( eST_Unit, 0xFF,
                        0xFF, 0xFF, eAPA_PCR, eAPD_Input,
                        usrc->m_plugId & 0x7F );
            }
        } else if (dynamic_cast<SignalSubunitAddress *>(src)) {
            SignalSubunitAddress *susrc=dynamic_cast<SignalSubunitAddress *>(src);
            if(susrc->m_plugId == 0xFE) {
                debugOutput(DEBUG_LEVEL_VERBOSE, "No/Invalid connection...\n");
                return -1;
            } else {
                p=m_unit->getPlugManager().getPlug( byteToSubunitType(susrc->m_subunitType),
                        susrc->m_subunitId, 0xFF, 0xFF, eAPA_SubunitPlug,
                        eAPD_Output, susrc->m_plugId);
            }
        } else return -1;

        if (p==NULL) {
            debugError("reported signal source plug not found for '%s'\n",
                       getName());
            return -1;
        }

        return p->getGlobalId();
    }

    return -1;
}

bool
Plug::setConnection( Plug& plug )
{
    SignalSourceCmd signalSourceCmd = setSrcPlugAddrToSignalCmd();
    setDestPlugAddrToSignalCmd( signalSourceCmd, plug );
    signalSourceCmd.setCommandType( AVCCommand::eCT_Control );
    signalSourceCmd.setVerbose( getDebugLevel() );

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

bool
Plug::propagateFromConnectedPlug( ) {

    if (getDirection() == eAPD_Output) {
        if (getInputConnections().size()==0) {
            debugOutput(DEBUG_LEVEL_NORMAL, "No input connections to propagate from, skipping.\n");
            return true;
        }
        if (getInputConnections().size()>1) {
            debugOutput(DEBUG_LEVEL_NORMAL, "Too many input connections to propagate from, using first one.\n");
        }

        Plug* p = *(getInputConnections().begin());
        return propagateFromPlug( p );

    } else if (getDirection() == eAPD_Input) {
        if (getOutputConnections().size()==0) {
            debugOutput(DEBUG_LEVEL_NORMAL, "No output connections to propagate from, skipping.\n");
            return true;
        }
        if (getOutputConnections().size()>1) {
            debugOutput(DEBUG_LEVEL_NORMAL, "Too many output connections to propagate from, using first one.\n");
        }

        Plug* p = *(getOutputConnections().begin());
        return propagateFromPlug( p );

    } else {
        debugError("plug with undefined direction\n");
        return false;
    }
}

bool
Plug::propagateFromPlug( Plug *p ) {
    debugOutput( DEBUG_LEVEL_VERBOSE,
                 "Propagating info from plug '%s' to plug '%s'\n",
                 p->getName(), getName() );

    if (m_clusterInfos.size()==0 || m_clusterInfos[0].m_buildSource == -1) {
        m_clusterInfos=p->m_clusterInfos;

    	if (m_clusterInfos.size() > 0) {
            m_clusterInfos[0].m_buildSource = 0; // No longer a temp instance
	}
    }

    m_nrOfChannels=p->m_nrOfChannels;

    return true;
}

int
Plug::getNrOfStreams() const
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
Plug::getNrOfChannels() const
{
    return m_nrOfChannels;
}

bool
Plug::setNrOfChannels(int i)
{
    m_nrOfChannels=i;
    return true;
}

int
Plug::getSampleRate() const
{
    if(getPlugAddressType()==eAPA_PCR) {
        if(getPlugDirection()==eAPD_Input) {
            InputPlugSignalFormatCmd cmd( m_unit->get1394Service() );
            cmd.m_form=0xFF;
            cmd.m_eoh=0xFF;
            cmd.m_fmt=0xFF;
            cmd.m_plug=getPlugId();

            cmd.setNodeId( m_unit->getConfigRom().getNodeId() );
            cmd.setSubunitType( eST_Unit  );
            cmd.setSubunitId( 0xff );

            cmd.setCommandType( AVCCommand::eCT_Status );

            if ( !cmd.fire() ) {
                debugError( "input plug signal format command failed\n" );
                return 0;
            }

            if (cmd.m_fmt != 0x10 ) {
                debugWarning("Incorrect FMT response received: 0x%02X\n",cmd.m_fmt);
            }

            return fdfSfcToSampleRate(cmd.m_fdf[0]);

        } else if (getPlugDirection()==eAPD_Output) {
            OutputPlugSignalFormatCmd cmd( m_unit->get1394Service() );
            cmd.m_form=0xFF;
            cmd.m_eoh=0xFF;
            cmd.m_fmt=0xFF;
            cmd.m_plug=getPlugId();

            cmd.setNodeId( m_unit->getConfigRom().getNodeId() );
            cmd.setSubunitType( eST_Unit  );
            cmd.setSubunitId( 0xff );

            cmd.setCommandType( AVCCommand::eCT_Status );

            if ( !cmd.fire() ) {
                debugError( "output plug signal format command failed\n" );
                return 0;
            }

            if (cmd.m_fmt != 0x10 ) {
                debugWarning("Incorrect FMT response received: 0x%02X\n",cmd.m_fmt);
            }

            return fdfSfcToSampleRate(cmd.m_fdf[0]);

        } else {
            debugError("PCR plug with undefined direction.\n");
            return 0;
        }
    }

    // fallback
    return convertESamplingFrequency( static_cast<ESamplingFrequency>( m_samplingFrequency ) );
}

bool
Plug::setSampleRate( int rate )
{
    // apple style
    if(getPlugAddressType()==eAPA_PCR) {
        if(getPlugDirection()==eAPD_Input) {
            InputPlugSignalFormatCmd cmd( m_unit->get1394Service() );
            cmd.m_eoh=1;
            cmd.m_form=0;
            cmd.m_fmt=0x10;
            cmd.m_plug=getPlugId();
            cmd.m_fdf[0]=sampleRateToFdfSfc(rate);
            cmd.m_fdf[1]=0xFF;
            cmd.m_fdf[2]=0xFF;

            cmd.setNodeId( m_unit->getConfigRom().getNodeId() );
            cmd.setSubunitType( eST_Unit  );
            cmd.setSubunitId( 0xff );

            cmd.setCommandType( AVCCommand::eCT_Control );

            if ( !cmd.fire() ) {
                debugError( "input plug signal format command failed\n" );
                return false;
            }

            if ( cmd.getResponse() == AVCCommand::eR_Accepted )
            {
                return true;
            }
            debugWarning( "output plug signal format command not accepted\n" );

        } else if (getPlugDirection()==eAPD_Output) {
            OutputPlugSignalFormatCmd cmd( m_unit->get1394Service() );
            cmd.m_eoh=1;
            cmd.m_form=0;
            cmd.m_fmt=0x10;
            cmd.m_plug=getPlugId();
            cmd.m_fdf[0]=sampleRateToFdfSfc(rate);
            cmd.m_fdf[1]=0xFF;
            cmd.m_fdf[2]=0xFF;

            cmd.setNodeId( m_unit->getConfigRom().getNodeId() );
            cmd.setSubunitType( eST_Unit  );
            cmd.setSubunitId( 0xff );

            cmd.setCommandType( AVCCommand::eCT_Control );

            if ( !cmd.fire() ) {
                debugError( "output plug signal format command failed\n" );
                return false;
            }

            if ( cmd.getResponse() == AVCCommand::eR_Accepted )
            {
                return true;
            }
            debugWarning( "output plug signal format command not accepted\n" );
        } else {
            debugError("PCR plug with undefined direction.\n");
            return false;
        }
    }

    // fallback: BeBoB style
    ESamplingFrequency samplingFrequency = parseSampleRate(rate);

    ExtendedStreamFormatCmd extStreamFormatCmd(
        m_unit->get1394Service(),
        ExtendedStreamFormatCmd::eSF_ExtendedStreamFormatInformationCommandList );
    UnitPlugAddress unitPlugAddress( UnitPlugAddress::ePT_PCR,
                                     getPlugId() );

    extStreamFormatCmd.setPlugAddress(
        PlugAddress(
            Plug::convertPlugDirection(getPlugDirection() ),
            PlugAddress::ePAM_Unit,
            unitPlugAddress ) );

    extStreamFormatCmd.setNodeId( m_unit->getConfigRom().getNodeId() );
    extStreamFormatCmd.setCommandType( AVCCommand::eCT_Status );

    int i = 0;
    bool cmdSuccess = false;
    bool correctFormatFound = false;

    do {
        extStreamFormatCmd.setIndexInStreamFormat( i );
        extStreamFormatCmd.setCommandType( AVCCommand::eCT_Status );
        extStreamFormatCmd.setVerbose( getDebugLevel() );

        cmdSuccess = extStreamFormatCmd.fire();

        if ( cmdSuccess
             && ( extStreamFormatCmd.getResponse() ==
                  AVCCommand::eR_Implemented ) )
        {
            ESamplingFrequency foundFreq = eSF_DontCare;

            FormatInformation* formatInfo =
                extStreamFormatCmd.getFormatInformation();
            FormatInformationStreamsCompound* compoundStream
                = dynamic_cast< FormatInformationStreamsCompound* > (
                    formatInfo->m_streams );
            if ( compoundStream ) {
                foundFreq =
                    static_cast< ESamplingFrequency >(
                        compoundStream->m_samplingFrequency );
            }

            FormatInformationStreamsSync* syncStream
                = dynamic_cast< FormatInformationStreamsSync* > (
                    formatInfo->m_streams );
            if ( syncStream ) {
                foundFreq =
                    static_cast< ESamplingFrequency >(
                        syncStream->m_samplingFrequency );
            }

            if ( foundFreq == samplingFrequency )
            {
                correctFormatFound = true;
                break;
            }
        }

        ++i;
    } while ( cmdSuccess
              && ( extStreamFormatCmd.getResponse() ==
                   ExtendedStreamFormatCmd::eR_Implemented ) );

    if ( !cmdSuccess ) {
        debugError( "setSampleRatePlug: Failed to retrieve format info\n" );
        return false;
    }

    if ( !correctFormatFound ) {
        debugError( "setSampleRatePlug: %s plug %d does not support "
                    "sample rate %d\n",
                    getName(),
                    getPlugId(),
                    convertESamplingFrequency( samplingFrequency ) );
        return false;
    }

    extStreamFormatCmd.setSubFunction(
        ExtendedStreamFormatCmd::eSF_ExtendedStreamFormatInformationCommand );
    extStreamFormatCmd.setCommandType( AVCCommand::eCT_Control );
    extStreamFormatCmd.setVerbose( getDebugLevel() );

    if ( !extStreamFormatCmd.fire() ) {
        debugError( "setSampleRate: Could not set sample rate %d "
                    "to %s plug %d\n",
                    convertESamplingFrequency( samplingFrequency ),
                    getName(),
                    getPlugId() );
        return false;
    }

    return true;
}

bool
Plug::supportsSampleRate( int rate )
{
    // fallback: BeBoB style
    ESamplingFrequency samplingFrequency = parseSampleRate(rate);

    ExtendedStreamFormatCmd extStreamFormatCmd(
        m_unit->get1394Service(),
        ExtendedStreamFormatCmd::eSF_ExtendedStreamFormatInformationCommandList );
    UnitPlugAddress unitPlugAddress( UnitPlugAddress::ePT_PCR,
                                     getPlugId() );

    extStreamFormatCmd.setPlugAddress(
        PlugAddress(
            Plug::convertPlugDirection(getPlugDirection() ),
            PlugAddress::ePAM_Unit,
            unitPlugAddress ) );

    extStreamFormatCmd.setNodeId( m_unit->getConfigRom().getNodeId() );
    extStreamFormatCmd.setCommandType( AVCCommand::eCT_Status );

    int i = 0;
    bool cmdSuccess = false;
    bool correctFormatFound = false;

    do {
        extStreamFormatCmd.setIndexInStreamFormat( i );
        extStreamFormatCmd.setCommandType( AVCCommand::eCT_Status );
        extStreamFormatCmd.setVerbose( getDebugLevel() );

        cmdSuccess = extStreamFormatCmd.fire();

        if ( cmdSuccess
             && ( extStreamFormatCmd.getResponse() ==
                  AVCCommand::eR_Implemented ) )
        {
            ESamplingFrequency foundFreq = eSF_DontCare;

            FormatInformation* formatInfo =
                extStreamFormatCmd.getFormatInformation();
            FormatInformationStreamsCompound* compoundStream
                = dynamic_cast< FormatInformationStreamsCompound* > (
                    formatInfo->m_streams );
            if ( compoundStream ) {
                foundFreq =
                    static_cast< ESamplingFrequency >(
                        compoundStream->m_samplingFrequency );
            }

            FormatInformationStreamsSync* syncStream
                = dynamic_cast< FormatInformationStreamsSync* > (
                    formatInfo->m_streams );
            if ( syncStream ) {
                foundFreq =
                    static_cast< ESamplingFrequency >(
                        syncStream->m_samplingFrequency );
            }

            if ( foundFreq == samplingFrequency )
            {
                correctFormatFound = true;
                break;
            }
        }

        ++i;
    } while ( cmdSuccess
              && ( extStreamFormatCmd.getResponse() ==
                   ExtendedStreamFormatCmd::eR_Implemented ) );

    if ( !cmdSuccess ) {
        debugError( "setSampleRatePlug: Failed to retrieve format info\n" );
        return false;
    }

    if ( !correctFormatFound ) {
        debugOutput(DEBUG_LEVEL_VERBOSE,
                    "setSampleRatePlug: %s plug %d does not support sample rate %d\n",
                    getName(),
                    getPlugId(),
                    convertESamplingFrequency( samplingFrequency ) );
        return false;
    }
    return true;
}

bool
Plug::discoverConnectionsFromSpecificData(
    EPlugDirection discoverDirection,
    PlugAddressSpecificData* plugAddress,
    PlugVector& connections )
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

    Plug* plug = getPlugDefinedBySpecificData(
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
Plug::addPlugConnection( PlugVector& connections,
                         Plug& plug )

{
    for ( PlugVector::iterator it = connections.begin();
          it != connections.end();
          ++it )
    {
        Plug* cPlug = *it;
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

SignalSourceCmd
Plug::setSrcPlugAddrToSignalCmd()
{
    SignalSourceCmd signalSourceCmd( m_unit->get1394Service() );

    switch( getSubunitType() ) {
    case eST_Unit:
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
    case eST_Music:
    case eST_Audio:
    {
        SignalSubunitAddress signalSubunitAddr;
        signalSubunitAddr.m_subunitType = getSubunitType();
        signalSubunitAddr.m_subunitId = getSubunitId();
        signalSubunitAddr.m_plugId = m_id;
        signalSourceCmd.setSignalSource( signalSubunitAddr );
    }
    break;
    default:
        debugError( "Unknown subunit type\n" );
    }

    signalSourceCmd.setNodeId( m_unit->getConfigRom().getNodeId() );
    signalSourceCmd.setSubunitType( eST_Unit  );
    signalSourceCmd.setSubunitId( 0xff );

    return signalSourceCmd;
}

void
Plug::setDestPlugAddrToSignalCmd(SignalSourceCmd& signalSourceCmd,
                                 Plug& plug)
{
    switch( plug.getSubunitType() ) {
    case eST_Unit:
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
    case eST_Music:
    case eST_Audio:
    {
        SignalSubunitAddress signalSubunitAddr;
        signalSubunitAddr.m_subunitType = plug.getSubunitType();
        signalSubunitAddr.m_subunitId = plug.getSubunitId();
        signalSubunitAddr.m_plugId = plug.m_id;
        signalSourceCmd.setSignalDestination( signalSubunitAddr );
    }
    break;
    default:
        debugError( "Unknown subunit type\n" );
    }
}

void
Plug::debugOutputClusterInfos( int debugLevel )
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

const Plug::ClusterInfo*
Plug::getClusterInfoByIndex(int index) const
{
    for ( Plug::ClusterInfoVector::const_iterator clit =
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
Plug::convertPlugDirection( EPlugDirection direction )
{
    PlugAddress::EPlugDirection dir;
    switch ( direction ) {
    case Plug::eAPD_Input:
        dir = PlugAddress::ePD_Input;
        break;
    case Plug::eAPD_Output:
        dir = PlugAddress::ePD_Output;
        break;
    default:
        dir = PlugAddress::ePD_Undefined;
    }
    return dir;
}

std::string
Plug::plugAddressTypeToString(enum EPlugAddressType t) {
    switch (t) {
        case eAPA_PCR:
            return string("PCR");
        case eAPA_ExternalPlug:
            return string("External");
        case eAPA_AsynchronousPlug:
            return string("Async");
        case eAPA_SubunitPlug:
            return string("Subunit");
        case eAPA_FunctionBlockPlug:
            return string("Function Block");
        default:
        case eAPA_Undefined:
            return string("Undefined");
    }
}

std::string
Plug::plugTypeToString(enum EPlugType t) {
    switch (t) {
        case eAPT_IsoStream:
            return string("IsoStream");
        case eAPT_AsyncStream:
            return string("AsyncStream");
        case eAPT_Midi:
            return string("MIDI");
        case eAPT_Sync:
            return string("Sync");
        case eAPT_Analog:
            return string("Analog");
        case eAPT_Digital:
            return string("Digital");
        default:
        case eAPT_Unknown:
            return string("Unknown");
    }
}

std::string
Plug::plugDirectionToString(enum EPlugDirection t) {
    switch (t) {
        case eAPD_Input:
            return string("Input");
        case eAPD_Output:
            return string("Output");
        case eAPD_Unknown:
            return string("Unknown");
    }
    return string("ERROR");
}

void
Plug::showPlug() const
{
    #ifdef DEBUG
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
    debugOutput( DEBUG_LEVEL_VERBOSE, "\tIncoming connections from: ");

    for ( PlugVector::const_iterator it = m_inputConnections.begin();
          it != m_inputConnections.end();
          ++it )
    {
        debugOutputShort(DEBUG_LEVEL_VERBOSE, "%s, ", (*it)->getName());
    }
    debugOutputShort(DEBUG_LEVEL_VERBOSE, "\n");

    debugOutput( DEBUG_LEVEL_VERBOSE, "\tOutgoing connections to: ");
    for ( PlugVector::const_iterator it = m_outputConnections.begin();
          it != m_outputConnections.end();
          ++it )
    {
        debugOutputShort(DEBUG_LEVEL_VERBOSE, "%s, ", (*it)->getName());
    }
    debugOutputShort(DEBUG_LEVEL_VERBOSE, "\n");

    debugOutput( DEBUG_LEVEL_VERBOSE, "\tChannel info:\n");
    unsigned int i=0;
    for ( Plug::ClusterInfoVector::const_iterator it = m_clusterInfos.begin();
          it != m_clusterInfos.end();
          ++it )
    {
        const Plug::ClusterInfo* clusterInfo = &( *it );

        debugOutput(DEBUG_LEVEL_VERBOSE, "         Cluster %s (idx=%2d, type=0x%02X, ch=%2d, format=0x%02X)\n",
            clusterInfo->m_name.c_str(), i, clusterInfo->m_portType, clusterInfo->m_nrOfChannels, clusterInfo->m_streamFormat);
        Plug::ChannelInfoVector channelInfos = clusterInfo->m_channelInfos;
        for ( Plug::ChannelInfoVector::const_iterator it = channelInfos.begin();
              it != channelInfos.end();
              ++it )
        {
            const Plug::ChannelInfo* channelInfo = &( *it );
            debugOutput(DEBUG_LEVEL_VERBOSE, "           Channel %s (pos=0x%02X, loc=0x%02X)\n",
                channelInfo->m_name.c_str(), channelInfo->m_streamPosition, channelInfo->m_location);
        }
        i++;
    }
    flushDebugOutput();
    #endif
}


Plug*
Plug::getPlugDefinedBySpecificData(
    UnitPlugSpecificDataPlugAddress* pUnitPlugAddress,
    SubunitPlugSpecificDataPlugAddress* pSubunitPlugAddress,
    FunctionBlockPlugSpecificDataPlugAddress* pFunctionBlockPlugAddress )
{
    subunit_type_t        subunitType       = 0xff;
    subunit_id_t          subunitId         = 0xff;
    function_block_type_t functionBlockType = 0xff;
    function_block_id_t   functionBlockId   = 0xff;
    EPlugAddressType      addressType       = eAPA_Undefined;
    EPlugDirection        direction         = eAPD_Unknown;
    plug_id_t             plugId            = 0xff;

    if ( !pUnitPlugAddress
         && !pSubunitPlugAddress
         && !pFunctionBlockPlugAddress )
    {
        debugError( "No correct specific data found\n" );
        return 0;
    }

    if ( pUnitPlugAddress ) {
        subunitType = eST_Unit;
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

    ESubunitType enumSubunitType =
        static_cast<ESubunitType>( subunitType );

    return m_unit->getPlugManager().getPlug(
        enumSubunitType,
        subunitId,
        functionBlockType,
        functionBlockId,
        addressType,
        direction,
        plugId );
}

Plug::EPlugDirection
Plug::toggleDirection( EPlugDirection direction ) const
{
    EPlugDirection newDirection;
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
Plug::serializeChannelInfos( std::string basePath,
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
        i++;
    }

    return result;
}

bool
Plug::deserializeChannelInfos( std::string basePath,
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
Plug::serializeClusterInfos( std::string basePath,
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
        i++;
    }

    return result;
}

bool
Plug::deserializeClusterInfos( std::string basePath,
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
Plug::serializeFormatInfos( std::string basePath,
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
        i++;
    }
    return result;
}

bool
Plug::deserializeFormatInfos( std::string basePath,
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
Plug::serialize( std::string basePath, Util::IOSerialize& ser ) const
{
    bool result=true;
    result &= ser.write( basePath + "m_subunitType", getSubunitType());
    result &= ser.write( basePath + "m_subunitId", getSubunitId());
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
    result &= serializePlugVector( basePath + "m_inputConnections", ser, m_inputConnections );
    result &= serializePlugVector( basePath + "m_outputConnections", ser, m_outputConnections );
    result &= ser.write( basePath + "m_globalId", m_globalId);

    return result;
}

Plug*
Plug::deserialize( std::string basePath,
                   Util::IODeserialize& deser,
                   Unit& unit,
                   PlugManager& plugManager )
{
    ESubunitType          subunitType;
    subunit_t             subunitId;
    function_block_type_t functionBlockType;
    function_block_id_t   functionBlockId;
    EPlugAddressType      addressType;
    EPlugDirection        direction;
    plug_id_t             id;
    int                   globalId;

    if ( !deser.isExisting( basePath + "m_subunitType" ) ) {
        return 0;
    }

    bool result=true;

    result  = deser.read( basePath + "m_subunitType", subunitType );
    result &= deser.read( basePath + "m_subunitId", subunitId );
    Subunit* subunit = unit.getSubunit( subunitType, subunitId );

    result &= deser.read( basePath + "m_functionBlockType", functionBlockType );
    result &= deser.read( basePath + "m_functionBlockId", functionBlockId );
    result &= deser.read( basePath + "m_addressType", addressType );
    result &= deser.read( basePath + "m_direction", direction );
    result &= deser.read( basePath + "m_id", id );
    result &= deser.read( basePath + "m_globalId", globalId );

    Plug* pPlug = unit.createPlug( &unit, subunit, functionBlockType, 
                                   functionBlockId, addressType, direction, 
                                   id, globalId);
    if ( !pPlug ) {
        return 0;
    }

    // this is needed to allow for the update of the subunit pointer later on
    // in the deserializeUpdateSubunit.
    pPlug->m_subunitType = subunitType;
    pPlug->m_subunitId = subunitId;

    result &= deser.read( basePath + "m_infoPlugType", pPlug->m_infoPlugType );
    result &= deser.read( basePath + "m_nrOfChannels", pPlug->m_nrOfChannels );
    result &= deser.read( basePath + "m_name", pPlug->m_name );
    result &= pPlug->deserializeClusterInfos( basePath + "m_clusterInfos", deser );
    result &= deser.read( basePath + "m_samplingFrequency", pPlug->m_samplingFrequency );
    result &= pPlug->deserializeFormatInfos( basePath + "m_formatInfos", deser );
    // input and output connections can't be processed here because not all plugs might
    // deserialized at this point. so we do that in deserializeUpdate.

    if ( !result ) {
        delete pPlug;
        return 0;
    }

    return pPlug;
}

bool
Plug::deserializeConnections( std::string basePath,
                              Util::IODeserialize& deser )
{
    bool result;

    result  = deserializePlugVector( basePath + "/m_inputConnections", deser,
                                     m_unit->getPlugManager(), m_inputConnections );
    result &= deserializePlugVector( basePath + "/m_outputConnections", deser,
                                     m_unit->getPlugManager(), m_outputConnections );
    return result;
}

bool
Plug::deserializeUpdateSubunit()
{
    m_subunit = m_unit->getSubunit( m_subunitType, m_subunitId );
    return true;
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

const char* avPlugAddressTypeToString( Plug::EPlugAddressType type )
{
    if ( type > ( int )( sizeof( avPlugAddressTypeStrings )
                         / sizeof( avPlugAddressTypeStrings[0] ) ) )
    {
        type = Plug::eAPA_Undefined;
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

const char* avPlugTypeToString( Plug::EPlugType type )
{
    if ( type > ( int )( sizeof( avPlugTypeStrings )
                         / sizeof( avPlugTypeStrings[0] ) ) )
    {
        type = Plug::eAPT_Unknown;
    }
    return avPlugTypeStrings[type];
}

const char* avPlugDirectionStrings[] =
{
    "Input",
    "Output",
    "Unknown",
};

const char* avPlugDirectionToString( Plug::EPlugDirection type )
{
    if ( type > ( int )( sizeof( avPlugDirectionStrings )
                         / sizeof( avPlugDirectionStrings[0] ) ) )
    {
        type = Plug::eAPD_Unknown;
    }
    return avPlugDirectionStrings[type];
}

/////////////////////////////////////


PlugManager::PlugManager(  )
: m_globalIdCounter( 0 )
{

}

PlugManager::PlugManager( const PlugManager& rhs )
: m_globalIdCounter( rhs.m_globalIdCounter )
{
    setDebugLevel( rhs.getDebugLevel() );
}

PlugManager::~PlugManager()
{
}

void
PlugManager::setVerboseLevel( int l )
{
    setDebugLevel(l);
    for ( PlugVector::iterator it = m_plugs.begin();
          it !=  m_plugs.end();
          ++it )
    {
        (*it)->setVerboseLevel(l);
    }
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting verbose level to %d...\n", l );
}

bool
PlugManager::addPlug( Plug& plug )
{
    m_plugs.push_back( &plug );
    // inherit debug level
    plug.setVerboseLevel(getDebugLevel());
    return true;
}

bool
PlugManager::remPlug( Plug& plug )
{
    for ( PlugVector::iterator it = m_plugs.begin();
          it !=  m_plugs.end();
          ++it )
    {
        Plug* plugIt = *it;
        if ( plugIt == &plug ) {
            m_plugs.erase( it );
            return true;
        }
    }
    return false;
}

// helper function
static void addConnection( PlugConnectionVector& connections,
                           Plug& srcPlug,
                           Plug& destPlug )
{
    for ( PlugConnectionVector::iterator it = connections.begin();
          it != connections.end();
          ++it )
    {
        PlugConnection* con = *it;
        if ( ( &( con->getSrcPlug() ) == &srcPlug )
             && ( &( con->getDestPlug() ) == &destPlug ) )
        {
            return;
        }
    }
    connections.push_back( new PlugConnection( srcPlug, destPlug ) );
}

bool
PlugManager::tidyPlugConnections(PlugConnectionVector& connections)
{
    connections.clear();
    for ( PlugVector::const_iterator it = m_plugs.begin();
          it !=  m_plugs.end();
          ++it )
    {
        Plug* plug = *it;
        for ( PlugVector::const_iterator it =
                  plug->getInputConnections().begin();
            it != plug->getInputConnections().end();
            ++it )
        {
            addConnection( connections, *( *it ), *plug );
        }
        plug->getInputConnections().clear();

        for ( PlugVector::const_iterator it =
                  plug->getOutputConnections().begin();
            it != plug->getOutputConnections().end();
            ++it )
        {
            addConnection( connections, *plug, *( *it ) );
        }
        plug->getOutputConnections().clear();
    }

    for ( PlugConnectionVector::iterator it = connections.begin();
          it != connections.end();
          ++it )
    {
        PlugConnection * con = *it;
        con->getSrcPlug().getOutputConnections().push_back(&( con->getDestPlug() ));
        con->getDestPlug().getInputConnections().push_back(&( con->getSrcPlug() ));

    }

    return true;
}

static void addConnectionOwner( PlugConnectionOwnerVector& connections,
                           Plug& srcPlug,
                           Plug& destPlug )
{

    for ( PlugConnectionOwnerVector::iterator it = connections.begin();
          it != connections.end();
          ++it )
    {
        PlugConnection& con = *it;
        if ( ( &( con.getSrcPlug() ) == &srcPlug )
             && ( &( con.getDestPlug() ) == &destPlug ) )
        {
            return;
        }
    }
    connections.push_back( PlugConnection( srcPlug, destPlug ) );
}


void
PlugManager::showPlugs() const
{
    if(getDebugLevel() < DEBUG_LEVEL_INFO) return;

    // \todo The information provided here could be better arranged. For a start it is ok, but
    // there is room for improvement. Something for a lazy sunday afternoon (tip: maybe drink some
    // beer to get into the mood)

    printf( "\nSummary\n" );
    printf( "-------\n\n" );
    printf( "Nr | AddressType     | Direction | SubUnitType | SubUnitId | FunctionBlockType | FunctionBlockId | Id   | Type         |Name\n" );
    printf( "---+-----------------+-----------+-------------+-----------+-------------------+-----------------+------+--------------+------\n" );

    for ( PlugVector::const_iterator it = m_plugs.begin();
          it !=  m_plugs.end();
          ++it )
    {
        Plug* plug = *it;

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

    PlugConnectionOwnerVector connections;

    for ( PlugVector::const_iterator it = m_plugs.begin();
          it !=  m_plugs.end();
          ++it )
    {
        Plug* plug = *it;
        for ( PlugVector::const_iterator it =
                  plug->getInputConnections().begin();
            it != plug->getInputConnections().end();
            ++it )
        {
            addConnectionOwner( connections, *( *it ), *plug );
        }
        for ( PlugVector::const_iterator it =
                  plug->getOutputConnections().begin();
            it != plug->getOutputConnections().end();
            ++it )
        {
            addConnectionOwner( connections, *plug, *( *it ) );
        }
    }

    printf( "digraph avcconnections {\n" );
    for ( PlugConnectionOwnerVector::iterator it = connections.begin();
          it != connections.end();
          ++it )
    {
        PlugConnection& con = *it;
        printf( "\t\"(%d) %s\" -> \"(%d) %s\"\n",
                con.getSrcPlug().getGlobalId(),
                con.getSrcPlug().getName(),
                con.getDestPlug().getGlobalId(),
                con.getDestPlug().getName() );
    }
    for ( PlugVector::const_iterator it = m_plugs.begin();
          it != m_plugs.end();
          ++it )
    {
        Plug* plug = *it;
        if ( plug->getFunctionBlockType() != 0xff ) {
            std::ostringstream strstrm;
            switch(plug->getFunctionBlockType()) {
                case 0x80:
                    strstrm << "Selector FB";
                    break;
                case 0x81:
                    strstrm << "Feature FB";
                    break;
                case 0x82:
                    strstrm << "Processing FB";
                    break;
                case 0x83:
                    strstrm << "CODEC FB";
                    break;
                default:
                    strstrm << plug->getFunctionBlockType();
            }

            if ( plug->getPlugDirection() == Plug::eAPD_Input ) {
                printf( "\t\"(%d) %s\" -> \"(%s, ID %d)\"\n",
                        plug->getGlobalId(),
                        plug->getName(),
                        strstrm.str().c_str(),
                        plug->getFunctionBlockId() );
            } else {
                printf( "\t\"(%s, ID %d)\" -> \t\"(%d) %s\"\n",
                        strstrm.str().c_str(),
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

    for ( PlugVector::const_iterator it = m_plugs.begin();
          it !=  m_plugs.end();
          ++it )
    {
        Plug* plug = *it;
        printf( "\t\"(%d) %s\" [color=%s,style=filled];\n",
                plug->getGlobalId(), plug->getName(),
                colorStrings[plug->getPlugAddressType() ] );
    }

    printf("}\n" );
    printf( "Use \"dot -Tps FILENAME.dot -o FILENAME.ps\" "
            "to generate graph\n");

    debugOutput( DEBUG_LEVEL_VERBOSE, "Plug details\n" );
    debugOutput( DEBUG_LEVEL_VERBOSE, "------------\n" );
    for ( PlugVector::const_iterator it = m_plugs.begin();
          it !=  m_plugs.end();
          ++it )
    {
        Plug* plug = *it;
        debugOutput( DEBUG_LEVEL_VERBOSE, "Plug %d:\n", plug->getGlobalId() );
        plug->showPlug();

    }
}

Plug*
PlugManager::getPlug( ESubunitType subunitType,
                      subunit_id_t subunitId,
                      function_block_type_t functionBlockType,
                      function_block_id_t functionBlockId,
                      Plug::EPlugAddressType plugAddressType,
                      Plug::EPlugDirection plugDirection,
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

    for ( PlugVector::const_iterator it = m_plugs.begin();
          it !=  m_plugs.end();
          ++it )
    {
        Plug* plug = *it;

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

Plug*
PlugManager::getPlug( int iGlobalId ) const
{
    for ( PlugVector::const_iterator it = m_plugs.begin();
          it !=  m_plugs.end();
          ++it )
    {
        Plug* pPlug = *it;
        if ( pPlug->getGlobalId() == iGlobalId ) {
            return pPlug;
        }
    }

    return 0;
}

PlugVector
PlugManager::getPlugsByType( ESubunitType subunitType,
                             subunit_id_t subunitId,
                             function_block_type_t functionBlockType,
                             function_block_id_t functionBlockId,
                             Plug::EPlugAddressType plugAddressType,
                             Plug::EPlugDirection plugDirection,
                             Plug::EPlugType type) const
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

    PlugVector plugVector;
    for ( PlugVector::const_iterator it = m_plugs.begin();
          it !=  m_plugs.end();
          ++it )
    {
        Plug* pPlug = *it;

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
PlugManager::serialize( std::string basePath, Util::IOSerialize& ser ) const
{
    bool result = true;
    int i = 0;
    for ( PlugVector::const_iterator it = m_plugs.begin();
          it !=  m_plugs.end();
          ++it )
    {
        Plug* pPlug = *it;
        std::ostringstream strstrm;
        strstrm << basePath << i;
        result &= pPlug->serialize( strstrm.str() + "/", ser );
        i++;
    }
    result &= ser.write( basePath + "m_globalIdCounter", m_globalIdCounter );

    return result;
}

PlugManager*
PlugManager::deserialize( std::string basePath,
                          Util::IODeserialize& deser,
                          Unit& unit )

{
    PlugManager* pMgr = new PlugManager;

    if ( !pMgr ) {
        return 0;
    }

    if(!deser.read( basePath + "m_globalIdCounter", pMgr->m_globalIdCounter )) {
        pMgr->m_globalIdCounter = 0;
    }

    int i = 0;
    Plug* pPlug = 0;
    do {
        std::ostringstream strstrm;
        strstrm << basePath << i;
        // unit still holds a null pointer for the plug manager
        // therefore we have to *this as additional argument
        pPlug = Plug::deserialize( strstrm.str() + "/",
                                   deser,
                                   unit,
                                   *pMgr );
        if ( pPlug ) {
            pMgr->m_plugs.push_back( pPlug );
            i++;
        }
    } while ( pPlug );

    return pMgr;
}

bool
PlugManager::deserializeUpdate( std::string basePath,
                                Util::IODeserialize& deser)
{
    bool result = true;

    for ( PlugVector::const_iterator it = m_plugs.begin();
          it !=  m_plugs.end();
          ++it )
    {

        Plug* pPlug = *it;

        std::ostringstream strstrm;
        strstrm << basePath << "Plug" << pPlug->getGlobalId();

        result &= pPlug->deserializeConnections( strstrm.str(), deser );
        result &= pPlug->deserializeUpdateSubunit();
    }

    return result;
}

////////////////////////////////////

PlugConnection::PlugConnection( Plug& srcPlug, Plug& destPlug )
    : m_srcPlug( &srcPlug )
    , m_destPlug( &destPlug )
{
}

PlugConnection::PlugConnection()
    : m_srcPlug( 0 )
    , m_destPlug( 0 )
{
}

bool
PlugConnection::serialize( std::string basePath, Util::IOSerialize& ser ) const
{
    bool result;
    result  = ser.write( basePath + "m_srcPlug", m_srcPlug->getGlobalId() );
    result &= ser.write( basePath + "m_destPlug", m_destPlug->getGlobalId() );
    return result;
}

PlugConnection*
PlugConnection::deserialize( std::string basePath,
                               Util::IODeserialize& deser,
                               Unit& unit )
{
    if ( !deser.isExisting( basePath + "m_srcPlug" ) ) {
        return 0;
    }
    PlugConnection* pConnection = new PlugConnection;
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

    pConnection->m_srcPlug  = unit.getPlugManager().getPlug( iSrcPlugId );
    pConnection->m_destPlug = unit.getPlugManager().getPlug( iDestPlugId );

    if ( !pConnection->m_srcPlug || !pConnection->m_destPlug ) {
        delete pConnection;
        return 0;
    }

    return pConnection;
}

ExtendedStreamFormatCmd
Plug::setPlugAddrToStreamFormatCmd(
    ExtendedStreamFormatCmd::ESubFunction subFunction)
{
    ExtendedStreamFormatCmd extStreamFormatInfoCmd(
        m_unit->get1394Service(),
        subFunction );
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
        extStreamFormatInfoCmd.setPlugAddress(
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

    extStreamFormatInfoCmd.setNodeId( m_unit->getConfigRom().getNodeId() );
    extStreamFormatInfoCmd.setCommandType( AVCCommand::eCT_Status );
    extStreamFormatInfoCmd.setSubunitId( getSubunitId() );
    extStreamFormatInfoCmd.setSubunitType( getSubunitType() );

    return extStreamFormatInfoCmd;
}

bool
serializePlugVector( std::string basePath,
                     Util::IOSerialize& ser,
                     const PlugVector& vec)
{
    bool result = true;
    int i = 0;
    for ( PlugVector::const_iterator it = vec.begin();
          it != vec.end();
          ++it )
    {
        const Plug* pPlug = *it;
        std::ostringstream strstrm;
        strstrm << basePath << i;

        result &= ser.write( strstrm.str() + "/global_id", pPlug->getGlobalId() );
        i++;
    }
    return result;
}

bool
deserializePlugVector( std::string basePath,
                       Util::IODeserialize& deser,
                       const PlugManager& plugManager,
                       PlugVector& vec )
{
    int i = 0;
    Plug* pPlug = 0;
    do {
        std::ostringstream strstrm;
        unsigned int iPlugId;

        strstrm << basePath << i;

        if ( !deser.isExisting( strstrm.str() + "/global_id" ) ) {
            // no more plugs found, so this is the normal return point
            return true;
        }

        if ( !deser.read( strstrm.str() + "/global_id", iPlugId ) ) {
            return false;
        }
  
        pPlug = plugManager.getPlug( iPlugId );
        if ( pPlug ) {
            vec.push_back( pPlug );
            i++;
        }
    } while ( pPlug );

    // if we reach here, the plug manager didn't find any plug with the id iPlugId.
    return false;
}

}
