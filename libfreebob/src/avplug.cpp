/* avplug.cpp
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

#include "avplug.h"

#include "libfreebobavc/avc_extended_plug_info.h"

IMPL_DEBUG_MODULE( AvPlug, AvPlug, DEBUG_LEVEL_VERBOSE );

AvPlug::AvPlug()
    : m_plugType( 0xff )
    , m_plugId( 0xff )
    , m_subunitType( 0xff )
    , m_subunitId( 0xff )
    , m_direction( 0xff )
    , m_nrOfChannels( 0 )
    , m_samplingFrequency( 0xff )
{
}

AvPlug::AvPlug( const AvPlug& rhs )
    : m_plugType( rhs.m_plugType )
    , m_plugId( rhs.m_plugId )
    , m_subunitType( rhs.m_subunitType )
    , m_subunitId( rhs.m_subunitType )
    , m_direction( rhs.m_direction )
    , m_name( rhs.m_name )
    , m_nrOfChannels( rhs.m_nrOfChannels )
    , m_samplingFrequency( rhs.m_samplingFrequency )
    , m_clusterInfos( rhs.m_clusterInfos )
{
}

AvPlug::~AvPlug()
{
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

int
AvPlug::getNrOfChannels()
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
AvPlug::getSampleRate()
{
    int value = 0;
    switch ( m_samplingFrequency ) {
    case eSF_22050Hz:
        value = 22050;
        break;
    case eSF_24000Hz:
        value = 24000;
        break;
    case eSF_32000Hz:
        value = 32000;
        break;
    case eSF_44100Hz:
        value = 44100;
        break;
    case eSF_48000Hz:
        value = 48000;
        break;
    case eSF_88200Hz:
        value = 88200;
        break;
    case eSF_96000Hz:
        value = 96000;
        break;
    case eSF_176400Hz:
        value = 176400;
        break;
    case eSF_192000Hz:
        value = 192000;
        break;
    default:
        value = 0;
    }

    return value;
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
