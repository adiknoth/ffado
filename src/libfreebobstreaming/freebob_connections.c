/*
 *   FreeBoB Streaming API
 *   FreeBoB = Firewire (pro-)audio for linux
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

/* freebob_connections.c
 *
 * Connection handling
 *
 */
#include <libraw1394/raw1394.h>
#include <libiec61883/iec61883.h>

#include "libfreebob/freebob_streaming.h"
#include "freebob_streaming_private.h"
#include "freebob_connections.h"
#include "freebob_debug.h"

raw1394handle_t freebob_open_raw1394 (int port)
{
	raw1394handle_t raw1394_handle;
	int ret;
	int stale_port_info;
  
	/* open the connection to libraw1394 */
	raw1394_handle = raw1394_new_handle ();
	if (!raw1394_handle) {
		if (errno == 0) {
			printError("This version of libraw1394 is "
				    "incompatible with your kernel\n");
		} else {
			printError("Could not create libraw1394 "
				    "handle: %s\n", strerror (errno));
		}

		return NULL;
	}
  
	/* set the port we'll be using */
	stale_port_info = 1;
	do {
		ret = raw1394_get_port_info (raw1394_handle, NULL, 0);
		if (ret <= port) {
			printError("IEEE394 port %d is not available\n", port);
			raw1394_destroy_handle (raw1394_handle);
			return NULL;
		}
  
		ret = raw1394_set_port (raw1394_handle, port);
		if (ret == -1) {
			if (errno != ESTALE) {
				printError("Couldn't use IEEE394 port %d: %s\n",
					    port, strerror (errno));
				raw1394_destroy_handle (raw1394_handle);
				return NULL;
			}
		}
		else {
			stale_port_info = 0;
		}
	} while (stale_port_info);
  
	return raw1394_handle;
}

int freebob_streaming_reset_connection(freebob_device_t * dev, freebob_connection_t *connection) {
	int s=0;
	int err=0;
	
	assert(dev);
	assert(connection);
	
	printEnter();
	
	for (s=0;s<connection->nb_streams;s++) {
		err=freebob_streaming_reset_stream(dev,&connection->streams[s]);
		if(err) {
			printError("Could not reset stream %d",s);
			break;
		}
		
	}
	
	// reset the event buffer, discard all content
	freebob_ringbuffer_reset(connection->event_buffer);
	
	// reset the timestamp buffer, discard all content
	freebob_ringbuffer_reset(connection->timestamp_buffer);
	
	// reset the event counter
	connection->status.events=0;
	
	// reset the boundary counter
	connection->status.frames_left = 0;
	
	// reset the counters
	connection->status.xruns = 0;
	connection->status.packets=0;	

#ifdef DEBUG
	connection->status.total_packets_prev=0;
#endif

	connection->status.dropped=0;

	// make sure the connection is polled next time
	if(connection->pfd) { // this can be called before everything is init'ed
		connection->pfd->events=POLLIN;
	}

	printExit();
	
	return 0;
	
}

int freebob_streaming_init_connection(freebob_device_t * dev, freebob_connection_t *connection) {
	int err=0;
	int s=0;
	
	connection->status.frames_left=0;
	
	connection->parent=dev;
		
	// create raw1394 handles
	connection->raw_handle=freebob_open_raw1394(connection->spec.port);
	if (!connection->raw_handle) {
		printError("Could not get raw1394 handle\n");
		return -ENOMEM;
	}
	raw1394_set_userdata(connection->raw_handle, (void *)connection);
	
	connection->iso.speed = RAW1394_ISO_SPEED_400;
	
	connection->iso.bandwidth=-1;
	connection->iso.startcycle=-1;
	
	// allocate the event ringbuffer
	if( !(connection->event_buffer=freebob_ringbuffer_create(
			(connection->spec.dimension * dev->options.nb_buffers * dev->options.period_size) * sizeof(quadlet_t)))) {
		printError("Could not allocate memory event ringbuffer");
		return -ENOMEM;
			
	}
	// allocate the temporary cluster buffer
	if( !(connection->cluster_buffer=(char *)calloc(connection->spec.dimension,sizeof(quadlet_t)))) {
		printError("Could not allocate temporary cluster buffer");
		freebob_ringbuffer_free(connection->event_buffer);
		return -ENOMEM;
	}
	
	// allocate the timestamp buffer
	if( !(connection->timestamp_buffer=freebob_ringbuffer_create(TIMESTAMP_BUFFER_SIZE * sizeof(freebob_timestamp_t)))) {
		printError("Could not allocate timestamp ringbuffer");
		freebob_ringbuffer_free(connection->event_buffer);
		free(connection->cluster_buffer);
		return -ENOMEM;
	}
	
	connection->total_delay=0;
	
	/*
	 * init the streams this connection is composed of
	 */
	assert(connection->spec.stream_info);
	connection->nb_streams=connection->spec.stream_info->nb_streams;
	
	connection->streams=calloc(connection->nb_streams, sizeof(freebob_stream_t));
	
	if(!connection->streams) {
		printError("Could not allocate memory for streams");
		free(connection->cluster_buffer);
		freebob_ringbuffer_free(connection->event_buffer);
		return -ENOMEM;
	}
		
	err=0;
	for (s=0;s<connection->nb_streams;s++) {
		err=freebob_streaming_init_stream(dev,&connection->streams[s],connection->spec.stream_info->streams[s]);
		if(err) {
			printError("Could not init stream %d",s);
			break;
		}
		
		// set the stream parent relation
		connection->streams[s].parent=connection;
		
		// register the stream in the stream list used for reading out
		if(connection->spec.direction==FREEBOB_CAPTURE) {
			freebob_streaming_register_capture_stream(dev, &connection->streams[s]);
		} else {
			freebob_streaming_register_playback_stream(dev, &connection->streams[s]);
		}
	}	
	if (err) {
		debugPrint(DEBUG_LEVEL_STARTUP,"  Cleaning up streams...\n");
		while(s--) { // TODO: check if this counts correctly
			debugPrint(DEBUG_LEVEL_STARTUP,"  Cleaning up stream %d...\n",s);
			freebob_streaming_cleanup_stream(dev, &connection->streams[s]);
		}
		
		free(connection->streams);
		free(connection->cluster_buffer);
		freebob_ringbuffer_free(connection->event_buffer);
		freebob_ringbuffer_free(connection->timestamp_buffer);
		
		return err;		
	}
	
	// FIXME: a flaw of the current approach is that a spec->stream_info isn't a valid pointer 
	connection->spec.stream_info=NULL;
	
	// put the connection into a know state
	freebob_streaming_reset_connection(dev, connection);

	return 0;
}

int freebob_streaming_cleanup_connection(freebob_device_t * dev, freebob_connection_t *connection) {

	unsigned int s;
	
	for (s=0;s<connection->nb_streams;s++) {
		debugPrint(DEBUG_LEVEL_STARTUP,"  Cleaning up stream %d...\n",s);
		freebob_streaming_cleanup_stream(dev, &connection->streams[s]);
	}
	
	free(connection->streams);
	free(connection->cluster_buffer);
	freebob_ringbuffer_free(connection->event_buffer);
	freebob_ringbuffer_free(connection->timestamp_buffer);
	
	raw1394_destroy_handle(connection->raw_handle);
/*
	assert(connection->status.packet_info_table);
	free(connection->status.packet_info_table);
	connection->status.packet_info_table=NULL;
*/
	
	return 0;
	
}

/**
 * Initializes a stream_t based upon an stream_info_t 
 */
int freebob_streaming_init_stream(freebob_device_t* dev, freebob_stream_t *dst, freebob_stream_spec_t *src) {
	assert(dev);
	assert(dst);
	assert(src);
	
	memcpy(&dst->spec,src,sizeof(freebob_stream_spec_t));
	
	// allocate the ringbuffer
	// the lock free ringbuffer needs one extra frame 
	// it can only be filled to size-1 bytes

	// keep the ringbuffer aligned by adding sizeof(sample_t) bytes instead of just 1
	int buffer_size_frames=dev->options.nb_buffers*dev->options.period_size+1;
	dst->buffer=freebob_ringbuffer_create(buffer_size_frames*sizeof(freebob_sample_t));
	
	
	// initialize the decoder stuff to use the per-stream read functions
	dst->buffer_type=freebob_buffer_type_per_stream;
	dst->user_buffer=NULL;
	
	// create any data structures for the decode buffers
	freebob_streaming_set_stream_buffer(dev, dst, NULL, freebob_buffer_type_per_stream);
	
	// no other init needed
	return 0;
	
}

/**
  * destroys a stream_t 
  */
void freebob_streaming_cleanup_stream(freebob_device_t* dev, freebob_stream_t *dst) {
	assert(dev);
	assert(dst);
	assert(dst->user_buffer);
	
	freebob_streaming_free_stream_buffer(dev, dst);
		
	freebob_ringbuffer_free(dst->buffer);
	return;
	
}

/**
  * puts a stream into a known state
  */
int freebob_streaming_reset_stream(freebob_device_t* dev, freebob_stream_t *dst) {
	assert(dev);
	assert(dst);
	
	freebob_ringbuffer_reset(dst->buffer);
	return 0;
}


int freebob_streaming_prefill_stream(freebob_device_t* dev, freebob_stream_t *stream) {
	assert(stream);
	int towrite=0;
	int written=0;
	char *buffer=NULL;
			
	switch(stream->spec.format) {
		case IEC61883_STREAM_TYPE_MBLA:
			towrite=dev->options.period_size*dev->options.nb_buffers*sizeof(freebob_sample_t);
			
			// calloc clears the allocated memory
			buffer=calloc(1,towrite);
			written=freebob_ringbuffer_write(stream->buffer,(char *)buffer,towrite);
			free(buffer);
			
			if(written != towrite) {
				assert(!"Could not prefill the playback stream ringbuffer, which should have been empty.");
				printError("Could not prefill the buffer. Written (%d/%d) of (%d/%d) bytes/frames\n",
					written,written/sizeof(freebob_sample_t),towrite,towrite/sizeof(freebob_sample_t));
				return -1;
			}
		break;
		case IEC61883_STREAM_TYPE_MIDI: // no prefill nescessary
		case IEC61883_STREAM_TYPE_SPDIF: // unsupported
		default:
		break;
	}
	return 0;
}
