/*
 * Copyright (C) 2005-2008 by Daniel Wagner
 * Copyright (C) 2005-2008 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
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

/*
 * This version uses the CPP API
 */

#include <config.h>

#include "libffado/ffado.h"

#include "debugmodule/debugmodule.h"
#include "fbtypes.h"
#include "devicemanager.h"
#include "ffadodevice.h"

#include <signal.h>

#include <argp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <vector>
#include <string>
#include <iostream>
#include <sstream>

using namespace std;

DECLARE_GLOBAL_DEBUG_MODULE;

// global's
const char *argp_program_version = PACKAGE_STRING;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;

// Program documentation.
static char doc[] = "FFADO -- a driver for Firewire Audio devices (test application)\n\n"
                    "OPERATION: Discover\n"
                    "           SetSamplerate samplerate\n"
                    "           SetClockSource [id]\n"
                    ;

// A description of the arguments we accept.
static char args_doc[] = "OPERATION";

struct arguments
{
    short    silent;
    long int verbose;
    long int port;
    long int use_cache;
    long int node_id;
    long int node_id_set;
    const char* args[2];
};

// The options we understand.
static struct argp_option options[] = {
    {"quiet",    'q',       0,    0,  "Don't produce any output" },
    {"silent",   's',       0,    OPTION_ALIAS },

    {"verbose",  'v', "level",    0,  "Produce verbose output" },
    {"cache",    'c', "enable",   0,  "Use AVC model cache (default=enabled)" },


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
    case 'c':
        if (arg) {
            arguments->use_cache = strtol( arg, &tail, 0 );
            if ( errno ) {
                fprintf( stderr,  "Could not parse 'cache' argument\n" );
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
        if (state->arg_num >= 3) {
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

int exitfunction( int retval ) {
    debugOutput( DEBUG_LEVEL_NORMAL, "Debug output flushed...\n" );
    flushDebugOutput();
    
    return retval;
}

int
main( int argc, char **argv )
{
    struct arguments arguments;

    // Default values.
    arguments.silent      = 0;
    arguments.verbose     = 0;
    arguments.use_cache   = 1;
    arguments.port        = 0;
    arguments.node_id     = 0;
    arguments.node_id_set = 0; // if we don't specify a node, discover all
    arguments.args[0]     = "";
    arguments.args[1]     = "";

    // Parse our arguments; every option seen by `parse_opt' will
    // be reflected in `arguments'.
    if ( argp_parse ( &argp, argc, argv, 0, 0, &arguments ) ) {
        fprintf( stderr, "Could not parse command line\n" );
        return exitfunction(-1);
    }

    printf("verbose level = %ld\n", arguments.verbose);
    setDebugLevel(arguments.verbose);

    printf( "Using ffado library version: %s\n\n", ffado_get_version() );

    if ( strcmp( arguments.args[0], "Discover" ) == 0 ) {
        DeviceManager *m_deviceManager = new DeviceManager();
        if ( !m_deviceManager ) {
            fprintf( stderr, "Could not allocate device manager\n" );
            return exitfunction(-1);
        }
        if ( arguments.verbose ) {
            m_deviceManager->setVerboseLevel(arguments.verbose);
        }
        if ( !m_deviceManager->initialize() ) {
            fprintf( stderr, "Could not initialize device manager\n" );
            delete m_deviceManager;
            return exitfunction(-1);
        }
        if ( arguments.verbose ) {
            m_deviceManager->setVerboseLevel(arguments.verbose);
        }
        if ( !m_deviceManager->discover(arguments.use_cache) ) {
            fprintf( stderr, "Could not discover devices\n" );
            delete m_deviceManager;
            return exitfunction(-1);
        }
        delete m_deviceManager;
        return exitfunction(0);
    } else if ( strcmp( arguments.args[0], "SetSamplerate" ) == 0 ) {
        char* tail;
        int samplerate = strtol( arguments.args[1], &tail, 0 );
        if ( errno ) {
            fprintf( stderr,  "Could not parse samplerate argument\n" );
            return exitfunction(-1);
        }

        DeviceManager *m_deviceManager = new DeviceManager();
        if ( !m_deviceManager ) {
            fprintf( stderr, "Could not allocate device manager\n" );
            return exitfunction(-1);
        }
        if ( arguments.verbose ) {
            m_deviceManager->setVerboseLevel(arguments.verbose);
        }
        if ( !m_deviceManager->initialize() ) {
            fprintf( stderr, "Could not initialize device manager\n" );
            delete m_deviceManager;
            return exitfunction(-1);
        }
        if ( arguments.verbose ) {
            m_deviceManager->setVerboseLevel(arguments.verbose);
        }
        if ( !m_deviceManager->discover(arguments.use_cache) ) {
            fprintf( stderr, "Could not discover devices\n" );
            delete m_deviceManager;
            return exitfunction(-1);
        }

        if(arguments.node_id_set) {
            FFADODevice* avDevice = m_deviceManager->getAvDevice( arguments.node_id );
            if ( avDevice ) {
                avDevice->setVerboseLevel(arguments.verbose);
                if ( ! avDevice->setSamplingFrequency( samplerate ) ) {
                    fprintf( stderr, "Could not set samplerate\n" );
                }
            }
        } else {
            int i=0;

            int devices_on_bus = m_deviceManager->getNbDevices();
            printf("  port = %ld, devices_on_bus = %d\n", arguments.port, devices_on_bus);

            for(i=0;i<devices_on_bus;i++) {
                int node_id=m_deviceManager->getDeviceNodeId(i);
                printf("  set samplerate for device = %d, node = %d\n", i, node_id);
                FFADODevice* avDevice = m_deviceManager->getAvDevice( node_id );
                if ( avDevice ) {
                    avDevice->setVerboseLevel(arguments.verbose);
                    if ( !avDevice->setSamplingFrequency( samplerate ) ) {
                        fprintf( stderr, "Could not set samplerate\n" );
                    }
                }
            }
        }
        delete m_deviceManager;
        return exitfunction(0);
    } else if ( strcmp( arguments.args[0], "SetClockSource" ) == 0 ) {
        char* tail;
        unsigned int targetid = (unsigned int)strtol( arguments.args[1], &tail, 0 );
        if ( errno ) {
            fprintf( stderr,  "Could not parse clock source argument\n" );
            targetid=0xFFFF;
        }
        DeviceManager *m_deviceManager = new DeviceManager();
        if ( !m_deviceManager ) {
            fprintf( stderr, "Could not allocate device manager\n" );
            return exitfunction(-1);
        }
        if ( arguments.verbose ) {
            m_deviceManager->setVerboseLevel(arguments.verbose);
        }
        if ( !m_deviceManager->initialize() ) {
            fprintf( stderr, "Could not initialize device manager\n" );
            delete m_deviceManager;
            return exitfunction(-1);
        }
        if ( arguments.verbose ) {
            m_deviceManager->setVerboseLevel(arguments.verbose);
        }
        if ( !m_deviceManager->discover(arguments.use_cache) ) {
            fprintf( stderr, "Could not discover devices\n" );
            delete m_deviceManager;
            return exitfunction(-1);
        }

        if(arguments.node_id_set) {
            FFADODevice* avDevice = m_deviceManager->getAvDevice( arguments.node_id );
            if ( avDevice ) {
                FFADODevice::ClockSource s;
            
                avDevice->setVerboseLevel(arguments.verbose);
                FFADODevice::ClockSourceVector sources=avDevice->getSupportedClockSources();
                for ( FFADODevice::ClockSourceVector::const_iterator it
                        = sources.begin();
                    it != sources.end();
                    ++it )
                {
                    FFADODevice::ClockSource c=*it;
                    printf( " Type: %s, Id: %d, Valid: %d, Active: %d, Description: %s\n",
                        FFADODevice::ClockSourceTypeToString(c.type), c.id, c.valid, c.active, c.description.c_str());
                    
                    if (c.id==targetid) {
                        s=*it;
                    }
                }
                
                if (s.type != FFADODevice::eCT_Invalid) {
                    printf("  set clock source to %d\n", s.id);
                    if ( ! avDevice->setActiveClockSource( s ) ) {
                        fprintf( stderr, "Could not set clock source\n" );
                    }
                } else {
                    printf("  no clock source with id %d found\n", targetid);
                }
            }
        } else {
            fprintf( stderr, "please specify a node\n" );
        }
        delete m_deviceManager;
        return exitfunction(0);
    }
}
