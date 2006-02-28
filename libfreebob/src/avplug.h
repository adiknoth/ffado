/* avplug.h
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

#ifndef AVPLUG_H
#define AVPLUG_H

#include "debugmodule/debugmodule.h"
#include "libfreebobavc/avc_definitions.h"

#include <vector>

class AvPlugCluster;
class ExtendedPlugInfoPlugChannelPositionSpecificData;

class AvPlug {
public:
    AvPlug();
    AvPlug( const AvPlug& rhs );
    virtual ~AvPlug();

    plug_type_t    getPlugType()
	{ return m_plugType; }
    plug_id_t      getPlugId()
	{ return m_plugId; }
    subunit_type_t getSubunitType()
	{ return m_subunitType; }
    subunit_id_t   getSubunitId()
	{ return m_subunitId; }
    const char*    getName()
	{ return m_name.c_str(); }
    plug_direction_t getPlugDirection()
	{ return m_direction; }
    sampling_frequency_t getSamplingFrequency()
	{ return m_samplingFrequency; }
    int getSampleRate(); // 22050, 24000, 32000, ...
    int getNrOfChannels();
    int getNrOfStreams();

    plug_type_t          m_plugType;
    plug_id_t            m_plugId;
    subunit_type_t       m_subunitType;
    subunit_id_t         m_subunitId;
    plug_direction_t     m_direction;
    std::string          m_name;
    nr_of_channels_t     m_nrOfChannels;
    sampling_frequency_t m_samplingFrequency;


    // ---
    // Channel & Cluster Info
    struct ChannelInfo {
        stream_position_t          m_streamPosition;
        stream_position_location_t m_location;
	std::string                m_name;
    };
    typedef std::vector<ChannelInfo> ChannelInfoVector;

    struct ClusterInfo {
	int                      m_index;
	port_type_t              m_portType;
	std::string              m_name;

        nr_of_channels_t         m_nrOfChannels;
        ChannelInfoVector        m_channelInfos;
	stream_format_t          m_streamFormat;
    };
    typedef std::vector<ClusterInfo> ClusterInfoVector;

    ClusterInfoVector        m_clusterInfos;

    // ---
    // Stream Format
    struct FormatInfo {
        FormatInfo()
            : m_samplingFrequency( eSF_DontCare )
            , m_isSyncStream( false )
            , m_audioChannels( 0 )
            , m_midiChannels( 0 )
            , m_index( 0xff )
            {}
	sampling_frequency_t  m_samplingFrequency;
	bool                  m_isSyncStream;
	number_of_channels_t  m_audioChannels;
	number_of_channels_t  m_midiChannels;
	byte_t                m_index;
    };
    typedef std::vector<FormatInfo> FormatInfoVector;

    FormatInfoVector         m_formatInfos;
    // ---

    void debugOutputClusterInfos( int debugLevel );

    bool copyClusterInfo(ExtendedPlugInfoPlugChannelPositionSpecificData&
                         channelPositionData );

    ClusterInfo* getClusterInfoByIndex(int index);
    ClusterInfoVector& getClusterInfos()
	{ return m_clusterInfos; }


    DECLARE_DEBUG_MODULE;
};

typedef std::vector<AvPlug*> AvPlugVector;

class AvPlugCluster {
public:
    AvPlugCluster();
    virtual ~AvPlugCluster();

    std::string  m_name;

    AvPlugVector m_avPlugs;
};

class AvPlugConnection {
public:
    AvPlugConnection();
    ~AvPlugConnection();

    AvPlug* m_srcPlug;
    AvPlug* m_destPlug;
};

typedef std::vector<AvPlugConnection*> AvPlugConnectionVector;

#endif
