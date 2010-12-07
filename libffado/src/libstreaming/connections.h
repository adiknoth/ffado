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

#ifndef _FFADO_CONNECTIONS_H
#define _FFADO_CONNECTIONS_H

#include "libutil/ringbuffer.h"

enum conn_callback_status {
    CONN_ERROR       =  -1,
    CONN_NEED_MORE   =   0,
    CONN_HAVE_ENOUGH =   1,
};

typedef enum conn_callback_status (*process_callback)
    (struct stream_info *stream, __u32 tsp, __u32 *data, int* data_len);

// connection info
#define CONN_STATE_BOGUS     0
#define CONN_STATE_CREATED   (1<<0)
#define CONN_STATE_PREPARED  (1<<1)
#define CONN_STATE_RUNNING   (1<<2)

// flags
#define CONN_TIMED_OUT       (1<<0)

#define SET_FLAG(reg, flag) \
    reg |= flag
#define UNSET_FLAG(reg, flag) \
    reg &= ~flag

#define FLAG_SET(reg, flag) \
    ((reg & flag) != 0)

#define FLAG_NOT_SET(reg, flag) \
    ((reg & flag) == 0)

struct stream_info;

struct connection
{
    __u64 closure;
    unsigned int type;
    unsigned int state;

    // tag to use on this connection
    // FIXME: SY?
    uint8_t tag;
    
    __u32 flags;

    // API information
    int fd;
    __u64 context_handle;

    int channel; // only used for debug
    
    // buffer position information
    size_t queue_ptr;
    size_t hw_ptr;
    size_t hdr_ptr;
    size_t payload_ptr;
    int hw_ptr_cycle; // cycle corresponding with hw_ptr

    // packet descriptor buffer
    size_t buffer_size;
    size_t npacket_descriptors;
    struct fw_cdev_iso_packet *packets;

    // header buffer
    size_t headers_length;
    __u32 *headers;
    size_t header_size;

    // payload buffer
    size_t payload_length;
    __u32 *payload;
    size_t packet_size;

    // attached streams
    struct stream_info *streams;
    size_t nstreams;
};

int conn_init(struct connection *c,
              int type, size_t port,
              uint8_t tag,
              size_t packet_size,
              size_t buffer_size, size_t iso_slack);
int conn_free(struct connection *c);

int conn_start_iso(struct connection *c, 
                   int channel, int start_cycle,
                   size_t irq_interval,
                   size_t irq_offset);
int conn_stop_iso(struct connection *c);

int conn_prepare_period(struct connection *c);

int conn_prepare_packets(struct connection *c, int npackets);
int conn_queue_packets(struct connection *c, size_t toqueue);

void conn_interrupt(struct connection *c, struct fw_cdev_event_iso_interrupt *i);
void conn_timeout(struct connection *c);

int conn_process_headers(struct connection *c);
int conn_process_data(struct connection *c, int max_packets);

int conn_request_interrupt(struct connection *c, size_t cycle);

// FIXME: should not be required outside of the connection?
int conn_get_queue_cycle(const struct connection *c);
int conn_get_next_hw_cycle(const struct connection *c);

#endif // _FFADO_CONNECTIONS_H
