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

#include "libutil/IpcRingBuffer.h"
#include "libutil/SystemTimeSource.h"

#include <argp.h>
#include <stdlib.h>
#include <iostream>
#include <signal.h>
#include <unistd.h>

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
const char *argp_program_version = "test-ipcringbuffer 0.1";
const char *argp_program_bug_address = "<ffado-devel@lists.sf.net>";
static char doc[] = "test-avccmd -- test program to test the ipc ringbuffer class.";
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

    #define TEST_SAMPLERATE 44100
    #define BUFF_SIZE 64
    #define NB_BUFFERS 4
    IpcRingBuffer* b = NULL;
    if(direction == 0) {
        b = new IpcRingBuffer("testbuff",
                              IpcRingBuffer::eBT_Master,
                              IpcRingBuffer::eD_Outward,
                              IpcRingBuffer::eB_Blocking,
                              NB_BUFFERS, BUFF_SIZE);
        if(b == NULL) {
            debugError("Could not create master\n");
            exit(-1);
        }
    } else {
        b = new IpcRingBuffer("testbuff",
                              IpcRingBuffer::eBT_Master,
                              IpcRingBuffer::eD_Inward,
                              IpcRingBuffer::eB_Blocking,
                              NB_BUFFERS, BUFF_SIZE);
        if(b == NULL) {
            debugError("Could not create master\n");
            exit(-1);
        }
    }

    b->setVerboseLevel(arguments.verbose);

    char buff[BUFF_SIZE];
    int cnt = 0;
    long int time_to_sleep = 1000*1000*BUFF_SIZE/TEST_SAMPLERATE;

    if(!b->init()) {
        debugError("Could not init buffer\n");
        goto out_err;
    }

    run=1;
    while(run) {
        if(direction == 0) {
            snprintf(buff, BUFF_SIZE, "test %d", cnt);
            if(cnt%1000==0) {
                printMessage("writing '%s'...\n", buff);
            }
            IpcRingBuffer::eResult res = b->Write(buff);
            if(res != IpcRingBuffer::eR_OK && res != IpcRingBuffer::eR_Again) {
                debugError("Could not write to segment\n");
                goto out_err;
            }
            if(res == IpcRingBuffer::eR_Again) {
                printMessage(" Try again on %d...\n", cnt);
            } else {
                cnt++;
            }
            usleep(time_to_sleep);
        } else {
            if(cnt%1000==0) {
                printMessage("reading...\n");
            }

            IpcRingBuffer::eResult res = b->Read(buff);
            if(res != IpcRingBuffer::eR_OK && res != IpcRingBuffer::eR_Again) {
                debugError("Could not receive from queue\n");
                goto out_err;
            }
            if(cnt%1000==0) {
                buff[BUFF_SIZE-1]=0;
                printMessage(" read: '%s'\n", buff);
            }
            if(res == IpcRingBuffer::eR_Again) {
                printMessage(" Try again on %d...\n", cnt);
            } else {
                cnt++;
            }
        }
    }

    delete b;
    return 0;

out_err:
    delete b;
    return -1;
}

