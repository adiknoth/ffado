/*
 * Copyright (C) 2009-2010 by Pieter Palmers
 *
 * Based upon code provided by Jay Fenlason
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

/*
 * Test programs for the new firewire kernel stack.
 * Dumpiso.
 */
 

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <fcntl.h>
#include <linux/firewire-cdev.h>
#include <linux/firewire-constants.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include <argp.h>

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#include <stdlib.h>
#include <string.h>

#include "libstreaming/connections.h"
#include "libstreaming/streamer.h"
#include "libstreaming/am824stream.h"

#include "debugmodule/debugmodule.h"
DECLARE_GLOBAL_DEBUG_MODULE;

#define MAX_EXTRA_ARGS 2

#define INCREMENT_AND_WRAP(val, incr, wrapat) \
    assert(incr < wrapat); \
    val += incr; \
    if(val >= wrapat) { \
        val -= wrapat; \
    } \
    assert(val < wrapat);

#include "libieee1394/cycletimer2.h"

#define FRAMES_PER_SECOND 48000
#define FRAMES_PER_PACKET 8

// Program documentation.
static char doc[] = "FFADO -- ISO receive test (new stack)\n\n";

// A description of the arguments we accept.
static char args_doc[] = "";

struct arguments
{
    long int verbose;
    long int port;
    long int channel;
    long int channel2;
    long int packetsize;
    long int period_size;
    long int nb_periods;
    long int frame_slack;
    long int iso_slack;
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
    {"channel1",  'c', "channel",  0,  "First iso channel to use" },
    {"channel2",  'd', "channel",  0,  "Second iso channel to use" },
    {"packetsize",  's', "packetsize",  0,  "packet size to use" },
    {"period_size",  'b', "period_size",  0,  "frames per period" },
    {"nb_periods",  'n', "nb_periods",  0,  "number of periods to buffer" },
    {"frame_slack",  'x', "frame_slack",  0,  "number of frames to allocate as extra slack" },
    {"iso_slack",  'y', "iso_slack",  0,  "number of extra packet buffers to allocate" },
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
    PARSE_ARG_LONG('c', arguments->channel, "channel1");
    PARSE_ARG_LONG('d', arguments->channel2, "channel2");
    PARSE_ARG_LONG('s', arguments->packetsize, "packetsize");
    PARSE_ARG_LONG('b', arguments->period_size, "period_size");
    PARSE_ARG_LONG('n', arguments->nb_periods, "nb_periods");
    PARSE_ARG_LONG('x', arguments->frame_slack, "frame_slack");
    PARSE_ARG_LONG('y', arguments->iso_slack, "iso_slack");
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


int main(int argc, char **argv)
{
    size_t c;
    int status;

    // Default values.
    arguments.verbose =  DEBUG_LEVEL_INFO;
    arguments.port = 0;
    arguments.channel = 0;
    arguments.channel2 = arguments.channel + 1;
    arguments.packetsize = 1024;
    arguments.period_size = 512;
    arguments.nb_periods = 2;
    // number of extra frames to buffer on top of the
    // nb_periods x period_size. adds to the latency
    arguments.frame_slack = 0;
    // number of extra ISO packet descriptors to queue,
    // doesn't add to the latency.
    arguments.iso_slack = 0;

    arguments.startoncycle = -1;
    arguments.countdown = 4;
    arguments.printinterval = 100;
    arguments.rtprio = 0;

    // Parse our arguments; every option seen by `parse_opt' will
    // be reflected in `arguments'.
    if ( argp_parse ( &argp, argc, argv, 0, 0, &arguments ) ) {
        debugError("Could not parse command line\n" );
        return -1;
    }

    // utility file descriptor
    char *device = NULL;
    if(asprintf(&device, "/dev/fw%zd", arguments.port) < 0) {
        debugError("Failed create device string\n");
        exit(-1);
    }

    // open the specified port
    debugOutput(DEBUG_LEVEL_INFO, "Open device %s...\n", device);

    int util_fd = open( device, O_RDWR );
    if(util_fd < 0) {
        debugError("Failed to open %s\n", device);
        free(device);
        exit(-1);
    }
    free(device);

    setDebugLevel(arguments.verbose);

    // AM824 settings
    struct am824_settings stream_settings;
    stream_settings.syt_interval = 8;

    // connection settings
    struct stream_settings settings;
    settings.type = STREAM_TYPE_RECEIVE;
    settings.channel = 0;
    settings.port = arguments.port;
    settings.max_packet_size = arguments.packetsize;
    settings.client_data = &stream_settings;
    settings.process_header  = NULL;
    settings.process_payload = receive_am824_packet_data;
    settings.packet_size_for_sync = FRAMES_PER_PACKET;

    // setup the streamer
    streamer_t strm = streamer_new();
    if(strm == NULL) {
        debugError("Could not allocate streamer\n");
        exit(-1);
    }
    __u32 conn_ids[16];

    if(streamer_init(strm, arguments.port,
                     arguments.period_size,
                     arguments.nb_periods,
                     arguments.frame_slack,
                     arguments.iso_slack,
                     FRAMES_PER_SECOND) < 0) {
        debugError("Failed to init streamer\n");
        close(util_fd);
        exit(-1);
    }

#if 0
    settings.channel = arguments.channel;
    settings.type = STREAM_TYPE_TRANSMIT;
    settings.process_header = NULL;
    settings.process_payload = transmit_am824_packet_data;
#else
    settings.channel = 0;
    settings.type = STREAM_TYPE_RECEIVE;
    settings.process_header = NULL;
    settings.process_payload = receive_am824_packet_data;
#endif

    status = streamer_add_stream(strm, &settings);
    if(status < 0) {
        debugError("Failed to add connection\n");
        streamer_free(strm);
        close(util_fd);
        exit(-1);
    }
    conn_ids[0] = status;

#if 1
    settings.channel     = arguments.channel2;
    settings.type        = STREAM_TYPE_TRANSMIT;
    settings.process_header = NULL;
    settings.process_payload = transmit_am824_packet_data;
#else
    settings.channel = 1;
    settings.type = STREAM_TYPE_RECEIVE;
    settings.process_header = NULL;
    settings.process_payload = receive_am824_packet_data;
#endif
    status = streamer_add_stream(strm, &settings);
    if(status < 0) {
        debugError("Failed to add connection\n");
        streamer_free(strm);
        close(util_fd);
        exit(-1);
    }
    conn_ids[1] = status;

#if 0
    settings.channel = 12;
    settings.type = STREAM_TYPE_TRANSMIT;
    settings.process_header = NULL;
    settings.process_payload = transmit_am824_packet_data;
#else
    settings.channel = arguments.channel;
    settings.type = STREAM_TYPE_RECEIVE;
    settings.process_header = NULL;
    settings.process_payload = receive_am824_packet_data;
#endif
    status = streamer_add_stream(strm, &settings);
    if(status < 0) {
        debugError("Failed to add connection\n");
        streamer_free(strm);
        close(util_fd);
        exit(-1);
    }
    conn_ids[2] = status;

    streamer_start_connection(strm, conn_ids[0], -1);
    streamer_set_sync_connection(strm, conn_ids[0]);

    int nruns = arguments.countdown;

    // start the streamer
    if(streamer_start(strm) < 0) {
        debugError("Could not start streamer\n");
        return -1;
    }

    do {
//         debugOutput(DEBUG_LEVEL_INFO,
        debugWarning(
                     ">>>>>>>>>> Run %d <<<<<<<<<<\n", 
                     (int)(arguments.countdown - nruns));
        if(arguments.countdown - nruns == 10) {
            streamer_start_connection(strm, conn_ids[1], -1);
        }
        if(arguments.countdown - nruns == 20) {
            streamer_start_connection(strm, conn_ids[2], -1);
        }
        if(arguments.countdown - nruns == 30) {
            settings.channel = 3;
            settings.type = STREAM_TYPE_RECEIVE;
            settings.process_header = NULL;
            settings.process_payload = receive_am824_packet_data;
            status = streamer_add_stream(strm, &settings);
            if(status < 0) {
                debugError("Failed to add connection\n");
                streamer_free(strm);
                close(util_fd);
                exit(-1);
            }
            conn_ids[3] = status;
        }
        if(arguments.countdown - nruns == 40) {
            streamer_start_connection(strm, conn_ids[3], -1);
        }
        if(arguments.countdown - nruns == 50) {
            streamer_set_sync_connection(strm, conn_ids[3]);
        }

        /* ============================== *
         *             WAIT               *
         * ============================== */
        if(streamer_wait_for_period(strm) < 0) {
            debugError("Failed to wait for period\n");
            goto out;
        }

        /* ============================== *
         *             READ               *
         * ============================== */
        streamer_read_frames(strm);

        /* ============================== *
         *             PROCESS            *
         * ============================== */

        /* ============================== *
         *             WRITE              *
         * ============================== */
        streamer_write_frames(strm);

        /* ============================== *
         *             REQUEUE            *
         * ============================== */

        if(streamer_queue_next_period(strm) < 0) {
            debugError("queue failed\n");
            goto out;
        }
    } while(--nruns);

out:

    if(streamer_stop(strm) < 0) {
        debugError("Could not stop streamer\n");
        return -1;
    }

    streamer_free(strm);

    close(util_fd);
    return 0;
}
