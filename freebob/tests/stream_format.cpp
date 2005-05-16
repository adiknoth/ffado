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

#include <argp.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <netinet/in.h>

#include <vector>
#include <iostream>
#include <iomanip>
#include <iterator>

using namespace std;

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


#define AVC1394_SET_OPERAND(x, n, v) x = (((x) & ~(0xFF000000 >> ((n)%4)*8 ) ) | ((v) << (((3-(n))%4)*8)))

// defs which different classes are using
typedef byte_t ctype_t;
typedef byte_t subunit_t;
typedef byte_t opcode_t;
typedef byte_t plug_type_t;
typedef byte_t plug_id_t;
typedef byte_t reserved_t;
typedef byte_t function_block_type_t;
typedef byte_t function_block_id_t;
typedef byte_t plug_direction_t;
typedef byte_t plug_address_mode_t;
typedef byte_t status_t;
typedef byte_t number_of_channels_t;
typedef byte_t stream_format_t;
typedef byte_t sampling_frequency_t;
typedef byte_t rate_control_t;
typedef byte_t number_of_stream_format_infos_t;

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

ostream& operator<<( ostream& stream, ESamplingFrequency freq )
{
    char* str;
    switch ( freq ) {
    case eSF_22050Hz:
        str = "22050";
        break;
    case eSF_24000Hz:
        str = "24000";
        break;
    case eSF_32000Hz:
        str = "32000";
        break;
    case eSF_44100Hz:
        str = "44100";
        break;
    case eSF_48000Hz:
        str = "48000";
        break;
    case eSF_88200Hz:
        str = "88200";
        break;
    case eSF_96000Hz:
        str = "96000";
        break;
    case eSF_176400Hz:
        str = "176400";
        break;
    case eSF_192000Hz:
        str = "192000";
        break;
    case eSF_DontCare:
    default:
        str = "unknown";
    }
    return stream << str;
};

enum ERateControl {
    eRC_Supported = 0x00,
    eRC_DontCare  = 0x01,
};


////////////////////////////////////////////////
// arg parsing
////////////////////////////////////////////////
const char *argp_program_version = "stream_format 0.1";
const char *argp_program_bug_address = "<freebob-devel@lists.sf.net>";
static char doc[] = "stream_format -- get/set test program for FreeBob";
static char args_doc[] = "NODE_ID PLUG_ID";
static struct argp_option options[] = {
    {"verbose",   'v', 0,           0,            "Produce verbose output" },
    {"test",      't', 0,           0,            "Do tests instead get/set action" },
    {"frequency", 'f', "FREQUENCY", 0,  "Set frequency" },
    {"port",      'p', "PORT",      0,  "Set port" },
   { 0 }
};

struct arguments
{
    arguments()
        : verbose( false )
        , test( false )
        , frequency( 0 )
        , port( 0 )
        {
            args[0] = 0;
            args[1] = 0;
        }

    char* args[2];
    bool  verbose;
    bool  test;
    int   frequency;
    int   port;
} arguments;

// Parse a single option.
static error_t
parse_opt( int key, char* arg, struct argp_state* state )
{
    // Get the input argument from `argp_parse', which we
    // know is a pointer to our arguments structure.
    struct arguments* arguments = ( struct arguments* ) state->input;

    char* tail;
    switch (key) {
    case 'v':
        arguments->verbose = true;
        break;
    case 't':
        arguments->test = true;
        break;
    case 'f':
        errno = 0;
        arguments->frequency = strtol(arg, &tail, 0);
        if (errno) {
            perror("argument parsing failed:");
            return errno;
        }
    case 'p':
        errno = 0;
        arguments->port = strtol(arg, &tail, 0);
        if (errno) {
            perror("argument parsing failed:");
            return errno;
        }
    case ARGP_KEY_ARG:
        if (state->arg_num >= 2) {
            // Too many arguments.
            argp_usage (state);
        }
        arguments->args[state->arg_num] = arg;
        break;
    case ARGP_KEY_END:
        if (state->arg_num < 2) {
            // Not enough arguments.
            argp_usage (state);
        }
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

////////////////////////////////////////////////


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

////////////////////////////////////////////////////////////

class PlugAddressData : public IBusData {
};

////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////

class FormatInformationStreams: public IBusData
{
public:
    FormatInformationStreams() {}
    virtual ~FormatInformationStreams() {}
};

////////////////////////////////////////////////////////////

// just empty declaration of sync stream
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

FormatInformationStreamsSync::FormatInformationStreamsSync()
    : FormatInformationStreams()
    , m_reserved0( 0xff )
    , m_samplingFrequency( eSF_DontCare )
    , m_rateControl( eRC_DontCare )
    , m_reserved1( 0xff )
{
}

bool
FormatInformationStreamsSync::serialize( IOSSerialize& se )
{
    se.write( m_reserved0, "FormatInformationStreamsSync reserved" );

    // we have to clobber some bits
    byte_t operand = ( m_samplingFrequency << 4 ) | 0x0e;
    if ( m_rateControl == eRC_DontCare) {
        operand |= 0x1;
    }
    se.write( operand, "FormatInformationStreamsSync sampling frequency and rate control" );

    se.write( m_reserved1, "FormatInformationStreamsSync reserved" );
    return true;
}

bool
FormatInformationStreamsSync::deserialize( IISDeserialize& de )
{
    de.read( &m_reserved0 );

    byte_t operand;
    de.read( &operand );
    m_samplingFrequency = operand >> 4;
    m_rateControl = operand & 0x01;

    de.read( &m_reserved1 );
    return true;
}

////////////////////////////////////////////////////////////

FormatInformationStreamsSync*
FormatInformationStreamsSync::clone() const
{
    return new FormatInformationStreamsSync( *this );
}


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
    virtual ~FormatInformation();

    virtual bool serialize( IOSSerialize& se );
    virtual bool deserialize( IISDeserialize& de );

    virtual FormatInformation* clone() const;

    format_hierarchy_root_t m_root;
    format_hierarchy_level1_t m_level1;
    format_hierarchy_level2_t m_level2;
    FormatInformationStreams* m_streams;
};

FormatInformation::FormatInformation()
    : IBusData()
    , m_root( eFHR_Invalid )
    , m_level1( eFHL1_AUDIOMUSIC_DONT_CARE )
    , m_level2( eFHL2_AM824_DONT_CARE )
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
    if ( m_root != eFHR_Invalid ) {
        se.write( m_root, "FormatInformation hierarchy root" );
        if ( m_level1 != eFHL1_AUDIOMUSIC_DONT_CARE ) {
            se.write( m_level1, "FormatInformation hierarchy level 1" );
            if ( m_level2 != eFHL2_AM824_DONT_CARE ) {
                se.write( m_level2, "FormatInformation hierarchy level 2" );
            }
        }
    }
    if ( m_streams ) {
        return m_streams->serialize( se );
    }
    return true;
}

bool
FormatInformation::deserialize( IISDeserialize& de )
{
    bool result = false;

    delete m_streams;
    m_streams = 0;

    // this code just parses BeBoB replies.
    de.read( &m_root );

    // just parse an audio and music hierarchy
    if ( m_root == eFHR_AudioMusic ) {
        de.read( &m_level1 );

        switch ( m_level1 ) {
        case eFHL1_AUDIOMUSIC_AM824:
        {
            // note: compound streams don't have a second level
            de.read( &m_level2 );

            if (m_level2 == eFHL2_AM824_SYNC_STREAM ) {
                if ( arguments.verbose ) {
                    printf( "+-------------------+\n" );
                    printf( "|  sync stream      |\n" );
                    printf( "+-------------------+\n" );
                }
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
            if ( arguments.verbose ) {
                printf( "+-------------------+\n" );
                printf( "|  compound stream  |\n" );
                printf( "+-------------------+\n" );
            }
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


///////////////////////////////////////////////////////////
////////////////// AVC Commands ///////////////////////////
///////////////////////////////////////////////////////////

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

    enum ECommandType {
        eCT_Control         = AVC1394_CTYP_CONTROL,
        eCT_Status          = AVC1394_CTYP_STATUS,
        eCT_SpecificInquiry = AVC1394_CTYP_SPECIFIC_INQUIRY,
        eCT_Notify          = AVC1394_CTYP_NOTIFY,
        eCT_GeneralInquiry  = AVC1394_CTYP_GENERAL_INQUIRY,
        eCT_Unknown         = 0xff,
    };
    typedef byte_t subfunction_t;
    typedef byte_t opcode_t;

    virtual bool serialize( IOSSerialize& se );
    virtual bool deserialize( IISDeserialize& de );
    virtual bool fire(ECommandType commandType,
                      raw1394handle_t handle,
                      unsigned int node_id ) = 0;

    inline EResponse getResponse();

protected:
    AVCCommand()
        : m_ctype( eCT_Unknown )
        , m_subunit( 0xff )
        , m_eResponse( eR_Unknown )
        {}
    virtual ~AVCCommand()
        {}

    inline bool parseResponse( byte_t response );
    bool setCommandType( ECommandType commandType );

private:
    ctype_t   m_ctype;
    subunit_t m_subunit; // Can't be set/get ATM (doesn't matter anyway because it's almost
                         // every time 0xff.

    EResponse m_eResponse;
};

bool
AVCCommand::serialize( IOSSerialize& se )
{
    se.write( m_ctype, "AVCCommand ctype" );
    se.write( m_subunit, "AVCCommand subunit" );
    return true;
}

bool
AVCCommand::deserialize( IISDeserialize& de )
{
    de.read( &m_ctype );
    de.read( &m_subunit );
    return true;
}

bool
AVCCommand::setCommandType( ECommandType commandType )
{
    m_ctype = commandType;
    return true;
}

AVCCommand::EResponse
AVCCommand::getResponse()
{
    return m_eResponse;
}

bool
AVCCommand::parseResponse( byte_t response )
{
    m_eResponse = static_cast<EResponse>( response );
    return true;
}

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

    ExtendedStreamFormatCmd( ESubFunction eSubFunction = eSF_ExtendedStreamFormatInformationCommand );
    virtual ~ExtendedStreamFormatCmd();

    bool setPlugAddress( const PlugAddress& plugAddress );
    bool setIndexInStreamFormat( const int index );
    bool setSubFunction( ESubFunction subFunction );

    virtual bool serialize( IOSSerialize& se );
    virtual bool deserialize( IISDeserialize& de );

    virtual bool fire( ECommandType commandType,
                       raw1394handle_t handle,
                       unsigned int node_id );

    inline status_t getStatus();
    FormatInformation* getFormatInformation();
    inline index_in_stream_format_t getIndex();

protected:
    opcode_t m_opcode;
    subfunction_t m_subFunction;
    PlugAddress* m_plugAddress;
    status_t m_status;
    index_in_stream_format_t m_indexInStreamFormat;
    FormatInformation* m_formatInformation;
};

ExtendedStreamFormatCmd::ExtendedStreamFormatCmd( ESubFunction eSubFunction )
    : AVCCommand()
    , m_opcode( AVC1394_STREAM_FORMAT_SUPPORT )
    , m_subFunction( eSubFunction )
    , m_status( eS_NotUsed )
    , m_indexInStreamFormat( 0 )
    , m_formatInformation( new FormatInformation )
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
    AVCCommand::serialize( se );
    se.write( m_opcode, "ExtendedStreamFormatCmd opcode" );
    se.write( m_subFunction, "ExtendedStreamFormatCmd subFunction" );
    m_plugAddress->serialize( se );
    se.write( m_status, "ExtendedStreamFormatCmd status" );
    if ( m_subFunction == eSF_ExtendedStreamFormatInformationCommandList ) {
        se.write( m_indexInStreamFormat, "indexInStreamFormat" );
    }
    m_formatInformation->serialize( se );
    return true;
}

bool
ExtendedStreamFormatCmd::deserialize( IISDeserialize& de )
{
    AVCCommand::deserialize( de );
    de.read( &m_opcode );
    de.read( &m_subFunction );
    m_plugAddress->deserialize( de );
    de.read( &m_status );
    if ( m_subFunction == eSF_ExtendedStreamFormatInformationCommandList ) {
        de.read( &m_indexInStreamFormat );
    }
    m_formatInformation->deserialize( de );
    return true;
}

bool
ExtendedStreamFormatCmd::fire( ECommandType commandType,
                               raw1394handle_t handle,
                               unsigned int node_id )
{
    bool result = false;

    #define STREAM_FORMAT_REQUEST_SIZE 10 // XXX random length
    union UPacket {
        quadlet_t     quadlet[STREAM_FORMAT_REQUEST_SIZE];
        unsigned char byte[STREAM_FORMAT_REQUEST_SIZE*4];
    };
    typedef union UPacket packet_t;

    packet_t  req;
    packet_t* resp;

    setCommandType( commandType );

    // initialize complete packet
    memset( &req,  0xff,  sizeof( req ) );

    BufferSerialize se( req.byte, sizeof( req ) );
    if ( !serialize( se ) ) {
        printf(  "ExtendedStreamFormatCmd::fire: Could not serialize comanndo\n" );
        return false;
    }

    // reorder the bytes to the correct layout
    for (int i = 0; i < STREAM_FORMAT_REQUEST_SIZE; ++i) {
        req.quadlet[i] = ntohl( req.quadlet[i] );
    }

    if ( arguments.verbose ) {
        // debug output
        puts("request:");
        for (int i = 0; i < STREAM_FORMAT_REQUEST_SIZE; ++i) {
            printf("  %2d: 0x%08x\n", i, req.quadlet[i]);
        }
    }

    resp = reinterpret_cast<packet_t*> ( avc1394_transaction_block( handle, node_id, req.quadlet, STREAM_FORMAT_REQUEST_SIZE,  1 ) );
    if ( resp ) {
        if ( arguments.verbose ) {
            // debug output
            puts("response:");
            for ( int i = 0; i < STREAM_FORMAT_REQUEST_SIZE; ++i ) {
                printf( "  %2d: 0x%08x\n", i, resp->quadlet[i] );
            }
        }

        // reorder the bytes to the correct layout
        for ( int i = 0; i < STREAM_FORMAT_REQUEST_SIZE; ++i ) {
            resp->quadlet[i] = htonl( resp->quadlet[i] );
        }

        if ( arguments.verbose ) {
            // a more detailed debug output
            printf( "\n" );
            printf( " idx type                       value\n" );
            printf( "-------------------------------------\n" );
            printf( "  %02d                     ctype: 0x%02x\n", 0, resp->byte[0] );
            printf( "  %02d subunit_type + subunit_id: 0x%02x\n", 1, resp->byte[1] );
            printf( "  %02d                    opcode: 0x%02x\n", 2, resp->byte[2] );

            for ( int i = 3; i < STREAM_FORMAT_REQUEST_SIZE * 4; ++i ) {
                printf( "  %02d                operand %2d: 0x%02x\n", i, i-3, resp->byte[i] );
            }
        }

        // parse output
        parseResponse( resp->byte[0] );
        switch ( getResponse() )
        {
            case eR_Implemented:
            {
                BufferDeserialize de( resp->byte, sizeof( req ) );
                deserialize( de );
                result = true;
            }
            break;
            case eR_Rejected:
                if ( m_subFunction == eSF_ExtendedStreamFormatInformationCommandList ) {
                    if ( arguments.verbose ) {
                        printf( "no futher stream formats defined\n" );
                    }
                    result = true;
                } else {
                    printf( "request rejected\n" );
                }
                break;
            default:
                printf( "unexpected response received (0x%x)\n", getResponse() );
        }
    } else {
	printf( "no response\n" );
    }

    return result;
}

status_t
ExtendedStreamFormatCmd::getStatus()
{
    return m_status;
}

FormatInformation*
ExtendedStreamFormatCmd::getFormatInformation()
{
    return m_formatInformation;
}

ExtendedStreamFormatCmd::index_in_stream_format_t
ExtendedStreamFormatCmd::getIndex()
{
    return m_indexInStreamFormat;
}

bool
ExtendedStreamFormatCmd::setSubFunction( ESubFunction subFunction )
{
    m_subFunction = subFunction;
    return true;
}

////////////////////////////////////////
// Test application
////////////////////////////////////////
void
doTest(raw1394handle_t handle, int node_id, int plug_id)
{

    ExtendedStreamFormatCmd extendedStreamFormatCmd = ExtendedStreamFormatCmd( ExtendedStreamFormatCmd::eSF_ExtendedStreamFormatInformationCommandList );
    UnitPlugAddress unitPlugAddress( 0x00,  plug_id );
    extendedStreamFormatCmd.setPlugAddress( PlugAddress( PlugAddress::eM_Subunit,
                                                         PlugAddress::ePD_Input,
                                                         unitPlugAddress ) );
    extendedStreamFormatCmd.setIndexInStreamFormat( 0 );
    extendedStreamFormatCmd.fire( AVCCommand::eCT_Status, handle, node_id );
    extendedStreamFormatCmd.setIndexInStreamFormat( 1 );
    extendedStreamFormatCmd.fire( AVCCommand::eCT_Status, handle, node_id );
    extendedStreamFormatCmd.setIndexInStreamFormat( 2 );
    extendedStreamFormatCmd.fire( AVCCommand::eCT_Status, handle, node_id );
    extendedStreamFormatCmd.setIndexInStreamFormat( 3 );
    extendedStreamFormatCmd.fire( AVCCommand::eCT_Status, handle, node_id );
    extendedStreamFormatCmd.setIndexInStreamFormat( 4 );
    extendedStreamFormatCmd.fire( AVCCommand::eCT_Status, handle, node_id );

    ExtendedStreamFormatCmd extendedStreamFormatCmdS = ExtendedStreamFormatCmd();
    extendedStreamFormatCmdS.setPlugAddress( PlugAddress( PlugAddress::eM_Subunit,
                                                          PlugAddress::ePD_Input,
                                                          unitPlugAddress ) );
    if ( extendedStreamFormatCmdS.fire( AVCCommand::eCT_Status, handle, node_id ) ) {
        CoutSerializer se;
        extendedStreamFormatCmdS.serialize( se );
    }

    return;
}

////////////////////////////////////////
// Main application
////////////////////////////////////////
struct FormatInfo {
    FormatInfo()
        : m_freq( eSF_DontCare )
        , m_format( 0 )
        , m_audioChannels( 0 )
        , m_midiChannels( 0 )
        , m_index( 0 )
        {}

    ESamplingFrequency m_freq;
    int                m_format; // 0 = compound, 1 = sync
    int                m_audioChannels;
    int                m_midiChannels;
    int                m_index;
};
typedef vector< struct FormatInfo > FormatInfoVector;

ostream& operator<<( ostream& stream, FormatInfo info )
{
    stream << info.m_freq << " Hz (";
    if ( info.m_format ) {
            stream << "sync ";
    } else {
        stream << "compound ";
    }
    stream << "stream, ";
    stream << "audio channels: " << info.m_audioChannels
           << ", midi channels: " << info.m_midiChannels << ")";
    return stream;
}


FormatInfo
createFormatInfo( ExtendedStreamFormatCmd& cmd )
{
    FormatInfo fi;

    FormatInformationStreamsSync* syncStream
        = dynamic_cast< FormatInformationStreamsSync* > ( cmd.getFormatInformation()->m_streams );
    if ( syncStream ) {
        fi.m_freq = static_cast<ESamplingFrequency>( syncStream->m_samplingFrequency );
        fi.m_format = 1;
    }

    FormatInformationStreamsCompound* compoundStream
        = dynamic_cast< FormatInformationStreamsCompound* > ( cmd.getFormatInformation()->m_streams );
    if ( compoundStream ) {
        fi.m_freq = static_cast<ESamplingFrequency>( compoundStream->m_samplingFrequency );
        fi.m_format = 0;
        for ( int j = 0; j < compoundStream->m_numberOfStreamFormatInfos; ++j )
        {
            switch ( compoundStream->m_streamFormatInfos[j]->m_streamFormat ) {
            case AVC1394_STREAM_FORMAT_AM824_MULTI_BIT_LINEAR_AUDIO_RAW:
                fi.m_audioChannels += compoundStream->m_streamFormatInfos[j]->m_numberOfChannels;
                break;
            case AVC1394_STREAM_FORMAT_AM824_MIDI_CONFORMANT:
                fi.m_midiChannels += compoundStream->m_streamFormatInfos[j]->m_numberOfChannels;
                break;
            default:
                cout << "addStreamFormat: unknown stream format for channel" << endl;
            }
        }
    }
    fi.m_index = cmd.getIndex();
    return fi;
}


bool
doApp(raw1394handle_t handle, int node_id, int plug_id, ESamplingFrequency frequency = eSF_DontCare)
{
    ExtendedStreamFormatCmd extendedStreamFormatCmd = ExtendedStreamFormatCmd( ExtendedStreamFormatCmd::eSF_ExtendedStreamFormatInformationCommandList );
    UnitPlugAddress unitPlugAddress( 0x00,  plug_id );
    extendedStreamFormatCmd.setPlugAddress( PlugAddress( PlugAddress::eM_Subunit,
                                                         PlugAddress::ePD_Input,
                                                         unitPlugAddress ) );

    int i = 0;
    FormatInfoVector supportedFormatInfos;
    bool cmdSuccess = false;

    do {
        extendedStreamFormatCmd.setIndexInStreamFormat( i );
        cmdSuccess = extendedStreamFormatCmd.fire( AVCCommand::eCT_Status, handle,  node_id );
        if ( cmdSuccess
             && ( extendedStreamFormatCmd.getResponse() == AVCCommand::eR_Implemented ) )
        {
            supportedFormatInfos.push_back( createFormatInfo( extendedStreamFormatCmd ) );
        }
        ++i;
    } while ( cmdSuccess && ( extendedStreamFormatCmd.getResponse() ==  ExtendedStreamFormatCmd::eR_Implemented ) );

    if ( !cmdSuccess ) {
        cerr << "Failed to retrieve format infos" << endl;
        return false;
    }

    cout << "Supported frequencies:" << endl;
    for ( FormatInfoVector::iterator it = supportedFormatInfos.begin();
          it != supportedFormatInfos.end();
          ++it )
    {
        cout << "  " << *it << endl;
    }

    if ( frequency != eSF_DontCare ) {
        cout << "Trying to set frequency (" << frequency << "): " << endl;

        FormatInfoVector::iterator it;
        for ( it = supportedFormatInfos.begin();
              it != supportedFormatInfos.end();
              ++it )
        {
            if ( it->m_freq == frequency ) {
                cout << "  - frequency " << frequency << " is supported" << endl;

                ExtendedStreamFormatCmd setFormatCmd = ExtendedStreamFormatCmd( ExtendedStreamFormatCmd::eSF_ExtendedStreamFormatInformationCommandList );
                setFormatCmd.setPlugAddress( PlugAddress( PlugAddress::eM_Subunit,
                                                          PlugAddress::ePD_Input,
                                                          unitPlugAddress ) );

                setFormatCmd.setIndexInStreamFormat( it->m_index );
                if ( !setFormatCmd.fire( AVCCommand::eCT_Status, handle,  node_id ) ) {
                    cout << "  - failed to retrieve format for index " << it->m_index << endl;
                    return false;
                }

                cout << "  - " << createFormatInfo( setFormatCmd ) << endl;

                setFormatCmd.setSubFunction( ExtendedStreamFormatCmd::eSF_ExtendedStreamFormatInformationCommand );
                if ( !setFormatCmd.fire( AVCCommand::eCT_Control,  handle,  node_id ) ) {
                    cout << "  - failed to set new format" << endl;
                    return false;
                }
                cout << "  - configuration operation succedded" << endl;
                break;
            }
        }
        if ( it == supportedFormatInfos.end() ) {
            cout << "  - frequency not supported by device. Operation failed" << endl;
        }
    }

    ExtendedStreamFormatCmd currentFormat = ExtendedStreamFormatCmd();
    currentFormat.setPlugAddress( PlugAddress( PlugAddress::eM_Subunit,
                                               PlugAddress::ePD_Input,
                                               unitPlugAddress ) );

    if ( currentFormat.fire( AVCCommand::eCT_Status, handle, node_id ) ) {
        cout << "Configured frequency: " << createFormatInfo( currentFormat ) << endl;
    }

    return true;
}

///////////////////////////////////////////////////////////////
ESamplingFrequency
parseFrequencyString( int frequency )
{
    ESamplingFrequency efreq;
    switch ( frequency ) {
    case 22050:
        efreq = eSF_22050Hz;
        break;
    case 24000:
        efreq = eSF_24000Hz;
        break;
    case 32000:
        efreq = eSF_32000Hz;
        break;
    case 44100:
        efreq = eSF_44100Hz;
        break;
    case 48000:
        efreq = eSF_48000Hz;
        break;
    case 88200:
        efreq = eSF_88200Hz;
        break;
    case 96000:
        efreq = eSF_96000Hz;
        break;
    case 176400:
        efreq = eSF_176400Hz;
        break;
    case 192000:
        efreq = eSF_192000Hz;
        break;
    default:
        efreq = eSF_DontCare;
    }

    return efreq;
}

int
main(int argc, char **argv)
{
    // arg parsing
    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    errno = 0;
    char* tail;
    int node_id = strtol(arguments.args[0], &tail, 0);
    if (errno) {
	perror("argument parsing failed:");
	return -1;
    }
    int plug_id = strtol(arguments.args[1], &tail, 0);
    if (errno) {
	perror("argument parsing failed:");
	return -1;
    }

    // create raw1394handle
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

    if (raw1394_set_port(handle, arguments.port) < 0) {
	fprintf(stderr, "Could not set port");
	raw1394_destroy_handle (handle);
	return -1;
    }

    if ( arguments.test ) {
        doTest( handle, node_id,  plug_id );
    } else {
        ESamplingFrequency efreq = parseFrequencyString( arguments.frequency );
        doApp( handle, node_id, plug_id, efreq );
    }

    raw1394_destroy_handle( handle );

    return 0;
}
