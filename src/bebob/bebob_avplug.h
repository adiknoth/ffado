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

#ifndef BEBOB_AVPLUG_H
#define BEBOB_AVPLUG_H

#include "libavc/ccm/avc_signal_source.h"
#include "libavc/streamformat/avc_extended_stream_format.h"
#include "libavc/general/avc_extended_plug_info.h"
#include "libavc/general/avc_extended_cmd_generic.h"
#include "libavc/avc_definitions.h"
#include "libavc/general/avc_generic.h"

#include "libutil/serialize.h"

#include "debugmodule/debugmodule.h"

#include <glibmm/ustring.h>

class Ieee1394Service;
class ConfigRom;

namespace BeBoB {

class AvDevice;
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
    eAPA_FunctionBlockPlug,
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

    // \todo This constructors sucks. too many parameters. fix it.
    AvPlug( Ieee1394Service& ieee1394Service,
        ConfigRom& configRom,
            AvPlugManager& plugManager,
        AVC::ESubunitType subunitType,
        AVC::subunit_id_t subunitId,
        AVC::function_block_type_t functionBlockType,
        AVC::function_block_type_t functionBlockId,
        EAvPlugAddressType plugAddressType,
        EAvPlugDirection plugDirection,
        AVC::plug_id_t plugId,
        int verboseLevel );
    AvPlug( const AvPlug& rhs );
    virtual ~AvPlug();

    bool discover();
    bool discoverConnections();

    bool inquireConnnection( AvPlug& plug );
    bool setConnection( AvPlug& plug );

    int getGlobalId() const
        { return m_globalId; }
    AVC::plug_id_t getPlugId() const
        { return m_id; }
    AVC::ESubunitType getSubunitType() const
        { return m_subunitType; }
    AVC::subunit_id_t getSubunitId() const
        { return m_subunitId; }
    const char* getName() const
        { return m_name.c_str(); }
    EAvPlugDirection getPlugDirection() const
        { return m_direction; }
    AVC::sampling_frequency_t getSamplingFrequency() const
        { return m_samplingFrequency; }
    int getSampleRate() const; // 22050, 24000, 32000, ...
    int getNrOfChannels() const;
    int getNrOfStreams() const;

    EAvPlugDirection getDirection() const
        { return m_direction; }
    EAvPlugAddressType getPlugAddressType() const
        { return m_addressType; }
    EAvPlugType getPlugType() const
        { return m_infoPlugType; }

    AVC::function_block_type_t getFunctionBlockType() const
        { return m_functionBlockType; }
    AVC::function_block_id_t getFunctionBlockId() const
        { return m_functionBlockId; }

    const AvPlugVector& getInputConnections() const
        { return m_inputConnections; }
    const AvPlugVector& getOutputConnections() const
        { return m_outputConnections; }

    static AVC::PlugAddress::EPlugDirection convertPlugDirection(
    EAvPlugDirection direction);

    void showPlug() const;

    bool serialize( Glib::ustring basePath, Util::IOSerialize& ser ) const;
    static AvPlug* deserialize( Glib::ustring basePath,
                                Util::IODeserialize& deser,
                                AvDevice& avDevice,
                                AvPlugManager& plugManager );

    bool deserializeUpdate( Glib::ustring basePath,
                            Util::IODeserialize& deser );

 public:
    struct ChannelInfo {
        AVC::stream_position_t          m_streamPosition;
        AVC::stream_position_location_t m_location;
        Glib::ustring                   m_name;
    };
    typedef std::vector<ChannelInfo> ChannelInfoVector;

    struct ClusterInfo {
        int                      m_index;
        AVC::port_type_t         m_portType;
        Glib::ustring            m_name;

        AVC::nr_of_channels_t    m_nrOfChannels;
        ChannelInfoVector        m_channelInfos;
        AVC::stream_format_t     m_streamFormat;
    };
    typedef std::vector<ClusterInfo> ClusterInfoVector;

    ClusterInfoVector& getClusterInfos()
        { return m_clusterInfos; }

protected:
    AvPlug();

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

    AVC::ExtendedPlugInfoCmd setPlugAddrToPlugInfoCmd();

    AVC::ExtendedStreamFormatCmd setPlugAddrToStreamFormatCmd(
            AVC::ExtendedStreamFormatCmd::ESubFunction subFunction);

    AVC::SignalSourceCmd setSrcPlugAddrToSignalCmd();

    void setDestPlugAddrToSignalCmd(
            AVC::SignalSourceCmd& signalSourceCmd, AvPlug& plug );

    void debugOutputClusterInfos( int debugLevel );

    bool copyClusterInfo(AVC::ExtendedPlugInfoPlugChannelPositionSpecificData&
                         channelPositionData );

    bool addPlugConnection( AvPlugVector& connections, AvPlug& plug );

    bool discoverConnectionsFromSpecificData(
        EAvPlugDirection discoverDirection,
        AVC::PlugAddressSpecificData* plugAddress,
        AvPlugVector& connections );

    AvPlug* getPlugDefinedBySpecificData(
        AVC::UnitPlugSpecificDataPlugAddress* pUnitPlugAddress,
        AVC::SubunitPlugSpecificDataPlugAddress* pSubunitPlugAddress,
        AVC::FunctionBlockPlugSpecificDataPlugAddress* pFunctionBlockPlugAddress );

    EAvPlugDirection toggleDirection( EAvPlugDirection direction ) const;

    const ClusterInfo* getClusterInfoByIndex( int index ) const;

    bool serializeChannelInfos( Glib::ustring basePath,
                                Util::IOSerialize& ser,
                                const ClusterInfo& clusterInfo ) const;
    bool deserializeChannelInfos( Glib::ustring basePath,
                                  Util::IODeserialize& deser,
                                  ClusterInfo& clusterInfo );

    bool serializeClusterInfos( Glib::ustring basePath,
                                Util::IOSerialize& ser ) const;
    bool deserializeClusterInfos( Glib::ustring basePath,
                                  Util::IODeserialize& deser );

    bool serializeFormatInfos( Glib::ustring basePath,
                               Util::IOSerialize& ser ) const;
    bool deserializeFormatInfos( Glib::ustring basePath,
                                 Util::IODeserialize& deser );

    bool serializeAvPlugVector( Glib::ustring basePath,
                                Util::IOSerialize& ser,
                                const AvPlugVector& vec) const;
    bool deserializeAvPlugVector( Glib::ustring basePath,
                                  Util::IODeserialize& deser,
                                  AvPlugVector& vec );

private:
    // Supported stream formats
    struct FormatInfo {
        FormatInfo()
            : m_samplingFrequency( AVC::eSF_DontCare )
            , m_isSyncStream( false )
            , m_audioChannels( 0 )
            , m_midiChannels( 0 )
            , m_index( 0xff )
            {}
    AVC::sampling_frequency_t  m_samplingFrequency;
    bool                       m_isSyncStream;
    AVC::number_of_channels_t  m_audioChannels;
    AVC::number_of_channels_t  m_midiChannels;
    byte_t                     m_index;
    };
    typedef std::vector<FormatInfo> FormatInfoVector;


    Ieee1394Service*             m_p1394Service;
    ConfigRom*                   m_pConfigRom;
    AVC::ESubunitType     m_subunitType;
    AVC::subunit_id_t                 m_subunitId;
    AVC::function_block_type_t        m_functionBlockType;
    AVC::function_block_id_t          m_functionBlockId;
    EAvPlugAddressType           m_addressType;
    EAvPlugDirection             m_direction;
    AVC::plug_id_t                    m_id;
    EAvPlugType                  m_infoPlugType;
    AVC::nr_of_channels_t             m_nrOfChannels;
    Glib::ustring                m_name;
    ClusterInfoVector            m_clusterInfos;
    AVC::sampling_frequency_t         m_samplingFrequency;
    FormatInfoVector             m_formatInfos;
    AvPlugVector                 m_inputConnections;
    AvPlugVector                 m_outputConnections;
    AvPlugManager*               m_plugManager;
    int                          m_verboseLevel;
    int                          m_globalId;
    static int                   m_globalIdCounter;

    DECLARE_DEBUG_MODULE;
};

const char* avPlugAddressTypeToString( AvPlug::EAvPlugAddressType addressType );
const char* avPlugTypeToString( AvPlug::EAvPlugType type );
const char* avPlugDirectionToString( AvPlug::EAvPlugDirection direction );

class AvPlugManager
{
public:
    AvPlugManager( int verboseLevel );
    AvPlugManager( const AvPlugManager& rhs );
    ~AvPlugManager();

    bool addPlug( AvPlug& plug );
    bool remPlug( AvPlug& plug );

    void showPlugs() const;

    AvPlug* getPlug( AVC::ESubunitType subunitType,
                     AVC::subunit_id_t subunitId,
                     AVC::function_block_type_t functionBlockType,
                     AVC::function_block_id_t functionBlockId,
                     AvPlug::EAvPlugAddressType plugAddressType,
                     AvPlug::EAvPlugDirection plugDirection,
                     AVC::plug_id_t plugId ) const;
    AvPlug* getPlug( int iGlobalId ) const;
    AvPlugVector getPlugsByType( AVC::ESubunitType subunitType,
                 AVC::subunit_id_t subunitId,
                 AVC::function_block_type_t functionBlockType,
                 AVC::function_block_id_t functionBlockId,
                 AvPlug::EAvPlugAddressType plugAddressType,
                 AvPlug::EAvPlugDirection plugDirection,
                 AvPlug::EAvPlugType type) const;

    bool serialize( Glib::ustring basePath, Util::IOSerialize& ser ) const;
    static  AvPlugManager* deserialize( Glib::ustring basePath,
                                        Util::IODeserialize& deser,
                                        AvDevice& avDevice );

private:
    AvPlugManager();

    int          m_verboseLevel;
    AvPlugVector m_plugs;

    DECLARE_DEBUG_MODULE;
};

class AvPlugConnection {
public:
    AvPlugConnection( AvPlug& srcPlug, AvPlug& destPlug );

    AvPlug& getSrcPlug() const
        { return *m_srcPlug; }
    AvPlug& getDestPlug() const
        { return *m_destPlug; }

    bool serialize( Glib::ustring basePath, Util::IOSerialize& ser ) const;
    static AvPlugConnection* deserialize( Glib::ustring basePath,
                                          Util::IODeserialize& deser,
                                          AvDevice& avDevice );
private:
    AvPlugConnection();

private:
    AvPlug* m_srcPlug;
    AvPlug* m_destPlug;
};

typedef std::vector<AvPlugConnection*> AvPlugConnectionVector;
typedef std::vector<AvPlugConnection> AvPlugConnectionOwnerVector;

}

#endif // BEBOB_AVPLUG_H
