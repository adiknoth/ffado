/* test-extplugcmd.cpp
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

#include "src/libfreebobavc/avc_extended_plug_info.h"
#include "src/libfreebobavc/avc_plug_info.h"
#include "src/libfreebobavc/serialize.h"
#include "src/libfreebobavc/ieee1394service.h"

#include <argp.h>


using namespace std;

////////////////////////////////////////////////
// arg parsing
////////////////////////////////////////////////
const char *argp_program_version = "test-extplugcmd 0.1";
const char *argp_program_bug_address = "<freebob-devel@lists.sf.net>";
static char doc[] = "test-extplugcmd -- get extended plug info";
static char args_doc[] = "NODE_ID PLUG_ID";
static struct argp_option options[] = {
    {"verbose",   'v', 0,           0,            "Produce verbose output" },
    {"test",      't', 0,           0,            "Do tests instead get/set action" },
    {"port",      'p', "PORT",      0,  "Set port" },
   { 0 }
};

struct arguments
{
    arguments()
        : verbose( false )
        , test( false )
        , port( 0 )
        {
            args[0] = 0;
            args[1] = 0;
        }

    char* args[2];
    bool  verbose;
    bool  test;
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
doTest(Ieee1394Service& ieee1394service, int node_id, int plug_id)
{
    PlugInfoCmd plugInfoCmd( &ieee1394service );
    plugInfoCmd.setCommandType( AVCCommand::eCT_Status );
    plugInfoCmd.setNodeId( node_id );
    plugInfoCmd.setVerbose( true );

    if ( !plugInfoCmd.fire() ) {
        return;
    }
    return;
}

////////////////////////////////////////
// Application
////////////////////////////////////////

bool
doApp(Ieee1394Service& ieee1394service, int node_id, int plug_id )
{
    ExtendedPlugInfoCmd extPlugInfoCmd( &ieee1394service );
    UnitPlugAddress unitPlugAddress( UnitPlugAddress::ePT_PCR, plug_id );
    extPlugInfoCmd.setPlugAddress( PlugAddress( PlugAddress::ePD_Output,
                                                PlugAddress::ePAM_Unit,
                                                unitPlugAddress ) );
    extPlugInfoCmd.setNodeId( node_id );
    extPlugInfoCmd.setCommandType( AVCCommand::eCT_Status );
    extPlugInfoCmd.setVerbose( arguments.verbose );
    ExtendedPlugInfoInfoType extendedPlugInfoInfoType( ExtendedPlugInfoInfoType::eIT_PlugType );
    extendedPlugInfoInfoType.initialize();
    extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );

    if ( extPlugInfoCmd.fire() ) {
        CoutSerializer se;
        extPlugInfoCmd.serialize( se );
    }

    ExtendedPlugInfoInfoType extendedPlugInfoInfoType2( ExtendedPlugInfoInfoType::eIT_PlugName );
    extendedPlugInfoInfoType.initialize();
    extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType2 );
    if ( extPlugInfoCmd.fire() ) {
        CoutSerializer se;
        extPlugInfoCmd.serialize( se );
    }


    return true;
}

///////////////////////////
// main
//////////////////////////
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

    Ieee1394Service ieee1394service;
    if ( !ieee1394service.initialize( arguments.port ) ) {
        fprintf( stderr, "could not set port on ieee1394service\n" );
        return -1;
    }

    if ( arguments.test ) {
        doTest( ieee1394service, node_id, plug_id );
    } else {
        doApp( ieee1394service, node_id, plug_id );
    }

    return 0;
}
