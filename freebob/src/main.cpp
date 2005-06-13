/* main.cpp
 * Copyright (C) 2004 by Daniel Wagner
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

#include <argp.h>
#include <stdlib.h>

#include <config.h>
#include "debugmodule.h"
#include "streamprocess.h"
#include "ipchandler.h"

DECLARE_GLOBAL_DEBUG_MODULE;

const char *argp_program_version = PACKAGE_STRING;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;

// Program documentation.
static char doc[] = "FreeBob -- a driver for BeBob devices";

// A description of the arguments we accept.
static char args_doc[] = "";

// The options we understand.
static struct argp_option options[] = {
    {"verbose",  'v', 0,    0,  "Produce verbose output" },
    {"quiet",    'q', 0,    0,  "Don't produce any output" },
    {"silent",   's', 0,    OPTION_ALIAS },
    {"time",     't', "sec",    0,  "Time in seconds until daemon stops itself" },
    {"port",     'p', "nr",    0,  "IEEE1394 Port to use" },
    { 0 }
};

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
        arguments->verbose = 1;
        break;
    case 't':
        if (arg) {
        	arguments->time = strtol( arg, &tail, 0 );
			if ( errno ) {
				debugGlobalError( "Could not parse 'time' argument\n" );
				return ARGP_ERR_UNKNOWN;
			}
		} else {
			if ( errno ) {
				debugGlobalError( "Could not parse 'time' argument\n" );
				return ARGP_ERR_UNKNOWN;
			}
		}
        break;
    case 'p':
        if (arg) {
			arguments->port = strtol( arg, &tail, 0 );
			if ( errno ) {
				debugGlobalError( "Could not parse 'port' argument\n" );
				return ARGP_ERR_UNKNOWN;
			}
		} else {
			if ( errno ) {
				debugGlobalError( "Could not parse 'port' argument\n" );
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

struct arguments* pMainArguments;

int
main( int argc,  char** argv )
{
    struct arguments arguments;
    pMainArguments = &arguments;

    // Default values.
    arguments.silent = 0;
    arguments.verbose = 0;
    arguments.time = -1;
    arguments.port = 0;

    // Parse our arguments; every option seen by `parse_opt' will
    // be reflected in `arguments'.
    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    setGlobalDebugLevel( DEBUG_LEVEL_ALL );

    StreamProcess* pStreamProcess = new StreamProcess();
    pStreamProcess->run( arguments.time );

    delete pStreamProcess;
    
    return 0;
}
