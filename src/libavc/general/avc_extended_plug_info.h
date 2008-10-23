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

#ifndef AVCEXTENDEDPLUGINFO_H
#define AVCEXTENDEDPLUGINFO_H

#include "avc_extended_cmd_generic.h"
#include "avc_generic.h"

#include <string>
#include <vector>

#define AVC1394_CMD_PLUG_INFO 0x02

namespace AVC {

class ExtendedPlugInfoPlugTypeSpecificData : public IBusData
{
public:
    enum EExtendedPlugInfoPlugType {
        eEPIPT_IsoStream   = 0x0,
        eEPIPT_AsyncStream = 0x1,
        eEPIPT_Midi        = 0x2,
        eEPIPT_Sync        = 0x3,
        eEPIPT_Analog      = 0x4,
        eEPIPT_Digital     = 0x5,

        eEPIPT_Unknown     = 0xff,
    };

    ExtendedPlugInfoPlugTypeSpecificData( EExtendedPlugInfoPlugType ePlugType =  eEPIPT_Unknown);
    virtual ~ExtendedPlugInfoPlugTypeSpecificData();

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );
    virtual ExtendedPlugInfoPlugTypeSpecificData* clone() const;

    typedef byte_t plug_type_t;

    plug_type_t m_plugType;
};

const char* extendedPlugInfoPlugTypeToString( plug_type_t plugType );

/////////////////////////////////////////

class ExtendedPlugInfoPlugNameSpecificData : public IBusData
{
public:
    ExtendedPlugInfoPlugNameSpecificData();
    virtual ~ExtendedPlugInfoPlugNameSpecificData();

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );
    virtual ExtendedPlugInfoPlugNameSpecificData* clone() const;

    std::string m_name;
};

/////////////////////////////////////////

class ExtendedPlugInfoPlugNumberOfChannelsSpecificData : public IBusData
{
public:
    ExtendedPlugInfoPlugNumberOfChannelsSpecificData();
    virtual ~ExtendedPlugInfoPlugNumberOfChannelsSpecificData();

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );
    virtual ExtendedPlugInfoPlugNumberOfChannelsSpecificData* clone() const;

    nr_of_channels_t m_nrOfChannels;
};


/////////////////////////////////////////

class ExtendedPlugInfoPlugChannelPositionSpecificData : public IBusData
{
public:
    enum ELocation {
        eEPI_LeftFront        = 0x01,
        eEPI_RightFront       = 0x02,
        eEPI_Center           = 0x03,
        eEPI_Subwoofer        = 0x04,
        eEPI_LeftSurround     = 0x05,
        eEPI_RightSurround    = 0x06,
        eEPI_LeftOfCenter     = 0x07,
        eEPI_RightOfCenter    = 0x08,
        eEPI_Surround         = 0x09,
        eEPI_SideLeft         = 0x0a,
        eEPI_SideRight        = 0x0b,
        eEPI_Top              = 0x0c,
        eEPI_Bottom           = 0x0d,
        eEPI_LeftFrontEffect  = 0x0e,
        eEPI_RightFrontEffect = 0x0f,
        eEPI_NoPostion        = 0xff,
    };

    struct ChannelInfo {
        stream_position_t          m_streamPosition;
        stream_position_location_t m_location;
    };

    typedef std::vector<ChannelInfo> ChannelInfoVector;

    struct ClusterInfo {
        nr_of_channels_t         m_nrOfChannels;
        ChannelInfoVector        m_channelInfos;
    };

    ExtendedPlugInfoPlugChannelPositionSpecificData();
    virtual ~ExtendedPlugInfoPlugChannelPositionSpecificData();

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );
    virtual ExtendedPlugInfoPlugChannelPositionSpecificData* clone() const;

    typedef std::vector<ClusterInfo> ClusterInfoVector;

    nr_of_clusters_t         m_nrOfClusters;
    ClusterInfoVector        m_clusterInfos;
};

/////////////////////////////////////////

class ExtendedPlugInfoPlugChannelNameSpecificData : public IBusData
{
public:
    ExtendedPlugInfoPlugChannelNameSpecificData();
    virtual ~ExtendedPlugInfoPlugChannelNameSpecificData();

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );
    virtual ExtendedPlugInfoPlugChannelNameSpecificData* clone() const;

    stream_position_t m_streamPosition;
    string_length_t   m_stringLength;
    std::string       m_plugChannelName;
};

/////////////////////////////////////////

class ExtendedPlugInfoPlugInputSpecificData : public IBusData
{
public:
    ExtendedPlugInfoPlugInputSpecificData();
    ExtendedPlugInfoPlugInputSpecificData( const ExtendedPlugInfoPlugInputSpecificData& rhs );
    virtual ~ExtendedPlugInfoPlugInputSpecificData();

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );
    virtual ExtendedPlugInfoPlugInputSpecificData* clone() const;

    PlugAddressSpecificData* m_plugAddress;
};


/////////////////////////////////////////

class ExtendedPlugInfoPlugOutputSpecificData : public IBusData
{
public:
    ExtendedPlugInfoPlugOutputSpecificData();
    ExtendedPlugInfoPlugOutputSpecificData( const ExtendedPlugInfoPlugOutputSpecificData& rhs );
    virtual ~ExtendedPlugInfoPlugOutputSpecificData();

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );
    virtual ExtendedPlugInfoPlugOutputSpecificData* clone() const;

    number_of_output_plugs_t m_nrOfOutputPlugs;

    typedef std::vector<PlugAddressSpecificData*> PlugAddressVector;
    PlugAddressVector m_outputPlugAddresses;
};

/////////////////////////////////////////

class ExtendedPlugInfoClusterInfoSpecificData : public IBusData
{
public:
    enum EPortType {
        ePT_Speaker            = 0x00,
        ePT_Headphone          = 0x01,
        ePT_Microphone         = 0x02,
        ePT_Line               = 0x03,
        ePT_SPDIF              = 0x04,
        ePT_ADAT               = 0x05,
        ePT_TDIF               = 0x06,
        ePT_MADI               = 0x07,
        ePT_Analog             = 0x08,
        ePT_Digital            = 0x09,
        ePT_MIDI               = 0x0a,
        ePT_NoType             = 0xff,
    };

    ExtendedPlugInfoClusterInfoSpecificData();
    virtual ~ExtendedPlugInfoClusterInfoSpecificData();

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );
    virtual ExtendedPlugInfoClusterInfoSpecificData* clone() const;

    cluster_index_t m_clusterIndex;
    port_type_t     m_portType;
    string_length_t m_stringLength;
    std::string     m_clusterName;
};

const char* extendedPlugInfoClusterInfoPortTypeToString( port_type_t portType );

/////////////////////////////////////////

#define AVC1394_EXTENDED_PLUG_INFO_INFO_TYPE_PLUG_TYPE                   0x00
#define AVC1394_EXTENDED_PLUG_INFO_INFO_TYPE_PLUG_NAME                   0x01
#define AVC1394_EXTENDED_PLUG_INFO_INFO_TYPE_PLUG_NO_OF_CHANNELS         0x02
#define AVC1394_EXTENDED_PLUG_INFO_INFO_TYPE_PLUG_CHANNEL_POSITION       0x03
#define AVC1394_EXTENDED_PLUG_INFO_INFO_TYPE_PLUG_CHANNEL_NAME           0x04
#define AVC1394_EXTENDED_PLUG_INFO_INFO_TYPE_PLUG_INPUT                  0x05
#define AVC1394_EXTENDED_PLUG_INFO_INFO_TYPE_PLUG_OUTPUT                 0x06
#define AVC1394_EXTENDED_PLUG_INFO_INFO_TYPE_PLUG_CLUSTER_INFO           0x07

class ExtendedPlugInfoInfoType : public IBusData
{
public:
    enum EInfoType {
        eIT_PlugType        = AVC1394_EXTENDED_PLUG_INFO_INFO_TYPE_PLUG_TYPE,
        eIT_PlugName        = AVC1394_EXTENDED_PLUG_INFO_INFO_TYPE_PLUG_NAME,
        eIT_NoOfChannels    = AVC1394_EXTENDED_PLUG_INFO_INFO_TYPE_PLUG_NO_OF_CHANNELS,
        eIT_ChannelPosition = AVC1394_EXTENDED_PLUG_INFO_INFO_TYPE_PLUG_CHANNEL_POSITION,
        eIT_ChannelName     = AVC1394_EXTENDED_PLUG_INFO_INFO_TYPE_PLUG_CHANNEL_NAME,
        eIT_PlugInput       = AVC1394_EXTENDED_PLUG_INFO_INFO_TYPE_PLUG_INPUT,
        eIT_PlugOutput      = AVC1394_EXTENDED_PLUG_INFO_INFO_TYPE_PLUG_OUTPUT,
        eIT_ClusterInfo     = AVC1394_EXTENDED_PLUG_INFO_INFO_TYPE_PLUG_CLUSTER_INFO,
    };

    ExtendedPlugInfoInfoType(EInfoType eInfoType);
    ExtendedPlugInfoInfoType( const ExtendedPlugInfoInfoType& rhs );
    virtual ~ExtendedPlugInfoInfoType();

    bool initialize();

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );
    virtual ExtendedPlugInfoInfoType* clone() const;

    info_type_t m_infoType;

    ExtendedPlugInfoPlugTypeSpecificData*               m_plugType;
    ExtendedPlugInfoPlugNameSpecificData*               m_plugName;
    ExtendedPlugInfoPlugNumberOfChannelsSpecificData*   m_plugNrOfChns;
    ExtendedPlugInfoPlugChannelPositionSpecificData*    m_plugChannelPosition;
    ExtendedPlugInfoPlugChannelNameSpecificData*        m_plugChannelName;
    ExtendedPlugInfoPlugInputSpecificData*              m_plugInput;
    ExtendedPlugInfoPlugOutputSpecificData*             m_plugOutput;
    ExtendedPlugInfoClusterInfoSpecificData*            m_plugClusterInfo;
};

const char* extendedPlugInfoInfoTypeToString( info_type_t infoType );

/////////////////////////////////////////////////////////

#define AVC1394_PLUG_INFO_SUBFUNCTION_EXTENDED_PLUG_INFO_CMD   0xC0
#define AVC1394_PLUG_INFO_SUBFUNCTION_SERIAL_BUS_NOT_USED      0xFF


class ExtendedPlugInfoCmd: public AVCCommand
{
public:
    enum ESubFunction {
        eSF_ExtendedPlugInfoCmd                  = AVC1394_PLUG_INFO_SUBFUNCTION_EXTENDED_PLUG_INFO_CMD,
        eSF_NotUsed                              = AVC1394_PLUG_INFO_SUBFUNCTION_SERIAL_BUS_NOT_USED,
    };

    ExtendedPlugInfoCmd( Ieee1394Service& ieee1394service,
                         ESubFunction eSubFunction = eSF_ExtendedPlugInfoCmd );
    ExtendedPlugInfoCmd( const ExtendedPlugInfoCmd& rhs );
    virtual ~ExtendedPlugInfoCmd();

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    bool setPlugAddress( const PlugAddress& plugAddress );
    bool setSubFunction( ESubFunction subFunction );
    subfunction_t getSubFunction( ) {return m_subFunction;};
    bool setInfoType( const ExtendedPlugInfoInfoType& infoType );
    ExtendedPlugInfoInfoType* getInfoType()
    { return m_infoType; }

    virtual const char* getCmdName() const
    { return "ExtendedPlugInfoCmd"; }

protected:
    subfunction_t             m_subFunction;
    PlugAddress*              m_plugAddress;
    ExtendedPlugInfoInfoType* m_infoType;
};

}

#endif
