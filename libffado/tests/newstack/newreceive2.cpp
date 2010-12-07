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

#include "connections.h"
#include "../systemtests/realtimetools.h"

typedef __u32 quadlet_t;

#include "libutil/ByteSwap.h"
#include "libstreaming/util/cip.h"
#include "libieee1394/cycletimer.h"

#define U(x) ((__u64)(x))

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
    {"channel1",  'c', "channel",  0,  "First iso channel to use" },
    {"channel2",  'd', "channel",  0,  "Second iso channel to use" },
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

    errno = 0;
    switch (key) {
    PARSE_ARG_LONG('v', arguments->verbose, "verbose");
    PARSE_ARG_LONG('p', arguments->port, "port");
    PARSE_ARG_LONG('c', arguments->channel, "channel1");
    PARSE_ARG_LONG('d', arguments->channel2, "channel2");
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

__u64 process_packet(struct connection *c, __u16 tsp, __u32 *data, size_t len)
{
    struct iec61883_packet *packet =
            (struct iec61883_packet *) data;
    assert(packet);

    int cycle   = (tsp >>  0) & 0x1FFF;
    int sec     = (tsp >> 13) & 0x7;

    bool ok = (packet->fdf != 0xFF) &&
              (packet->fmt == 0x10) &&
              (len >= 2*sizeof(__u32));

    __u64 last_timestamp = (__u64)(-1);
    if(ok) {
        if(packet->syt != 0xFFFF) {
            last_timestamp = sytRecvToFullTicks2(
                    (__u32)CondSwapFromBus16(packet->syt),
                    (tsp << 12));

            debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
                        "[%02d] %03ds %04dc | %08X %08X | %12lld %03ds %04dc %04dt\n",
                        c->channel, sec, cycle,
                        CondSwapFromBus32(*data), CondSwapFromBus32(*(data+1)),
                        last_timestamp,
                        TICKS_TO_SECS(last_timestamp),
                        TICKS_TO_CYCLES(last_timestamp),
                        TICKS_TO_OFFSET(last_timestamp));
            ffado_ringbuffer_write(c->timestamps, (const char *)&last_timestamp, sizeof(last_timestamp));
        } else {
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
                        "[%02d] %03ds %04dc | %08X %08X | [empty]\n",
                        c->channel, sec, cycle,
                        CondSwapFromBus32(*data),
                        CondSwapFromBus32(*(data+1)));
        }
    } else {
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
                    "[%02d] %03ds %04dc | %08X %08X | [bogus data]\n",
                    c->channel, sec, cycle,
                    CondSwapFromBus32(*data), CondSwapFromBus32(*(data+1)));
    }

    return last_timestamp;
}

int main(int argc, char **argv)
{
    int u, c;

    // Default values.
    arguments.verbose =  DEBUG_LEVEL_INFO;
    arguments.port = 0;
    arguments.channel = 0;
    arguments.channel2 = arguments.channel + 1;
    arguments.packetsize = 1024;
    arguments.buffersize = 64;
    arguments.interval = -1;
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

    // fix up arguments
    if(arguments.interval < 0) {
        arguments.interval = arguments.buffersize / 2;
    } else if (arguments.interval*2 > arguments.buffersize) {
        arguments.interval = arguments.buffersize / 2;
    }

    setDebugLevel(arguments.verbose);
//     set_realtime_priority(10);

    debugOutput(DEBUG_LEVEL_INFO, 
            "ISO test:  %ld bytes/packet, %ld packets/interrupt...\n",
            arguments.packetsize, arguments.interval);

    // setup the connection using the arguments
#define MAX_NB_CONNECTIONS 8
    struct connection conn[MAX_NB_CONNECTIONS];

    int nb_connections = 0;
    if(conn_init(&conn[nb_connections], arguments.port,
                       arguments.buffersize,
                       arguments.interval,
                       arguments.packetsize,
                       arguments.channel,
                       arguments.startoncycle,
                       process_packet, NULL) != 0)
    {
        exit(-1);
    }
    nb_connections++;

    if(conn_init(&conn[nb_connections], arguments.port,
                       arguments.buffersize,
                       arguments.interval,
                       arguments.packetsize,
                       arguments.channel2,
                       arguments.startoncycle,
                       process_packet, NULL) != 0)
    {
        exit(-1);
    }
    nb_connections++;

    int status;

    debugOutput(DEBUG_LEVEL_INFO, "Start ISO receive...\n");
    // start reception
    for(c = 0; c < nb_connections; c++) {
        if(conn_start_iso(&conn[c]) != 0) {
            for(u = 0; u < nb_connections; u++) {
                conn_free(&conn[u]);
            }
            exit(-1);
        }
    }

    // present two views on the event
    union
    {
        char buf[4096];
        union fw_cdev_event e;
    } e;

    // read the packets that are sent using blocking I/O

    int nruns = arguments.countdown;
    __u64 front_tsps[nb_connections];
    int highest_at = 0;
    int ticks_per_frame = TICKS_PER_SECOND / 48000;
    int half_a_frame = ticks_per_frame / 2;

    do {
        debugOutput(DEBUG_LEVEL_VERBOSE, 
                    "Run %d\n", 
                    (int)(arguments.countdown - nruns));
        struct pollfd fds[nb_connections];
        int status;

        for(u = 0; u < nb_connections; u++) {
            fds[u].fd = conn[u].fd;
            fds[u].events = POLLIN;
        }

        status = poll ( fds, nb_connections, -1 );
        if(status < 0) {
            for(u = 0; u < nb_connections; u++) {
                conn_free(&conn[u]);
            }
            exit(-1);
        }

        for(c = 0; c < nb_connections; c++) {
            if(fds[c].revents & POLLIN ) {
                // read from the fd
                int nread = read ( conn[c].fd, &e, sizeof ( e ) );
                if ( nread < 1 ) {
                    debugError("Failed to read fd\n");
                    exit(-1);
                }

                // determine event type
                switch ( e.e.common.type )
                {
                    case FW_CDEV_EVENT_BUS_RESET:
                        debugError("BUS RESET\n");
        //                 goto out;
                        goto nextevent;
        
                    case FW_CDEV_EVENT_RESPONSE:
                        debugError("Event response\n");
                        goto out;
        
                    case FW_CDEV_EVENT_REQUEST:
                        debugError("Event request\n");
                        goto out;
        
                    case FW_CDEV_EVENT_ISO_INTERRUPT:
                        break;
                }

                if(1) {
                    // we have received an ISO interrupt
                    int npackets = e.e.iso_interrupt.header_length / conn[c].header_size;
                    debugOutput(DEBUG_LEVEL_VERBOSE,  
                            "ISO interrupt, header_len = %d, cycle = [%1ds, %4dc], " 
                            "received %d pkt\n", 
                            (int)e.e.iso_interrupt.header_length,
                            (int)(e.e.iso_interrupt.cycle >> 13) & 0x7,
                            (int)(e.e.iso_interrupt.cycle >> 0) & 0x1FFF,
                            npackets);
            
                    conn_consume(&conn[c], e.e.iso_interrupt.header, npackets);
                }
            }
        }

        if(1) {
            // the min nb of timestamps (= data packets) available
            int tsps_available = 9999; // FIXME
            for(c = 0; c < nb_connections; c++) {
                int ntimestamps = ffado_ringbuffer_read_space(conn[c].timestamps) / sizeof(__u64);
                if(ntimestamps < tsps_available) tsps_available = ntimestamps;
            }
            if(tsps_available > 0) {
    
                // figure out how big the delay between the streams is
                for(c = 0; c < nb_connections; c++) {
                    __u64 tspx;
                    // fetch the first timestamp
                    if(ffado_ringbuffer_peek(conn[c].timestamps, 
                                            (char *)&tspx,
                                            sizeof(__u64)) == sizeof(__u64)) {
                        front_tsps[c] = tspx;
                    } else {
                        debugWarning("No first timestamp for connection %d\n", c);
                    }
                }

                // determine the stream with the highest presentation time at the front
                // of the buffer. This is the stream we'll have to sync to as its
                // the one that has the least data ready
                for(c = 1; c < nb_connections; c++) {
                    int diff = diffTicks(front_tsps[c], front_tsps[highest_at]);
                    if(diff > half_a_frame) {
                        highest_at = c;
                    }
                }
                debugOutput(DEBUG_LEVEL_INFO, 
                            "Highest PT at %d: %12d [%3ds %4dc %4dt]\n", 
                            highest_at, 
                            front_tsps[highest_at],
                            TICKS_TO_SECS(front_tsps[highest_at]),
                            TICKS_TO_CYCLES(front_tsps[highest_at]),
                            TICKS_TO_OFFSET(front_tsps[highest_at]));
    
                // consume timestamps to align the streams
                for(c = 0; c < nb_connections; c++) {
                    int ntimestamps = ffado_ringbuffer_read_space(conn[c].timestamps) / sizeof(__u64);
                    do {
                        __u64 tspx;
                        // fetch the first timestamp
                        if(ffado_ringbuffer_peek(conn[c].timestamps, 
                                                (char *)&tspx,
                                                sizeof(__u64)) != sizeof(__u64)) {
                            debugWarning("No first timestamp for connection %d\n", c);
                            break;
                        }
                        // consume the timestamp if it has already passed
                        int diff = diffTicks(front_tsps[highest_at], tspx);
                        if(diff > half_a_frame) {
                            ffado_ringbuffer_read(conn[c].timestamps, 
                                                (char *)&tspx, sizeof(__u64));
                            debugOutput(DEBUG_LEVEL_INFO, 
                                        "Dropping from %d: %12d [%3ds %4dc %4dt] (%12d, %12d)\n",
                                        c, 
                                        tspx,
                                        TICKS_TO_SECS(tspx),
                                        TICKS_TO_CYCLES(tspx),
                                        TICKS_TO_OFFSET(tspx),
                                        diff, diff * 48000 / TICKS_PER_SECOND
                                       );
//                             conn_advance_sync(&conn[c], 1);

                        } else {
                            break;
                        }
                        ntimestamps--;
                    } while(ntimestamps);
                    
                }

                // recalculate the minimum number of available tsps
                for(c = 0; c < nb_connections; c++) {
                    int ntimestamps = ffado_ringbuffer_read_space(conn[c].timestamps) / sizeof(__u64);
                    if(ntimestamps < tsps_available) tsps_available = ntimestamps;
                }

                // consume the timestamps FIXME: make this one period of TSPS
                for(u = 0; u < tsps_available; u++) {
                    __u64 tsp_ref;
                    ffado_ringbuffer_peek(conn[highest_at].timestamps,
                                          (char *)&tsp_ref, sizeof(__u64));

//                     debugOutput(DEBUG_LEVEL_INFO, "Consume %3d: ", u);
                    for(c = 0; c < nb_connections; c++) {
                        __u64 tspx;
                        ffado_ringbuffer_read(conn[c].timestamps, 
                                            (char *)&tspx, sizeof(__u64));
//                         debugOutputShort(DEBUG_LEVEL_INFO,
//                                         "[%3ds %4dc %4dt, %5d, %5d] ", 
//                                         TICKS_TO_SECS(tspx),
//                                         TICKS_TO_CYCLES(tspx),
//                                         TICKS_TO_OFFSET(tspx),
//                                         diffTicks(tsp_ref, tspx),
//                                         diffTicks(tsp_ref, tspx) * 48000LL /
//                                                 ((int)TICKS_PER_SECOND));
                    }
//                     debugOutputShort(DEBUG_LEVEL_INFO, "\n");
                }

                debugOutput(DEBUG_LEVEL_INFO, "Front TSP: %12d ", front_tsps[0]);
                for(c = 1; c < nb_connections; c++) {
                    debugOutputShort(DEBUG_LEVEL_INFO, 
                                    "%12d [%12d]",
                                    front_tsps[c],
                                    diffTicks(front_tsps[c], front_tsps[0]));
                }
                debugOutputShort(DEBUG_LEVEL_INFO, "\n");
            }
        }
nextevent:
        int i=0; // dummy

    } while(--nruns);

out:
    for(u = 0; u < nb_connections; u++) {
        conn_free(&conn[u]);
    }
    return 0;
}
