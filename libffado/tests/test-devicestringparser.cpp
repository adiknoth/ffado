/*
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <argp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debugmodule/debugmodule.h"

#include "src/DeviceStringParser.h"

DECLARE_GLOBAL_DEBUG_MODULE;

// Program documentation.
static char doc[] = "FFADO -- Watchdog test\n\n";

// A description of the arguments we accept.
static char args_doc[] = "";


struct arguments
{
    short verbose;
};

// The options we understand.
static struct argp_option options[] = {
    {"verbose",     'v',    "n",    0,  "Verbose level" },
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
        case 'v':
            if (arg) {
                arguments->verbose = strtoll( arg, &tail, 0 );
                if ( errno ) {
                    fprintf( stderr, "Could not parse 'verbose' argument\n" );
                    return ARGP_ERR_UNKNOWN;
                }
            } else {
                if ( errno ) {
                    fprintf( stderr, "Could not parse 'verbose' argument\n" );
                    return ARGP_ERR_UNKNOWN;
                }
            }
            break;
       default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

// Our argp parser.
static struct argp argp = { options, parse_opt, args_doc, doc };

int main(int argc, char *argv[])
{

    struct arguments arguments;

    // Default values.
    arguments.verbose        = DEBUG_LEVEL_VERY_VERBOSE;

    // Parse our arguments; every option seen by `parse_opt' will
    // be reflected in `arguments'.
    if ( argp_parse ( &argp, argc, argv, 0, 0, &arguments ) ) {
        fprintf( stderr, "Could not parse command line\n" );
        exit(1);
    }

    setDebugLevel(arguments.verbose);

    DeviceStringParser p = DeviceStringParser();
    p.setVerboseLevel(arguments.verbose);

    p.parseString("hw:0;hw:1");
    p.parseString("hw:0;hw:1;");
    p.parseString("hw:0,0;hw:1,1;");
    p.parseString("hw:1,1;hw:1,0");
    p.parseString("guid:0x123456789");
    p.show();

    return EXIT_SUCCESS;
}


