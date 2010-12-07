/*
 * Copyright (C) 2005-2008 by Pieter Palmers
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

#include "debugmodule/debugmodule.h"

#include "libutil/PosixSharedMemory.h"

#include <argp.h>
#include <stdlib.h>
#include <iostream>
#include <signal.h>

using namespace Util;

DECLARE_GLOBAL_DEBUG_MODULE;

#define MAX_ARGS 2

int run=1;
int lastsig=-1;
static void sighandler (int sig)
{
    run = 0;
}

////////////////////////////////////////////////
// arg parsing
////////////////////////////////////////////////
const char *argp_program_version = "test-shm 0.1";
const char *argp_program_bug_address = "<ffado-devel@lists.sf.net>";
static char doc[] = "test-avccmd -- test program to test the shared memory class.";
static char args_doc[] = "DIRECTION";
static struct argp_option options[] = {
    {"verbose",  'v', "level",    0,  "Produce verbose output" },
   { 0 }
};

struct arguments
{
    arguments()
        : nargs ( 0 )
        , verbose( false )
        {
            args[0] = 0;
        }

    char* args[MAX_ARGS];
    int   nargs;
    long int verbose;
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
        if (arg) {
            arguments->verbose = strtol( arg, &tail, 0 );
            if ( errno ) {
                fprintf( stderr,  "Could not parse 'verbose' argument\n" );
                return ARGP_ERR_UNKNOWN;
            }
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
        if(arguments->nargs <= 0) {
            printMessage("not enough arguments\n");
            return -1;
        }
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
    signal (SIGINT, sighandler);
    signal (SIGPIPE, sighandler);

    // arg parsing
    if ( argp_parse ( &argp, argc, argv, 0, 0, &arguments ) ) {
        fprintf( stderr, "Could not parse command line\n" );
        exit(-1);
    }

    setDebugLevel(arguments.verbose);

    errno = 0;
    char* tail;
    long int direction = strtol( arguments.args[0], &tail, 0 );
    if ( errno ) {
        fprintf( stderr,  "Could not parse direction argument\n" );
        exit(-1);
    }

    printMessage("Testing shared memory direction %ld\n", direction);

    PosixSharedMemory s = PosixSharedMemory("testseg", 1024);
    s.setVerboseLevel(arguments.verbose);

    if(direction == 0) {
        if(!s.Create(PosixSharedMemory::eD_ReadWrite)) {
            debugError("Could not create segment\n");
            exit(-1);
        }
    } else {
        if(!s.Open(PosixSharedMemory::eD_ReadOnly)) {
            debugError("Could not open segment\n");
             exit(-1);
       }
    }

    if(!s.LockInMemory(true)) {
        debugError("Could not memlock segment\n");
    }

    int offset=0;
    int len = 64;
    char buff[len];

    int cnt = 0;
    run=1;
    long int time_to_sleep = 1000*1000;
    while(run) {
        if(direction == 0) {
            snprintf(buff, len, "test %d", cnt++);
            printMessage("writing '%s'...\n", buff);
            if(s.Write(offset, buff, len) != PosixSharedMemory::eR_OK) {
                debugError("Could not write to segment\n");
                goto out_err;
            }
            usleep(time_to_sleep);
        } else {
            printMessage("reading...\n");
            if(s.Read(offset, buff, len) != PosixSharedMemory::eR_OK) {
                debugError("Could not receive from queue\n");
                goto out_err;
            }
            buff[len-1]=0;
            printMessage(" read: '%s'\n", buff);
            usleep(time_to_sleep * 110/100);
        }
    }

    if(!s.LockInMemory(false)) {
        debugError("Could not mem-unlock segment\n");
    }

    return 0;

out_err:
    if(!s.LockInMemory(false)) {
        debugError("Could not mem-unlock segment\n");
    }
    return -1;
}

