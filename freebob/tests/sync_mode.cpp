/* sync_mode.cpp
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
#include "avc_plug_info.h"
#include "avc_signal_source.h"
#include "serialize.h"

#include <argp.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <iostream>

using namespace std;

////////////////////////////////////////////////
// arg parsing
////////////////////////////////////////////////
const char *argp_program_version = "sync_mode 0.1";
const char *argp_program_bug_address = "<freebob-devel@lists.sf.net>";
static char doc[] = "sync_mode -- get/set sync mode for FreeBob";
static char args_doc[] = "NODE_ID";
static struct argp_option options[] = {
    {"verbose",   'v', 0,           0,     "Produce verbose output" },
    {"test",      't', 0,           0,     "Do tests instead get/set action" },
    {"sync",      's', "SYNC_MODE", 0,     "Set sync mode" },
    {"port",      'p', "PORT",      0,     "Set port" },
   { 0 }
};

struct arguments
{
    arguments()
        : verbose( false )
        , test( false )
        , sync_mode( 0 )
        , port( 0 )
        {
            args[0] = 0;
        }

    char* args[1];
    bool  verbose;
    bool  test;
    char* sync_mode;
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
    case 's':
        arguments->sync_mode = arg;
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
        if (state->arg_num >= 1) {
            // Too many arguments.
            argp_usage (state);
        }
        arguments->args[state->arg_num] = arg;
        break;
    case ARGP_KEY_END:
        if (state->arg_num < 1) {
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
bool
doTest( raw1394handle_t handle, int node_id )
{
    /*
    ExtendedStreamFormatCmd extendedStreamFormatCmd;
    UnitPlugAddress unitPlugAddress( 0x00,  0x00 );
    extendedStreamFormatCmd.setPlugAddress( PlugAddress( PlugAddress::eM_Subunit,
                                                         PlugAddress::ePD_Input,
                                                         unitPlugAddress ) );
    extendedStreamFormatCmd.setVerbose( arguments.verbose );
    extendedStreamFormatCmd.setCommandType( AVCCommand::eCT_Status );
    extendedStreamFormatCmd.fire( handle, node_id );

    PlugInfoCmd plugInfoCmd;
    plugInfoCmd.setVerbose( arguments.verbose );
    plugInfoCmd.setCommandType( AVCCommand::eCT_Status );
    if ( plugInfoCmd.fire( handle,  node_id ) ) {
        CoutSerializer se;
        plugInfoCmd.serialize( se );
    }

    plugInfoCmd.setSubFunction( PlugInfoCmd::eSF_SerialBusAsynchonousPlug );
    plugInfoCmd.setCommandType( AVCCommand::eCT_Status );
    if ( plugInfoCmd.fire( handle,  node_id ) ) {
        CoutSerializer se;
        plugInfoCmd.serialize( se );
    }
    */

    SignalSourceCmd signalSourceCmd;
    SignalUnitAddress dest;
    dest.m_plugId = 0x00;
    signalSourceCmd.setSignalDestination( dest );
    signalSourceCmd.setCommandType( AVCCommand::eCT_Status );
    signalSourceCmd.setVerbose( arguments.verbose );

    CoutSerializer se;
    signalSourceCmd.serialize( se );

    if ( signalSourceCmd.fire( handle, node_id ) ) {
        signalSourceCmd.serialize( se );
    }

    return true;
}

////////////////////////////////////////
// Main application
////////////////////////////////////////

bool
doApp( raw1394handle_t handle, int node_id )
{
    // Following sync sources always exists:
    // - Unit input plug 0 = SYT match
    // - Unit input plut 1 = sync stream
    // - Music subunit sync output plug = internal sync (CSP)
    //
    // Following sync sources are device specific:
    // - All unit external input plugs which have a sync information (WS, SPDIF, ...)


    // First we have to find the music subunit sync input plug (address)
    {
        PlugInfoCmd plugInfoCmd;
        plugInfoCmd.setVerbose( arguments.verbose );
        plugInfoCmd.setCommandType( AVCCommand::eCT_Status );
        plugInfoCmd.setSubunitType( AVCCommand::eST_Music );
        plugInfoCmd.setSubunitId( 0x00 );

        if ( plugInfoCmd.fire( handle,  node_id ) ) {
            for ( int plugIdx = 0;
                  plugIdx < plugInfoCmd.m_destinationPlugs;
                  ++plugIdx )
            {
                // Find out the format of all plugs
                ExtendedStreamFormatCmd extendedStreamFormatCmd;

                SubunitPlugAddress subunitPlugAddress( plugIdx );
                extendedStreamFormatCmd.setPlugAddress( PlugAddress( PlugAddress::ePD_Input,
                                                                     PlugAddress::ePAM_Subunit,
                                                                     subunitPlugAddress ) );
                extendedStreamFormatCmd.setSubunitType( AVCCommand::eST_Music );
                extendedStreamFormatCmd.setSubunitId( 0x00 );
                extendedStreamFormatCmd.setCommandType( AVCCommand::eCT_Status );
                extendedStreamFormatCmd.setVerbose( arguments.verbose );

                if ( extendedStreamFormatCmd.fire( handle, node_id ) ) {
                    FormatInformation* formatInformation = extendedStreamFormatCmd.getFormatInformation();
                    if ( formatInformation
                         && ( formatInformation->m_root   == FormatInformation::eFHR_AudioMusic )
                         && ( formatInformation->m_level1 == FormatInformation::eFHL1_AUDIOMUSIC_AM824 )
                         && ( formatInformation->m_level2 == FormatInformation::eFHL2_AM824_SYNC_STREAM ) )
                    {
                        cout << "sync plug in music subunit is plug " << plugIdx << endl;
                    }
                }
            }
        }
    }

    return true;

    // Find out how many external input plugs exits.
    // Every one of them is possible a sync source
    {
        PlugInfoCmd plugInfoCmd;
        plugInfoCmd.setVerbose( arguments.verbose );
        plugInfoCmd.setCommandType( AVCCommand::eCT_Status );

        if ( plugInfoCmd.fire( handle, node_id ) ) {
            for ( int plugIdx = 0;
                  plugIdx < plugInfoCmd.m_externalInputPlugs;
                  ++plugIdx )
            {
                // Find out the format of all plugs
                ExtendedStreamFormatCmd extendedStreamFormatCmd;

                UnitPlugAddress unitPlugAddress( UnitPlugAddress::ePT_ExternalPlug, plugIdx );
                extendedStreamFormatCmd.setPlugAddress( PlugAddress( PlugAddress::ePD_Input,
                                                                     PlugAddress::ePAM_Unit,
                                                                     unitPlugAddress ) );

                extendedStreamFormatCmd.setCommandType( AVCCommand::eCT_Status );
                extendedStreamFormatCmd.setVerbose( arguments.verbose );
                if ( extendedStreamFormatCmd.fire( handle, node_id ) ) {
                    cout << "plug idx: " << plugIdx << endl;
                    CoutSerializer se;
                    extendedStreamFormatCmd.serialize( se );
                    cout << endl;
                }
            }
        }
    }

    return true;
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
        doTest( handle, node_id );
    } else {
        doApp( handle, node_id );
    }

    raw1394_destroy_handle( handle );

    return 0;
}
