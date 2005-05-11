/* stream_format.cpp
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

#include <libavc1394/avc1394.h>
#include <libraw1394/raw1394.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <netinet/in.h>

#include <vector>
#include <iostream>
#include <iomanip>

using namespace std;

#define AVC1394_STREAM_FORMAT_SUPPORT            (0x2F<<8)
#define AVC1394_STREAM_FORMAT_SUBFUNCTION_INPUT  0x00
#define AVC1394_STREAM_FORMAT_SUBFUNCTION_OUTPUT 0x01

// BridgeCo extensions
#define AVC1394_STREAM_FORMAT_SUBFUNCTION_EXTENDED_STREAM_FORMAT_SINGLE_REQUEST 0xC0
#define AVC1394_STREAM_FORMAT_SUBFUNCTION_EXTENDED_STREAM_FORMAT_LIST_REQUEST   0xC1


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


#define AVC1394_SET_OPERAND(x, n, v) x = (((x) & ~(0xFF000000 >> ((n)%4)*8 ) ) | ((v) << (((3-(n))%4)*8)))

class AVCCommand
{
public:
    enum EResponse {
        eR_Unknown        = 0,
        eR_NotImplemented = AVC1394_RESP_NOT_IMPLEMENTED,
        eR_Accepted       = AVC1394_RESP_ACCEPTED,
        eR_Rejected       = AVC1394_RESP_REJECTED,
        eR_InTransition   = AVC1394_RESP_IN_TRANSITION,
        eR_Implemented    = AVC1394_RESP_IMPLEMENTED,
        eR_Changed        = AVC1394_RESP_CHANGED,
        eR_Interim        = AVC1394_RESP_INTERIM,
    };

    virtual bool fire( raw1394handle_t handle, unsigned int node_id ) = 0;

    inline EResponse getResponse()
        {
            return m_eResponse;
        }

protected:
    AVCCommand()
        : m_eResponse( eR_Unknown )
        {}
    virtual ~AVCCommand()
        {}

    inline bool parseResponse( byte_t response )
        {
            m_eResponse = static_cast<EResponse>( response );
            return true;
        }

private:
    EResponse m_eResponse;
};

typedef byte_t plug_type_t;
typedef byte_t plug_id_t;
typedef byte_t reserved_t;
typedef byte_t function_block_type_t;
typedef byte_t function_block_id_t;
typedef byte_t plug_direction_t;
typedef byte_t plug_address_mode_t;
typedef byte_t status_t;

///////////////////////////////////////////////////////////
//////////////// De/Serialize Stuff ///////////////////////
///////////////////////////////////////////////////////////

class IOSSerialize {
public:
    virtual bool write( byte_t value, const char* name = "" ) = 0;
    virtual bool write( quadlet_t value, const char* name = "" ) = 0;
};

class CoutSerializer: public IOSSerialize {
public:
    virtual bool write( byte_t value, const char* name = "" );
    virtual bool write( quadlet_t value,  const char* name = "" );
};

bool
CoutSerializer::write( byte_t d, const char* name )
{
    cout << name << ": 0x" << setfill( '0' ) << hex << static_cast<unsigned int>( d ) << endl;
    return true;
}

bool
CoutSerializer::write( quadlet_t d, const char* name )
{
    cout << name << ": 0x" << setfill( '0' ) << setw( 8 ) << hex << d << endl;
    return true;
}

class BufferSerialize: public IOSSerialize {
public:
    BufferSerialize( unsigned char* buffer, size_t length )
        : m_buffer( buffer )
        , m_curPos( m_buffer )
        , m_length( length )
        {}

    virtual bool write( byte_t value, const char* name = "" );
    virtual bool write( quadlet_t value,  const char* name = "" );

protected:
    inline bool isCurPosValid() const;

private:
    unsigned char* m_buffer;
    unsigned char* m_curPos;
    size_t m_length;
};

bool
BufferSerialize::write( byte_t value, const char* name )
{
    bool result = false;
    if ( isCurPosValid() ) {
        *m_curPos = value;
        m_curPos += sizeof( byte_t );
        result = true;
    }
    return result;
}

bool
BufferSerialize::write( quadlet_t value,  const char* name )
{
    bool result = false;
    if ( isCurPosValid() ) {
        * ( quadlet_t* )m_curPos = value;
        m_curPos += sizeof( quadlet_t );
        result = true;
    }
    return result;
}

bool
BufferSerialize::isCurPosValid() const
{
    if ( static_cast<size_t>( ( m_curPos - m_buffer ) ) >= m_length ) {
        return false;
    }
    return true;
}

//////////////////////////////////////////////////

class IISDeserialize {
public:
    virtual bool read( byte_t* value ) = 0;
    virtual bool read( quadlet_t* value ) = 0;
};

class BufferDeserialize: public IISDeserialize {
public:
    BufferDeserialize( const unsigned char* buffer, size_t length )
        : m_buffer( const_cast<unsigned char*>( buffer ) )
        , m_curPos( m_buffer )
        , m_length( length )
        {}

    virtual bool read( byte_t* value );
    virtual bool read( quadlet_t* value );

protected:
    inline bool isCurPosValid() const;

private:
    unsigned char* m_buffer; // start of the buffer
    unsigned char* m_curPos; // current read pos
    size_t m_length; // length of buffer
};

bool
BufferDeserialize::read( byte_t* value )
{
    bool result = false;
    if ( isCurPosValid() ) {
        *value = *m_curPos;
        m_curPos += sizeof( byte_t );
        result = true;
    }
    return result;
}

bool
BufferDeserialize::read( quadlet_t* value )
{
    bool result = false;
    if ( isCurPosValid() ) {
        *value = *m_curPos;
        m_curPos += sizeof( quadlet_t );
        result = true;;
    }
    return result;
}

bool
BufferDeserialize::isCurPosValid() const
{
    if ( static_cast<size_t>( ( m_curPos - m_buffer ) ) >= m_length ) {
        return false;
    }
    return true;
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////



class IBusData {
public:
    virtual bool serialize( IOSSerialize& se ) = 0;
    virtual bool deserialize( IISDeserialize& de ) = 0;

    virtual IBusData* clone() const = 0;
};

class PlugAddressData : public IBusData {
};

class UnitPlugAddress : public PlugAddressData
{
public:
    UnitPlugAddress( plug_id_t plugType,  plug_type_t plugId )
        : m_plugType( plugType )
        , m_plugId( plugId )
        , m_reserved( 0xff )
        {}

    virtual bool serialize( IOSSerialize& se )
        {
            se.write( m_plugType, "UnitPlugAddress plugType" );
            se.write( m_plugId, "UnitPlugAddress plugId" );
            se.write( m_reserved, "UnitPlugAddress reserved" );
            return true;
        }

    virtual bool deserialize( IISDeserialize& de )
        {
            de.read( &m_plugType );
            de.read( &m_plugId );
            de.read( &m_reserved );
            return true;
        }

    virtual UnitPlugAddress* clone() const
        {
            return new UnitPlugAddress( *this );
        }

    plug_id_t   m_plugType;
    plug_type_t m_plugId;
    reserved_t  m_reserved;
};

class SubunitPlugAddress : public PlugAddressData
{
public:
   SubunitPlugAddress( plug_id_t plugId )
       : m_plugId( plugId )
       , m_reserved0( 0xff )
       , m_reserved1( 0xff )
        {}

    virtual bool serialize( IOSSerialize& se )
        {
            se.write( m_plugId, "SubunitPlugAddress plugId" );
            se.write( m_reserved0, "SubunitPlugAddress reserved0" );
            se.write( m_reserved1, "SubunitPlugAddress reserved1" );
            return true;
        }

    virtual bool deserialize( IISDeserialize& de )
        {
            de.read( &m_plugId );
            de.read( &m_reserved0 );
            de.read( &m_reserved1 );
            return true;
        }

    virtual SubunitPlugAddress* clone() const
        {
            return new SubunitPlugAddress( *this );
        }

    plug_id_t m_plugId;
    reserved_t m_reserved0;
    reserved_t m_reserved1;
};

class FunctionBlockPlugAddress : public PlugAddressData
{
public:
    FunctionBlockPlugAddress( function_block_type_t functionBlockType,
                              function_block_id_t functionBlockId,
                              plug_id_t plugId )
        : m_functionBlockType( functionBlockType )
        , m_functionBlockId( functionBlockId )
        , m_plugId( plugId )
        {}

    virtual bool serialize( IOSSerialize& se )
        {
            se.write( m_functionBlockType, "FunctionBlockPlugAddress functionBlockType" );
            se.write( m_functionBlockId, "FunctionBlockPlugAddress functionBlockId" );
            se.write( m_plugId, "FunctionBlockPlugAddress plugId" );
            return true;
        }

    virtual bool deserialize( IISDeserialize& de )
        {
            de.read( &m_functionBlockType );
            de.read( &m_functionBlockId );
            de.read( &m_plugId );
            return true;
        }

    virtual FunctionBlockPlugAddress* clone() const
        {
            return new FunctionBlockPlugAddress( *this );
        }

    function_block_type_t m_functionBlockType;
    function_block_id_t   m_functionBlockId;
    plug_id_t             m_plugId;
};

class PlugAddress : public IBusData {
public:
    enum EMode {
        eM_Subunit = 0x00,
        eM_Plugin  = 0x01,
    };

    enum EPlugDirection {
        ePD_Input  = 0x00,
        ePD_Output = 0x01,
    };

    enum EPlugAddressMode {
        ePAM_Unit          = 0x00,
        ePAM_Subunit       = 0x01,
        ePAM_FunctionBlock = 0x02,
    };

    PlugAddress( plug_direction_t plugDirection,
                 plug_address_mode_t plugAddressMode,
                 UnitPlugAddress& unitPlugAddress );

    PlugAddress( plug_direction_t plugDirection,
                 plug_address_mode_t plugAddressMode,
                 SubunitPlugAddress& subUnitPlugAddress );

    PlugAddress( plug_direction_t plugDirection,
                 plug_address_mode_t plugAddressMode,
                 FunctionBlockPlugAddress& functionBlockPlugAddress );

    PlugAddress( const PlugAddress& pa );

    virtual ~PlugAddress();

    virtual bool serialize( IOSSerialize& se );
    virtual bool deserialize( IISDeserialize& de );

    virtual PlugAddress* clone() const
    	{
            return new PlugAddress( *this );
    	}

    plug_direction_t    m_plugDirection;
    plug_address_mode_t m_addressMode;

    PlugAddressData*    m_plugAddressData;
};

PlugAddress::PlugAddress( plug_direction_t plugDirection,
                          plug_address_mode_t plugAddressMode,
                          UnitPlugAddress& unitPlugAddress )
    : m_plugDirection( plugDirection )
    , m_addressMode( plugAddressMode )
    , m_plugAddressData( new UnitPlugAddress( unitPlugAddress ) )
{
}

PlugAddress::PlugAddress( plug_direction_t plugDirection,
                          plug_address_mode_t plugAddressMode,
                          SubunitPlugAddress& subUnitPlugAddress )
    : m_plugDirection( plugDirection )
    , m_addressMode( plugAddressMode )
    , m_plugAddressData( new SubunitPlugAddress( subUnitPlugAddress ) )
{
}

PlugAddress::PlugAddress( plug_direction_t plugDirection,
                          plug_address_mode_t plugAddressMode,
                          FunctionBlockPlugAddress& functionBlockPlugAddress )
    : m_plugDirection( plugDirection )
    , m_addressMode( plugAddressMode )
    , m_plugAddressData( new FunctionBlockPlugAddress( functionBlockPlugAddress ) )
{
}

PlugAddress::PlugAddress( const PlugAddress& pa )
    : m_plugDirection( pa.m_plugDirection )
    , m_addressMode( pa.m_addressMode )
    , m_plugAddressData( dynamic_cast<PlugAddressData*>( pa.m_plugAddressData->clone() ) )
{
}

PlugAddress::~PlugAddress()
{
    delete m_plugAddressData;
    m_plugAddressData = 0;
}

bool
PlugAddress::serialize( IOSSerialize& se )
{
    se.write( m_plugDirection, "PlugAddress plugDirection" );
    se.write( m_addressMode, "PlugAddress addressMode" );
    return m_plugAddressData->serialize( se );
}

bool
PlugAddress::deserialize( IISDeserialize& de )
{
    de.read( &m_plugDirection );
    de.read( &m_addressMode );
    return m_plugAddressData->deserialize( de );
}


class StreamFormatInfo: public IBusData
{
public:
    StreamFormatInfo();

    virtual bool serialize( IOSSerialize& se );
    virtual bool deserialize( IISDeserialize& de );

    virtual StreamFormatInfo* clone() const;

    byte_t m_numberOfChannels;
    byte_t m_streamFormat;
};

StreamFormatInfo::StreamFormatInfo()
    : IBusData()
{
}

bool
StreamFormatInfo::serialize( IOSSerialize& se )
{
    se.write( m_numberOfChannels, "StreamFormatInfo numberOfChannels" );
    se.write( m_streamFormat, "StreamFormatInfo streamFormat" );
    return true;
}

bool
StreamFormatInfo::deserialize( IISDeserialize& de )
{
    de.read( &m_numberOfChannels );
    de.read( &m_streamFormat );
    return true;
}


StreamFormatInfo*
StreamFormatInfo::clone() const
{
    return new StreamFormatInfo( *this );
}

class FormatInformationStreams: public IBusData
{
};

// just empty declaration of sync stream
class FormatInformationStreamsSync: public FormatInformationStreams
{
public:
    FormatInformationStreamsSync();

    virtual bool serialize( IOSSerialize& se );
    virtual bool deserialize( IISDeserialize& de );
    virtual FormatInformationStreamsSync* clone() const;
};

FormatInformationStreamsSync::FormatInformationStreamsSync()
    : FormatInformationStreams()
{
}

bool
FormatInformationStreamsSync::serialize( IOSSerialize& se )
{
    return true;
}

bool
FormatInformationStreamsSync::deserialize( IISDeserialize& de )
{
    return true;
}

FormatInformationStreamsSync*
FormatInformationStreamsSync::clone() const
{
    return new FormatInformationStreamsSync( *this );
}

// Who's the owner of this enum?
enum ESamplingFrequency {
    eSF_22050Hz = 0x00,
    eSF_24000Hz = 0x01,
    eSF_32000Hz = 0x02,
    eSF_44100Hz = 0x03,
    eSF_48000Hz = 0x04,
    eSF_88200Hz = 0x0A,
    eSF_96000Hz = 0x05,
    eSF_176400Hz = 0x06,
    eSF_192000Hz = 0x07,
    eSF_DontCare = 0x0F,
};

enum ERateControl {
    eRC_Supported = 0x00,
    eRC_DontCare  = 0x01,
};

class FormatInformationStreamsCompound: public FormatInformationStreams
{
public:
    FormatInformationStreamsCompound();
    virtual ~FormatInformationStreamsCompound();

    virtual bool serialize( IOSSerialize& se );
    virtual bool deserialize( IISDeserialize& de );
    virtual FormatInformationStreamsCompound* clone() const;

    byte_t m_samplingFrequency;
    byte_t m_rateControl;
    byte_t m_numberOfStreamFormatInfos;
    typedef std::vector< StreamFormatInfo* > StreamFormatInfoVector;
    StreamFormatInfoVector m_streamFormatInfos;
};

FormatInformationStreamsCompound::FormatInformationStreamsCompound()
    : FormatInformationStreams()
    , m_samplingFrequency( eSF_DontCare )
    , m_rateControl( eRC_DontCare )
    , m_numberOfStreamFormatInfos( 0 )
{
}

FormatInformationStreamsCompound::~FormatInformationStreamsCompound()
{
    for ( StreamFormatInfoVector::iterator it = m_streamFormatInfos.begin();
          it != m_streamFormatInfos.end();
          ++it )
    {
        delete *it;
    }
}

bool
FormatInformationStreamsCompound::serialize( IOSSerialize& se )
{
    se.write( m_samplingFrequency, "FormatInformationStreamsCompound samplingFrequency" );
    se.write( m_rateControl, "FormatInformationStreamsCompound rateControl" );
    se.write( m_numberOfStreamFormatInfos, "FormatInformationStreamsCompound numberOfStreamFormatInfos" );
    for ( StreamFormatInfoVector::iterator it = m_streamFormatInfos.begin();
          it != m_streamFormatInfos.end();
          ++it )
    {
        ( *it )->serialize( se );
    }
    return true;
}

bool
FormatInformationStreamsCompound::deserialize( IISDeserialize& de )
{
    de.read( &m_samplingFrequency );
    de.read( &m_rateControl );
    de.read( &m_numberOfStreamFormatInfos );
    for ( int i = 0; i < m_numberOfStreamFormatInfos; ++i ) {
        StreamFormatInfo* streamFormatInfo = new StreamFormatInfo;
        if ( !streamFormatInfo->deserialize( de ) ) {
            return false;
        }
        m_streamFormatInfos.push_back( streamFormatInfo );
    }
    return true;
}

FormatInformationStreamsCompound*
FormatInformationStreamsCompound::clone() const
{
    return new FormatInformationStreamsCompound( *this );
}

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

    FormatInformation();
    virtual ~FormatInformation();

    virtual bool serialize( IOSSerialize& se );
    virtual bool deserialize( IISDeserialize& de );

    virtual FormatInformation* clone() const;

    FormatInformationStreams* m_streams;
};

FormatInformation::FormatInformation()
    : IBusData()
    , m_streams( 0 )
{
}

FormatInformation::~FormatInformation()
{
    delete m_streams;
    m_streams = 0;
}

bool
FormatInformation::serialize( IOSSerialize& se )
{
    if ( m_streams ) {
        return m_streams->serialize( se );
    }
    return true;
}

bool
FormatInformation::deserialize( IISDeserialize& de )
{
    bool result = false;

    // this code just parses BeBoB replies.
    byte_t level0;
    de.read( &level0 );

    // just parse an audio and music hierarchy
    if ( level0 == eFHR_AudioMusic ) {
        byte_t level1;
        de.read( &level1 );

        switch ( level1 ) {
        case eFHL1_AUDIOMUSIC_AM824:
        {
            // note: compound streams don't have a second level

            byte_t level2;
            de.read( &level2 );

            if (level2 == eFHL2_AM824_SYNC_STREAM ) {
                printf( "...sync stream...\n" );
                m_streams = new FormatInformationStreamsSync();
                result = m_streams->deserialize( de );
            } else {
                // anything but the sync stream workds currently.
                printf( "could not parse format information. (format hierarchy level 2 not recognized)\n" );
            }
        }
        break;
        case  eFHL1_AUDIOMUSIC_AM824_COMPOUND:
        {
            printf( "..compound stream...\n" );
            m_streams = new FormatInformationStreamsCompound();
            result = m_streams->deserialize( de );
        }
        break;
        default:
            printf( "could not parse format information. (format hierarchy level 1 not recognized)\n" );
        }
    }

    return result;
}

FormatInformation*
FormatInformation::clone() const
{
    return new FormatInformation( *this );
}

class ExtendedStreamFormatCmd: public AVCCommand
{
public:
    enum ESubFunction {
        eSF_Input                                               = AVC1394_STREAM_FORMAT_SUBFUNCTION_INPUT,
        eSF_Output                                              = AVC1394_STREAM_FORMAT_SUBFUNCTION_OUTPUT,
        eSF_ExtendedStreamFormatInformationCommandSingleRequest = AVC1394_STREAM_FORMAT_SUBFUNCTION_EXTENDED_STREAM_FORMAT_SINGLE_REQUEST,
        eSF_ExtendedStreamFormatInformationCommandListRequest   = AVC1394_STREAM_FORMAT_SUBFUNCTION_EXTENDED_STREAM_FORMAT_LIST_REQUEST,
    };

    enum EStatus {
        eS_Active         = AVC1394_EXTENDED_STREAM_FORMAT_INFO_STATUS_ACTIVE,
        eS_Inactive       = AVC1394_EXTENDED_STREAM_FORMAT_INFO_STATUS_INACTIVE,
        eS_NoStreamFormat = AVC1394_EXTENDED_STREAM_FORMAT_INFO_STATUS_NO_STREAM_FORMAT,
        eS_NotUsed        = AVC1394_EXTENDED_STREAM_FORMAT_INFO_STATUS_NOT_USED,
    };

    ExtendedStreamFormatCmd( ESubFunction eSubFunction );
    virtual ~ExtendedStreamFormatCmd();

    bool setPlugAddress( const PlugAddress& plugAddress );
    bool setIndexInStreamFormat( const int index );

    bool serialize( IOSSerialize& se );
    bool serialize2( IOSSerialize& se );
    bool deserialize( IISDeserialize& de );

    virtual bool fire( raw1394handle_t handle, unsigned int node_id );

protected:
    ESubFunction m_eSubFunction;
    PlugAddress* m_plugAddress;
    byte_t       m_indexInStreamFormat;
    FormatInformation* m_formatInformation;
};

ExtendedStreamFormatCmd::ExtendedStreamFormatCmd( ESubFunction eSubFunction )
    : AVCCommand()
    , m_eSubFunction( eSubFunction )
//    , m_plugAddress( new PlugAddress( PlugAddress::eM_Subunit, PlugAddress::ePD_Input, UnitPlugAddress( 0x00, 0x00 ) ) )
    , m_indexInStreamFormat( 0 )
    , m_formatInformation( 0 )
{
    UnitPlugAddress unitPlugAddress( 0x00, 0x00 );
    m_plugAddress = new PlugAddress( PlugAddress::eM_Subunit, PlugAddress::ePD_Input, unitPlugAddress );
}

ExtendedStreamFormatCmd::~ExtendedStreamFormatCmd()
{
    delete m_plugAddress;
    m_plugAddress = 0;
    delete m_formatInformation;
    m_formatInformation = 0;
}

bool
ExtendedStreamFormatCmd::setPlugAddress( const PlugAddress& plugAddress )
{
    delete m_plugAddress;
    m_plugAddress = plugAddress.clone();
    return true;
}

bool
ExtendedStreamFormatCmd::setIndexInStreamFormat( const int index )
{
    m_indexInStreamFormat = index;
    return true;
}

bool
ExtendedStreamFormatCmd::serialize( IOSSerialize& se )
{
    quadlet_t header =
        AVC1394_CTYPE_STATUS
	| AVC1394_SUBUNIT_TYPE_UNIT
	| AVC1394_SUBUNIT_ID_IGNORE
	| AVC1394_STREAM_FORMAT_SUPPORT
        | m_eSubFunction;

    se.write( header, "AVC header" );
    m_plugAddress->serialize( se );
    if ( m_eSubFunction == eSF_ExtendedStreamFormatInformationCommandListRequest ) {
        se.write( m_indexInStreamFormat, "indexInStreamFormat" );
    }
    return true;
}

bool
ExtendedStreamFormatCmd::serialize2( IOSSerialize& se )
{
    return m_formatInformation->serialize( se );
}

bool
ExtendedStreamFormatCmd::deserialize( IISDeserialize& de )
{
    return m_formatInformation->deserialize( de );
}


bool
ExtendedStreamFormatCmd::fire( raw1394handle_t handle, unsigned int node_id )
{
    #define STREAM_FORMAT_REQUEST_SIZE 10 // XXX random length
    union UPacket {
        quadlet_t     quadlet[STREAM_FORMAT_REQUEST_SIZE];
        unsigned char byte[STREAM_FORMAT_REQUEST_SIZE*4];
    };
    typedef union UPacket packet_t;

    packet_t  req;
    packet_t* resp;

    // initialize complete packet
    memset( &req,  0xff,  sizeof( req ) );

    // set first quadlet (which holds the header, the opcode and the first operand.
    // the first operand is handle here to avoid complicated reordering (endian problems)
    // later on.

    BufferSerialize se( req.byte, sizeof( req ) );
    if ( !serialize( se ) ) {
        printf(  "ExtendedStreamFormatCmd::fire: Could not serialize comanndo\n" );
        return false;
    }

    // reorder the bytes to the correct layout
    // note that the first quadlet is already in the correct order
    for (int i = 1; i < STREAM_FORMAT_REQUEST_SIZE; ++i) {
        req.quadlet[i] = ntohl( req.quadlet[i] );
    }

    // debug output
    puts("request:");
    for (int i = 0; i < STREAM_FORMAT_REQUEST_SIZE; ++i) {
          printf("  %2d: 0x%08x\n", i, req.quadlet[i]);
    }

    resp = reinterpret_cast<packet_t*> ( avc1394_transaction_block( handle, node_id, req.quadlet, STREAM_FORMAT_REQUEST_SIZE,  1 ) );
    if ( resp ) {
        for ( int i = 0; i < STREAM_FORMAT_REQUEST_SIZE; ++i ) {
            resp->quadlet[i] = htonl( resp->quadlet[i] );
        }

        // debug output
        puts("response:");
        for ( int i = 0; i < STREAM_FORMAT_REQUEST_SIZE; ++i ) {
	    printf( "  %2d: 0x%08x\n", i, resp->quadlet[i] );
	}
        printf( "\n" );
        printf( " idx type                       value\n" );
        printf( "-------------------------------------\n" );
        printf( "  %02d                     ctype: 0x%02x\n", 0, resp->byte[0] );
        printf( "  %02d subunit_type + subunit_id: 0x%02x\n", 1, resp->byte[1] );
        printf( "  %02d                    opcode: 0x%02x\n", 2, resp->byte[2] );

        for ( int i = 3; i < STREAM_FORMAT_REQUEST_SIZE * 4; ++i ) {
            printf( "  %02d                operand %2d: 0x%02x\n", i, i-3, resp->byte[i] );
        }

        // check output
        parseResponse( resp->byte[0] );
        if ( getResponse() == eR_Implemented ) {
            // deserialize here
            #define AVC_CMD_HEADER_SIZE 3
            #define EXTENDED_STREAM_FORMAT_OPERAND_POS_STATUS 6
            switch ( resp->byte[AVC_CMD_HEADER_SIZE + EXTENDED_STREAM_FORMAT_OPERAND_POS_STATUS] ) {
            case eS_Active:
                printf( "format is set and a signal is streaming through or to the plug\n" );
                break;
            case eS_Inactive:
                printf( "format is set but no signal is stremaing through the plug\n" );
                break;
            case eS_NoStreamFormat:
                printf( "there is no stream format set, plug has no streaming capabilities\n" );
                break;
            case eS_NotUsed:
                printf( "not used: used for specific inquiry response\n" );
                break;
            default:
                printf( "unknown status\n" );
            }

            // maybe auto pointers might a better idea
            if (m_formatInformation ) {
                delete m_formatInformation;
            }
            m_formatInformation = new FormatInformation;

            // let's parse the answer
            byte_t* ptr;
            if ( m_eSubFunction == eSF_ExtendedStreamFormatInformationCommandSingleRequest ) {
                ptr = &( resp->byte[AVC_CMD_HEADER_SIZE + 7] );
            } else {
                ptr = &( resp->byte[AVC_CMD_HEADER_SIZE + 8] );
            }
            BufferDeserialize de( ptr, resp->byte - ptr );
            m_formatInformation->deserialize( de );
        } else if ( ( getResponse() == eR_Rejected )
                    && ( m_eSubFunction == eSF_ExtendedStreamFormatInformationCommandListRequest ) ) {
            printf( "no futher stream formats defined\n" );
        } else {
            printf( "unexpected response received\n" );
        }
    } else {
	printf("no response\n");
    }
    return true;
}


////////////////////////////////////////
// Test application
///////////////////////////////////////

int
main(int argc, char **argv)
{
    if (argc < 3) {
	printf("usage: %s <NODE_ID> <PLUG_ID>\n", argv[0]);
	return 0;
    }

    errno = 0;
    char* tail;
    int node_id = strtol(argv[1], &tail, 0);
    if (errno) {
	perror("argument parsing failed:");
	return -1;
    }
    int plug_id = strtol(argv[2], &tail, 0);
    if (errno) {
	perror("argument parsing failed:");
	return -1;
    }

    raw1394handle_t handle;
    handle = raw1394_new_handle ();
    if (!handle) {
	if (!errno) {
	    perror("lib1394raw not compatable\n");
	} else {
	    fprintf(stderr, "Could not get 1394 handle");
	    fprintf(stderr, "Is ieee1394, driver, and raw1394 loaded?\n");
	}
	return -1;
    }

    if (raw1394_set_port(handle, 0) < 0) {
	fprintf(stderr, "Could not set port");
	raw1394_destroy_handle (handle);
	return -1;
    }

    ExtendedStreamFormatCmd extendedStreamFormatCmd = ExtendedStreamFormatCmd( ExtendedStreamFormatCmd::eSF_ExtendedStreamFormatInformationCommandListRequest );
    UnitPlugAddress unitPlugAddress( 0x00,  plug_id );
    extendedStreamFormatCmd.setPlugAddress( PlugAddress( PlugAddress::eM_Subunit,
                                                         PlugAddress::ePD_Input,
                                                         unitPlugAddress ) );
/*
    extendedStreamFormatCmd.setIndexInStreamFormat( 0 );
    extendedStreamFormatCmd.fire( handle, node_id );
    extendedStreamFormatCmd.setIndexInStreamFormat( 1 );
    extendedStreamFormatCmd.fire( handle, node_id );
    extendedStreamFormatCmd.setIndexInStreamFormat( 2 );
    extendedStreamFormatCmd.fire( handle, node_id );
    extendedStreamFormatCmd.setIndexInStreamFormat( 3 );
    extendedStreamFormatCmd.fire( handle, node_id );
    extendedStreamFormatCmd.setIndexInStreamFormat( 4 );
    extendedStreamFormatCmd.fire( handle, node_id );
*/
    ExtendedStreamFormatCmd extendedStreamFormatCmdS = ExtendedStreamFormatCmd( ExtendedStreamFormatCmd::eSF_ExtendedStreamFormatInformationCommandSingleRequest );
    extendedStreamFormatCmdS.setPlugAddress( PlugAddress( PlugAddress::eM_Subunit,
                                                          PlugAddress::ePD_Input,
                                                          unitPlugAddress ) );
    extendedStreamFormatCmdS.fire( handle, node_id );

    CoutSerializer se;
    extendedStreamFormatCmdS.serialize2( se );

    raw1394_destroy_handle( handle );

    return 0;
}
