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

#include "avc_extended_stream_format.h"
#include "serialize.h"

#include <libavc1394/avc1394.h>
#include <libraw1394/raw1394.h>

#include <argp.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include <vector>
#include <iterator>

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
        break;
    case 'p':
        errno = 0;
        arguments->port = strtol(arg, &tail, 0);
        if (errno) {
            perror("argument parsing failed:");
            return errno;
        }
        break;
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
    extendedStreamFormatCmd.setVerboseMode( arguments.verbose );

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
    extendedStreamFormatCmdS.setVerboseMode( arguments.verbose );

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
typedef std::vector< struct FormatInfo > FormatInfoVector;

std::ostream& operator<<( std::ostream& stream, FormatInfo info )
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
                std::cout << "addStreamFormat: unknown stream format for channel" << std::endl;
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
    extendedStreamFormatCmd.setVerboseMode( arguments.verbose );

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
        std::cerr << "Failed to retrieve format infos" << std::endl;
        return false;
    }

    std::cout << "Supported frequencies:" << std::endl;
    for ( FormatInfoVector::iterator it = supportedFormatInfos.begin();
          it != supportedFormatInfos.end();
          ++it )
    {
        std::cout << "  " << *it << std::endl;
    }

    if ( frequency != eSF_DontCare ) {
        std::cout << "Trying to set frequency (" << frequency << "): " << std::endl;

        FormatInfoVector::iterator it;
        for ( it = supportedFormatInfos.begin();
              it != supportedFormatInfos.end();
              ++it )
        {
            if ( it->m_freq == frequency ) {
                std::cout << "  - frequency " << frequency << " is supported" << std::endl;

                ExtendedStreamFormatCmd setFormatCmd = ExtendedStreamFormatCmd( ExtendedStreamFormatCmd::eSF_ExtendedStreamFormatInformationCommandList );
                setFormatCmd.setVerboseMode( arguments.verbose );
                setFormatCmd.setPlugAddress( PlugAddress( PlugAddress::eM_Subunit,
                                                          PlugAddress::ePD_Input,
                                                          unitPlugAddress ) );

                setFormatCmd.setIndexInStreamFormat( it->m_index );
                if ( !setFormatCmd.fire( AVCCommand::eCT_Status, handle,  node_id ) ) {
                    std::cout << "  - failed to retrieve format for index " << it->m_index << std::endl;
                    return false;
                }

                std::cout << "  - " << createFormatInfo( setFormatCmd ) << std::endl;

                setFormatCmd.setSubFunction( ExtendedStreamFormatCmd::eSF_ExtendedStreamFormatInformationCommand );
                if ( !setFormatCmd.fire( AVCCommand::eCT_Control,  handle,  node_id ) ) {
                    std::cout << "  - failed to set new format" << std::endl;
                    return false;
                }
                std::cout << "  - configuration operation succedded" << std::endl;
                break;
            }
        }
        if ( it == supportedFormatInfos.end() ) {
            std::cout << "  - frequency not supported by device. Operation failed" << std::endl;
        }
    }

    ExtendedStreamFormatCmd currentFormat = ExtendedStreamFormatCmd();
    currentFormat.setPlugAddress( PlugAddress( PlugAddress::eM_Subunit,
                                               PlugAddress::ePD_Input,
                                               unitPlugAddress ) );
    currentFormat.setVerboseMode( arguments.verbose );

    if ( currentFormat.fire( AVCCommand::eCT_Status, handle, node_id ) ) {
        std::cout << "Configured frequency: " << createFormatInfo( currentFormat ) << std::endl;
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
