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
#include <cstring>
#include <signal.h>

#include <math.h>

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
static char args_doc[] = "";
static struct argp_option options[] = {
    {"verbose",  'v', "level",    0,  "Produce verbose output" },
    {"playback", 'o', "count",    0,  "Number of playback channels" },
    {"capture",  'i', "count",    0,  "Number of capture channels" },
    {"period",  'p', "frames",    0,  "Period size in frames" },
    {"nb_buffers",  'n', "count",    0,  "Number of IPC buffers" },
  { 0 }
};

struct arguments
{
    arguments()
        : nargs ( 0 )
        , verbose( false )
        , playback( 0 )
        , capture( 0 )
        {
            args[0] = 0;
        }

    char* args[MAX_ARGS];
    int   nargs;
    long int verbose;
    long int playback;
    long int capture;
    long int period;
    long int nb_buffers;
    long int test_tone;
    float test_tone_freq;
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
    case 'o':
        if (arg) {
            arguments->playback = strtol( arg, &tail, 0 );
            if ( errno ) {
                fprintf( stderr,  "Could not parse 'playback' argument\n" );
                return ARGP_ERR_UNKNOWN;
            }
        }
        break;
    case 'i':
        if (arg) {
            arguments->capture = strtol( arg, &tail, 0 );
            if ( errno ) {
                fprintf( stderr,  "Could not parse 'capture' argument\n" );
                return ARGP_ERR_UNKNOWN;
            }
        }
        break;
    case 'p':
        if (arg) {
            arguments->period = strtol( arg, &tail, 0 );
            if ( errno ) {
                fprintf( stderr,  "Could not parse 'periods' argument\n" );
                return ARGP_ERR_UNKNOWN;
            }
        }
        break;
    case 'n':
        if (arg) {
            arguments->nb_buffers = strtol( arg, &tail, 0 );
            if ( errno ) {
                fprintf( stderr,  "Could not parse 'nb_buffers' argument\n" );
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
//         if(arguments->nargs <= 0) {
//             printMessage("not enough arguments\n");
//             return -1;
//         }
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

    arguments.verbose           = 6;
    arguments.period            = 1024;
    arguments.nb_buffers        = 3;
    arguments.playback          = 0;
    arguments.capture           = 0;
    arguments.test_tone_freq    = 0.01;
    arguments.test_tone         = 1;

    // arg parsing
    if ( argp_parse ( &argp, argc, argv, 0, 0, &arguments ) ) {
        fprintf( stderr, "Could not parse command line\n" );
        exit(-1);
    }

    setDebugLevel(arguments.verbose);

    if(arguments.playback == 0 && arguments.capture == 0) {
        debugError("No playback nor capture channels requested\n");
        return -1;
    }

    printMessage("Testing shared memory streaming IPC\n");
    printMessage(" period %d, nb_buffers %d, playback %d, capture %d\n",
                 arguments.period, arguments.nb_buffers,
                 arguments.playback,
                 arguments.capture );

    // prepare the IPC buffers
    unsigned int capture_buffsize = arguments.capture * arguments.period * 4;
    unsigned int playback_buffsize = arguments.playback * arguments.period * 4;
    IpcRingBuffer* capturebuffer = NULL;
    IpcRingBuffer* playbackbuffer = NULL;
    if(arguments.playback) {
        playbackbuffer = new IpcRingBuffer("playbackbuffer",
                              IpcRingBuffer::eBT_Slave,
                              IpcRingBuffer::eD_Outward,
                              IpcRingBuffer::eB_Blocking,
                              arguments.nb_buffers, playback_buffsize);
        if(playbackbuffer == NULL) {
            debugError("Could not create playbackbuffer\n");
            exit(-1);
        }
        if(!playbackbuffer->init()) {
            debugError("Could not init playbackbuffer\n");
            delete playbackbuffer;
            exit(-1);
        }
        playbackbuffer->setVerboseLevel(arguments.verbose);
    }
    if(arguments.capture) {
        capturebuffer = new IpcRingBuffer("capturebuffer",
                              IpcRingBuffer::eBT_Slave,
                              IpcRingBuffer::eD_Inward,
                              IpcRingBuffer::eB_Blocking,
                              arguments.nb_buffers, capture_buffsize);
        if(capturebuffer == NULL) {
            debugError("Could not create capturebuffer\n");
            delete playbackbuffer;
            exit(-1);
        }
        if(!capturebuffer->init()) {
            debugError("Could not init capturebuffer\n");
            delete playbackbuffer;
            delete capturebuffer;
            exit(-1);
        }
        capturebuffer->setVerboseLevel(arguments.verbose);
    }


    // dummy buffers
    char capture_buff[capture_buffsize];
    char playback_buff[playback_buffsize];

    int cnt = 0;
    int pbkcnt = 0;

    float frame_counter = 0.0;
    float sine_advance = 0.0;
    float amplitude = 0.97;
    sine_advance = 2.0*M_PI*arguments.test_tone_freq;
    uint32_t sine_buff[arguments.period];

    run=1;
    while(run) {
        // test tone generation
        if (arguments.test_tone) {
            // generate the test tone
            for (int i=0; i < arguments.period; i++) {
                float v = amplitude * sin(sine_advance * (frame_counter + (float)i));
                v = (v * 2147483392.0);
                int32_t tmp = ((int) v);
                tmp = tmp >> 8;
                memcpy(&sine_buff[i], &tmp, 4);
            }
            for(int j=0; j<arguments.playback; j++) {
                uint32_t *target = (uint32_t *)(playback_buff + j*arguments.period*4);
                memcpy(target, &sine_buff, arguments.period*4);
            }
        }
        frame_counter += arguments.period;

        // write the data
        IpcRingBuffer::eResult res;
        if(playbackbuffer) {
            res = playbackbuffer->Write(playback_buff);
            if(res != IpcRingBuffer::eR_OK && res != IpcRingBuffer::eR_Again) {
                debugError("Could not write to segment\n");
                goto out_err;
            }
            if(res == IpcRingBuffer::eR_Again) {
                printMessage(" Try playback again on %d...\n", cnt);
            } else {
                if(pbkcnt%100==0) {
                    printMessage(" Period %d...\n", pbkcnt);
                }
                pbkcnt++;
            }
        }
        // read data
        if (capturebuffer) {
            res = capturebuffer->Read(capture_buff);
            if(res != IpcRingBuffer::eR_OK && res != IpcRingBuffer::eR_Again) {
                debugError("Could not receive from queue\n");
                goto out_err;
            }
            if(res == IpcRingBuffer::eR_Again) {
                printMessage(" Try again on %d...\n", cnt);
            } else {
                if(cnt%10==0) {
                    uint32_t *tmp = (uint32_t *)capture_buff;
                    for(int i=0;i<arguments.capture;i++) {
                        printMessage(" channel %d: ", i);
                        for(int j=0; j < 6;j+=1) {
                            uint32_t tmp2 = tmp[j] << 8;
                            int32_t *tmp3 = (int32_t *)&tmp2;
                            printMessageShort("%10d ", *tmp3);
                        }
                        tmp += arguments.period;
                        printMessageShort("\n");
                    }
                }
                cnt++;
            }
        }
    }

    delete capturebuffer;
    delete playbackbuffer;
    return 0;

out_err:
    delete capturebuffer;
    delete playbackbuffer;
    return -1;
}

