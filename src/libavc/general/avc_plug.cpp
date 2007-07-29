/*
 * Copyright (C)      2007 by Pieter Palmers
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

#include "avc_plug.h"
#include "avc_unit.h"
#include "libieee1394/configrom.h"

#include "libieee1394/ieee1394service.h"
#include "../util/avc_serialize.h"

#include <sstream>

namespace AVC {

int Plug::m_globalIdCounter = 0;

IMPL_DEBUG_MODULE( Plug, Plug, DEBUG_LEVEL_NORMAL );
IMPL_DEBUG_MODULE( PlugManager, PlugManager, DEBUG_LEVEL_NORMAL );

Plug::Plug( Unit* unit,
            Subunit* subunit,
            function_block_type_t functionBlockType,
            function_block_id_t functionBlockId,
            EPlugAddressType plugAddressType,
            EPlugDirection plugDirection,
            plug_id_t plugId )
    : m_unit(unit)
    , m_subunit(subunit)
    , m_functionBlockType( functionBlockType )
    , m_functionBlockId( functionBlockId )
    , m_addressType( plugAddressType )
    , m_direction( plugDirection )
    , m_id( plugId )
    , m_infoPlugType( eAPT_Unknown )
    , m_nrOfChannels( 0 )
    , m_globalId( m_globalIdCounter++ )
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

Plug::Plug( const Plug& rhs )
    : m_unit ( rhs.m_unit )
    , m_subunit ( rhs.m_subunit )
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

ESubunitType
Plug::getSubunitType() const
{
    return (m_subunit==NULL?eST_Unit:m_subunit->getSubunitType()); 
}

subunit_id_t 
Plug::getSubunitId() const
{
    return (m_subunit==NULL?eST_Unit:m_subunit->getSubunitId());
}

/*
bool
Plug::discover()
{
    if ( !discoverPlugType() ) {
        debugError( "discover: Could not discover plug type (%d,%d,%d,%d,%d)\n",
                    m_unit->getConfigRom().getNodeId(), m_subunitType, m_subunitId, m_direction, m_id );
        return false;
    }

    if ( !discoverName() ) {
        debugError( "Could not discover name (%d,%d,%d,%d,%d)\n",
                    m_unit->getConfigRom().getNodeId(), m_subunitType, m_subunitId, m_direction, m_id );
        return false;
    }

    if ( !discoverNoOfChannels() ) {
        debugError( "Could not discover number of channels "
                    "(%d,%d,%d,%d,%d)\n",
                    m_unit->getConfigRom().getNodeId(), m_subunitType, m_subunitId, m_direction, m_id );
        return false;
    }

    if ( !discoverChannelPosition() ) {
        debugError( "Could not discover channel positions "
                    "(%d,%d,%d,%d,%d)\n",
                    m_unit->getConfigRom().getNodeId(), m_subunitType, m_subunitId, m_direction, m_id );
        return false;
    }

    if ( !discoverChannelName() ) {
        debugError( "Could not discover channel name "
                    "(%d,%d,%d,%d,%d)\n",
                    m_unit->getConfigRom().getNodeId(), m_subunitType, m_subunitId, m_direction, m_id );
        return false;
    }

    if ( !discoverClusterInfo() ) {
        debugError( "Could not discover channel name "
                    "(%d,%d,%d,%d,%d)\n",
                    m_unit->getConfigRom().getNodeId(), m_subunitType, m_subunitId, m_direction, m_id );
        return false;
    }

    if ( !discoverStreamFormat() ) {
        debugError( "Could not discover stream format "
                    "(%d,%d,%d,%d,%d)\n",
                    m_unit->getConfigRom().getNodeId(), m_subunitType, m_subunitId, m_direction, m_id );
        return false;
    }

    if ( !discoverSupportedStreamFormats() ) {
        debugError( "Could not discover supported stream formats "
                    "(%d,%d,%d,%d,%d)\n",
                    m_unit->getConfigRom().getNodeId(), m_subunitType, m_subunitId, m_direction, m_id );
        return false;
    }

    return unit->getPlugManager().addPlug( *this );
}

bool
Plug::discoverConnections()
{
    return discoverConnectionsInput() && discoverConnectionsOutput();
}
*/

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

int
Plug::getSampleRate() const
{
    return convertESamplingFrequency( static_cast<ESamplingFrequency>( m_samplingFrequency ) );
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

void
Plug::showPlug() const
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
    EPlugAddressType    addressType       = eAPA_Undefined;
    EPlugDirection      direction         = eAPD_Unknown;
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
Plug::serializeChannelInfos( Glib::ustring basePath,
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
Plug::deserializeChannelInfos( Glib::ustring basePath,
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
Plug::serializeClusterInfos( Glib::ustring basePath,
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
Plug::deserializeClusterInfos( Glib::ustring basePath,
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
Plug::serializeFormatInfos( Glib::ustring basePath,
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
Plug::deserializeFormatInfos( Glib::ustring basePath,
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
Plug::serializePlugVector( Glib::ustring basePath,
                               Util::IOSerialize& ser,
                               const PlugVector& vec) const
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
Plug::deserializePlugVector( Glib::ustring basePath,
                                 Util::IODeserialize& deser,
                                 PlugVector& vec )
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
                Plug* pPlug = m_unit->getPlugManager().getPlug( iPlugId );
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
Plug::serialize( Glib::ustring basePath, Util::IOSerialize& ser ) const
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
    result &= ser.write( basePath + "m_verbose_level", getDebugLevel());
    result &= ser.write( basePath + "m_globalId", m_globalId);
    result &= ser.write( basePath + "m_globalIdCounter", m_globalIdCounter );

    return result;
}

Plug*
Plug::deserialize( Glib::ustring basePath,
                     Util::IODeserialize& deser,
                     Unit& unit,
                     PlugManager& plugManager )
{
    Plug* pPlug = new Plug;
    if ( !pPlug ) {
        return 0;
    }

    pPlug->m_unit = &unit;
    
    bool result=true;
    
    ESubunitType subunitType;
    result  = deser.read( basePath + "m_subunitType", subunitType );
    subunit_t subunitId;
    result &= deser.read( basePath + "m_subunitId", subunitId );
    pPlug->m_subunit = unit.getSubunit( subunitType, subunitType );
    
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
    int level;
    result &= deser.read( basePath + "m_verbose_level", level );
    setDebugLevel(level);
    result &= deser.read( basePath + "m_globalId", pPlug->m_globalId );
    result &= deser.read( basePath + "m_globalIdCounter", pPlug->m_globalIdCounter );

    if ( !result ) {
        delete pPlug;
        return 0;
    }

    return pPlug;
}

bool
Plug::deserializeUpdate( Glib::ustring basePath,
                           Util::IODeserialize& deser )
{
    bool result;

    result  = deserializePlugVector( basePath + "m_inputConnections", deser, m_inputConnections );
    result &= deserializePlugVector( basePath + "m_outputConnections", deser, m_outputConnections );

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
{

}

PlugManager::PlugManager( const PlugManager& rhs )
{
    setDebugLevel( rhs.getDebugLevel() );
}

PlugManager::~PlugManager()
{
}

bool
PlugManager::addPlug( Plug& plug )
{
    m_plugs.push_back( &plug );
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
static void addConnection( PlugConnectionOwnerVector& connections,
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
            addConnection( connections, *( *it ), *plug );
        }
        for ( PlugVector::const_iterator it =
                  plug->getOutputConnections().begin();
            it != plug->getOutputConnections().end();
            ++it )
        {
            addConnection( connections, *plug, *( *it ) );
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
PlugManager::serialize( Glib::ustring basePath, Util::IOSerialize& ser ) const
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

    return result;
}

PlugManager*
PlugManager::deserialize( Glib::ustring basePath,
                            Util::IODeserialize& deser,
                            Unit& unit )

{
    PlugManager* pMgr = new PlugManager;

    if ( !pMgr ) {
        return 0;
    }

    int i = 0;
    bool bFinished = false;
    do {
        std::ostringstream strstrm;
        strstrm << basePath << i;
        // unit still holds a null pointer for the plug manager
        // therefore we have to *this as additional argument
        Plug* pPlug = Plug::deserialize( strstrm.str() + "/",
                                             deser,
                                             unit,
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
PlugConnection::serialize( Glib::ustring basePath, Util::IOSerialize& ser ) const
{
    bool result;
    result  = ser.write( basePath + "m_srcPlug", m_srcPlug->getGlobalId() );
    result &= ser.write( basePath + "m_destPlug", m_destPlug->getGlobalId() );
    return result;
}

PlugConnection*
PlugConnection::deserialize( Glib::ustring basePath,
                               Util::IODeserialize& deser,
                               Unit& unit )
{
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

}
