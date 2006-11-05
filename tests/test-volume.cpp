/* test-volume.cpp
 * Copyright (C) 2006 by Daniel Wagner
 *
 * This file is part of FreeBoB.
 *
 * FreeBoB is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBoB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBoB; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */

#include "libfreebobavc/avc_feature_function_block.h"
#include "libfreebobavc/serialize.h"
#include "libfreebobavc/ieee1394service.h"

#include <argp.h>

using namespace std;

////////////////////////////////////////////////
// arg parsing
////////////////////////////////////////////////
const char *argp_program_version = "test-volume 0.1";
const char *argp_program_bug_address = "<freebob-devel@lists.sf.net>";
static char doc[] = "test-extplugcmd -- tests some extended plug info commands on a BeBoB device";
static char args_doc[] = "NODE_ID FFB_ID VOL";
static struct argp_option options[] = {
    {"verbose",   'v', 0,           0,  "Produce verbose output" },
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
        }

    char* args[3];
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
        if (state->arg_num >= 3) {
            // Too many arguments.
            argp_usage (state);
        }
        arguments->args[state->arg_num] = arg;
        break;
    case ARGP_KEY_END:
        if (state->arg_num < 3 ) {
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
// Application
////////////////////////////////////////

bool
doApp(Ieee1394Service& ieee1394service, int node_id, int ffb_id, int vol )
{
    FeatureFunctionBlockCmd ffbCmd( &ieee1394service );
    ffbCmd.setNodeId( node_id );
    ffbCmd.setSubunitId( 0x01 );
    ffbCmd.setCommandType( AVCCommand::eCT_Control );
    ffbCmd.setFunctionBlockId( ffb_id );
    ffbCmd.m_volumeControl.m_volume = vol;

    ffbCmd.setVerbose( arguments.verbose );
    if ( !ffbCmd.fire() ) {
        printf( "cmd failed\n" );
    }
    CoutSerializer se;
    ffbCmd.serialize( se );
    return true;
}

///////////////////////////
// main
//////////////////////////
int
main(int argc, char **argv)
{
    // arg parsing
    argp_parse( &argp, argc, argv, 0, 0, &arguments );

    errno = 0;
    char* tail;
    int node_id = strtol( arguments.args[0], &tail, 0 );
    int ffb_id  = strtol( arguments.args[1], &tail, 0 );
    int vol     = strtol( arguments.args[2], &tail, 0 );

    if (errno) {
	perror("argument parsing failed:");
	return -1;
    }
    Ieee1394Service ieee1394service;
    if ( !ieee1394service.initialize( arguments.port ) ) {
        fprintf( stderr, "could not set port on ieee1394service\n" );
        return -1;
    }

    doApp( ieee1394service, node_id, ffb_id, vol );

    return 0;
}
