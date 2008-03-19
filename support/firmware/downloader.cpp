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

#include "downloader.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "debugmodule/debugmodule.h"

#include <iostream>

#include <stdlib.h>

using namespace std;

DECLARE_GLOBAL_DEBUG_MODULE;

static char args_doc[] = "GUID OPERATION";
static struct argp _argp = { options, parse_opt, args_doc, doc }; 
struct argp* argp = &_argp;
static struct arguments _args = { {0}, };
struct arguments* args = &_args;

error_t
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
    case 'f':
        arguments->force = 1;
        break;
    case 'b':
        arguments->no_bootloader_restart = 1;
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
            printDeviceList();
            exit(0);
        }
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

void 
printDeviceList()
{
    Ieee1394Service service;
    if ( !service.initialize( args->port ) ) {
        cerr << "Could not initialize IEEE 1394 service" << endl;
        exit(-1);
    }

    cout << "Node id        GUID                  Vendor - Model" << endl;
    for (int i = 0; i < service.getNodeCount(); i++) {
        ConfigRom crom(service, i);
        if (!crom.initialize())
            break;

        cout << i << "             "
             << " 0x" <<  crom.getGuidString()
             << "    '" << crom.getVendorName()
             << "' - '" << crom.getModelName() << "'" << endl;
    }
}
