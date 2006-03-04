/* avplug.h
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

#ifndef AVPLUG_H
#define AVPLUG_H

#include "libfreebobavc/avc_signal_source.h"
#include "libfreebobavc/avc_extended_stream_format.h"
#include "libfreebobavc/avc_extended_plug_info.h"
#include "libfreebobavc/avc_extended_cmd_generic.h"
#include "libfreebobavc/avc_definitions.h"
#include "libfreebobavc/avc_generic.h"
#include "libfreebob/xmlparser.h"

#include "debugmodule/debugmodule.h"

class Ieee1394Service;

class AvPlugManager;
class AvPlug;

typedef std::vector<AvPlug*> AvPlugVector;

class AvPlug {
public:

    enum EAvPlugAddressType {
	eAPA_PCR,
	eAPA_ExternalPlug,
	eAPA_AsynchronousPlug,
	eAPA_SubunitPlug,
	eAPA_Undefined,
    };

    enum EAvPlugType {
        eAPT_IsoStream,   
        eAPT_AsyncStream,
        eAPT_Midi,
        eAPT_Sync,
        eAPT_Analog,
        eAPT_Digital,
        eAPT_Unknown,	
    };

    enum EAvPlugDirection {
	eAPD_Input,
	eAPD_Output,
	eAPD_Unknown,
    };

    AvPlug( Ieee1394Service& ieee1394Service,
	    int m_nodeId,
            AvPlugManager& plugManager,
	    AVCCommand::ESubunitType subunitType,
	    subunit_id_t subunitId,
	    EAvPlugAddressType plugAddressType,
	    EAvPlugDirection plugDirection,
	    plug_id_t plugId );
    AvPlug( const AvPlug& rhs );
    virtual ~AvPlug();

    bool discover();
    bool discoverConnections();

    bool inquireConnnection( AvPlug& plug );

    plug_id_t      getPlugId()
	{ return m_id; }
    AVCCommand::ESubunitType getSubunitType()
	{ return m_subunitType; }
    subunit_id_t   getSubunitId()
	{ return m_subunitId; }
    const char*    getName()
	{ return m_name.c_str(); }
    EAvPlugDirection getPlugDirection()
	{ return m_direction; }
    sampling_frequency_t getSamplingFrequency()
	{ return m_samplingFrequency; }
    int getSampleRate(); // 22050, 24000, 32000, ...
    int getNrOfChannels();
    int getNrOfStreams();

    EAvPlugDirection getDirection() const
        { return m_direction; }
    EAvPlugAddressType getPlugAddressType() const
        { return m_addressType; }
    EAvPlugType getPlugType() const
	{ return m_infoPlugType; }

    const AvPlugVector& getInputConnections() const
        { return m_inputConnections; }
    const AvPlugVector& getOutputConnections() const
        { return m_outputConnections; }

    bool addXmlDescription( xmlNodePtr conectionSet );
    bool addXmlDescriptionStreamFormats( xmlNodePtr streamFormats );

    PlugAddress::EPlugDirection convertPlugDirection();

protected:
    bool discoverPlugType();
    bool discoverName();
    bool discoverNoOfChannels();
    bool discoverChannelPosition();
    bool discoverChannelName();
    bool discoverClusterInfo();
    bool discoverStreamFormat();
    bool discoverSupportedStreamFormats();
    bool discoverConnectionsInput();
    bool discoverConnectionsOutput();

    ExtendedPlugInfoCmd setPlugAddrToPlugInfoCmd();
    ExtendedStreamFormatCmd setPlugAddrToStreamFormatCmd(ExtendedStreamFormatCmd::ESubFunction subFunction);
    SignalSourceCmd setSrcPlugAddrToSignalCmd();
    void setDestPlugAddrToSignalCmd( SignalSourceCmd& signalSourceCmd, AvPlug& plug );

    void debugOutputClusterInfos( int debugLevel );

    bool copyClusterInfo(ExtendedPlugInfoPlugChannelPositionSpecificData&
                         channelPositionData );

private:
    Ieee1394Service*             m_1394Service;

    int                          m_nodeId;
    AVCCommand::ESubunitType     m_subunitType;
    subunit_id_t                 m_subunitId;
    EAvPlugAddressType           m_addressType;
    EAvPlugDirection             m_direction;
    plug_id_t                    m_id;

    // Info plug type
    EAvPlugType m_infoPlugType;

    // Number of channels
    nr_of_channels_t             m_nrOfChannels;

    // Plug name
    std::string                  m_name;

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

    // Sampling frequency
    sampling_frequency_t m_samplingFrequency;

    // Supported stream formats
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

    ClusterInfo* getClusterInfoByIndex( int index );

    AvPlugVector             m_inputConnections;
    AvPlugVector             m_outputConnections;

    AvPlugManager*           m_plugManager;

    DECLARE_DEBUG_MODULE;
};

const char* avPlugAddressTypeToString( AvPlug::EAvPlugAddressType addressType );
const char* avPlugTypeToString( AvPlug::EAvPlugType type );
const char* avPlugDirectionToString( AvPlug::EAvPlugDirection direction );

class AvPlugManager
{
public:
    AvPlugManager();
    ~AvPlugManager();

    bool addPlug( AvPlug& plug );
    bool remPlug( AvPlug& plug );

    void showPlugs();

    AvPlug* getPlug( AVCCommand::ESubunitType subunitType,
                     subunit_id_t subunitId,
                     AvPlug::EAvPlugAddressType plugAddressType,
                     AvPlug::EAvPlugDirection plugDirection,
                     plug_id_t plugId );
    // We expect only one sync plug which matches
    AvPlug* getPlugByType( AVCCommand::ESubunitType subunitType,
			   subunit_id_t subunitId,
			   AvPlug::EAvPlugAddressType plugAddressType,
			   AvPlug::EAvPlugDirection plugDirection,
			   AvPlug::EAvPlugType type);

private:
    AvPlugVector m_plugs;

    DECLARE_DEBUG_MODULE;
};

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
