/*
 * Copyright (C) 2009-2010 by Pieter Palmers
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

#ifndef _FFADO_STREAMER_H
#define _FFADO_STREAMER_H

#define CTR_BAD_TIMESTAMP ((__u32)(-1))

#define MAX_NB_CONNECTIONS_XMT 4
#define MAX_NB_CONNECTIONS_RCV 4

#warning remove this header
#include "libffado/ffado.h"

typedef struct streamer * streamer_t;

enum stream_type {
    STREAM_TYPE_TRANSMIT,
    STREAM_TYPE_RECEIVE,
};

enum stream_port_state {
    STREAM_PORT_STATE_ON,
    STREAM_PORT_STATE_OFF,
};

struct stream_settings {
    enum stream_type type;

    size_t port;
    int channel;
    size_t tag;

    size_t max_packet_size;

    size_t packet_size_for_sync;

    void * client_data;
    process_callback process_header;
    process_callback process_payload;

    // substream info
    size_t nb_substreams;
    void **substream_buffers;
    char **substream_names;
    enum stream_port_state *substream_states;
    ffado_streaming_stream_type *substream_types;
};

struct stream_info {
    int todo;
    size_t offset;
    float tpf;

    // timestamp that holds the timestamp of the previous packet
    // NOTE: should be internal to the streaming engine?
    // used only for debugging and to detect non-consequtive cycles
    __u32 last_recv_tsp;

    // last timestamp processed by the stream processing function
    // NOTE: should be timestamp of last frame? (lastframe + 1)?
    // NOTE: should maybe be private?
    __u32 last_tsp;

    // sync timestamp as set by the streaming engine when a
    // period starts. the stream callback should use this to
    // find out when to send a next frame. it can also update
    // this timestamp to prepare for the next packet as the
    // streaming interface will only set this on a period boundary.
    //
    // For a receive stream:
    // this timestamp corresponds with the first frame
    // expected by the streaming system on receive.
    // i.e. the first frame put into the buffer by the
    // stream callback should correspond to sync_tsp (+/- 0.5 frame)
    //
    // For a playback stream:
    // for playback it is the timestamp of the next frame to send.
    __u32 base_tsp;

    struct stream_info *master;
    streamer_t streamer;
    struct connection *connection;

    struct stream_settings *settings;
};

streamer_t streamer_new();
int streamer_init(streamer_t s, unsigned int util_port,
                  size_t period_size, size_t nb_periods,
                  size_t frame_slack, size_t iso_slack,
                  size_t nominal_frame_rate);
void streamer_free(streamer_t s);

int streamer_start(streamer_t s);
int streamer_stop(streamer_t s);

int streamer_wait_for_period(streamer_t s);

int streamer_read_frames(streamer_t s);
int streamer_write_frames(streamer_t s);

int streamer_queue_next_period(streamer_t s);

// TODO: which of the following are required?
int streamer_add_stream(streamer_t s, struct stream_settings *settings);
int streamer_start_connection(streamer_t s, __u32 id, int cycle);
int streamer_set_sync_connection(streamer_t s, __u32 id);

void streamer_print_stream_state(streamer_t s);

// stream settings manipulation

int stream_settings_alloc_substreams(struct stream_settings *s, size_t nb_substreams);
void stream_settings_free_substreams(struct stream_settings *s);

void stream_settings_set_substream_name(struct stream_settings *s,
                                        unsigned int i, const char *name);
void stream_settings_set_substream_type(struct stream_settings *s,
                                        unsigned int i,
                                        ffado_streaming_stream_type type);
void stream_settings_set_substream_state(struct stream_settings *s,
                                         unsigned int i,
                                         enum stream_port_state state);

#endif