/* test-freebob.c
 * Copyright (C) 2005 by Daniel Wagner
 * Copyright (C) 2007 by Pieter Palmers
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

/*
 * This version uses the CPP API
 */

#include <config.h>

#include "libfreebob/freebob.h"
#include "libfreebob/freebob_bounce.h"

#include "debugmodule/debugmodule.h"
#include "fbtypes.h"
#include "devicemanager.h"
#include "iavdevice.h"

#include <signal.h>

#include <argp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

DECLARE_GLOBAL_DEBUG_MODULE;

int run=1;
static void sighandler (int sig)
{
    run = 0;
}

const char *argp_program_version = PACKAGE_STRING;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;

// Program documentation.
static char doc[] = "FreeBoB -- a driver for Firewire Audio devices (test application)\n\n"
                    "OPERATION: discover\n"
                    "           setsamplerate\n";

// A description of the arguments we accept.
static char args_doc[] = "OPERATION";

struct arguments
{
    short silent;
    short verbose;
    int   port;
    int   node_id;
    int   node_id_set;
    char* args[2];  
};

// The options we understand.
static struct argp_option options[] = {
    {"quiet",    'q',       0,    0,  "Don't produce any output" },
    {"silent",   's',       0,    OPTION_ALIAS },

    {"verbose",  'v', "level",    0,  "Produce verbose output" },


    {"node",     'n',    "id",    0,  "Node to use" },
    {"port",     'p',    "nr",    0,  "IEEE1394 Port to use" },
    { 0 }
};

//-------------------------------------------------------------

// Parse a single option.
static error_t
parse_opt( int key, char* arg, struct argp_state* state )
{
    // Get the input argument from `argp_parse', which we
    // know is a pointer to our arguments structure.
    struct arguments* arguments = ( struct arguments* ) state->input;
    char* tail;

    switch (key) {
    case 'q': case 's':
        arguments->silent = 1;
        break;
    case 'v':
        if (arg) {
            arguments->verbose = strtol( arg, &tail, 0 );
            if ( errno ) {
                fprintf( stderr,  "Could not parse 'verbose' argument\n" );
                return ARGP_ERR_UNKNOWN;
            }
        }
        break;
    case 'p':
        if (arg) {
            arguments->port = strtol( arg, &tail, 0 );
            if ( errno ) {
                fprintf( stderr,  "Could not parse 'port' argument\n" );
                return ARGP_ERR_UNKNOWN;
            }
        } else {
            if ( errno ) {
                fprintf( stderr, "Could not parse 'port' argumen\n" );
                return ARGP_ERR_UNKNOWN;
            }
        }
        break;
    case 'n':
        if (arg) {
            arguments->node_id = strtol( arg, &tail, 0 );
            if ( errno ) {
                fprintf( stderr,  "Could not parse 'node' argument\n" );
                return ARGP_ERR_UNKNOWN;
            }
            arguments->node_id_set=1;
        } else {
            if ( errno ) {
                fprintf( stderr, "Could not parse 'node' argumen\n" );
                return ARGP_ERR_UNKNOWN;
            }
        }
        break;
    case ARGP_KEY_ARG:
        if (state->arg_num >= 2) {
            // Too many arguments.
            argp_usage( state );
        }
        arguments->args[state->arg_num] = arg;
        break;
    case ARGP_KEY_END:
        if (state->arg_num < 1) {
        // Not enough arguments.
        argp_usage( state );
        }
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

// Our argp parser.
static struct argp argp = { options, parse_opt, args_doc, doc };

int
main( int argc, char **argv )
{
    struct arguments arguments;

    // Default values.
    arguments.silent      = 0;
    arguments.verbose     = 0;
    arguments.port        = 0;
    arguments.node_id     = 0;
    arguments.node_id_set = 0; // if we don't specify a node, discover all
    arguments.args[0]     = "";
    arguments.args[1]     = "";

    // Parse our arguments; every option seen by `parse_opt' will
    // be reflected in `arguments'.
    if ( argp_parse ( &argp, argc, argv, 0, 0, &arguments ) ) {
        fprintf( stderr, "Could not parse command line\n" );
        return -1;
    }

    printf("verbose level = %d\n", arguments.verbose);

    printf( "Using freebob library version: %s\n\n", freebob_get_version() );

    DeviceManager *m_deviceManager = new DeviceManager();
    if ( !m_deviceManager ) {
        fprintf( stderr, "Could not allocate device manager\n" );
        return -1;
    }
    if ( !m_deviceManager->initialize( arguments.port ) ) {
        fprintf( stderr, "Could not initialize device manager\n" );
        delete m_deviceManager;
        return -1;
    }

    if ( strcmp( arguments.args[0], "discover" ) == 0 ) {
        if ( m_deviceManager->discover(arguments.verbose) ) {
            fprintf( stderr, "Could not discover devices\n" );
            delete m_deviceManager;
            return -1;
        }
    } else if ( strcmp( arguments.args[0], "setsamplerate" ) == 0 ) {
        char* tail;
        int samplerate = strtol( arguments.args[1], &tail, 0 );
        if ( errno ) {
            fprintf( stderr,  "Could not parse samplerate argument\n" );
            return -1;
        }

        if ( m_deviceManager->discover(arguments.verbose) ) {
            fprintf( stderr, "Could not discover devices\n" );
            delete m_deviceManager;
            return -1;
        }
    
        if(arguments.node_id_set) {
            IAvDevice* avDevice = m_deviceManager->getAvDevice( arguments.node_id );
            if ( avDevice ) {
                if ( avDevice->setSamplingFrequency( parseSampleRate( samplerate ) ) ) {
                    m_deviceManager->discover(arguments.verbose);
                } else {
                    fprintf( stderr, "Could not set samplerate\n" );
                }
            }
        } else {
        int i=0;
            
        int devices_on_bus = m_deviceManager->getNbDevices();
        printf("  port = %d, devices_on_bus = %d\n", arguments.port, devices_on_bus);

        for(i=0;i<devices_on_bus;i++) {
            int node_id=m_deviceManager->getDeviceNodeId(i);
            printf("  set samplerate for device = %d, node = %d\n", i, node_id);
            IAvDevice* avDevice = m_deviceManager->getAvDevice( node_id );
            if ( avDevice ) {
                if ( !avDevice->setSamplingFrequency( parseSampleRate( samplerate ) ) ) {
                    fprintf( stderr, "Could not set samplerate\n" );
                }
            }
        }

        }
    } else {
        printf( "unknown operation\n" );
    }

    return 0;
}
