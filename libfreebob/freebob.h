/* freebob.h
 * Copyright (C) 2005 Pieter Palmers
 *
 * This file is part of FreeBoB
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#ifndef FREEBOB_H
#define FREEBOB_H

#define FREEBOB_MAX_NAME_LEN 256

enum freebob_direction {
    FREEBOB_CAPTURE  = 0,
    FREEBOB_PLAYBACK = 1,
};

typedef struct freebob_handle* freebob_handle_t;

/*
 * Buffer specification
 */
typedef struct _freebob_stream_spec freebob_stream_spec_t;
typedef struct _freebob_stream_info freebob_stream_info_t;
typedef struct _freebob_connection_spec freebob_connection_spec_t;
typedef struct _freebob_connection_info freebob_connection_info_t;

/*
 * Stream specification
 */
struct _freebob_stream_spec {
    int location;
    int position;
    int format;
    int type;
    int destination_port;
    char name[FREEBOB_MAX_NAME_LEN];
};

struct _freebob_stream_info {
    int nb_streams;
    freebob_stream_spec_t** streams;
};

/*
 * Connection specification
 */
struct _freebob_connection_spec {
    int id;
    int port;
    int node;
    int plug;
    int dimension;  /* due to the midi stuff, the dimension isn't equal */
                    /* to the number of streams */
    int samplerate; /* this should be equal for all connections when */
                    /* using jack. maybe not when using other api's */
    int iso_channel;
    enum freebob_direction direction;
    int is_master;
    freebob_stream_info_t* stream_info;
};

/*
 * topology info
 */
struct _freebob_connection_info {
    int direction;
    int nb_connections;
    freebob_connection_spec_t** connections;
};


#ifdef __cplusplus
extern "C" {
#endif

freebob_handle_t
freebob_new_handle( int port );

int
freebob_destroy_handle( freebob_handle_t freebob_handle );

int
freebob_discover_devices( freebob_handle_t freebob_handle );


freebob_connection_info_t*
freebob_get_connection_info( freebob_handle_t freebob_handle,
			     int node_id,
			     enum freebob_direction direction );

void
freebob_free_connection_info( freebob_connection_info_t* connection_info );
void
freebob_free_connection_spec( freebob_connection_spec_t* connection_spec );
void
freebob_free_stream_info( freebob_stream_info_t* stream_info );
void
freebob_free_stream_spec( freebob_stream_spec_t* stream_spec );

void
freebob_print_connection_info( freebob_connection_info_t* connection_info );

const char*
freebob_get_version();

#ifdef __cplusplus
}
#endif

#endif /* FREEBOB_H */
