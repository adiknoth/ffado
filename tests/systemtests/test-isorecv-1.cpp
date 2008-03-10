/*
 * Parts Copyright (C) 2005-2008 by Pieter Palmers
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

/*
 * based on howdyget.c (unknown source, maybe Maas Digital LLC)
 */

#include <libraw1394/raw1394.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include <argp.h>

#include "debugmodule/debugmodule.h"
#include "realtimetools.h"

uint32_t count = 0;

DECLARE_GLOBAL_DEBUG_MODULE;

#define MAX_EXTRA_ARGS 2
// Program documentation.
// Program documentation.
static char doc[] = "FFADO -- ISO receive test\n\n";

// A description of the arguments we accept.
static char args_doc[] = "";

struct arguments
{
    long int verbose;
    long int port;
    long int channel;
    long int packetsize;
    long int buffersize;
    long int interval;
    long int countdown;
    long int startoncycle;
    long int printinterval;
    long int rtprio;
    char* args[MAX_EXTRA_ARGS];
};

// The options we understand.
static struct argp_option options[] = {
    {"verbose",  'v', "level",    0,  "Verbose level" },
    {"port",  'p', "port",  0,  "firewire port to use" },
    {"channel",  'c', "channel",  0,  "iso channel to use" },
    {"packetsize",  's', "packetsize",  0,  "packet size to use" },
    {"buffersize",  'b', "buffersize",  0,  "number of packets to buffer" },
    {"interval",  'i', "interval",  0,  "interrupt interval in packets" },
    {"startoncycle",  't', "cycle",  0,  "start on a specific cycle" },
     {"countdown",  'u', "count",  0,  "number of iterations to run" },
    {"printinterval",  'r', "packets",  0,  "number of packets between successive status prints" },
    {"rtprio",  'P', "prio",  0,  "real time priority of the iterator process/thread (0 = no RT)" },
    { 0 }
};

// Parse a single option.
#define PARSE_ARG_LONG(XXletterXX, XXvarXX, XXdescXX) \
    case XXletterXX: \
        if (arg) { \
            XXvarXX = strtol( arg, &tail, 0 ); \
            if ( errno ) { \
                fprintf( stderr,  "Could not parse '%s' argument\n", XXdescXX ); \
                return ARGP_ERR_UNKNOWN; \
            } \
        } \
        break;

static error_t
parse_opt( int key, char* arg, struct argp_state* state )
{
    // Get the input argument from `argp_parse', which we
    // know is a pointer to our arguments structure.
    struct arguments* arguments = ( struct arguments* ) state->input;
    char* tail;

    switch (key) {
    PARSE_ARG_LONG('v', arguments->verbose, "verbose");
    PARSE_ARG_LONG('p', arguments->port, "port");
    PARSE_ARG_LONG('c', arguments->channel, "channel");
    PARSE_ARG_LONG('s', arguments->packetsize, "packetsize");
    PARSE_ARG_LONG('b', arguments->buffersize, "buffersize");
    PARSE_ARG_LONG('i', arguments->interval, "interval");
    PARSE_ARG_LONG('u', arguments->countdown, "countdown");
    PARSE_ARG_LONG('r', arguments->printinterval, "printinterval");
    PARSE_ARG_LONG('t', arguments->startoncycle, "startoncycle");
    PARSE_ARG_LONG('P', arguments->rtprio, "rtprio");

    case ARGP_KEY_ARG:
        break;
    case ARGP_KEY_END:
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

// Our argp parser.
static struct argp argp = { options, parse_opt, args_doc, doc };

// the global arguments struct
struct arguments arguments;

int last_cycle = -1;

static enum raw1394_iso_disposition myISOGetter(raw1394handle_t handle,
        unsigned char *data,
        unsigned int len,
        unsigned char channel,
        unsigned char tag,
        unsigned char sy,
        unsigned int cycle,
        unsigned int dropped1)
{
    unsigned int skipped = (dropped1 & 0xFFFF0000) >> 16;
    unsigned int dropped = dropped1 & 0xFFFF;

    //debugOutput(DEBUG_LEVEL_INFO, "In handler\n");
    //sprintf((char *)data, "Hello World %8u\n", count);
    if (last_cycle >= 0) {
        if (cycle != ((unsigned int)last_cycle + 1) % 8000) {
            debugOutput(DEBUG_LEVEL_INFO, "nonmonotonic cycles: this: %04d, last: %04d, dropped: 0x%08X\n", cycle, last_cycle, dropped1);
        }
    }
    last_cycle = cycle;
    count++;
    if(dropped) debugOutput(DEBUG_LEVEL_INFO, "Dropped packets! (%d)\n", dropped);
    if(skipped) debugOutput(DEBUG_LEVEL_INFO, "Skipped packets! (%d)\n", skipped);
    if (count % arguments.printinterval == 0) debugOutput(DEBUG_LEVEL_INFO, "cycle=%d count=%d\n", cycle, count); 
    return RAW1394_ISO_OK;
}

int main(int argc, char **argv)
{
    raw1394handle_t handle;

    // Default values.
    arguments.verbose = DEBUG_LEVEL_VERBOSE;
    arguments.port = 0;
    arguments.channel = 0;
    arguments.packetsize = 1024;
    arguments.buffersize = 100;
    arguments.interval = -1;
    arguments.startoncycle = -1;
    arguments.countdown = 10000;
    arguments.printinterval = 100;
    arguments.rtprio = 0;

    // Parse our arguments; every option seen by `parse_opt' will
    // be reflected in `arguments'.
    if ( argp_parse ( &argp, argc, argv, 0, 0, &arguments ) ) {
        debugError("Could not parse command line\n" );
        return -1;
    }

    debugOutput(DEBUG_LEVEL_INFO, "Get 1394 handle...\n");
    handle = raw1394_new_handle();
    if(!handle)
    {
        perror("raw1394_new_handle");
        return -1;
    }

    debugOutput(DEBUG_LEVEL_INFO, "Select 1394 port %d...\n", arguments.port);
    do
    {
        if (raw1394_get_port_info(handle, NULL, 0) < 0)
        {
            perror("raw1394_get_port_info");
            return 1;
        }
        raw1394_set_port(handle, arguments.port);
    } while (errno == ESTALE);

    if(errno)
    {
        perror("raw1394_set_port");
        return 1;
    }

    debugOutput(DEBUG_LEVEL_INFO, "Init ISO receive...\n");
    if(raw1394_iso_recv_init(handle,
                myISOGetter,
                arguments.buffersize,
                arguments.packetsize,
                arguments.channel,
                RAW1394_DMA_DEFAULT,
                arguments.interval))
    {
        perror("raw1394_iso_recv_init");
        return 1;
    }

    debugOutput(DEBUG_LEVEL_INFO, "Start ISO receive...\n");
    if(raw1394_iso_recv_start(handle, arguments.startoncycle, 0xF, 0))
    {
        perror("raw1394_iso_recv_start");
        return 1;
    }

    debugOutput(DEBUG_LEVEL_INFO, "Setting RT priority (%d)...\n", arguments.rtprio);
    set_realtime_priority(arguments.rtprio);

    debugOutput(DEBUG_LEVEL_INFO, "Starting iterate loop...\n");
    int countdown = arguments.countdown;
    while(countdown--)
    {
        if(raw1394_loop_iterate(handle))
        {
            perror("raw1394_loop_iterate");
            return 1;
        }
    }
    return 0;
}
