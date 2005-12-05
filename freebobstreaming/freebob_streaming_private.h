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

/* freebob_streaming_private.h
 *
 * Private stuff related to the freebob streaming API
 *
 */
 
#ifndef __FREEBOB_STREAMING_PRIVATE_H__
#define __FREEBOB_STREAMING_PRIVATE_H__

#ifndef DEBUG
#define DEBUG
#endif

#ifdef __cplusplus
extern "C" {
#endif
#define AMDTP_MAX_PACKET_SIZE 2048

#define IEC61883_STREAM_TYPE_MIDI   0x0D
#define IEC61883_STREAM_TYPE_SPDIF  0x00
#define IEC61883_STREAM_TYPE_MBLA   0x06

#define IEC61883_AM824_LABEL_MASK 			0xFF000000
#define IEC61883_AM824_GET_LABEL(x) 		((x & 0xFF000000) >> 24)

#define IEC61883_AM824_LABEL_MIDI_NO_DATA 	0x80 
#define IEC61883_AM824_LABEL_MIDI_1X      	0x81 
#define IEC61883_AM824_LABEL_MIDI_2X      	0x82
#define IEC61883_AM824_LABEL_MIDI_3X      	0x83

#include <freebobctl/freebobctl.h>
#include <libraw1394/raw1394.h>
#include <libiec61883/iec61883.h>
#include <pthread.h>
#include <linux/types.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <poll.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <endian.h>

#include <pthread.h>
#include <semaphore.h>

#include <assert.h>

#include "ringbuffer.h"
#include "freebob_streams.h"
#include "freebob_connections.h"

#define FALSE 0
#define TRUE 1


/**
 * Structure to keep track of the packetizer thread status
 */
 
typedef struct _freebob_packetizer_status freebob_packetizer_status_t;
struct _freebob_packetizer_status {
	sem_t transfer_boundary;
	sem_t transfer_ack;
	int retval;
	int status;
	int run;	
	
	pthread_t transfer_thread;

};

/**
 * Device structure
 */

struct _freebob_device
{	

	freebob_device_info_t device_info;

	freebob_options_t options;

	int xrun_detected;
	int xrun_count;
	
	/* packet transfer thread */
	freebob_packetizer_status_t packetizer;

	/* ISO connections */
	freebob_connection_t *sync_master_connection;
	
	int nb_connections;
	int nb_connections_playback;
	int nb_connections_capture;
	freebob_connection_t *connections;
	
	int nb_capture_streams;
	freebob_stream_t **capture_streams;
	int nb_playback_streams;
	freebob_stream_t **playback_streams;
	
	/*
	 * Sync'ed streams are streams that count when determining the period boundary
	 * 
	 */
	int nb_synced_capture_streams;
	freebob_stream_t **synced_capture_streams;
	int nb_synced_playback_streams;
	freebob_stream_t **synced_playback_streams;

}; 


int freebob_streaming_prefill_playback_streams(freebob_device_t *dev);

int freebob_streaming_stop_iso(freebob_device_t *dev);
int freebob_streaming_start_iso(freebob_device_t *dev);

/**
 * Registers the stream in the stream list
 */
void freebob_streaming_register_capture_stream(freebob_device_t *dev, freebob_stream_t *stream);
void freebob_streaming_register_playback_stream(freebob_device_t *dev, freebob_stream_t *stream);

/**
 * Thread related
 */
int
freebob_streaming_create_thread (pthread_t* thread,
			   int priority,
			   int realtime,
			   void*(*start_routine)(void*),
			   void* arg);
			   
int freebob_streaming_start_thread(freebob_device_t *dev);
int freebob_streaming_stop_thread(freebob_device_t *dev);
			   
void * freebob_iso_packet_iterator(void *arg);

#ifdef __cplusplus
}
#endif

#endif
