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

#include "src/fireworks/fireworks_device.h"
#include "src/fireworks/fireworks_firmware.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "debugmodule/debugmodule.h"

#include "devicemanager.h"

#include <argp.h>
#include <iostream>

#include <string.h>
#include <string>

DECLARE_GLOBAL_DEBUG_MODULE;

using namespace FireWorks;
////////////////////////////////////////////////
// arg parsing
////////////////////////////////////////////////
const char *argp_program_version = "fireworks-downloader 0.1";
const char *argp_program_bug_address = "<ffado-devel@lists.sf.net>";
static char doc[] = "fireworks-downloader -- firmware downloader application for ECHO Fireworks devices\n\n"
                    "OPERATION: display\n"
                    "           firmware FILE\n";
static char args_doc[] = "NODE_ID OPERATION";
static struct argp_option options[] = {
    {"verbose",   'v', "level",     0,  "Produce verbose output" },
    {"port",      'p', "PORT",      0,  "Set port" },
    { 0 }
};

struct arguments
{
    arguments()
        : verbose( 0 )
        , port( 0 )
        {
            args[0] = 0;
            args[1] = 0;
            args[2] = 0;
        }

    char* args[3];
    short verbose;
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
        if (arg) {
            arguments->verbose = strtol( arg, &tail, 0 );
            if ( errno ) {
                debugError( "Could not parse 'verbose' argument\n" );
                return ARGP_ERR_UNKNOWN;
            }
        }
        break;
    case 'p':
        errno = 0;
        arguments->port = strtol(arg, &tail, 0);
        if (errno) {
            debugError("argument parsing failed: %s\n",
                       strerror(errno));
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


int
main( int argc, char** argv )
{
    using namespace std;

    // arg parsing
    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    errno = 0;
    char* tail;
    int node_id = strtol(arguments.args[0], &tail, 0);
    if (errno) {
        debugError("argument parsing failed: %s\n",
                    strerror(errno));
        return -1;
    }

    Ieee1394Service service;
    if ( !service.initialize( arguments.port ) ) {
        debugError("Could not initialize IEEE 1394 service\n");
        return -1;
    }
    service.setVerboseLevel( arguments.verbose );

    ConfigRom *configRom = new ConfigRom(service, node_id );
    if (configRom == NULL) {
        debugError("Could not create ConfigRom\n");
        return -1;
    }
    configRom->setVerboseLevel( arguments.verbose );

    if ( !configRom->initialize() ) {
        debugError( "Could not read config rom from device (node id %d).\n",
                    node_id );
        delete configRom;
        return -1;
    }

    if ( !Device::probe(*configRom) ) {
        debugError( "Device with node id %d is not an ECHO FireWorks device.\n",
                    node_id );
        delete configRom;
        return -1;
    }
    
    DeviceManager d = DeviceManager();
    Device *dev = new Device(d, std::auto_ptr<ConfigRom>(configRom) );
    if (dev == NULL) {
        debugError("Could not create FireWorks::Device\n");
        delete configRom;
        return -1;
    }

    // create the firmware util class
    FirmwareUtil util = FirmwareUtil(*dev);
    util.setVerboseLevel( arguments.verbose );
    
    if ( strcmp( arguments.args[1], "firmware" ) == 0 ) {
        if (!arguments.args[2] ) {
            cerr << "FILE argument is missing" << endl;
            delete dev;
            return -1;
        }
        std::string str( arguments.args[2] );

        // load the file
        Firmware f = Firmware();
        f.setVerboseLevel( arguments.verbose );
        
        f.loadFile(str);
        f.show();

    } else if ( strcmp( arguments.args[1], "display" ) == 0 ) {
        // nothing to do
        dev->showDevice();
    } else {
        cout << "Unknown operation" << endl;
    }

    delete dev;
    return 0;
}
