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
 * ISO xmit stall test
 * This test stalls on certain controllers from a packet size = 225 on
 *
 * Controllers affected:
 *  O2 Micro, Inc. Firewire (IEEE 1394) (rev 02)
 *  Texas Instruments XIO2200(A) IEEE-1394a-2000 Controller (PHY/Link) (rev 01)
 * not affected:
 *  NEC Corporation uPD72874 IEEE1394 OHCI 1.1 3-port PHY-Link Ctrlr (rev 01)
 *
 * gcc -g -O2 -Wall -D_REENTRANT -std=c99 -D_GNU_SOURCE -o testxmitstall testxmitstall.c -lm -lpthread -lraw1394
 *
 */

/*
 * based on howdysend.c (unknown source, maybe Maas Digital LLC)
 */

#include <libraw1394/raw1394.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <argp.h>

#include "debugmodule/debugmodule.h"
#include "realtimetools.h"

#define SPEED           RAW1394_ISO_SPEED_400

uint32_t count = 0;

DECLARE_GLOBAL_DEBUG_MODULE;

#define MAX_EXTRA_ARGS 2
// Program documentation.
// Program documentation.
static char doc[] = "FFADO -- ISO transmit stall test\n\n";

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
    long int prebuffers;
    long int startoncycle;
    long int countdown;
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
    {"prebuffers",  'x', "packets",  0,  "number of packets to prebuffer" },
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

    errno = 0;
    switch (key) {
    PARSE_ARG_LONG('v', arguments->verbose, "verbose");
    PARSE_ARG_LONG('p', arguments->port, "port");
    PARSE_ARG_LONG('c', arguments->channel, "channel");
    PARSE_ARG_LONG('s', arguments->packetsize, "packetsize");
    PARSE_ARG_LONG('b', arguments->buffersize, "buffersize");
    PARSE_ARG_LONG('i', arguments->interval, "interval");
    PARSE_ARG_LONG('x', arguments->prebuffers, "prebuffers");
    PARSE_ARG_LONG('t', arguments->startoncycle, "startoncycle");
    PARSE_ARG_LONG('u', arguments->countdown, "countdown");
    PARSE_ARG_LONG('r', arguments->printinterval, "printinterval");
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

int32_t last_cycle = -1;

static enum raw1394_iso_disposition myISOSender(raw1394handle_t handle,
        unsigned char *data,
        uint32_t *len,
        unsigned char *tag,
        unsigned char *sy,
        int32_t cycle,
        uint32_t dropped1)
{
    unsigned int skipped = (dropped1 & 0xFFFF0000) >> 16;
    unsigned int dropped = dropped1 & 0xFFFF;

    //debugOutput(DEBUG_LEVEL_INFO, "In handler\n");
    //sprintf((char *)data, "Hello World %8u\n", count);
    if (last_cycle >= 0) {
        if (cycle != (last_cycle + 1) % 8000) {
            debugOutput(DEBUG_LEVEL_INFO, "nonmonotonic cycles: this: %04d, last: %04d, dropped: 0x%08X\n", cycle, last_cycle, dropped1);
        }
    }
    last_cycle = cycle;
    count++;
    *len = arguments.packetsize;
    *tag = 1;
    *sy = 0;
    if(dropped) debugOutput(DEBUG_LEVEL_INFO, "Dropped packets! (%d)\n", dropped);
    if(skipped) debugOutput(DEBUG_LEVEL_INFO, "Skipped packets! (%d)\n", skipped);
    if (count % arguments.printinterval == 0) debugOutput(DEBUG_LEVEL_INFO, "cycle=%d count=%d\n", cycle, count); 
    return RAW1394_ISO_OK;
}

void prepareXmit(raw1394handle_t handle)
{
    if(raw1394_iso_xmit_init(handle,
                myISOSender,
                arguments.buffersize,
                arguments.packetsize,
                arguments.channel,
                SPEED,
                arguments.interval))
    {
        perror("raw1394_iso_xmit_init");
        exit(1);
    }

    if(raw1394_iso_xmit_start(handle, arguments.startoncycle, arguments.prebuffers))
    {
        perror("raw1394_iso_xmit_start");
        exit(1);
    }
}

int32_t myResetHandler(raw1394handle_t handle, uint32_t generation)
{    
    debugOutput(DEBUG_LEVEL_INFO, "Reset happened\n");    
    raw1394_iso_shutdown(handle);
    raw1394_update_generation(handle, generation);
    prepareXmit(handle);
    return 0;
}

int32_t main(int32_t argc, char **argv)
{
    raw1394handle_t handle;

    // Default values.
    arguments.verbose = DEBUG_LEVEL_VERBOSE;
    arguments.port = 0;
    arguments.channel = 0;
    arguments.packetsize = 1024;
    arguments.buffersize = 100;
    arguments.interval = -1;
    arguments.prebuffers = 0;
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

    debugOutput(DEBUG_LEVEL_NORMAL, "verbose level = %d\n", arguments.verbose);
    setDebugLevel(arguments.verbose);

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
        raw1394_set_port(handle, arguments.port );
    } while (errno == ESTALE);

    if(errno)
    {
        perror("raw1394_set_port");
        return 1;
    }

    debugOutput(DEBUG_LEVEL_INFO, "Prepare/start ISO transmit...\n");
    prepareXmit(handle);

    if(raw1394_busreset_notify(handle, RAW1394_NOTIFY_ON))
    {
        perror("raw1394_busreset_notify");
        exit(1);
    }
    raw1394_set_bus_reset_handler(handle, myResetHandler);

    debugOutput(DEBUG_LEVEL_INFO, "Setting RT priority (%d)...\n", arguments.rtprio);
    set_realtime_priority(arguments.rtprio);

    int countdown = arguments.countdown;
    debugOutput(DEBUG_LEVEL_INFO, "Starting iterate loop...\n");
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
