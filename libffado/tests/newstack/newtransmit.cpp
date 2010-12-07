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

#include <stdlib.h>

#include <string.h>

#define U(x) ((__u64)(x))

#define X_MAGIC(x) FW_CDEV_ISO_HEADER_LENGTH(0) | FW_CDEV_ISO_TAG(1) | FW_CDEV_ISO_PAYLOAD_LENGTH((x))

#include "debugmodule/debugmodule.h"
DECLARE_GLOBAL_DEBUG_MODULE;

#define MAX_EXTRA_ARGS 2

// Program documentation.
static char doc[] = "FFADO -- ISO transmit test (new stack)\n\n";

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

    errno = 0;
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

// #define METHOD_1

// connection info
struct connection
{
    int fd;
    __u32 *mem;
    int start_cycle;
    __u64 closure;
    __u32 channel;
    int next_to_queue;
    struct fw_cdev_iso_packet *packets;
};

static int packetcount = 0;
int generate_packet(__u16 tsp, __u32 *data, size_t *len)
{
    int cycle   = (tsp >>  0) & 0x1FFF;
    int sec     = (tsp >> 13) & 0x7;

    *len = arguments.packetsize;

    memset(data, 0, *len);
    *data     = ntohl( 0x12345678 );
    *(data+1) = ntohl( tsp );
    *(data+2) = ntohl( packetcount );

    debugOutput(DEBUG_LEVEL_INFO, 
            "%08d | %03ds %04dc | %08X %4d %8d",
            packetcount, sec, cycle, (int)ntohl(*data), 
            (int)ntohl(*(data+1)), (int)ntohl(*(data+2)));
    debugOutputShort(DEBUG_LEVEL_INFO, "\n");

    packetcount++;
    return 0;
}

int main(int argc, char **argv)
{

    // Default values.
    arguments.verbose =  DEBUG_LEVEL_INFO;
    arguments.port = 0;
    arguments.channel = 0;
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

    debugOutput(DEBUG_LEVEL_INFO, 
            "ISO test:  %ld bytes/packet, %ld packets/interrupt...\n",
            arguments.packetsize, arguments.interval);

    // setup the connection using the arguments
    struct connection x;

    size_t packets_size = arguments.buffersize*sizeof(struct fw_cdev_iso_packet);
    x.packets = (struct fw_cdev_iso_packet *) malloc(packets_size);

    if(x.packets == NULL) {
        debugError("Failed to allocate packet struct memory\n");
        exit(-1);
    }
    memset(x.packets, 0, arguments.buffersize*sizeof(struct fw_cdev_iso_packet));

    x.channel = arguments.channel;
    x.start_cycle = arguments.startoncycle;
    
    char *device = NULL;
    if(asprintf(&device, "/dev/fw%ld", arguments.port) < 0) {
        debugError("Failed create device string\n");
        exit(-1);
    }

    // open the specified port
    debugOutput(DEBUG_LEVEL_INFO, 
            "Open device %s...\n",
            device);

    x.fd = open( device, O_RDWR );
    if(x.fd < 0) {
        debugError("Failed to open %s\n", device);
        exit(-1);
    }

    debugOutput(DEBUG_LEVEL_INFO, "MMAP memory...\n");
    // map memory for ???
    x.mem = (__u32 *)mmap(NULL,
                          arguments.buffersize*arguments.packetsize,
                          PROT_READ|PROT_WRITE, 
                          MAP_SHARED, x.fd, 0);
    if(x.mem == MAP_FAILED) {
        debugError("Failed to map memory\n");
        exit(-1);
    }

    int status;

    debugOutput(DEBUG_LEVEL_INFO, "Create context...\n");
    // create the receive context
    struct fw_cdev_create_iso_context create;
    create.type = FW_CDEV_ISO_CONTEXT_TRANSMIT;
    create.header_size = 4;
    create.channel = x.channel;
    create.speed = SCODE_400;
    create.closure = x.closure;
    create.handle = 0;
    status = ioctl ( x.fd, FW_CDEV_IOC_CREATE_ISO_CONTEXT, &create );
    if(status < 0) {
        debugError("Failed to create context\n");
        exit(-1);
    }
    if(create.handle != 0) {
        debugError("This is not the first ISO handle?\n");
        exit(-1);
    }

    debugOutput(DEBUG_LEVEL_INFO, "Prepare packet structs...\n");
    // initialize the packet info's.
    int u;
    for (u = 0; u < arguments.buffersize; u++ ) {
        // generate the packet data
        __u32 *data = x.mem + u * arguments.packetsize/4;
        size_t len = 0;
        int cycle;
        if (x.start_cycle >= 0) {
            cycle = x.start_cycle + u;
        } else {
            cycle = 0;
        }
        generate_packet(cycle, data, &len);
        debugOutputShort(DEBUG_LEVEL_INFO, "\n");

        if(len > arguments.packetsize) {
            debugError("packet data too large");
            exit(-1);
        }
        
        x.packets[u].control = X_MAGIC(len);
        // schedule the interrupts
        if((u+1) % arguments.interval == 0) {
            debugOutput(DEBUG_LEVEL_VERBOSE,
                    " request interrupt at %d\n", u);
            x.packets[u].control |= FW_CDEV_ISO_INTERRUPT;
        }
    }

    x.next_to_queue = 0; // the next packet struct is the first one

    debugOutput(DEBUG_LEVEL_INFO, "Queue packet structs...\n");
    // queue the packet reception structures
    struct fw_cdev_queue_iso queue;
    queue.packets = U ( x.packets );
    queue.data = U ( x.mem );
    queue.size = packets_size;
    queue.handle = create.handle;
    status = ioctl ( x.fd, FW_CDEV_IOC_QUEUE_ISO, &queue );
    if(status < 0) {
        debugError("Failed to queue packet descriptors\n");
        exit(-1);
    }

    debugOutput(DEBUG_LEVEL_INFO, "Start ISO transmit...\n");
    // start reception
    struct fw_cdev_start_iso start;
    start.cycle = x.start_cycle;
    start.sync = 0;
    start.tags = FW_CDEV_ISO_CONTEXT_MATCH_ALL_TAGS;
    start.handle = create.handle;
    status = ioctl ( x.fd, FW_CDEV_IOC_START_ISO, &start );
    if(status < 0) {
        debugError("Failed to start ISO context\n");
        exit(-1);
    }

    // present two views on the event
    union
    {
        char buf[4096];
        union fw_cdev_event e;
    } e;

    // read the packets that are sent using blocking I/O

    int nruns = arguments.countdown;
    do {
        debugOutput(DEBUG_LEVEL_VERBOSE, 
                    "Run %d\n", 
                    (int)(arguments.countdown - nruns));
        // read from the fd
        int nread = read ( x.fd, &e, sizeof ( e ) );
        if ( nread < 1 ) {
            debugError("Failed to read fd\n");
            exit(-1);
        }
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
            int npackets = e.e.iso_interrupt.header_length / 4;
            debugOutput(DEBUG_LEVEL_VERBOSE,  
                    "ISO interrupt, header_len = %d, cycle = %d, " 
                    "npackets %d pkt\n", 
                    (int)e.e.iso_interrupt.header_length,
                    (int)e.e.iso_interrupt.cycle,
                    npackets);
    
            int u2 = x.next_to_queue;
            __u32 tsp = ntohl(e.e.iso_interrupt.header[npackets-1]);
            __u32 last_cycle_sent = tsp & 0x1fff;
            int secs = (tsp >> 13) & 0x7;
            // the next cycle to be sent should account for already queued
            // packets
            // therefore add the arguments.buffersize
            // and subtract npackets as we don't start from 
            // e.e.iso_interrupt.header[0]
            // but
            // e.e.iso_interrupt.header[npackets-1]
            __u32 next_cycle = last_cycle_sent + arguments.buffersize - npackets;
            while(next_cycle >= 8000) {
                secs += 1;
                secs &= 0x7;
                next_cycle -= 8000;
            }

            for (u = 0; u < npackets; u++ )
            {
                // packets are out, prepare next packet
                next_cycle++;
                if(next_cycle == 8000) {
                    secs += 1;
                    secs &= 0x7;
                    next_cycle = 0;
                }

                // generate the packet data
                __u32 *data = x.mem + u2 * arguments.packetsize/4;

                if(1) { // this is a check
                    __u32 predicted_tsp = ntohl(*(data + 1));
                    __u32 effective_tsp = ntohl(e.e.iso_interrupt.header[u]);
                    effective_tsp &= 0xFFFF;
    
                    int pred_cycle   = (predicted_tsp >>  0) & 0x1FFF;
                    int pred_sec     = (predicted_tsp >> 13) & 0x7;
                    int eff_cycle    = (effective_tsp >>  0) & 0x1FFF;
                    int eff_sec      = (effective_tsp >> 13) & 0x7;
    
                    if (predicted_tsp != effective_tsp) {
                        debugWarning("cycle mismatch: [%08X] %ds %dc != [%08X] %ds %dc\n",
                                    predicted_tsp, pred_sec, pred_cycle,
                                    effective_tsp, eff_sec, eff_cycle); 
                    }
                }

                size_t len = 0;
                tsp = (secs << 13) | next_cycle;
                generate_packet(tsp, data, &len);

                if(len > arguments.packetsize) {
                    debugError("packet data too large");
                    exit(-1);
                }
 
                // requeue the descriptors
                x.packets[u2].control = X_MAGIC(len);
                if((u2+1) % arguments.interval == 0) {
                    debugOutput(DEBUG_LEVEL_VERBOSE,
                            " request interrupt at %d\n", u2);
                    x.packets[u2].control |= FW_CDEV_ISO_INTERRUPT;
                }
                u2++;
                if(u2 > arguments.buffersize) {
                    debugError(
                            " strange, u2 = %d > %d\n",
                            u2, (int)arguments.buffersize);
                }
            }

            // do the actual re-queue
            do {
                // this is the amount of packets one can enqueue in one go
                int toqueue = arguments.buffersize - x.next_to_queue;
                // is it more than we have?
                if(toqueue > npackets) {
                    toqueue = npackets;
                }
                struct fw_cdev_iso_packet *p = x.packets;
                p += x.next_to_queue;
                queue.packets = U ( p );
                queue.data = U ( x.mem + x.next_to_queue * arguments.packetsize/4 );
                queue.size = toqueue * sizeof(struct fw_cdev_iso_packet);
                queue.handle = create.handle;
                status = ioctl ( x.fd, FW_CDEV_IOC_QUEUE_ISO, &queue );
                if(status < 0) {
                    debugError("Failed to queue packet descriptors\n");
                    exit(-1);
                }
    
                debugOutput(DEBUG_LEVEL_VERBOSE,
                        "requeue %d from %d => %d\n",
                        toqueue, x.next_to_queue, x.next_to_queue + toqueue-1);
    
                npackets -= toqueue;
                x.next_to_queue += toqueue;
                if(x.next_to_queue == arguments.buffersize) {
                    x.next_to_queue = 0;
                } else if (x.next_to_queue > arguments.buffersize) {
                    // this cannot happen
                    debugError("x.next_to_queue > arguments.buffersize\n");
                    exit(-1);
                }
    
            } while(npackets);
        }
nextevent:
        int i=0; // dummy

    } while(--nruns);

out:
    return 0;
}
