/*
 * Copyright (C) 2009-2010 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
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

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <fcntl.h>
#include <linux/firewire-cdev.h>
#include <linux/firewire-constants.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include <stdlib.h>
#include <string.h>

#include "libutil/ByteSwap.h"

#define U(x) ((__u64)(x))

#include "libieee1394/cycletimer2.h"

#include "debugmodule/debugmodule.h"
DECLARE_GLOBAL_DEBUG_MODULE;

#include "connections.h"
#include "streamer.h"

#define INCREMENT_AND_WRAP(val, incr, wrapat) \
    assert(incr <= wrapat); \
    val += incr; \
    if(val >= wrapat) { \
        val -= wrapat; \
    } \
    assert(val < wrapat);

static inline void conn_show_ptrs(struct connection *c, const char *msg);
static inline int conn_create_context(struct connection *c, int channel);

static inline size_t conn_get_header_read_space(const struct connection *c);
static inline size_t conn_get_payload_space(const struct connection *c);
static inline size_t conn_get_queue_space(const struct connection *c);

static inline int conn_get_header_cycle(const struct connection *c);
static inline int conn_get_payload_cycle(const struct connection *c);

void conn_dump_packet(uint32_t *data, 
                      unsigned int len,
                      unsigned int header_len,
                      unsigned int row_len) {
    char tmpbuff[1024];
    int n = 0;
    int i = 0;
    int j;
    int printed;

    if(len < header_len) {
        debugError("Bogus packet, no header\n");
        return;
    }

    debugOutput(DEBUG_LEVEL_VERBOSE, "=== CONN packet ===\n");

    n = 0;
    printed = snprintf(tmpbuff + n, 1024-n, " H: [PKTLEN: %4d]", len);
    n += printed;
    for(j = 0; j < header_len; j++) {
        printed = snprintf(tmpbuff + n, 1024-n, "%08X ", *data);
        n += printed;
        data++;
    }
    printed = snprintf(tmpbuff + n, 1024-n, "\n");
    n += printed;
    len -= header_len;
    debugOutput(DEBUG_LEVEL_VERBOSE, "%s", tmpbuff);

    if(len % row_len != 0) {
        debugError("Bogus payload len: %d\n", len);
        return;
    }

    while(len) {
        n = 0;
        printed = snprintf(tmpbuff + n, 1024-n, "%2d: ", i);
        n += printed;
        for(j = 0; j < row_len; j++) {
            printed = snprintf(tmpbuff + n, 1024-n, "%08X ", *data);
            n += printed;
            data++;
        }
        printed = snprintf(tmpbuff + n, 1024-n, "\n");
        n += printed;
        len -= row_len;
        i++;
        debugOutput(DEBUG_LEVEL_VERBOSE, "%s", tmpbuff);
    }

}

int conn_init(struct connection *c,
              int type, size_t port, uint8_t tag,
              size_t packet_size,
              size_t buffer_size, size_t iso_slack)
{
    assert(c != NULL);
    debugOutput(DEBUG_LEVEL_INFO,
                "[%p] Initializing connection: %zd+%zd descriptors, %zd bytes/packet\n",
                c, buffer_size, iso_slack, packet_size);

    c->header_size = 0;
    c->tag = tag;

    switch(type) {
        case FW_CDEV_ISO_CONTEXT_RECEIVE:
            c->type = FW_CDEV_ISO_CONTEXT_RECEIVE;
            // this code currently assumes header length = 8
            // as we require the time of arrival timestamp
            c->header_size = 8;
            break;
        case FW_CDEV_ISO_CONTEXT_TRANSMIT:
            c->type = FW_CDEV_ISO_CONTEXT_TRANSMIT;
            // the response for a packet transmission is
            // exactly 1 quadlet/packet long, always
            c->header_size = 4;
            break;
        default:
            debugError("Bogus type");
            return -1;
    }

    c->flags = 0;

    // further initialization
    c->context_handle = -1;
    c->npacket_descriptors = buffer_size + iso_slack;
    c->buffer_size = buffer_size;

    assert(c->npacket_descriptors > 0);
    assert(packet_size > 0);
    c->packet_size = packet_size;

    debugOutput(DEBUG_LEVEL_INFO, 
                " Type.................: %s\n", 
                c->type == FW_CDEV_ISO_CONTEXT_RECEIVE ? "Receive" : "Transmit");
    debugOutput(DEBUG_LEVEL_INFO, 
                " Header / Packet size.: %zd / %zd\n", 
                c->header_size, c->packet_size);
    debugOutput(DEBUG_LEVEL_INFO, 
                " Buffer size..........: %zd\n", 
                c->npacket_descriptors);

    // open file
    char *device = NULL;
    if(asprintf(&device, "/dev/fw%zd", port) < 0) {
        debugError("[%p] Failed create device string\n", c);
        free(c->packets);
        return -1;
    }

    // open the specified port
    debugOutput(DEBUG_LEVEL_INFO, 
                "[%p] Open device %s...\n", c,
                device);

    c->fd = open( device, O_RDWR );
    if(c->fd < 0) {
        debugError("[%p] Failed to open %s\n", c, device);
        free(device);
        return -1;
    }
    free(device);

    //// packet descriptors ////
    // allocate packet descriptors
    c->packets = (struct fw_cdev_iso_packet *) malloc(c->npacket_descriptors * sizeof(struct fw_cdev_iso_packet));

    if(c->packets == NULL) {
        debugError("[%p] Failed to allocate packet struct memory\n", c);
        close(c->fd);
        return -1;
    }
    memset(c->packets, 0, c->npacket_descriptors * sizeof(struct fw_cdev_iso_packet));


    // init the read/write pointers
    // the position in c->packets of:
    // queue_ptr: the last packet descriptor queued + 1
    // hw_ptr:    the last header received + 1 (at interrupt)
    // hdr_ptr:   the last header read + 1
    // payload_ptr:  the last packet of which the payload is completely read + 1
    c->queue_ptr = 0; // no packets queued
    c->hw_ptr    = 0; // no headers received
    c->hdr_ptr   = 0; // no headers received
    c->payload_ptr  = 0; // no data read
    c->hw_ptr_cycle = -1; // receive cycle unknown

    //// header memory ////
    debugOutput(DEBUG_LEVEL_INFO, "[%p] Allocate header memory...\n", c);
    c->headers_length = c->npacket_descriptors * c->header_size;
    if(c->headers_length) {
        c->headers = (__u32 *) malloc(c->headers_length);
    } else {
        c->headers = NULL;
    }

    //// payload memory ////
    // mem-map memory for payload data
    debugOutput(DEBUG_LEVEL_INFO, "[%p] MMAP memory...\n", c);
    c->payload_length = c->npacket_descriptors * packet_size;
    c->payload = (__u32 *)mmap(NULL, c->payload_length,
                               PROT_READ|PROT_WRITE, 
                               MAP_SHARED, c->fd, 0);
    if(c->payload == MAP_FAILED) {
        debugError("[%p] Failed to map memory\n", c);
        free(c->packets);
        close(c->fd);
        return -1;
    }

    //// streams ////
    c->nstreams = 0;
    c->streams = NULL;

    c->closure = 0;
    c->state = CONN_STATE_PREPARED;
    return 0;
}

int conn_free(struct connection *c)
{
    assert(c != NULL);
    if(c->state == CONN_STATE_CREATED) {
        return 0;
    }

    if(c->state == CONN_STATE_RUNNING) {
        conn_stop_iso(c);
    }

    if(c->packets) {
        free(c->packets);
        c->packets = NULL;
        c->npacket_descriptors = 0;
    }
    if(c->headers) {
        free(c->headers);
        c->headers = NULL;
        c->headers_length = 0;
    }

    if(c->payload) {
        munmap(c->payload, c->payload_length);
        c->payload_length = 0;
    }

    if(c->fd) {
        close(c->fd);
    }

    c->context_handle = -1;
    c->state = CONN_STATE_CREATED;
    return 0;
}

int conn_create_context(struct connection *c, int channel)
{
    assert(c);
    int status;
    if(channel < 0 || channel >= 64) {
        debugError("[%p] Channel %d out of range\n", c, channel);
        return -1;
    }

    c->channel = channel;

    // create the ISO context
    struct fw_cdev_create_iso_context create;
    create.type = c->type;
    create.header_size = c->header_size;
    create.channel = channel;
    create.speed = SCODE_400;
    create.closure = c->closure;
    create.handle = 0;
    status = ioctl ( c->fd, FW_CDEV_IOC_CREATE_ISO_CONTEXT, &create );
    if(status < 0) {
        debugError("[%p] Failed to create context\n", c);
        return -1;
    }
    c->context_handle = create.handle;
    return 0;
}

int conn_start_iso(struct connection *c, 
                   int channel, int start_cycle,
                   size_t irq_interval,
                   size_t irq_offset)
{
    assert(c);
    assert(c->state == CONN_STATE_PREPARED);
    assert(start_cycle >= -1);
    assert(start_cycle < 8000);

    debugOutput(DEBUG_LEVEL_INFO,
                "[%p] Start %s connection for channel %d on cycle %d\n", c, 
                c->type == FW_CDEV_ISO_CONTEXT_TRANSMIT ? "transmit" : "receive",
                channel, start_cycle);

    //// ISO context ////
    // initialize the context
    debugOutput(DEBUG_LEVEL_INFO, "[%p] Create context...\n", c);
    if(conn_create_context(c, channel) != 0) {
        return -1;
    }

    // init the read/write pointers
    // the position in c->packets of:
    // queue_ptr: the last packet descriptor queued + 1
    // hw_ptr:    the last header received + 1 (at interrupt)
    // hdr_ptr:   the last header read + 1
    // payload_ptr:  the last packet of which the payload is completely read + 1
    c->queue_ptr = 0; // no packets queued
    c->hw_ptr    = 0; // no headers received
    c->hdr_ptr   = 0; // no headers received
    c->payload_ptr  = 0; // no data read

    // initialize the hw position pointer with the start cycle
    c->hw_ptr_cycle = start_cycle;

    // queue the first batch of descriptors
    debugOutput(DEBUG_LEVEL_INFO,
                "[%p] Init connection: %zd packets, IRQ each %zd cycles, IRQ offset %zd\n",
                c, c->buffer_size, irq_interval, irq_offset);

    size_t npackets;
    // if it is a transmit one, we'll have to queue some payload here
    if(c->type == FW_CDEV_ISO_CONTEXT_TRANSMIT) {
        // write the initial set of empty frames
        int tmp = conn_process_data(c, c->buffer_size);
        if(tmp <= 0) {
            debugError("[%p] failed to generate packet data\n", c);
            return -1;
        }
        npackets = tmp;
    } else {
        npackets = c->buffer_size;
    }

    assert(npackets > 0);

    // prepare the packet headers
    if(conn_prepare_packets(c, npackets) < 0) {
        return -1;
    }

    // set interrupts
    size_t pos = irq_interval - 1;
    while(pos < npackets) {
        size_t new_pos = pos;
        INCREMENT_AND_WRAP(new_pos, irq_offset, c->npacket_descriptors);
        debugOutput(DEBUG_LEVEL_INFO, "[%p]  interrupt at %zd...\n", c, new_pos);
        c->packets[new_pos].control |= FW_CDEV_ISO_INTERRUPT;
        pos += irq_interval;
    }

    // queue
    if(conn_queue_packets(c, npackets) < 0) {
        return -1;
    }

    int status;
    struct fw_cdev_start_iso start;
    start.cycle = start_cycle;
    start.sync = 0;
    start.tags = FW_CDEV_ISO_CONTEXT_MATCH_ALL_TAGS;
    start.handle = c->context_handle;
    status = ioctl ( c->fd, FW_CDEV_IOC_START_ISO, &start );
    if(status < 0) {
        debugError("[%p] failed to start ISO context\n", c);
        return -1;
    }

    c->state = CONN_STATE_RUNNING;
    return 0;
}

int conn_stop_iso(struct connection *c)
{
    assert(c);
    assert(c->state == CONN_STATE_RUNNING);
    int status;
    struct fw_cdev_stop_iso stop;
    stop.handle = c->context_handle;
    status = ioctl ( c->fd, FW_CDEV_IOC_STOP_ISO, &stop );
    if(status < 0) {
        debugError("[%p] failed to stop ISO context\n", c);
        return -1;
    }

    c->state = CONN_STATE_PREPARED;
    return 0;
}

int conn_prepare_packets(struct connection *c, int npackets)
{
    assert(c);
    debugOutput(DEBUG_LEVEL_VERBOSE,
                "[%p] prepare %3d packets\n",
                c, npackets);

    if(npackets == 0) return 0;
    // NOTE: don't allow to fully fill the buffer as that 
    //       will mess up the indexing system (as full == empty)
    size_t free_space = conn_get_queue_space(c);
    if(free_space == 1) return 0; // FIXME: fix buffer indexing

    size_t npackets_todo;
    if(npackets < 0) {
        // queue as many as possible
        if(c->type == FW_CDEV_ISO_CONTEXT_TRANSMIT) {
            npackets_todo = free_space;
        } else {
            npackets_todo = free_space - 1;
        }
    } else {
        npackets_todo = npackets;
        // queue an exact amount
        if(c->type == FW_CDEV_ISO_CONTEXT_TRANSMIT) {
            if(npackets_todo > free_space) {
                debugWarning("[%p] cannot prepare %zd packets, only have %zd packets of payload\n",
                            c, npackets_todo, free_space);
    
                // This can happen if the interrupt got delayed for one reason or the other
                // it's not an error per-se, as we might recover from this
                npackets_todo = free_space;
            }
        } else {
            if(npackets_todo >= free_space) {
                debugWarning("[%p] cannot prepare %zd packets, only have %zd packets of space\n",
                            c, npackets_todo, free_space-1);
    
                // This can happen if the interrupt got delayed for one reason or the other
                // it's not an error per-se, as we might recover from this
                npackets_todo = free_space - 1;
            }
        }
    }

#ifdef DEBUG
    size_t end = c->queue_ptr;
    INCREMENT_AND_WRAP(end, npackets_todo-1, c->npacket_descriptors);

    int queue_ptr_cycle = conn_get_queue_cycle(c);
    int end_cycle = -1;
    if(queue_ptr_cycle >= 0) {
        end_cycle = queue_ptr_cycle;
        INCREMENT_AND_WRAP(end_cycle, npackets_todo-1, 8000);
    }

    debugOutput(DEBUG_LEVEL_VERBOSE,
                "[%p] %3zd (cycle %4d) => %3zd (cycle %4d) | %6zd\n",
                 c, c->queue_ptr, queue_ptr_cycle,
                 end, end_cycle,
                 npackets_todo);
#endif

    size_t u, u2;
    for (u = c->queue_ptr; u < c->queue_ptr + npackets_todo; u++ ) {
        if(u < c->npacket_descriptors) {
            u2 = u;
        } else {
            u2 = u - c->npacket_descriptors;
        }

        if(c->type == FW_CDEV_ISO_CONTEXT_TRANSMIT) {
            c->packets[u2].control = FW_CDEV_ISO_HEADER_LENGTH(0)
                                     | FW_CDEV_ISO_PAYLOAD_LENGTH(c->headers[u2])
                                     | FW_CDEV_ISO_TAG(c->tag);
        } else {
            c->packets[u2].control = FW_CDEV_ISO_HEADER_LENGTH(8)
                                   | FW_CDEV_ISO_PAYLOAD_LENGTH(c->packet_size);

        }
    }

#ifdef DEBUG
    if(c->type == FW_CDEV_ISO_CONTEXT_TRANSMIT) {
        debugOutput(DEBUG_LEVEL_VERBOSE,
                    "[%p] prepared %zd packets, had payload for: %zd\n",
                    c, npackets_todo, free_space);
    } else {
        debugOutput(DEBUG_LEVEL_VERBOSE,
                    "[%p] prepared %zd packets, had space for: %zd\n",
                    c, npackets_todo, free_space);
    }
#endif

    return npackets_todo;
}

void dump_packet_buffer(struct connection *c, int limit)
{
    size_t i = 0;
    for(i=0; i < c->npacket_descriptors;i++) {
        size_t i2 = i;
        INCREMENT_AND_WRAP(i2, c->hw_ptr, c->npacket_descriptors);
        size_t cycle = i;
        INCREMENT_AND_WRAP(cycle, c->hw_ptr_cycle, 8000);

        int print = limit == 0;
        print |= c->packets[i2].control & FW_CDEV_ISO_INTERRUPT;
        print |= i2 == c->queue_ptr;
        print |= i2 == c->hw_ptr;

        if(print) {
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
                        "[%p] %3zd [%4zd]: %08X %d %s%s%s%s\n",
                        c, i2, cycle, c->packets[i2].control, 
                        (c->packets[i2].control & FW_CDEV_ISO_INTERRUPT) != 0,
                        i2 == c->queue_ptr ? "Q" : " ",
                        i2 == c->hw_ptr ? "I" : " ",
                        i2 == c->hdr_ptr ? "H" : " ",
                        i2 == c->payload_ptr ? "P" : " "
                        );
        }
    }
}

int conn_request_interrupt(struct connection *c, size_t cycle)
{
    assert(c);
    assert(cycle < 8000);
    assert(c->hw_ptr_cycle >= 0);

    debugOutput(DEBUG_LEVEL_INFO,
                "[%p] request interrupt at cycle %4zd\n",
                c, cycle);

    // find the packet buffer position that corresponds with
    // the requested cycle
    int diff = diffCycles(cycle, c->hw_ptr_cycle);

    if(diff >= (int)c->npacket_descriptors) {
        // this means that the requested cycle is more than one buffer
        // in the future. we cannot address this point yet.
        diff = c->npacket_descriptors - 1;
        size_t new_cycle = c->hw_ptr_cycle;
        INCREMENT_AND_WRAP(new_cycle, diff, 8000);

        debugWarning("[%p] cycle %zd too far in the future, setting on %zd instead\n",
                     c, cycle, new_cycle);
    }

    assert(diff >= 0 && diff < (int)c->npacket_descriptors);

    size_t pos = c->hw_ptr;
    INCREMENT_AND_WRAP(pos, (size_t)diff, c->npacket_descriptors);

    debugOutput(DEBUG_LEVEL_VERBOSE,
                "[%p]  cycle %4zd corresponds with pos %zd\n",
                c, cycle, pos);

    // check if the pos is in the writable space
    if(c->queue_ptr < c->hw_ptr) {
        if(pos < c->queue_ptr || pos >= c->hw_ptr) {
            debugError("[%p] pos %zd has not been consumed yet (W: %zd | R: %zd)\n",
                        c, pos, c->queue_ptr, c->hw_ptr);
            return -1;
        }
    } else {
        if(!(pos < c->hw_ptr || pos >= c->queue_ptr)) {
            debugError("[%p] pos %zd has not been consumed yet (W: %zd | R: %zd)\n",
                        c, pos, c->queue_ptr, c->hw_ptr);
            return -1;
        }
    }

    c->packets[pos].control |= FW_CDEV_ISO_INTERRUPT;
    dump_packet_buffer(c, 1);
    return 0;
}

int conn_queue_packets(struct connection *c, size_t npackets)
{
    debugOutput(DEBUG_LEVEL_VERBOSE,
                "[%p] queue %zd packets\n", c, npackets);
    assert(c);
    if(npackets == 0) return 0;

    int status;

    size_t free_space = conn_get_queue_space(c);
    if(free_space == 1) return 0; // FIXME: fix buffer indexing

    if(c->type == FW_CDEV_ISO_CONTEXT_TRANSMIT) {
        if(npackets > free_space) {
            debugWarning("[%p] cannot queue %zd packets, only have %zd packets of payload\n",
                        c, npackets, free_space);

    //         return -1;
            // This can happen if the interrupt got delayed for one reason or the other
            // it's not an error per-se, as we might recover from this
            npackets = free_space;
        }
    } else {
        if(npackets >= free_space) {
            debugWarning("[%p] cannot queue %zd packets, only have %zd packets of space\n",
                        c, npackets, free_space-1);
    //         return -1;
            // This can happen if the interrupt got delayed for one reason or the other
            // it's not an error per-se, as we might recover from this
            npackets = free_space-1;
        }
    }


    // do the actual re-queue
    do {
        assert(c->queue_ptr < c->npacket_descriptors);
        // this is the amount of packets one can enqueue in one go
        // for receive we can queue big chunks at once, but for
        // transmit we can only do this when the packet length is
        // constant. Therefore we queue them one-by-one for the moment
        size_t toqueue = 1;
        if(c->type == FW_CDEV_ISO_CONTEXT_RECEIVE) {
            toqueue = c->npacket_descriptors - c->queue_ptr;
            // is it more than we have to queue?
            if(toqueue > npackets) {
                toqueue = npackets;
            }
        }

//         debugOutput(DEBUG_LEVEL_VERBOSE,
//                 "[%p] queueing %zd, from %zd => %zd\n",
//                 c, toqueue, c->queue_ptr, 
//                 c->queue_ptr + toqueue-1);

        // queue the packet reception structures
        struct fw_cdev_queue_iso queue;
        queue.packets = U ( c->packets + c->queue_ptr );
        queue.data = U ( c->payload + c->queue_ptr * c->packet_size/4 );
        queue.size = toqueue * sizeof(struct fw_cdev_iso_packet);
        queue.handle = c->context_handle;
//         if(c->type == FW_CDEV_ISO_CONTEXT_TRANSMIT) {
//             int cycle = conn_get_queue_cycle(c);
//             debugOutput(DEBUG_LEVEL_VERBOSE, ">>>>>>> cycle %4d [%04X]\n", cycle, cycle);
//             conn_dump_packet((uint32_t *)queue.data, c->packet_size/4, 2, 11);
//         }

        status = ioctl ( c->fd, FW_CDEV_IOC_QUEUE_ISO, &queue );
        if(status < 0) {
            debugError("[%p] Failed to queue packet descriptors\n", c);
            return -1;
        }

        npackets -= toqueue;
        INCREMENT_AND_WRAP(c->queue_ptr,
                           toqueue,
                           c->npacket_descriptors);
    } while(npackets);

    // this is an overflow condition
    if(c->type != FW_CDEV_ISO_CONTEXT_TRANSMIT) {
        assert(c->queue_ptr != c->payload_ptr);
    }

    conn_show_ptrs(c, "queued");

    debugOutput(DEBUG_LEVEL_VERBOSE,
                "[%p] queued %zd packets too little\n", c, npackets);

    return npackets;
}


/**
 * executed to process the headers that result from an interrupt event
 * @param c 
 * @param headers 
 * @param npackets 
 * @return 
 */
void conn_interrupt(struct connection *c, struct fw_cdev_event_iso_interrupt *i)
{
    assert(c);
    assert(c->state == CONN_STATE_PREPARED || c->state == CONN_STATE_RUNNING);
    assert(c->headers);

    size_t header_size_bytes = c->header_size;
    // we have received an ISO interrupt
    int npackets = i->header_length / header_size_bytes;
    debugOutput(DEBUG_LEVEL_INFO,"\033[33m"
            "[%p] ISO interrupt [ch %2d], header_len = %d, header_size = %zd, cycle = [%1ds, %4dc], " 
            "%d pkt\033[0m\n", 
            c, c->streams->settings->channel,
            (int)i->header_length, c->header_size,
            (int)(i->cycle >> 13) & 0x7,
            (int)(i->cycle >> 0) & 0x1FFF,
            npackets);

    if(npackets == 0) return;

    size_t u = c->hw_ptr;
    size_t ndescriptors = c->npacket_descriptors;
    size_t header_size_quads = header_size_bytes / 4;
    __u32 * headers = i->header;
    assert(headers);

    do {
        __u32 *target = c->headers + u * header_size_quads;
        assert(ndescriptors > u);
        size_t tocopy = ndescriptors - u;
        // is it more than we have to queue?
        if((int)tocopy > npackets) {
            tocopy = npackets;
        }

#ifdef DEBUG
        // check the previous header content
        // to check the cycle number
        size_t x = 0;
        for(x = 0; x < tocopy; x++) {
            __u32 tsp;
            int req_cycle = -1;
            int total_npackets = i->header_length / header_size_bytes;
            int ndone = total_npackets - npackets;
            if(c->hw_ptr_cycle >= 0) {
                req_cycle = (c->hw_ptr_cycle + ndone + x) % 8000;
            }

            int eff_cycle = (i->cycle & 0x1FFF);
            if(c->type == FW_CDEV_ISO_CONTEXT_RECEIVE) {
                tsp = CondSwapFromBus32(*(headers + x * header_size_quads + 1));
                eff_cycle = tsp & 0x1FFF;
            } else {
                tsp = CondSwapFromBus32(*(headers + x * header_size_quads));
                eff_cycle = tsp & 0x1FFF;
            }

            if(req_cycle >= 0 && req_cycle != eff_cycle) {
                debugWarning("hdr cycle mismatch: req=%d, eff=%d\n",
                             req_cycle, eff_cycle);
            }
        }
#endif

        memcpy(target, headers, tocopy * header_size_bytes);
        headers += tocopy * header_size_quads;
        debugOutput(DEBUG_LEVEL_VERBOSE,
                    "[%p] copy %zd headers from %zd => %zd\n",
                    c, tocopy, u, u + tocopy-1);

        npackets -= tocopy;
        INCREMENT_AND_WRAP(u, (size_t)tocopy, ndescriptors);
    } while(npackets);

    // update the hw ptr and the cycle it corresponds to
    int tmp = u - 1;
    if(tmp < 0) tmp += c->npacket_descriptors;
    assert(tmp < (int)c->npacket_descriptors);
    assert(tmp >= 0);

    // calculate the cycle of the HW pointer
    // by fetching the last cycle processed and adding one cycle
    __u32 cycle = i->cycle & 0x1FFF;
    INCREMENT_AND_WRAP(cycle, 1, 8000);

    // update the hw pointer
    c->hw_ptr = u;
    c->hw_ptr_cycle = cycle;

    conn_show_ptrs(c, "interrupt");
}

/**
 * executed to indicate that a stream has timed out
 * @param c 
 * @param headers 
 * @param npackets 
 * @return 
 */
void conn_timeout(struct connection *c)
{
    assert(c);
    assert(c->state == CONN_STATE_PREPARED || c->state == CONN_STATE_RUNNING);

    debugWarning("[%p] timed out\n", c);
    SET_FLAG(c->flags, CONN_TIMED_OUT);

    conn_show_ptrs(c, "timeout");
}

/**
 * 
 * @param c 
 * @return 
 */
int conn_prepare_period(struct connection *c)
{
    debugOutput(DEBUG_LEVEL_VERBOSE,
                "[%p] preparing period\n", c);

    if(FLAG_SET(c->flags, CONN_TIMED_OUT)) {
        c->hw_ptr_cycle = -1;
        c->streams->last_tsp = CTR_BAD_TIMESTAMP;
        UNSET_FLAG(c->flags, CONN_TIMED_OUT);
    }
    return 0;
}

/**
 * 
 * @param c 
 * @return 
 */
int conn_process_headers(struct connection *c)
{
    debugOutput(DEBUG_LEVEL_VERBOSE,
                "[%p] processing headers\n", c);
    assert(c->nstreams == 1);

    size_t u = c->hdr_ptr;
    size_t ndescriptors = c->npacket_descriptors;
    size_t data_size_quads = c->packet_size/4;

    size_t nheaders_available = conn_get_header_read_space(c);

    assert(c->nstreams == 1);
    process_callback callback = c->streams->settings->process_header;
    size_t nheaders_done = 0;
    if(callback) { // FIXME: remove this callback alltogether?
        enum conn_callback_status status = CONN_NEED_MORE;
        while(nheaders_available && (status == CONN_NEED_MORE)) {
            //__u32 *header = c->headers + u * header_size_quads;
            __u32 *data   = c->payload + u * data_size_quads;

            // NOTE: this is rather unfinished as its not used at present

            // call the associated callback
            debugWarning("timestamp not correct!!!");
            __u32 tsp = INVALID_TIMESTAMP_TICKS;
            status = callback(c->streams, tsp, data, NULL);
    
            // update counters and pointers
            INCREMENT_AND_WRAP(u, 1, ndescriptors);
            nheaders_available--;
            nheaders_done++;
        }
    
        switch(status) {
            case CONN_ERROR:
                debugError("[%p] Failed to process packet header\n", c);
                return -1;
            case CONN_NEED_MORE:
                // not enough packets received (XRUN?)
                break;
            case CONN_HAVE_ENOUGH:
                // everything is ok
                break;
        }
    } else {
        INCREMENT_AND_WRAP(u, nheaders_available, ndescriptors);
        nheaders_done = nheaders_available;
    }
    c->hdr_ptr = u;

    debugOutput(DEBUG_LEVEL_VERBOSE,
                "[%p] processed %zd headers (still %zd available)\n",
                c, nheaders_done, nheaders_available);

    conn_show_ptrs(c, "header");

    return 0;
}

// NOTE: can also be that we have excess data in the packet,
//       i.e. the nframes ends up in the middle of a packet

int conn_process_data(struct connection *c, int max_packets)
{
    debugOutput(DEBUG_LEVEL_VERBOSE,
                "[%p] processing data (max %d packets)\n", c, max_packets);
    assert(c->nstreams == 1);

    size_t u = c->payload_ptr;
    size_t ndescriptors = c->npacket_descriptors;
    size_t header_size_bytes = c->header_size;
    size_t header_size_quads = header_size_bytes / 4;
    size_t max_packet_size_bytes = c->packet_size;
    size_t data_size_quads = max_packet_size_bytes / 4;

    size_t packets_todo = conn_get_payload_space(c);
    if(packets_todo == ndescriptors) {
        packets_todo--;
    }

    int cycle = conn_get_payload_cycle(c);

    if(max_packets > 0 && max_packets < (int)packets_todo) {
        packets_todo = max_packets;
    }

    assert(c->nstreams == 1);
    assert(c->headers);
    assert(c->payload);

    process_callback callback = c->streams->settings->process_payload;
    assert(callback);

    size_t npackets_done = 0;
    enum conn_callback_status status = CONN_NEED_MORE;
    while(packets_todo &&
          (status == CONN_NEED_MORE)) {
        __u32 *header = c->headers + u * header_size_quads;
        __u32 *data   = c->payload + u * data_size_quads;

        __u32 tsp;
        int len;
        if(c->type == FW_CDEV_ISO_CONTEXT_TRANSMIT) {
            tsp = cycle;
            if(cycle >= 0) {
                INCREMENT_AND_WRAP(cycle, 1, 8000);
            }
        } else {
            __u32 h0 = CondSwapFromBus32(*header);
            len     = (h0 >> 16) & 0xFFFF;
            // int tag     = (h0 >> 14) & 0x3;
            int channel = (h0 >>  8) & 0x3F;
            // int tcode   = (h0 >>  4) & 0xF;
            // int sy      = (h0 >>  0) & 0xF;
            assert(channel == (int)c->streams->settings->channel);
            tsp = CondSwapFromBus32( *(header+1) );
        }

        // call the associated callback
        status = callback(c->streams, tsp, data, &len);

        // FIXME: this is a bit clumsy
        if(c->type == FW_CDEV_ISO_CONTEXT_TRANSMIT) {
            if(len > (int)max_packet_size_bytes) {
                debugError("packet data too large");
                exit(-1);
            }
            // save packet length for later use (descriptor queue)
            *header = len;
        }

        // update counters and pointers
        INCREMENT_AND_WRAP(u, 1, ndescriptors);
        packets_todo--;
        npackets_done++;
    }

    switch(status) {
        case CONN_ERROR:
            debugError("[%p] Failed to process packet data\n", c);
            return -1;
        case CONN_NEED_MORE:
            // not enough packets received (XRUN?)
            break;
        case CONN_HAVE_ENOUGH:
            // everything is ok
            break;
    }

    c->payload_ptr = u;

    if(c->type == FW_CDEV_ISO_CONTEXT_TRANSMIT) {
        debugOutput(DEBUG_LEVEL_VERBOSE,
                    "[%p] got payload for %zd packets (still room for %zd packets)\n",
                    c, npackets_done, conn_get_payload_space(c));
    } else {
        debugOutput(DEBUG_LEVEL_VERBOSE,
                    "[%p] consumed payload of %zd packets (still %zd packets left)\n",
                    c, npackets_done, conn_get_payload_space(c));
    }
    conn_show_ptrs(c, "data");

    return npackets_done;
}


/**
 * Returns the amount of packets for which headers
 * can be read
 * @param c 
 * @return 
 */
size_t conn_get_header_read_space(const struct connection *c)
{
    int diff = c->hw_ptr - c->hdr_ptr;
    if(diff < 0) diff += c->npacket_descriptors;
    assert(diff >= 0 && diff < (int)c->npacket_descriptors);
    return diff;
}

/**
 * Returns the amount of packets for which payload data
 * can be read/written
 * @param c 
 * @return 
 */
size_t conn_get_payload_space(const struct connection *c)
{
    int diff = c->hdr_ptr - c->payload_ptr;
    if(c->type == FW_CDEV_ISO_CONTEXT_RECEIVE) {
        if(diff < 0) diff += c->npacket_descriptors;
        assert(diff >= 0 && diff < (int)c->npacket_descriptors);
    } else {
        if(diff <= 0) diff += c->npacket_descriptors;
        assert(diff > 0 && diff <= (int)c->npacket_descriptors);
    }
    return diff;
}

/**
 * The amount of packets that can be queued
 * packets can be (re-)queued.
 * For receive connections, it is assumed that headers
 * are read before payload, hence that the payload ptr
 * will give the last packet released.
 * For transmit connections, it is assumed that payload
 * is written before packets are queued.
 * @param c 
 * @return 
 */
size_t conn_get_queue_space(const struct connection *c)
{
    if(c->payload_ptr == c->queue_ptr) {
        return c->npacket_descriptors;
    }

    int diff = c->payload_ptr - c->queue_ptr;
    if(diff < 0) diff += c->npacket_descriptors;

    assert(diff >= 0 && diff <= (int)c->npacket_descriptors);
    return diff;
}

/**
 * The next cycle that will be processed by the hardware.
 * 
 * @param c 
 * @return 
 */
int conn_get_next_hw_cycle(const struct connection *c)
{
    assert(c);
    return c->hw_ptr_cycle;
}

/**
 * The cycle of the next packet to be queued
 * @param c 
 * @return 
 */
int conn_get_queue_cycle(const struct connection *c)
{
    assert(c);
    if(c->hw_ptr_cycle < 0) return -1;
    size_t curr_queue_cycle = c->hw_ptr_cycle;
    int diff = c->queue_ptr - c->hw_ptr;
    if(diff < 0) diff += c->npacket_descriptors;
    assert(diff >= 0 && diff < (int)c->npacket_descriptors);
    INCREMENT_AND_WRAP(curr_queue_cycle, diff, 8000);
    return curr_queue_cycle;
}

/**
 * The cycle of the next packet header to read
 * @param c 
 * @return 
 */
int conn_get_header_cycle(const struct connection *c)
{
    assert(c);
    if(c->hw_ptr_cycle < 0) return -1;
    int cycle = c->hw_ptr_cycle;
    int diff = c->hw_ptr - c->hdr_ptr;
    if(diff < 0) diff += c->npacket_descriptors;
    assert(diff >= 0 && diff < (int)c->npacket_descriptors);
    cycle -= diff;
    if(cycle < 0) cycle += 8000;
    assert(cycle >= 0 && cycle < 8000);
    return cycle;
}

/**
 * The cycle of the next payload buffer
 * @param c 
 * @return 
 */
int conn_get_payload_cycle(const struct connection *c)
{
    assert(c);
    if(c->hw_ptr_cycle < 0) return -1;
    int cycle = c->hw_ptr_cycle;
    if(c->type == FW_CDEV_ISO_CONTEXT_RECEIVE) {
        int diff = c->hw_ptr - c->payload_ptr;
        if(diff < 0) diff += c->npacket_descriptors;
        assert(diff >= 0 && diff < (int)c->npacket_descriptors);
        cycle -= diff;
    } else {
        int diff = c->payload_ptr - c->hw_ptr;
        if(diff < 0) diff += c->npacket_descriptors;
        assert(diff >= 0 && diff < (int)c->npacket_descriptors);
        INCREMENT_AND_WRAP(cycle, diff, 8000);
    }
    if(cycle < 0) cycle += 8000;
    assert(cycle >= 0 && cycle < 8000);
    return cycle;
}

void conn_show_ptrs(struct connection *c, const char *msg) {
    if(c->type == FW_CDEV_ISO_CONTEXT_RECEIVE) {
        debugOutput(DEBUG_LEVEL_VERBOSE,
                    "[%p] R %9s => [P: %3zd , %4dc] [H: %3zd , %4dc] [I: %3zd , %4dc] [Q: %3zd , %4dc]\n",
                    c, msg,
                    c->payload_ptr, conn_get_payload_cycle(c),
                    c->hdr_ptr, conn_get_header_cycle(c),
                    c->hw_ptr, conn_get_next_hw_cycle(c),
                    c->queue_ptr, conn_get_queue_cycle(c));
    } else {
        debugOutput(DEBUG_LEVEL_VERBOSE,
                    "[%p] X %9s => [H: %3zd , %4dc] [I: %3zd , %4dc] [Q: %3zd , %4dc] [P: %3zd , %4dc]\n",
                    c, msg,
                    c->hdr_ptr, conn_get_header_cycle(c),
                    c->hw_ptr, conn_get_next_hw_cycle(c),
                    c->queue_ptr, conn_get_queue_cycle(c),
                    c->payload_ptr, conn_get_payload_cycle(c));
    }
}
