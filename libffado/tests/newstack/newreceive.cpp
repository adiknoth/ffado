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

#define U(x) ((__u64)(x))

#define R_MAGIC(x) FW_CDEV_ISO_HEADER_LENGTH(8) | FW_CDEV_ISO_PAYLOAD_LENGTH((x))

#include "debugmodule/debugmodule.h"
DECLARE_GLOBAL_DEBUG_MODULE;

#define MAX_EXTRA_ARGS 2

// Program documentation.
static char doc[] = "FFADO -- ISO receive test (new stack)\n\n";

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

int process_packet(__u16 tsp, __u32 *data, size_t len)
{
    int cycle   = (tsp >>  0) & 0x1FFF;
    int sec     = (tsp >> 13) & 0x7;
    __u32 d0, d1, d2;

    d0 = ntohl ( *data );
    d1 = ntohl ( *(data+1) );
    d2 = ntohl ( *(data+2) );

    debugOutputShort(DEBUG_LEVEL_INFO, "\r");
    debugOutput(DEBUG_LEVEL_INFO, 
            "%03ds %04dc | %08X %4d %8d",
             sec, cycle, (int)d0, (int)d1, (int)d2);

    if((cycle != (int)d1) || (d0 != 0x12345678)) {
        debugOutputShort(DEBUG_LEVEL_INFO, "\n");
        debugOutput(DEBUG_LEVEL_INFO,
                    "Cycle %d has bogus data\n",
                    cycle);
    }
    return 0;
}

#include <string.h>

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
    struct connection r;

    size_t packets_size = arguments.buffersize*sizeof(struct fw_cdev_iso_packet);
    r.packets = (struct fw_cdev_iso_packet *) malloc(packets_size);

    if(r.packets == NULL) {
        debugError("Failed to allocate packet struct memory\n");
        exit(-1);
    }
    memset(r.packets, 0, arguments.buffersize*sizeof(struct fw_cdev_iso_packet));

    
    r.channel = arguments.channel;
    r.start_cycle = arguments.startoncycle;
    
    char *device = NULL;
    if(asprintf(&device, "/dev/fw%ld", arguments.port) < 0) {
        debugError("Failed create device string\n");
        exit(-1);
    }

    // open the specified port
    debugOutput(DEBUG_LEVEL_INFO, 
            "Open device %s...\n",
            device);

    r.fd = open( device, O_RDWR );
    if(r.fd < 0) {
        debugError("Failed to open %s\n", device);
        exit(-1);
    }

    debugOutput(DEBUG_LEVEL_INFO, "MMAP memory...\n");
    // map memory for ???
    r.mem = (__u32 *)mmap(NULL, arguments.buffersize*arguments.packetsize, 
                          PROT_READ|PROT_WRITE, 
                          MAP_SHARED, r.fd, 0);
    if(r.mem == MAP_FAILED) {
        debugError("Failed to map memory\n");
        exit(-1);
    }

    int status;

    debugOutput(DEBUG_LEVEL_INFO, "Create context...\n");
    // create the receive context
    struct fw_cdev_create_iso_context create;
    create.type = FW_CDEV_ISO_CONTEXT_RECEIVE;
    create.header_size = 8;
    create.channel = r.channel;
    create.speed = SCODE_400;
    create.closure = r.closure;
    create.handle = 0;
    status = ioctl ( r.fd, FW_CDEV_IOC_CREATE_ISO_CONTEXT, &create );
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
        r.packets[u].control = R_MAGIC(arguments.packetsize);
        // schedule the interrupts
        if((u+1) % arguments.interval == 0) {
            debugOutput(DEBUG_LEVEL_VERBOSE,
                    " request interrupt at %d\n", u);
            r.packets[u].control |= FW_CDEV_ISO_INTERRUPT;
        }
    }

    r.next_to_queue = 0; // the next packet struct is the first one

    debugOutput(DEBUG_LEVEL_INFO, "Queue packet structs...\n");
    // queue the packet reception structures
    struct fw_cdev_queue_iso queue;
    queue.packets = U ( r.packets );
    queue.data = U ( r.mem );
    queue.size = packets_size;
    queue.handle = create.handle;
    status = ioctl ( r.fd, FW_CDEV_IOC_QUEUE_ISO, &queue );
    if(status < 0) {
        debugError("Failed to queue packet descriptors\n");
        exit(-1);
    }

    debugOutput(DEBUG_LEVEL_INFO, "Start ISO receive...\n");
    // start reception
    struct fw_cdev_start_iso start;
    start.cycle = r.start_cycle;
    start.sync = 0;
    start.tags = FW_CDEV_ISO_CONTEXT_MATCH_ALL_TAGS;
    start.handle = create.handle;
    status = ioctl ( r.fd, FW_CDEV_IOC_START_ISO, &start );
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
        int nread = read ( r.fd, &e, sizeof ( e ) );
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
            int npackets_received = e.e.iso_interrupt.header_length / create.header_size;
            debugOutput(DEBUG_LEVEL_VERBOSE,  
                    "ISO interrupt, header_len = %d, cycle = %d, " 
                    "received %d pkt\n", 
                    (int)e.e.iso_interrupt.header_length,
                    (int)e.e.iso_interrupt.cycle,
                    npackets_received);
    
            int u2 = r.next_to_queue;
            for (u = 0; u < npackets_received; u++ )
            {
                __u32 h0, tsp;
                h0 = ntohl ( e.e.iso_interrupt.header[u*2] );
                tsp = ntohl ( e.e.iso_interrupt.header[u*2+1] );
    
                int len     = (h0 >> 16) & 0xFFFF;
                int tag     = (h0 >> 14) & 0x3;
                int channel = (h0 >>  8) & 0x3F;
                int tcode   = (h0 >>  4) & 0xF;
                int sy      = (h0 >>  0) & 0xF;
    
                // check the data
                __u32 *data = r.mem + u2 * arguments.packetsize/4;

                process_packet(tsp, data, len);

                // immediately prepare for requeue
                r.packets[u2].control = R_MAGIC(arguments.packetsize);
                if((u2+1) % arguments.interval == 0) {
                    debugOutput(DEBUG_LEVEL_VERBOSE,
                            " request interrupt at %d\n", u2);
                    r.packets[u2].control |= FW_CDEV_ISO_INTERRUPT;
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
                int toqueue = arguments.buffersize - r.next_to_queue;
                // is it more than we have?
                if(toqueue > npackets_received) {
                    toqueue = npackets_received;
                }
                struct fw_cdev_iso_packet *p = r.packets;
                p += r.next_to_queue;
                queue.packets = U ( p );
                queue.data = U ( r.mem + r.next_to_queue * arguments.packetsize/4 );
                queue.size = toqueue * sizeof(struct fw_cdev_iso_packet);
                queue.handle = create.handle;
                status = ioctl ( r.fd, FW_CDEV_IOC_QUEUE_ISO, &queue );
                if(status < 0) {
                    debugError("Failed to queue packet descriptors\n");
                    exit(-1);
                }
    
                debugOutput(DEBUG_LEVEL_VERBOSE,
                        "requeue %d from %d => %d\n",
                        toqueue, r.next_to_queue, r.next_to_queue + toqueue-1);
    
                npackets_received -= toqueue;
                r.next_to_queue += toqueue;
                if(r.next_to_queue == arguments.buffersize) {
                    r.next_to_queue = 0;
                } else if (r.next_to_queue > arguments.buffersize) {
                    // this cannot happen
                    debugError("r.next_to_queue > arguments.buffersize\n");
                    exit(-1);
                }
    
            } while(npackets_received);
        }
nextevent:
        int i=0; // dummy

    } while(--nruns);

out:
    return 0;
}
