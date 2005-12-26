/* $Id$ */

/*
 *   FreeBob Streaming API
 *   FreeBob = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2005 Pieter Palmers <pieterpalmers@users.sourceforge.net>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * 
 *
 */

/* freebob_connections.h
 *
 * Connection handling
 *
 */
#ifndef __FREEBOB_CONNECTIONS_H__
#define __FREEBOB_CONNECTIONS_H__

#include "cip.h"

#include <freebobctl/freebobctl.h>
#include "ringbuffer.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ISO/AM824 packet structures
 */
#if __BYTE_ORDER == __BIG_ENDIAN

struct iso_packet_header {
	unsigned int data_length : 16;
	unsigned int tag         : 2;
	unsigned int channel     : 6;
	unsigned int tcode       : 4;
	unsigned int sy          : 4;
};


typedef struct packet_info packet_info_t;
struct packet_info {
	/* The first part of this structure is a copy of the CIP header 
	 * This can be memcpy'd from the packet itself.
	 */
	/* First quadlet */
	unsigned int dbs      : 8;
	unsigned int eoh0     : 2;
	unsigned int sid      : 6;

	unsigned int dbc      : 8;
	unsigned int fn       : 2;
	unsigned int qpc      : 3;
	unsigned int sph      : 1;
	unsigned int reserved : 2;

	/* Second quadlet */
	unsigned int fdf      : 8;
	unsigned int eoh1     : 2;
	unsigned int fmt      : 6;

	unsigned int syt      : 16;

	/* the second part is some extra info */
	unsigned int nevents;
	unsigned int length;
};

#elif __BYTE_ORDER == __LITTLE_ENDIAN

struct iso_packet_header {
	unsigned int data_length : 16;
	unsigned int channel     : 6;
	unsigned int tag         : 2;
	unsigned int sy          : 4;
	unsigned int tcode       : 4;
};

typedef struct packet_info packet_info_t;
struct packet_info {
	/* The first part of this structure is a copy of the CIP header 
	 * This can be memcpy'd from the packet itself.
	 */
	/* First quadlet */
	unsigned int sid      : 6;
	unsigned int eoh0     : 2;
	unsigned int dbs      : 8;

	unsigned int reserved : 2;
	unsigned int sph      : 1;
	unsigned int qpc      : 3;
	unsigned int fn       : 2;
	unsigned int dbc      : 8;

	/* Second quadlet */
	unsigned int fmt      : 6;
	unsigned int eoh1     : 2;
	unsigned int fdf      : 8;

	unsigned int syt      : 16;

	/* the second part is some extra info */
	unsigned int nevents;
	unsigned int length;
};


#else

#error Unknown bitfield type

#endif

/*
 * Connection definition
 */

typedef struct _freebob_iso_status {
	int buffers;
	int prebuffers;
	int irq_interval;
	int startcycle;
	
	int iso_channel;
	int do_disconnect;
	int bandwidth;
	
	int hostplug;
	
	enum raw1394_iso_speed speed;
} freebob_iso_status_t;

typedef struct _freebob_connection_status {
	int packets;
	int events;
	//int total_packets;
	int total_events;

	int frames_left;
	int iso_channel;
	
	int xruns;
		
	struct iec61883_cip cip;
	
	int dropped;
	
	int fdf;
	
	int packet_info_table_size;
	packet_info_t *packet_info_table;
	
	struct _freebob_connection_status *master;

#ifdef DEBUG
	int total_packets_prev;

#endif
} freebob_connection_status_t;

typedef struct _freebob_stream freebob_stream_t;
typedef struct _freebob_connection freebob_connection_t;

struct _freebob_connection {
	freebob_connection_spec_t spec;
	freebob_connection_status_t status;
	freebob_iso_status_t iso;
	
	/*
	 * The streams this connection is composed of
	 */
	int nb_streams;
 	freebob_stream_t *streams;
	
	/* 
	 * This buffers between the packet thread and the read/write routines
	 */
	//jack_ringbuffer_t * dma_buffer;
	
	/*
	 * This is a temporary buffer to decode/encode from samples from/to events
	 */
	//quadlet_t *event_buffer;

	raw1394handle_t raw_handle;
		
};

/** 
 * Stream definition
 */

struct _freebob_stream {
	freebob_stream_spec_t        spec;
	
	freebob_ringbuffer_t *buffer;
	freebob_connection_t *parent;
	
};

/* Initializes a stream_t based upon an stream_info_t */
int freebob_streaming_init_stream(freebob_device_t* dev, freebob_stream_t *dst, freebob_stream_spec_t *src);

/* prefills a stream when the format needs it */
int freebob_streaming_prefill_stream(freebob_device_t* dev, freebob_stream_t *stream);

/* destroys a stream_t */
void freebob_streaming_cleanup_stream(freebob_device_t* dev, freebob_stream_t *dst);

/* put a stream into a known state */
int freebob_streaming_reset_stream(freebob_device_t* dev, freebob_stream_t *dst);


/**
 * freebob_encode_stream_to_events
 *
 * Encodes events from a buffer to AM824 events, according to a specific stream specification.
 *	
 * This will read nsamples samples from the stream ringbuffer, and encode them into events conform
 * the stream specification.
 *
 * NOTE: the buffer has to be big enough
 * 
 *  connection    : The connection the stream belongs to
 *	stream        : Contains the stream structure
 *  events        : Pointer to a buffer that will hold the encoded events
 *  nsamples      : The number of samples to encode
 *  dbc           : Data Block Counter, needed for mux of midi streams
 */
 
int 
freebob_encode_stream_to_events(freebob_connection_t *connection,
								freebob_stream_t *stream, 
								quadlet_t* events, 
								unsigned int nsamples,
								unsigned int dbc
								);

/**
 * freebob_decode_events_to_stream
 *
 * Decodes events from a buffer of AM824 events, to samples of the correct type in a stream structure.
 * Skips events marked as empty.
 *	
 * This will decode nframes at maximum.
 *
 *  connection    : The connection the stream belongs to
 *	stream        : Contains the stream structure
 *  events        : Pointer to a buffer that holds the encoded events
 *  nsamples      : The number of samples available
 *  dbc           : Data Block Counter, needed for demux of midi streams
 */

int 
freebob_decode_events_to_stream(freebob_connection_t *connection,
								freebob_stream_t *stream, 
								quadlet_t* events, 
								unsigned int nsamples,
								unsigned int dbc
								);


int freebob_streaming_init_connection(freebob_device_t * dev, freebob_connection_t *connection);
int freebob_streaming_cleanup_connection(freebob_device_t * dev, freebob_connection_t *connection);
int freebob_streaming_reset_connection(freebob_device_t * dev, freebob_connection_t *connection);



#ifdef __cplusplus
}
#endif

#endif
