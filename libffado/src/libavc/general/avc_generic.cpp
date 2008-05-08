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

#include "avc_generic.h"
#include "libutil/cmd_serialize.h"
#include "libieee1394/ieee1394service.h"

#include "debugmodule/debugmodule.h"

#include "libutil/Time.h"

#include "libutil/ByteSwap.h"

#define DEBUG_EXTRA_VERBOSE 5


namespace AVC {

IMPL_DEBUG_MODULE( AVCCommand, AVCCommand, DEBUG_LEVEL_NORMAL );
IMPL_DEBUG_MODULE( IBusData, IBusData, DEBUG_LEVEL_VERBOSE );

int AVCCommand::m_time = 0;

AVCCommand::AVCCommand( Ieee1394Service& ieee1394service,
                        opcode_t opcode )
    : m_p1394Service( &ieee1394service )
    , m_nodeId( 0 )
    , m_ctype( eCT_Unknown )
    , m_subunit( 0xff )
    , m_opcode( opcode )
    , m_eResponse( eR_Unknown )
{

}

bool
AVCCommand::serialize( Util::Cmd::IOSSerialize& se )
{
    // XXX \todo improve Util::Cmd::IOSSerialize::write interface
    char* buf;
    asprintf( &buf, "AVCCommand ctype ('%s')",
              responseToString( static_cast<AVCCommand::EResponse>( m_ctype ) ) );
    se.write( m_ctype, buf );
    free( buf );

    asprintf( &buf, "AVCCommand subunit (subunit_type = %d, subunit_id = %d)",
              getSubunitType(), getSubunitId() );
    se.write( m_subunit, buf );
    free( buf );

    se.write( m_opcode, "AVCCommand opcode" );
    return true;
}

bool
AVCCommand::deserialize( Util::Cmd::IISDeserialize& de )
{
    de.read( &m_ctype );
    de.read( &m_subunit );
    de.read( &m_opcode );
    return true;
}

bool
AVCCommand::setCommandType( ECommandType commandType )
{
    m_ctype = commandType;
    m_commandType = commandType;
    return true;
}

AVCCommand::ECommandType
AVCCommand::getCommandType()
{
    return m_commandType;
}

AVCCommand::EResponse
AVCCommand::getResponse()
{
    return m_eResponse;
}

bool
AVCCommand::setSubunitType(ESubunitType subunitType)
{
    byte_t subT = subunitType;

    m_subunit = ( subT << 3 ) | ( m_subunit & 0x7 );
    return true;
}

bool
AVCCommand::setNodeId( fb_nodeid_t nodeId )
{
    m_nodeId = nodeId;
    return true;
}

bool
AVCCommand::setSubunitId(subunit_id_t subunitId)
{
    m_subunit = ( subunitId & 0x7 ) | ( m_subunit & 0xf8 );
    return true;
}

ESubunitType
AVCCommand::getSubunitType()
{
    return static_cast<ESubunitType>( ( m_subunit >> 3 ) );
}

subunit_id_t
AVCCommand::getSubunitId()
{
    return m_subunit & 0x7;
}

bool
AVCCommand::setVerbose( int verboseLevel )
{
    setDebugLevel(verboseLevel);
    return true;
}

int
AVCCommand::getVerboseLevel()
{
    return getDebugLevel();
}


void
AVCCommand::showFcpFrame( const unsigned char* buf,
                          unsigned short frameSize ) const
{
    // use an intermediate buffer to avoid a load of very small print's that cause the 
    // message ringbuffer to overflow
    char msg[DEBUG_MAX_MESSAGE_LENGTH];
    int chars_written=0;
    for ( int i = 0; i < frameSize; ++i ) {
        if ( ( i % 16 ) == 0 ) {
            if ( i > 0 ) {
                debugOutputShort(DEBUG_LEVEL_VERY_VERBOSE, "%s\n", msg);
                chars_written=0;
            }
            chars_written+=snprintf(msg+chars_written,DEBUG_MAX_MESSAGE_LENGTH-chars_written,"  %3d:\t", i );;
        } else if ( ( i % 4 ) == 0 ) {
            chars_written+=snprintf(msg+chars_written,DEBUG_MAX_MESSAGE_LENGTH-chars_written," ");
        }
        chars_written+=snprintf(msg+chars_written,DEBUG_MAX_MESSAGE_LENGTH-chars_written, "%02x ", buf[i] );
    }
    if (chars_written != 0) {
        debugOutputShort(DEBUG_LEVEL_VERY_VERBOSE, "%s\n", msg );
    } else {
        debugOutputShort(DEBUG_LEVEL_VERY_VERBOSE, "\n" );
    }
}

bool
AVCCommand::fire()
{
    memset( &m_fcpFrame,  0x0,  sizeof( m_fcpFrame ) );

    Util::Cmd::BufferSerialize se( m_fcpFrame, sizeof( m_fcpFrame ) );
    if ( !serialize( se ) ) {
        debugFatal(  "fire: Could not serialize\n" );
        return false;
    }

    unsigned short fcpFrameSize = se.getNrOfProducesBytes();

    if (getDebugLevel() >= DEBUG_LEVEL_VERY_VERBOSE) {
        debugOutputShort( DEBUG_LEVEL_VERY_VERBOSE, "%s:\n", getCmdName() );
        debugOutputShort( DEBUG_LEVEL_VERY_VERBOSE,  "  Request:\n");
        showFcpFrame( m_fcpFrame, fcpFrameSize );

        Util::Cmd::StringSerializer se_dbg;
        serialize( se_dbg );
        
        // output the debug message in smaller chunks to avoid problems
        // with a max message size
        unsigned int chars_to_write=se_dbg.getString().size();
        unsigned int chars_written=0;
        while (chars_written<chars_to_write) {
            debugOutputShort(DEBUG_LEVEL_VERY_VERBOSE, "%s\n",
                         se_dbg.getString().substr(chars_written, DEBUG_MAX_MESSAGE_LENGTH).c_str());
            chars_written += DEBUG_MAX_MESSAGE_LENGTH-1;
        }
    }

    unsigned int resp_len;
    quadlet_t* resp = m_p1394Service->transactionBlock( m_nodeId,
                                                        (quadlet_t*)m_fcpFrame,
                                                        ( fcpFrameSize+3 ) / 4,
                                                        &resp_len );
    bool result = false;
    if ( resp ) {
        resp_len *= 4;
        unsigned char* buf = ( unsigned char* ) resp;

        m_eResponse = ( EResponse )( *buf );
        switch ( m_eResponse )
        {
        case eR_Accepted:
        case eR_Implemented:
        case eR_Rejected:
        case eR_NotImplemented:
        {
            Util::Cmd::BufferDeserialize de( buf, resp_len );
            result = deserialize( de );

            debugOutputShort( DEBUG_LEVEL_VERY_VERBOSE,"  Response:\n");
            showFcpFrame( buf, de.getNrOfConsumedBytes() );

            Util::Cmd::StringSerializer se_dbg;
            serialize( se_dbg );

            // output the debug message in smaller chunks to avoid problems
            // with a max message size
            unsigned int chars_to_write=se_dbg.getString().size();
            unsigned int chars_written=0;
            while (chars_written<chars_to_write) {
                debugOutputShort(DEBUG_LEVEL_VERY_VERBOSE, "%s\n",
                            se_dbg.getString().substr(chars_written, DEBUG_MAX_MESSAGE_LENGTH).c_str());
                chars_written += DEBUG_MAX_MESSAGE_LENGTH-1;
            }

        }
        break;
        default:
            debugWarning( "unexpected response received (0x%x)\n", m_eResponse );
            debugOutputShort( DEBUG_LEVEL_VERY_VERBOSE,"  Response:\n");

            Util::Cmd::BufferDeserialize de( buf, resp_len );
            deserialize( de );

            showFcpFrame( buf, de.getNrOfConsumedBytes() );

        }
    } else {
       debugWarning( "no response\n" );
    }

    debugOutputShort( DEBUG_LEVEL_VERY_VERBOSE, "\n" );

    m_p1394Service->transactionBlockClose();

    SleepRelativeUsec( m_time );

    return result;
}

void
AVCCommand::setSleepAfterAVCCommand( int time )
{
    m_time = time;
}

const char* subunitTypeStrings[] =
{
    "Monitor",
    "Audio",
    "Printer",
    "Disc recorder",
    "Tape recorder/VCR",
    "Tuner",
    "CA",
    "Video camera",
    "unknown",
    "Panel",
    "Bulletin board",
    "Camera storage",
    "Music",
};

const char*
subunitTypeToString( subunit_type_t subunitType )
{
    if ( subunitType == eST_Unit ) {
        return "Unit";
    }
    if ( subunitType > ( int ) ( sizeof( subunitTypeStrings )
             / sizeof( subunitTypeStrings[0] ) ) ) {
        return "unknown";
    } else {
        return subunitTypeStrings[subunitType];
    }
}

const char* responseToStrings[] =
{
    "control",
    "status",
    "specific inquiry",
    "notify",
    "general inquiry",
    "reserved for future specification",
    "reserved for future specification",
    "reserved for future specification",
    "not implemented",
    "accepted",
    "rejected",
    "in transition",
    "implemented/stable",
    "changed"
    "reserved for future specification",
    "interim",
};

const char*
responseToString( AVCCommand::EResponse eResponse )
{
    if ( eResponse > ( int )( sizeof( responseToStrings ) / sizeof( responseToStrings[0] ) ) ) {
        return "unknown";
    }
    return responseToStrings[eResponse];
}

}
