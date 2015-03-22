/*
 * Copyright (C) 2005-2008 by Pieter Palmers
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

#include "config.h"

#include <libraw1394/raw1394.h>
#include <libiec61883/iec61883.h>

#include "debugmodule/debugmodule.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"
#include "libutil/cmd_serialize.h"
#include "libavc/general/avc_generic.h"
#include "libutil/Time.h"
#include "libutil/ByteSwap.h"

#include "bebob/focusrite/focusrite_cmd.h"
#include "bebob/focusrite/focusrite_generic.h"
using namespace BeBoB::Focusrite;

#include <argp.h>
#include <stdlib.h>
#include <iostream>

using namespace std;
using namespace AVC;
using namespace Util;

DECLARE_GLOBAL_DEBUG_MODULE;

#define MAX_ARGS 1000

////////////////////////////////////////////////
// arg parsing
////////////////////////////////////////////////
const char *argp_program_version = "test-focusrite 0.1";
const char *argp_program_bug_address = "<ffado-devel@lists.sf.net>";
static char doc[] = "test-avccmd -- test program to examine the focusrite vendor dependent commands.";
static char args_doc[] = "NODE_ID";
static struct argp_option options[] = {
    {"verbose",   'v', 0,           0,  "Produce verbose output" },
    {"port",      'p', "PORT",      0,  "Set port" },
    {"node",      'n', "NODE",      0,  "Set node" },
   { 0 }
};

struct arguments
{
    arguments()
        : nargs ( 0 )
        , verbose( false )
        , test( false )
        , port( 0 )
        {
            args[0] = 0;
        }

    char* args[MAX_ARGS];
    int   nargs;
    bool  verbose;
    bool  test;
    int   port;
    int   node;
} arguments;

// Parse a single option.
static error_t
parse_opt( int key, char* arg, struct argp_state* state )
{
    // Get the input argument from `argp_parse', which we
    // know is a pointer to our arguments structure.
    struct arguments* arguments = ( struct arguments* ) state->input;

    char* tail;
    errno = 0;
    switch (key) {
    case 'v':
        arguments->verbose = true;
        break;
    case 't':
        arguments->test = true;
        break;
    case 'p':
        arguments->port = strtol(arg, &tail, 0);
        if (errno) {
            perror("argument parsing failed:");
            return errno;
        }
        break;
    case 'n':
        arguments->node = strtol(arg, &tail, 0);
        if (errno) {
            perror("argument parsing failed:");
            return errno;
        }
        break;
    case ARGP_KEY_ARG:
        if (state->arg_num >= MAX_ARGS) {
            // Too many arguments.
            argp_usage (state);
        }
        arguments->args[state->arg_num] = arg;
        arguments->nargs++;
        break;
    case ARGP_KEY_END:

        
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

///////////////////////////
// main
//////////////////////////
int
main(int argc, char **argv)
{
    // arg parsing
    if ( argp_parse ( &argp, argc, argv, 0, 0, &arguments ) ) {
        fprintf( stderr, "Could not parse command line\n" );
        exit(-1);
    }
    errno = 0;

    Ieee1394Service *m_1394Service = new Ieee1394Service();
    if ( !m_1394Service ) {
        debugFatal( "Could not create Ieee1349Service object\n" );
        return false;
    }

    if ( !m_1394Service->initialize( arguments.port ) ) {
        debugFatal( "Could not initialize Ieee1349Service object\n" );
        delete m_1394Service;
        m_1394Service = 0;
        return false;
    }
    
    FocusriteVendorDependentCmd cmd( *m_1394Service );
        cmd.setVerbose( DEBUG_LEVEL_NORMAL );
    
    #define TOTAL_IDS_TO_SCAN 110
    uint32_t old_vals[TOTAL_IDS_TO_SCAN+1];
    m_1394Service->setVerboseLevel(DEBUG_LEVEL_INFO);
    
    while(1) {
        for (int id=88; id<TOTAL_IDS_TO_SCAN;id++) {
            quadlet_t value;
            int ntries=5;
            bool retval = false;
            fb_nodeaddr_t addr = FR_PARAM_SPACE_START + (id * 4);
            fb_nodeid_t nodeId = arguments.node | 0xFFC0;
//             if (id==64) continue; // metering
//             if (id==65) continue; // metering
//             if (id==66) continue; // metering
//             if (id==67) continue; // metering
            while(ntries-- && !(retval = m_1394Service->read_quadlet(nodeId, addr, &value))) {
                SleepRelativeUsec(10000);
            }
            if (!retval) {
                debugError( " read from %16"PRIX64" failed (id: %d)\n", addr, id);
            } else {
                value = CondSwapFromBus32(value);
            
                if (old_vals[id] != value) {
                    printf("%04d changed from %08X to %08X\n", id,  old_vals[id], value);
                    old_vals[id] = value;
                }
            }
        }
        SleepRelativeUsec(1000000);
    }


    delete m_1394Service;

    return 0;
}

