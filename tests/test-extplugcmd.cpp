/* test-extplugcmd.cpp
 * Copyright (C) 2005,06 by Daniel Wagner
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

#include "libavc/avc_extended_plug_info.h"
#include "libavc/avc_plug_info.h"
#include "libavc/avc_serialize.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include <argp.h>

using namespace std;

////////////////////////////////////////////////
// arg parsing
////////////////////////////////////////////////
const char *argp_program_version = "test-extplugcmd 0.2";
const char *argp_program_bug_address = "<freebob-devel@lists.sf.net>";
static char doc[] = "test-extplugcmd -- tests some extended plug info commands on a BeBoB device";
static char args_doc[] = "NODE_ID";
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

    char* args[1];
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
// Application
////////////////////////////////////////

bool
doPlugType( Ieee1394Service& ieee1394service, int node_id )
{
    ExtendedPlugInfoCmd extPlugInfoCmd( ieee1394service );
    UnitPlugAddress unitPlugAddress( UnitPlugAddress::ePT_PCR, 0 );
    extPlugInfoCmd.setPlugAddress( PlugAddress( PlugAddress::ePD_Input,
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

    ExtendedPlugInfoInfoType* infoType = extPlugInfoCmd.getInfoType();
    if ( infoType
         && infoType->m_plugType )
    {
            plug_type_t plugType = infoType->m_plugType->m_plugType;

            printf( "iso input plug %d is of type %d (%s)\n",
                    0,
                    plugType,
                    extendedPlugInfoPlugTypeToString( plugType ) );
    } else {
        fprintf( stderr, "Not plug name specific data found\n" );
        return false;
    }

    return true;
}


bool
doPlugName( Ieee1394Service& ieee1394service, int node_id )
{
    ExtendedPlugInfoCmd extPlugInfoCmd( ieee1394service );
    UnitPlugAddress unitPlugAddress( UnitPlugAddress::ePT_PCR, 0 );
    extPlugInfoCmd.setPlugAddress( PlugAddress( PlugAddress::ePD_Input,
                                                PlugAddress::ePAM_Unit,
                                                unitPlugAddress ) );
    extPlugInfoCmd.setNodeId( node_id );
    extPlugInfoCmd.setCommandType( AVCCommand::eCT_Status );
    extPlugInfoCmd.setVerbose( arguments.verbose );
    ExtendedPlugInfoInfoType extendedPlugInfoInfoType( ExtendedPlugInfoInfoType::eIT_PlugName );
    extendedPlugInfoInfoType.initialize();
    extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );

    if ( extPlugInfoCmd.fire() ) {
        CoutSerializer se;
        extPlugInfoCmd.serialize( se );
    }

    ExtendedPlugInfoInfoType* infoType = extPlugInfoCmd.getInfoType();
    if ( infoType
         && infoType->m_plugName )
    {
        printf( "iso input plug %d has name '%s'\n",
                0,
               infoType->m_plugName->m_name.c_str() );
    } else {
        fprintf( stderr, "Not plug name specific data found\n" );
        return false;
    }

    return true;
}

bool
doApp(Ieee1394Service& ieee1394service, int node_id )
{
    bool success;

    success = doPlugType( ieee1394service, node_id );
    success &= doPlugName( ieee1394service, node_id );

    return success;
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
    Ieee1394Service ieee1394service;
    if ( !ieee1394service.initialize( arguments.port ) ) {
        fprintf( stderr, "could not set port on ieee1394service\n" );
        return -1;
    }

    doApp( ieee1394service, node_id );

    return 0;
}
