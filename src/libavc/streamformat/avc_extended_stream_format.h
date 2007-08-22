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

#ifndef AVCEXTENDEDSTREAMFROMAT_H
#define AVCEXTENDEDSTREAMFROMAT_H

#include "../general/avc_generic.h"
#include "../general/avc_extended_cmd_generic.h"

#include <libavc1394/avc1394.h>
#include <iostream>
#include <vector>

namespace AVC {

#define AVC1394_STREAM_FORMAT_SUPPORT            0x2F
#define AVC1394_STREAM_FORMAT_SUBFUNCTION_INPUT  0x00
#define AVC1394_STREAM_FORMAT_SUBFUNCTION_OUTPUT 0x01

// BridgeCo extensions
#define AVC1394_STREAM_FORMAT_SUBFUNCTION_EXTENDED_STREAM_FORMAT      0xC0
#define AVC1394_STREAM_FORMAT_SUBFUNCTION_EXTENDED_STREAM_FORMAT_LIST 0xC1


#define AVC1394_STREAM_FORMAT_HIERARCHY_ROOT_DVCR       0x80
#define AVC1394_STREAM_FORMAT_HIERARCHY_ROOT_AUDIOMUSIC 0x90
#define AVC1394_STREAM_FORMAT_HIERARCHY_ROOT_INVALID    0xFF

#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_1_DVCR_SD525_60                     0x00
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_1_DVCR_SDL525_60                    0x04
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_1_DVCR_HD1125_60                    0x08
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_1_DVCR_SD625_60                     0x80
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_1_DVCR_SDL625_50                    0x84
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_1_DVCR_HD1250_50                    0x88
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_1_AUDIOMUSIC_AM824                  0x00
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_1_AUDIOMUSIC_24_4_AUDIO_PACK        0x01
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_1_AUDIOMUSIC_32_FLOATING_POINT_DATA 0x02
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_1_AUDIOMUSIC_AM824_COMPOUND         0x40
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_1_AUDIOMUSIC_DONT_CARE              0xFF


#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_IEC60968_3                            0x00
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_IEC61937_3                            0x01
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_IEC61937_4                            0x02
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_IEC61937_5                            0x03
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_IEC61937_6                            0x04
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_IEC61937_7                            0x05
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_MULTI_BIT_LINEAR_AUDIO_RAW            0x06
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_MULTI_BIT_LINEAR_AUDIO_DVD_AUDIO      0x07
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_ONE_BIT_AUDIO_PLAIN_RAW               0x08
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_ONE_BIT_AUDIO_PLAIN_SACD              0x09
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_ONE_BIT_AUDIO_ENCODED_RAW             0x0A
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_ONE_BIT_AUDIO_ENCODED_SACD            0x0B
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_HIGH_PRECISION_MULTIBIT_LINEAR_AUDIO  0x0C
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_MIDI_CONFORMANT                       0x0D
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_SYNC_STREAM                           0x40
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_DONT_CARE                             0xFF

#define AVC1394_STREAM_FORMAT_AM824_IEC60968_3                            0x00
#define AVC1394_STREAM_FORMAT_AM824_IEC61937_3                            0x01
#define AVC1394_STREAM_FORMAT_AM824_IEC61937_4                            0x02
#define AVC1394_STREAM_FORMAT_AM824_IEC61937_5                            0x03
#define AVC1394_STREAM_FORMAT_AM824_IEC61937_6                            0x04
#define AVC1394_STREAM_FORMAT_AM824_IEC61937_7                            0x05
#define AVC1394_STREAM_FORMAT_AM824_MULTI_BIT_LINEAR_AUDIO_RAW            0x06
#define AVC1394_STREAM_FORMAT_AM824_MULTI_BIT_LINEAR_AUDIO_DVD_AUDIO      0x07
#define AVC1394_STREAM_FORMAT_AM824_HIGH_PRECISION_MULTIBIT_LINEAR_AUDIO  0x0C
#define AVC1394_STREAM_FORMAT_AM824_MIDI_CONFORMANT                       0x0D
#define AVC1394_STREAM_FORMAT_AM824_DONT_CARE                             0xFF

/*
// As defined in 'AV/C Stream Format Information Specification 1.0 TA Document 2001002'
// Not used for extended stream format
#define AVC1394_STREAM_FORMAT_SUPPORT_STATUS_SUPPORTED_AND_CONFIGURED              0x00
#define AVC1394_STREAM_FORMAT_SUPPORT_STATUS_SUPPORTED_AND_HAS_NOT_BEEN_CONFIGURED 0x01
#define AVC1394_STREAM_FORMAT_SUPPORT_STATUS_SUPPORTED_AND_READY_TO_CONFIGURE      0x02
#define AVC1394_STREAM_FORMAT_SUPPORT_STATUS_SUPPORTED_AND_NOT_CONFIGURED          0x03
#define AVC1394_STREAM_FORMAT_SUPPORT_STATUS_NOT_SUPPORTED                         0x04
// 0x05 - 0xFE reserved
#define AVC1394_STREAM_FORMAT_SUPPORT_STATUS_NO_INFORMATION                        0xFF
*/

#define AVC1394_EXTENDED_STREAM_FORMAT_INFO_STATUS_ACTIVE           0x00
#define AVC1394_EXTENDED_STREAM_FORMAT_INFO_STATUS_INACTIVE         0x01
#define AVC1394_EXTENDED_STREAM_FORMAT_INFO_STATUS_NO_STREAM_FORMAT 0x02
#define AVC1394_EXTENDED_STREAM_FORMAT_INFO_STATUS_NOT_USED         0xff

class IOSSerialize;
class IISDeserialize;

enum ERateControl {
    eRC_Supported = 0x00,
    eRC_DontCare  = 0x01,
};

////////////////////////////////////////////////////////////

class StreamFormatInfo: public IBusData
{
public:
    StreamFormatInfo();

    virtual bool serialize( IOSSerialize& se );
    virtual bool deserialize( IISDeserialize& de );

    virtual StreamFormatInfo* clone() const;

    number_of_channels_t m_numberOfChannels;
    stream_format_t      m_streamFormat;
};
std::ostream& operator<<( std::ostream& stream, StreamFormatInfo info );

////////////////////////////////////////////////////////////

class FormatInformationStreams: public IBusData
{
public:
    FormatInformationStreams() {}
    virtual ~FormatInformationStreams() {}
};

////////////////////////////////////////////////////////////

class FormatInformationStreamsSync: public FormatInformationStreams
{
public:
    FormatInformationStreamsSync();

    virtual bool serialize( IOSSerialize& se );
    virtual bool deserialize( IISDeserialize& de );
    virtual FormatInformationStreamsSync* clone() const;

    reserved_t           m_reserved0;
    sampling_frequency_t m_samplingFrequency;
    rate_control_t       m_rateControl;
    reserved_t           m_reserved1;
};

////////////////////////////////////////////////////////////

class FormatInformationStreamsCompound: public FormatInformationStreams
{
public:
    FormatInformationStreamsCompound();
    virtual ~FormatInformationStreamsCompound();

    virtual bool serialize( IOSSerialize& se );
    virtual bool deserialize( IISDeserialize& de );
    virtual FormatInformationStreamsCompound* clone() const;

    sampling_frequency_t                     m_samplingFrequency;
    rate_control_t                           m_rateControl;
    number_of_stream_format_infos_t          m_numberOfStreamFormatInfos;

    typedef std::vector< StreamFormatInfo* > StreamFormatInfoVector;
    StreamFormatInfoVector                   m_streamFormatInfos;
};
std::ostream& operator<<( std::ostream& stream, FormatInformationStreamsCompound info );


////////////////////////////////////////////////////////////

class FormatInformation: public IBusData
{
public:
    enum EFormatHierarchyRoot {
        eFHR_DVCR            = AVC1394_STREAM_FORMAT_HIERARCHY_ROOT_DVCR,
        eFHR_AudioMusic      = AVC1394_STREAM_FORMAT_HIERARCHY_ROOT_AUDIOMUSIC,
        eFHR_Invalid         = AVC1394_STREAM_FORMAT_HIERARCHY_ROOT_INVALID,
    };

    enum EFomatHierarchyLevel1 {
        eFHL1_DVCR_SD525_60                     = AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_1_DVCR_SD525_60,
        eFHL1_DVCR_SDL525_60                    = AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_1_DVCR_SDL525_60,
        eFHL1_DVCR_HD1125_60                    = AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_1_DVCR_HD1125_60,
        eFHL1_DVCR_SD625_60                     = AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_1_DVCR_SD625_60,
        eFHL1_DVCR_SDL625_50                    = AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_1_DVCR_SDL625_50,
        eFHL1_DVCR_HD1250_50                    = AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_1_DVCR_HD1250_50,
        eFHL1_AUDIOMUSIC_AM824                  = AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_1_AUDIOMUSIC_AM824,
        eFHL1_AUDIOMUSIC_24_4_AUDIO_PACK        = AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_1_AUDIOMUSIC_24_4_AUDIO_PACK,
        eFHL1_AUDIOMUSIC_32_FLOATING            = AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_1_AUDIOMUSIC_32_FLOATING_POINT_DATA,
        eFHL1_AUDIOMUSIC_AM824_COMPOUND         = AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_1_AUDIOMUSIC_AM824_COMPOUND,
        eFHL1_AUDIOMUSIC_DONT_CARE              = AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_1_AUDIOMUSIC_DONT_CARE,
    };

    enum EFormatHierarchyLevel2 {
        eFHL2_AM824_IEC60968_3 = AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_IEC60968_3,
        eFHL2_AM824_IEC61937_3 = AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_IEC61937_3,
        eFHL2_AM824_IEC61937_4 = AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_IEC61937_4,
        eFHL2_AM824_IEC61937_5 = AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_IEC61937_5,
        eFHL2_AM824_IEC61937_6 = AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_IEC61937_6,
        eFHL2_AM824_IEC61937_7 = AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_IEC61937_7,
        eFHL2_AM824_MULTI_BIT_LINEAR_AUDIO_RAW = AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_MULTI_BIT_LINEAR_AUDIO_RAW,
        eFHL2_AM824_MULTI_BIT_LINEAR_AUDIO_DVD_AUDIO = AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_MULTI_BIT_LINEAR_AUDIO_DVD_AUDIO,
        eFHL2_AM824_ONE_BIT_AUDIO_PLAIN_RAW = AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_ONE_BIT_AUDIO_PLAIN_RAW,
        eFHL2_AM824_ONE_BIT_AUDIO_PLAIN_SACD = AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_ONE_BIT_AUDIO_PLAIN_SACD,
        eFHL2_AM824_ONE_BIT_AUDIO_ENCODED_RAW = AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_ONE_BIT_AUDIO_ENCODED_RAW,
        eFHL2_AM824_ONE_BIT_AUDIO_ENCODED_SACD = AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_ONE_BIT_AUDIO_ENCODED_SACD,
        eFHL2_AM824_HIGH_PRECISION_MULTIBIT_LINEAR_AUDIO = AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_HIGH_PRECISION_MULTIBIT_LINEAR_AUDIO,
        eFHL2_AM824_MIDI_CONFORMANT = AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_MIDI_CONFORMANT,
        eFHL2_AM824_SYNC_STREAM = AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_SYNC_STREAM,
        eFHL2_AM824_DONT_CARE = AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_DONT_CARE,
    };

    typedef byte_t format_hierarchy_root_t;
    typedef byte_t format_hierarchy_level1_t;
    typedef byte_t format_hierarchy_level2_t;

    FormatInformation();
    FormatInformation( const FormatInformation& rhs );
    virtual ~FormatInformation();

    virtual bool serialize( IOSSerialize& se );
    virtual bool deserialize( IISDeserialize& de );

    virtual FormatInformation* clone() const;

    format_hierarchy_root_t m_root;
    format_hierarchy_level1_t m_level1;
    format_hierarchy_level2_t m_level2;
    FormatInformationStreams* m_streams;
};

///////////////////////////////////////////////////////////

class ExtendedStreamFormatCmd: public AVCCommand
{
public:
    enum ESubFunction {
        eSF_Input                                               = AVC1394_STREAM_FORMAT_SUBFUNCTION_INPUT,
        eSF_Output                                              = AVC1394_STREAM_FORMAT_SUBFUNCTION_OUTPUT,
        eSF_ExtendedStreamFormatInformationCommand              = AVC1394_STREAM_FORMAT_SUBFUNCTION_EXTENDED_STREAM_FORMAT,
        eSF_ExtendedStreamFormatInformationCommandList          = AVC1394_STREAM_FORMAT_SUBFUNCTION_EXTENDED_STREAM_FORMAT_LIST,
    };

    enum EStatus {
        eS_Active         = AVC1394_EXTENDED_STREAM_FORMAT_INFO_STATUS_ACTIVE,
        eS_Inactive       = AVC1394_EXTENDED_STREAM_FORMAT_INFO_STATUS_INACTIVE,
        eS_NoStreamFormat = AVC1394_EXTENDED_STREAM_FORMAT_INFO_STATUS_NO_STREAM_FORMAT,
        eS_NotUsed        = AVC1394_EXTENDED_STREAM_FORMAT_INFO_STATUS_NOT_USED,
    };
    typedef byte_t status_t;
    typedef byte_t index_in_stream_format_t;

    ExtendedStreamFormatCmd( Ieee1394Service& ieee1349service, ESubFunction eSubFunction = eSF_ExtendedStreamFormatInformationCommand );
    ExtendedStreamFormatCmd( const ExtendedStreamFormatCmd& rhs );
    virtual ~ExtendedStreamFormatCmd();

    bool setPlugAddress( const PlugAddress& plugAddress );
    bool setIndexInStreamFormat( const int index );
    bool setSubFunction( ESubFunction subFunction );

    virtual bool serialize( IOSSerialize& se );
    virtual bool deserialize( IISDeserialize& de );

    EStatus getStatus();
    FormatInformation* getFormatInformation();
    index_in_stream_format_t getIndex();

    virtual const char* getCmdName() const
    { return "ExtendedStreamFormatCmd"; }

protected:
    subfunction_t            m_subFunction;
    PlugAddress*             m_plugAddress;
    status_t                 m_status;
    index_in_stream_format_t m_indexInStreamFormat;
    FormatInformation*       m_formatInformation;
};

}

#endif // AVCEXTENDEDSTREAMFROMAT_H
