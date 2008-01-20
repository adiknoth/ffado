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

#include "src/bebob/bebob_dl_mgr.h"
#include "src/bebob/bebob_dl_bcd.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include <argp.h>
#include <iostream>


////////////////////////////////////////////////
// arg parsing
////////////////////////////////////////////////
const char *argp_program_version = "bridgeco-downloader 0.1";
const char *argp_program_bug_address = "<ffado-devel@lists.sf.net>";
static char doc[] = "bridgeco-downloader -- firmware downloader application for BridgeCo devices\n\n"
                    "OPERATION: display\n"
                    "           setguid GUID\n"
                    "           firmware FILE\n"
                    "           cne FILE\n"
                    "           bcd FILE\n";
static char args_doc[] = "NODE_ID OPERATION";
static struct argp_option options[] = {
    {"verbose",   'v', "level",     0,  "Produce verbose output" },
    {"port",      'p', "PORT",      0,  "Set port" },
    {"force",     'f', 0,           0,  "Force firmware download" },
    {"noboot",    'b', 0,           0,  "Do no start bootloader (bootloader is already running)" },
    { 0 }
};

struct arguments
{
    arguments()
        : verbose( 0 )
        , port( 0 )
        , force( 0 )
        {
            args[0] = 0;
            args[1] = 0;
            args[2] = 0;
        }

    char* args[3];
    short verbose;
    int   port;
    int   force;
    int   no_bootloader_restart;
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
                fprintf( stderr,  "Could not parse 'verbose' argument\n" );
                return ARGP_ERR_UNKNOWN;
            }
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
    perror("argument parsing failed:");
    return -1;
    }

    Ieee1394Service service;
    if ( !service.initialize( arguments.port ) ) {
        cerr << "Could not initialize IEEE 1394 service" << endl;
        return -1;
    }

    service.setVerboseLevel( arguments.verbose );
    BeBoB::BootloaderManager blMgr( service, node_id );
    if ( arguments.force == 1 ) {
        blMgr.setForceOperations( true );
    }
    blMgr.getConfigRom()->printConfigRom();
    blMgr.printInfoRegisters();

    if ( strcmp( arguments.args[1], "setguid" ) == 0 ) {
        if (!arguments.args[2] ) {
            cerr << "guid argument is missing" << endl;
            return -1;
        }

        char* tail;
        fb_octlet_t guid = strtoll(arguments.args[2], &tail, 0 );

        if ( !blMgr.programGUID( guid ) ) {
            cerr << "Failed to set GUID" << endl;
            return -1;
        } else {
            cout << "new GUID programmed" << endl;
        }
    } else if ( strcmp( arguments.args[1], "firmware" ) == 0 ) {
        if (!arguments.args[2] ) {
            cerr << "FILE argument is missing" << endl;
            return -1;
        }
        std::string str( arguments.args[2] );

        blMgr.setStartBootloader( arguments.no_bootloader_restart != 1 );
        if ( !blMgr.downloadFirmware( str ) ) {
            cerr << "Failed to download firmware" << endl;
            return -1;
        } else {
            cout << "Firmware download was successful" << endl;
        }
    } else if ( strcmp( arguments.args[1], "cne" ) == 0 ) {
        if (!arguments.args[2] ) {
            cerr << "FILE argument is missing" << endl;
            return -1;
        }
        std::string str( arguments.args[2] );

        if ( !blMgr.downloadCnE( str ) ) {
            cerr << "Failed to download CnE" << endl;
            return -1;
        } else {
            cout << "CnE download was successful" << endl;
        }
    } else if ( strcmp( arguments.args[1], "display" ) == 0 ) {
        // nothing to do
    } else if ( strcmp( arguments.args[1], "bcd" ) == 0 ) {
        if ( !arguments.args[2] ) {
            cerr << "FILE arguments is missing" << endl;
            return -1;
        }
        BeBoB::BCD* bcd = new BeBoB::BCD::BCD( arguments.args[2] );
        if ( !bcd ) {
            cerr << "Could no open file " << arguments.args[2] << endl;
            return -1;
        }
        if ( !bcd->parse() ) {
            cerr << "Could not parse file " << arguments.args[2] << endl;
            return -1;
        }

        bcd->displayInfo();
        delete bcd;
    } else {
        cout << "Unknown operation" << endl;
    }

    return 0;
}
