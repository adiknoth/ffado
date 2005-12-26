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

/* freebob_connections.c
 *
 * Connection handling
 *
 */
#include <libraw1394/raw1394.h>
#include <libiec61883/iec61883.h>

#include "freebob_streaming.h"
#include "freebob_streaming_private.h"
#include "freebob_connections.h"
#include "freebob_streams.h"
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
	
	// reset the event counter
	connection->status.events=0;
	
	// reset the boundary counter
	connection->status.frames_left = 0;
	
	// reset the xrun counter
	connection->status.xruns = 0;
	
	printExit();
	
	return 0;
	
}

int freebob_streaming_init_connection(freebob_device_t * dev, freebob_connection_t *connection) {
	int err=0;
	int s=0;
	
	connection->status.frames_left=0;
					
	// create raw1394 handles
	connection->raw_handle=freebob_open_raw1394(connection->spec.port);
	if (!connection->raw_handle) {
		printError("Could not get raw1394 handle\n");
		return -ENOMEM;
	}
	raw1394_set_userdata(connection->raw_handle, (void *)connection);
	
	// these have quite some influence on latency
	connection->iso.buffers = dev->options.iso_buffers;
	connection->iso.prebuffers = dev->options.iso_prebuffers;
	connection->iso.irq_interval = dev->options.iso_irq_interval;
	
	connection->iso.speed = RAW1394_ISO_SPEED_400;
	
	connection->iso.bandwidth=-1;
	connection->iso.startcycle=-1;
	/*
	connection->status.packet_info_table_size=dev->options.table_size;
	
 	connection->status.packet_info_table=calloc(connection->status.packet_info_table_size,sizeof(packet_info_t));
 	if(!connection->status.packet_info_table) {
		printError("FREEBOB: Could not allocate dma buffer memory for connection_info->packet_info_table");
		return -ENOMEM;
	}
	debugPrintWithTimeStamp(DEBUG_LEVEL_BUFFERS, "Allocated packet table of %d bytes\n",connection->status.packet_info_table_size*sizeof(packet_info_t));
	*/
	/*
	new_trigger_value=2*(period_size%(sample_rate*connection->table_size/8000));
	
	if (new_trigger_value>client->trigger) {
		client->trigger=new_trigger_value;
	}		
	debugPrint(DEBUG_LEVEL_STARTUP,"Trigger set to %d\n",client->trigger);
	
	*/
	
	// init the streams present
	/*
	 * init the streams this connection is composed of
	 */
	assert(connection->spec.stream_info);
	connection->nb_streams=connection->spec.stream_info->nb_streams;
	
	connection->streams=calloc(connection->nb_streams, sizeof(freebob_stream_t));
	
	if(!connection->streams) {
		printError("Could not allocate memory for streams");
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
// 		free(connection->status.packet_info_table);
		
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

	memcpy(&dst->spec,src,sizeof(freebob_stream_spec_t));
	
	// allocate the ringbuffer
	// the lock free ringbuffer needs one extra frame 
	// it can only be filled to size-1 bytes
	int buffer_size_frames=dev->options.nb_buffers*dev->options.period_size+1;
	dst->buffer=freebob_ringbuffer_create(buffer_size_frames*sizeof(freebob_sample_t));
	
	// no other init needed
	return 0;
	
}

/**
  * destroys a stream_t 
  */
void freebob_streaming_cleanup_stream(freebob_device_t* dev, freebob_stream_t *dst) {
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
	
int 
freebob_decode_events_to_stream(freebob_connection_t *connection,
								freebob_stream_t *stream, 
								quadlet_t* events, 
								unsigned int nsamples,
								unsigned int dbc
								) {
	quadlet_t *target_event;
	
	// temporary buffer
	freebob_sample_t buff[nsamples];
	freebob_sample_t *buffer=buff;
	
	unsigned int j=0;
	int written=0;
	
	assert (stream);
	assert (stream->spec.position < connection->spec.dimension);

	if (stream->spec.format== IEC61883_STREAM_TYPE_MBLA) {
		target_event=(quadlet_t *)(events + stream->spec.position);
			
		for(j = 0; j < nsamples; j += 1) { // decode max nsamples
			*(buffer)=(ntohl((*target_event) ) & 0x00FFFFFF);
//			fprintf(stderr,"[%03d, %02d: %08p %08X %08X]\n",j, stream->spec.position, target_event, *target_event, *buffer);
			buffer++;
			target_event+=connection->spec.dimension;
		}

// 		fprintf(stderr,"rb write [%02d: %08p %08p]\n",stream->spec.position, stream, stream->buffer);
	
// 	fprintf(stderr,"rb write [%02d: %08p %08p %08X, %d, %d]\n",stream->spec.position, stream, stream->buffer, *buffer, nsamples, written);
		
		written=freebob_ringbuffer_write(stream->buffer, (char *)buff, nsamples*sizeof(freebob_sample_t));
		
// 	fprintf(stderr,"rb write1[%02d: %08p %08p %08X, %d, %d]\n",stream->spec.position, stream, stream->buffer, *buffer, nsamples, written);
		
		return written/sizeof(freebob_sample_t);
	
	} else if (stream->spec.format == IEC61883_STREAM_TYPE_MIDI) {
		/* idea:
		spec says: current_midi_port=(dbc+j)%8;
		assume dbc=0, if we keep the nframes a multiple of 8
		=> if we start at (stream->location-1) [due to location_min=1], 
			we'll start at the right event for the midi port.
		=> if we increment j with 8, we stay at the right event.
		*/
		
/*		for(j = stream->spec.location-1; j < nsamples; j += 8) {
			target_event=(quadlet_t *)(events + ((j * connection->spec.dimension) + stream->spec.position));
			
			if(IEC61883_AM824_GET_LABEL(*target_event) != IEC61883_AM824_LABEL_MIDI_NO_DATA) {
				*(((quadlet_t *)buffer) + written)=ntohl(*target_event);

				written++;

			}
		}
		
		freebob_ringbuffer_write(stream->buffer, (char *)buffer, written*sizeof(freebob_sample_t));
	*/	
		return nsamples; // for midi streams we always indicate a full write
	
	} else {//if (stream->spec.format == IEC61883_STREAM_TYPE_SPDIF) {
		return nsamples; // for unsupported we always indicate a full read
	
	}
	
	return 0;


}

int 
freebob_encode_stream_to_events(freebob_connection_t *connection,
								freebob_stream_t *stream, 
								quadlet_t* events, 
								unsigned int nsamples,
								unsigned int dbc
								) {
	quadlet_t *target_event;
	freebob_sample_t buff[nsamples];
	freebob_sample_t *buffer=buff;
	unsigned int j=0;
	unsigned int read=0;
	
	assert (stream);
	assert (stream->spec.position < connection->spec.dimension);
	
	if(stream->spec.format == IEC61883_STREAM_TYPE_MBLA) { // MBLA
		target_event=(quadlet_t *)(events + (stream->spec.position));
		
		read=freebob_ringbuffer_read(stream->buffer, (char *)buffer, nsamples*sizeof(freebob_sample_t))/sizeof(freebob_sample_t);
		
		for(j = 0; j < nsamples; j+=1) {
			*target_event = htonl((*(buffer) & 0x00FFFFFF) | 0x40000000);
			buffer++;
			target_event+=connection->spec.dimension;
		}
		return read;
		
	} else if (stream->spec.format == IEC61883_STREAM_TYPE_MIDI) {
	/*	int base=stream->spec.location-1;
		// due to the mux of the midi channel
		int max_midi_events=nsamples/8;
		
		read=freebob_ringbuffer_read(stream->buffer, (char *)buffer, max_midi_events*sizeof(freebob_sample_t))/sizeof(freebob_sample_t);

		for(j = 0; j < read; j += 1) {
			quadlet_t tmp;
			target_event=(quadlet_t *)(events + ((base + j * 8 * connection->spec.dimension) + stream->spec.position));
			tmp=*(buffer+j) | 0x81000000; // only MIDI X1
			*target_event=htonl(tmp);
		}
		
		for(; j < max_midi_events; j += 1) {
			quadlet_t tmp;
			target_event=(quadlet_t *)(events + ((base + j * 8 * connection->spec.dimension) + stream->spec.position));
			tmp=0x80000000; // MIDI NO-DATA
			*target_event=htonl(tmp);
		}
		*/
		return nsamples; // for midi we always indicate a full read
	
	} else { //if (stream->spec.format == IEC61883_STREAM_TYPE_SPDIF) {
		return nsamples; // for unsupported we always indicate a full read
	
	}
	return 0;

}
