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

typedef __u32 quadlet_t;

#include "libutil/ByteSwap.h"
#include "libstreaming/util/cip.h"
#include "libieee1394/cycletimer2.h"

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
    long int period_size;
    long int nb_periods;
    long int slack;
//     long int interval;
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
    {"slack",  'x', "slack",  0,  "number of frames to allocate as extra slack" },
//     {"interval",  'i', "interval",  0,  "interrupt interval in packets" },
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
    PARSE_ARG_LONG('x', arguments->slack, "slack");
//     PARSE_ARG_LONG('i', arguments->interval, "interval");
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

#define DLL_PI        (3.141592653589793238)
#define DLL_SQRT2     (1.414213562373095049)
#define DLL_2PI       (2.0 * DLL_PI)

// these are the defaults
#define DLL_OMEGA     (DLL_2PI * 0.01)
#define DLL_COEFF_B   (DLL_SQRT2 * DLL_OMEGA)
#define DLL_COEFF_C   (DLL_OMEGA * DLL_OMEGA)

__u64 process_packet(struct connection *c, __u16 tsp, __u32 *data, size_t len)
{
    assert(c);
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
            assert(packet->dbs);
            last_timestamp = sytRecvToFullTicks2(
                    (__u32)CondSwapFromBus16(packet->syt),
                    (tsp << 12));
            
            if(TICKS_TO_SYT(last_timestamp) != CondSwapFromBus16(packet->syt)) {
                debugError("Bogus TSP %x | %x\n", 
                           (unsigned int)TICKS_TO_SYT(last_timestamp), 
                           (unsigned int)CondSwapFromBus16(packet->syt));
            }
            
            unsigned int nframes = ((len / sizeof (__u32)) - 2)/packet->dbs;
            float current_rate = c->sync.tpf;
            if(c->sync.last_tsp != (__u32)-1) {
                int delta = diffTicks(last_timestamp, c->sync.last_tsp);
                current_rate = ((float)delta) / ((float)nframes);
                float err = current_rate - c->sync.tpf;
                c->sync.tpf = 0.01 * err + c->sync.tpf;
            }
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
                        "[%02d] %03ds %04dc | %08X %08X | %12d %03ds %04dc %04dt | %f %f\n",
                        c->channel, (unsigned int)sec, (unsigned int)cycle,
                        (unsigned int)CondSwapFromBus32(*data),
                         (unsigned int)CondSwapFromBus32(*(data+1)),
                        (unsigned int)last_timestamp,
                        (unsigned int)TICKS_TO_SECS(last_timestamp),
                        (unsigned int)TICKS_TO_CYCLES(last_timestamp),
                        (unsigned int)TICKS_TO_OFFSET(last_timestamp),
                       current_rate, c->sync.tpf);
            c->sync.nframes += nframes;
            c->sync.last_tsp = last_timestamp;
            c->sync.last_recv = tsp;
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

    return 0;
}

// int getTimestampForFront(struct sync_info *sync)
// {
//     assert(sync);
// //     float tpf = TICKS_PER_SECOND / (float)FRAMES_PER_SECOND;
//     float tpf = sync->tpf;
//     
//     if (x >= TICKS_PER_SECOND * MAX_SECONDS) {
//         x -= TICKS_PER_SECOND * MAX_SECONDS;
//     }
// 
//     // fixed-point arithmetic
//     // check values to avoid overflow
//     assert(FRAMES_PER_PACKET <= 64); // 2 * max per spec
//     assert(tpf <= 4096); // is nominal TPF for approx 6000Hz
//     // FPP => 7 bits
//     // TPF => 13 bits
//     // hence still 12 bits left in a 32bit number
//     // therefore we can shift the TPF with 4 bits
//     // before multiplying without any risk of overflow
//     __u32 ticks_per_packet = FRAMES_PER_PACKET;
//     ticks_per_packet *= (tpf * (2^4)); // point at 4
// 
//     // before shift:
//     //  max tsp is 0x0BB80000 - 1
//     //  max ticks_per_packet is 0x0400000
//     //  therefore the sum is max 0x0BF80000
//     // hence the point can stay at 4
//     __u32 tsp = sync->last_tsp << 4; // point at 4
// 
//     __u32 tsp_next_frame = tsp + ticks_per_packet;
//     if (tsp_next_frame >= TICKS_PER_SECOND * MAX_SECONDS << 4) {
//         tsp_next_frame -= TICKS_PER_SECOND * MAX_SECONDS << 4;
//     }
// 
//     // tsp is the timestamp for the last packet in the block, hence:
//     assert(sync->nframes <= 1<<14); // 
//     __u32 buffer_in_ticks = (tpf * (2^4)) * sync->nframes;
// 
//     int32_t tsp_first_frame = substractTicks(tsp_next_frame, buffer_in_ticks);
//     return tsp_first_frame;
// }

int getTimestampForFront(struct sync_info *sync)
{
    assert(sync);
//     float tpf = TICKS_PER_SECOND / (float)FRAMES_PER_SECOND;
    float tpf = sync->tpf;

    float ticks_per_packet = (float)FRAMES_PER_PACKET * tpf;

    float tsp = sync->last_tsp;

    float tsp_next_frame = tsp + ticks_per_packet;
    if (tsp_next_frame >= TICKS_PER_SECOND * MAX_SECONDS) {
        tsp_next_frame -= TICKS_PER_SECOND * MAX_SECONDS;
    }

    // tsp is the timestamp for the last packet in the block, hence:
    float buffer_in_ticks = tpf * sync->nframes;

    float tsp_first_frame = tsp_next_frame - buffer_in_ticks;
    if(tsp_first_frame < 0.0) {
        tsp_first_frame += TICKS_PER_SECOND * MAX_SECONDS;
    }
    return tsp_first_frame;
}

int getTimestampForBack(struct sync_info *sync)
{
    assert(sync);
//     float tpf = TICKS_PER_SECOND / (float)FRAMES_PER_SECOND;
    float tpf = sync->tpf;

    float ticks_per_packet = (float)FRAMES_PER_PACKET * tpf;

    float tsp = sync->last_tsp;

    float tsp_next_frame = tsp + ticks_per_packet;
    if (tsp_next_frame >= TICKS_PER_SECOND * MAX_SECONDS) {
        tsp_next_frame -= TICKS_PER_SECOND * MAX_SECONDS;
    }

    return tsp_next_frame;
}

int consumeFrames(struct sync_info *sync, size_t nframes)
{
    assert(sync);
    sync->nframes -= nframes;
    return nframes;
}

int poll_all_connections_once(struct connection *conn, struct pollfd *fds, size_t nb_connections)
{
    assert(conn);
    assert(fds);
    assert(nb_connections > 0);

    struct fw_cdev_get_cycle_timer ctr;

    // present two views on the event
    union
    {
        char buf[4096];
        union fw_cdev_event e;
    } e;

    size_t c;
    int fds_left_to_poll = 0;
    do {
        int status;

        status = poll ( fds, nb_connections, -1 );
        if(status < 0) {
            debugError("poll failed\n");
            return -1;
        }

        // read cycle timer
        status = ioctl ( conn[0].fd, FW_CDEV_IOC_GET_CYCLE_TIMER, &ctr );
        if(status < 0) {
            debugError("Failed to get ctr\n");
            return -1;
        }

        debugOutput(DEBUG_LEVEL_INFO, 
                    " poll return at [%3ds %4dc %4dt]\n",
                (int)CYCLE_TIMER_GET_SECS(ctr.cycle_timer),
                (int)CYCLE_TIMER_GET_CYCLES(ctr.cycle_timer),
                (int)CYCLE_TIMER_GET_OFFSET(ctr.cycle_timer)
                );

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
                        goto nextevent;
        
                    case FW_CDEV_EVENT_RESPONSE:
                        debugError("Event response\n");
                        return -1;
        
                    case FW_CDEV_EVENT_REQUEST:
                        debugError("Event request\n");
                        return -1;
        
                    case FW_CDEV_EVENT_ISO_INTERRUPT:
                        break;
                }

                if(1) {
                    // disable polling on this FD
                    fds[c].events &= ~POLLIN;

                    // we have received an ISO interrupt
                    int npackets = e.e.iso_interrupt.header_length / conn[c].header_size;
                    debugOutput(DEBUG_LEVEL_INFO,
                            "[%2zd] ISO interrupt, header_len = %d, cycle = [%1ds, %4dc], " 
                            "received %d pkt\n", 
                            c, (int)e.e.iso_interrupt.header_length,
                            (int)(e.e.iso_interrupt.cycle >> 13) & 0x7,
                            (int)(e.e.iso_interrupt.cycle >> 0) & 0x1FFF,
                            npackets);

                    conn_consume(&conn[c], e.e.iso_interrupt.header, npackets);

                }
            }
        }

nextevent:
        fds_left_to_poll = 0;
        for(c = 0; c < nb_connections; c++) {
            if(fds[c].events & POLLIN) {
                fds_left_to_poll = 1;
                break;
            }
        }
        if(fds_left_to_poll) {
            debugWarning("need more\n");
        }
    } while(fds_left_to_poll);

    return 0;
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
    arguments.period_size = 512;
    arguments.nb_periods = 2;
    arguments.slack = 0;
//     arguments.interval = -1;
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

    // figure out buffer sizing
    int buffersize_frames = (arguments.period_size * arguments.nb_periods + arguments.slack);
    int buffersize_cycles = buffersize_frames * CYCLES_PER_SECOND / FRAMES_PER_SECOND;
//     int buffersize_cycles = 1000;
    int interval = arguments.period_size * CYCLES_PER_SECOND / FRAMES_PER_SECOND;

    setDebugLevel(arguments.verbose);

    debugOutput(DEBUG_LEVEL_INFO, 
            "ISO test: buffer: %d packets, %ld bytes/packet, %d packets/interrupt...\n",
            buffersize_cycles, arguments.packetsize, interval);

    // setup the connection using the arguments
#define MAX_NB_CONNECTIONS 8
    struct connection conn[MAX_NB_CONNECTIONS];

    int nb_connections = 0;
    if(conn_init(&conn[nb_connections], arguments.port,
                       buffersize_cycles,
                       interval,
                       arguments.packetsize,
                       arguments.channel,
                       arguments.startoncycle,
                       process_packet, NULL) != 0)
    {
        exit(-1);
    }
    nb_connections++;

    if(conn_init(&conn[nb_connections], arguments.port,
                       buffersize_cycles,
                       interval,
                       arguments.packetsize,
                       arguments.channel2,
                       arguments.startoncycle,
                       process_packet, NULL) != 0)
    {
        exit(-1);
    }
    nb_connections++;

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

    // read the packets that are sent using blocking I/O

    int nruns = arguments.countdown;

    size_t sync_conn = 0;
    do {
        struct fw_cdev_get_cycle_timer ctr;
        int status;
        status = ioctl ( conn[0].fd, FW_CDEV_IOC_GET_CYCLE_TIMER, &ctr );
        if(status < 0) {
            goto out;
        }

        struct pollfd fds[nb_connections];
        for(c = 0; c < nb_connections; c++) {
            fds[c].fd = conn[c].fd;
            fds[c].events = POLLIN;
        }

        // bitfield, if a connection is not synced its bit should be set.
        __u32 sync_status = 0;
        float ticks_per_frame = conn[sync_conn].sync.tpf;
        int half_a_frame = ticks_per_frame / 2;

        __u32 sync_tsp;
        if(conn[sync_conn].sync.last_tsp == (__u32)-1) {
            // the sync connection is not ready yet
            sync_tsp = (__u32)-1;
            sync_status = (__u32)-1;
        } else {
            // the sync connection is ready
            sync_tsp = getTimestampForFront(&conn[sync_conn].sync);
            for(c = 0; c < nb_connections; c++) {
                if(conn[c].sync.last_tsp == (__u32)-1) {
                    sync_status |= (1 << c);
                } else {
                    __u64 tsp = getTimestampForFront(&conn[c].sync);
                    int diff = diffTicks(tsp, sync_tsp);
                    // FIXME: account for larger sync tolerances (e.g. one packet)
                    if((diff > half_a_frame) || (diff < -half_a_frame)) {
                        sync_status |= (1 << c);
                    }
                }
            }

        }

        debugOutput(DEBUG_LEVEL_INFO, 
                    ">>>>> Run %d starts at [%3ds %4dc %4dt], Sync status: %04X,"
                    " Sync: [%3ds %4dc %4dt]\n", 
                    (int)(arguments.countdown - nruns),
                    (int)CYCLE_TIMER_GET_SECS(ctr.cycle_timer),
                    (int)CYCLE_TIMER_GET_CYCLES(ctr.cycle_timer),
                    (int)CYCLE_TIMER_GET_OFFSET(ctr.cycle_timer),
                    sync_status,
                    (int)TICKS_TO_SECS(sync_tsp),
                    (int)TICKS_TO_CYCLES(sync_tsp),
                    (int)TICKS_TO_OFFSET(sync_tsp)
                   );

        if(sync_tsp != (__u32)-1) {
            // figure out when the presentation time of the first frame of
            // each period boundary will be. These are the optimal points
            // for interrupts as at that point we should have received 
            // sufficient data for one period.

            // sync_tsp is the TSP of the first frame in the frame buffer.
            // this run should fill this frame buffer up to one period.
            // this means that the interrupt for this run should occur
            // at sync_tsp + one_period_in_ticks

            // note: the interrupts calculated are:
            //  0: optimal tsp for the start of the period we will capture
            //  1: optimal current interrupt tsp, i.e. the tsp this interrupt
            //     should have had.
            //  2: optimal IRQ cycle for the next period (already queued)
            // ...
            //  arguments.nb_periods + 1: to be queued in this run
            
            debugOutput(DEBUG_LEVEL_INFO, "Interrupts should occur at:\n");
            for(c = 1; c <= arguments.nb_periods + 2; c++) {
                __u64 irq_tsp = addTicks(sync_tsp, conn[0].sync.tpf * c * arguments.period_size );
                debugOutput(DEBUG_LEVEL_INFO,
                            "%2d: [%3ds %4dc %4dt]\n", c,
                            (int)TICKS_TO_SECS(irq_tsp),
                            (int)TICKS_TO_CYCLES(irq_tsp),
                            (int)TICKS_TO_OFFSET(irq_tsp));
            }
        }

        if(poll_all_connections_once(conn, fds, nb_connections) < 0) {
            goto out;
        }

        int int_cycle = -1;
        if (sync_tsp != (__u32)-1) {
            __u32 one_period_in_ticks = ticks_per_frame * arguments.period_size;

            // the interrupt should be (arguments.nb_periods + 1) later
            __u32 irq_tsp = addTicks(sync_tsp, (arguments.nb_periods + 1)*one_period_in_ticks);
            int_cycle = TICKS_TO_CYCLES(irq_tsp);

            debugOutput(DEBUG_LEVEL_INFO, 
                        "  >>> IRQ: [%3ds %4dc %4dt]\n",
                        (int)TICKS_TO_SECS(irq_tsp),
                        (int)TICKS_TO_CYCLES(irq_tsp),
                        (int)TICKS_TO_OFFSET(irq_tsp)
                    );
        }

        // queue a new set of packets
        for(c = 0; c < nb_connections; c++) {
            int tmp_int_cycle;

            int curr_write_cycle = conn[c].read_ptr_cycle;
            INCREMENT_AND_WRAP(curr_write_cycle, conn_get_read_space(&conn[c]), 8000);

            if(int_cycle < 0) {
                debugOutput(DEBUG_LEVEL_INFO, "no irq cycle defined\n");
                tmp_int_cycle = curr_write_cycle;
                INCREMENT_AND_WRAP(tmp_int_cycle, interval-1, 8000);
            } else {
                tmp_int_cycle = int_cycle;
            }

            // FIXME: the number of packets to queue should be pre-calculated
            //        based upon the still required frames, i.o.w by the amount
            //        cycles required to arrive at the next interrupt
            int npackets = diffCycles(tmp_int_cycle, curr_write_cycle);
            npackets += 1;
//             if(npackets < 0) npackets += conn[c].buffersize;
            debugOutput(DEBUG_LEVEL_INFO,
                        "will request interrupt at %d, queue %d packets to get there\n",
                        tmp_int_cycle, npackets
                       );

            // prepare the packets for re-queue
            if(conn_prepare_packets(&conn[c], npackets) < 0) {
                goto out;
            }

            conn_request_interrupt(&conn[c], tmp_int_cycle);

            // do the actual re-queue
            if(conn_queue_packets(&conn[c], npackets) < 0) {
                goto out;
            }
        }

        // now figure out the time shift
        {
            __u64 highest_tsp = getTimestampForFront(&conn[0].sync);
            int highest_at = 0;
            // int ticks_per_frame = TICKS_PER_SECOND / FRAMES_PER_SECOND;

            // determine the stream with the highest presentation time at the front
            // of the buffer. This is the stream we'll have to sync to as its
            // the one that has the least data available
            for(c = 1; c < nb_connections; c++) {
                __u64 this_tsp = getTimestampForFront(&conn[c].sync);
                int diff = diffTicks(this_tsp, highest_tsp);
                if(diff > half_a_frame) {
                    highest_at = c;
                    highest_tsp = this_tsp;
                }
            }
            debugOutput(DEBUG_LEVEL_INFO, 
                        "Highest PT at %u: %12u [%3ds %4dc %4dt]\n", 
                        highest_at, 
                        (unsigned int)highest_tsp,
                        (int)TICKS_TO_SECS(highest_tsp),
                        (int)TICKS_TO_CYCLES(highest_tsp),
                        (int)TICKS_TO_OFFSET(highest_tsp));

            // consume frames to align the streams
            for(c = 0; c < nb_connections; c++) {
                if(c == highest_at) continue;
                while(conn[c].sync.nframes > 0) {
                    __u64 this_tsp = getTimestampForFront(&conn[c].sync);
                    int diff = diffTicks(highest_tsp, this_tsp);
                    if(diff > half_a_frame) {
                        conn[c].sync.nframes--;
                        debugOutput(DEBUG_LEVEL_INFO, 
                                    "Dropping frame from %d: %12u [%3ds %4dc %4dt] (%12d, %12d)\n",
                                    c, 
                                    (unsigned int)this_tsp,
                                    (int)TICKS_TO_SECS(this_tsp),
                                    (int)TICKS_TO_CYCLES(this_tsp),
                                    (int)TICKS_TO_OFFSET(this_tsp),
                                    diff, (int)(diff * conn[c].sync.tpf / TICKS_PER_SECOND
                                        ));

                    } else {
                        debugOutput(DEBUG_LEVEL_INFO, 
                                    "Stream %d in sync: %12u [%3ds %4dc %4dt] (%12d, %12d)\n",
                                    c, 
                                    (unsigned int)this_tsp,
                                    (int)TICKS_TO_SECS(this_tsp),
                                    (int)TICKS_TO_CYCLES(this_tsp),
                                    (int)TICKS_TO_OFFSET(this_tsp),
                                    diff, (int)(diff * conn[c].sync.tpf / TICKS_PER_SECOND
                                            ));
                        break;
                    }
                }
            }

            // show stream info
            __u64 sync_tsp = getTimestampForFront(&conn[highest_at].sync);
            debugOutput(DEBUG_LEVEL_INFO, "Front TSP: %12u ", (unsigned int)sync_tsp);
            for(c = 0; c < nb_connections; c++) {
                if(conn[c].sync.nframes > 0) {
                    __u64 this_tsp = getTimestampForFront(&conn[c].sync);
                    debugOutputShort(DEBUG_LEVEL_INFO, 
                                    "{%12u [%6d] %6d} ",
                                    (unsigned int)this_tsp,
                                    (int)diffTicks(this_tsp, sync_tsp),
                                    conn[c].sync.nframes);
                } else {
                    debugOutputShort(DEBUG_LEVEL_INFO, 
                                    "{no frames} ");
                }
            }
            debugOutputShort(DEBUG_LEVEL_INFO, "\n");
        }

        // after all fd's have been iterated, we can consume the frames
        for(c = 0; c < nb_connections; c++) {
            consumeFrames(&conn[c].sync, arguments.period_size);
        }

    } while(--nruns);

out:
    // clean up
    for(u = 0; u < nb_connections; u++) {
        conn_stop_iso(&conn[u]);
    }
    for(u = 0; u < nb_connections; u++) {
        conn_free(&conn[u]);
    }
    return 0;
}
