/* $Id$ */

/*
 *   FreeBoB Streaming API
 *   FreeBoB = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2005 Pieter Palmers <pieterpalmers@users.sourceforge.net>
 *
 *   This program is free software {} you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation {} either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY {} without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program {} if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * 
 *
 */

/* freebob_streaming.c
 *
 * Implementation of the FreeBoB Streaming API
 *
 */

#include "libfreebob/freebob_streaming.h"
#include "freebob_streaming_private.h"
#include "freebob_connections.h"
#include "freebob_debug.h"
#include "thread.h"
#include "messagebuffer.h"

#include <signal.h>
#include <unistd.h>

/**
 * Callbacks
 */

static enum raw1394_iso_disposition 
iso_slave_receive_handler(raw1394handle_t handle, unsigned char *data, 
        unsigned int length, unsigned char channel,
        unsigned char tag, unsigned char sy, unsigned int cycle, 
        unsigned int dropped);
        
static enum raw1394_iso_disposition 
iso_master_receive_handler(raw1394handle_t handle, unsigned char *data, 
        unsigned int length, unsigned char channel,
        unsigned char tag, unsigned char sy, unsigned int cycle, 
        unsigned int dropped);
        
static enum raw1394_iso_disposition 
iso_slave_transmit_handler(raw1394handle_t handle,
		unsigned char *data, unsigned int *length,
		unsigned char *tag, unsigned char *sy,
		int cycle, unsigned int dropped);
		
static enum raw1394_iso_disposition 
iso_master_transmit_handler(raw1394handle_t handle,
		unsigned char *data, unsigned int *length,
		unsigned char *tag, unsigned char *sy,
		int cycle, unsigned int dropped);
			
static int freebob_am824_recv(char *data, 
							  int nevents, unsigned int offset, unsigned int dbc,
							  freebob_connection_t *connection);

static int freebob_am824_xmit(char *data, 
							  int nevents, unsigned int offset, unsigned int dbc,
							  freebob_connection_t *connection);
			       
int freebob_streaming_reset_playback_streams(freebob_device_t *dev);

int g_verbose=0;

freebob_device_t *freebob_streaming_init (freebob_device_info_t *device_info, freebob_options_t options) {
	int i;
	int c;
	int err=0;
	freebob_device_t* dev=NULL;
	int discover_verbose=0;
	
	freebob_messagebuffer_init();

	g_verbose=options.verbose;

	if(g_verbose) discover_verbose=5;

	assert(device_info);
	
	printMessage("FreeBoB Streaming Device Init\n");
	printMessage(" Using FreeBoB lib version %s\n",freebob_get_version());
	printMessage(" Device information:\n");
	
	printMessage(" Device options:\n");
	/* driver related setup */
	printMessage("  Port                     : %d\n",options.port);
	printMessage("  Device Node Id           : %d\n",options.node_id);
	printMessage("  Samplerate               : %d\n",options.sample_rate);
	printMessage("  Period Size              : %d\n",options.period_size);
	printMessage("  Nb Buffers               : %d\n",options.nb_buffers);
	printMessage("  Directions               : %X\n",options.directions);
	
	// initialize the freebob_device
	// allocate memory
	dev=calloc(1,sizeof(freebob_device_t));
	
	if(!dev) {
		printError("cannot allocate memory for dev structure!\n");
		return NULL;
	}
	
	// clear the device structure
	memset(dev,0,sizeof(freebob_device_t));
	
	// copy the arguments to the device structure
	memcpy(&dev->device_info, device_info, sizeof(freebob_device_info_t));
	memcpy(&dev->options, &options, sizeof(freebob_options_t));
	
	// read in the device specification
	
	/* load the connections*/
	/* 
	 * This info should be provided by the Freebob CML application
	 *
	 */
	freebob_connection_info_t *libfreebob_capture_connections=NULL;
	freebob_connection_info_t *libfreebob_playback_connections=NULL;

	dev->fb_handle = freebob_new_handle(options.port);
	if (!dev->fb_handle) {
		free(dev);
		printError("cannot create libfreebob handle\n");
		return NULL;
	}

	if (freebob_discover_devices(dev->fb_handle, discover_verbose)!=0) {
		freebob_destroy_handle(dev->fb_handle);
		free(dev);
		printError("device discovering failed\n");
		return NULL;
	}

	/* Try and set the samplerate
 	 * This should be done after device discovery
	 * but before reading the bus description as the device capabilities can change
	 */

    if(options.node_id > -1) {
        if (freebob_set_samplerate(dev->fb_handle, options.node_id, options.sample_rate) != 0) {
            freebob_destroy_handle(dev->fb_handle);
            free(dev);
            printError("Failed to set samplerate...\n");
            return NULL;
        }

	} else {
	    int devices_on_bus = freebob_get_nb_devices_on_bus(dev->fb_handle);
	    debugPrint(DEBUG_LEVEL_STARTUP,"port = %d, devices_on_bus = %d\n", options.port, devices_on_bus);
			
	    for(i=0;i<devices_on_bus;i++) {
		int node_id=freebob_get_device_node_id(dev->fb_handle, i);
		debugPrint(DEBUG_LEVEL_STARTUP,"set samplerate for device = %d, node = %d\n", i, node_id);
				
		if (freebob_set_samplerate(dev->fb_handle, node_id, options.sample_rate) != 0) {
			freebob_destroy_handle(dev->fb_handle);
			free(dev);
			printError("Failed to set samplerate...\n");
			return NULL;
		}
	    }

	}

	/* Read the connection specification
 	 */
 
	if(!(options.directions & FREEBOB_IGNORE_CAPTURE)) {
		libfreebob_capture_connections=freebob_get_connection_info(dev->fb_handle, options.node_id, 0);
	}

	if(!(options.directions & FREEBOB_IGNORE_PLAYBACK)) {
		libfreebob_playback_connections=freebob_get_connection_info(dev->fb_handle, options.node_id, 1);
	}
 

	if (libfreebob_capture_connections) {
		dev->nb_connections_capture=libfreebob_capture_connections->nb_connections;
		// FIXME:
		//dev->nb_connections_capture=0;
	} else {
		dev->nb_connections_capture=0;
	}	
	
	if (libfreebob_playback_connections) {
		dev->nb_connections_playback=libfreebob_playback_connections->nb_connections;
		// FIXME:
// 		dev->nb_connections_playback=0;
	} else {
		dev->nb_connections_playback=0;
	}
	// FIXME: temporary disable the playback
// 	dev->nb_connections_playback=0;
	
	dev->nb_connections=dev->nb_connections_playback+dev->nb_connections_capture;
	/* see if there are any connections */
	if (!dev->nb_connections) {
		printError("No connections specified, bailing out\n");
		if(libfreebob_capture_connections) { free(libfreebob_capture_connections); libfreebob_capture_connections=NULL; }
		if(libfreebob_playback_connections) { free(libfreebob_playback_connections); libfreebob_playback_connections=NULL; }
		freebob_destroy_handle(dev->fb_handle);
		free(dev);
		
		return NULL;
	}
	
	dev->connections=calloc(dev->nb_connections_playback+dev->nb_connections_capture, sizeof(freebob_connection_t));
	
	
	for (c=0;c<dev->nb_connections_capture;c++) {
		memcpy(&dev->connections[c].spec, libfreebob_capture_connections->connections[c], sizeof(freebob_connection_spec_t));
		dev->connections[c].spec.direction=FREEBOB_CAPTURE;
	}
	for (c=0;c<dev->nb_connections_playback;c++) {
		memcpy(&dev->connections[c+dev->nb_connections_capture].spec, libfreebob_playback_connections->connections[c], sizeof(freebob_connection_spec_t));
		dev->connections[c+dev->nb_connections_capture].spec.direction=FREEBOB_PLAYBACK;
	}
	
	/* Figure out a master connection.
	 * Either it is given in the spec libfreebob
	 * Or it is the first connection defined (capture connections first)
	 */
	int master_found=FALSE;
	
	for (c=0;c<dev->nb_connections_capture+dev->nb_connections_playback;c++) {
		if (dev->connections[c].spec.is_master==TRUE) {
			master_found=TRUE;
			if(dev->options.sample_rate<0) {
				dev->options.sample_rate=dev->connections[c].spec.samplerate;
			}
			break;
		}
	}
	
	if((!master_found) && (dev->nb_connections_capture+dev->nb_connections_playback > 0)) {
		dev->connections[0].spec.is_master=TRUE;
		if(dev->options.sample_rate<0) {
			dev->options.sample_rate=dev->connections[0].spec.samplerate;
		}
	}

	// initialize all connections
	for(i=0; i < dev->nb_connections; i++) {
		freebob_connection_t *connection= &(dev->connections[i]);
		if ((err=freebob_streaming_init_connection(dev, connection))<0) {
			printError("failed to init connection %d\n",i);
			break;
		}
	}
	
	if(libfreebob_capture_connections) { freebob_free_connection_info(libfreebob_capture_connections); libfreebob_capture_connections=NULL; }
	if(libfreebob_playback_connections) { freebob_free_connection_info(libfreebob_playback_connections); libfreebob_playback_connections=NULL; }
	
	// prepare the FD map	
	// the FD map is not stricly nescessary due to dev->fdmap[i]=dev->connections[i];
	// but makes things flexible at the moment
	
	assert(dev->nb_connections==dev->nb_connections_capture+dev->nb_connections_playback);
	
	dev->nfds   = dev->nb_connections;
	dev->pfds   = malloc (sizeof (struct pollfd) * dev->nfds);
	dev->fdmap  = malloc (sizeof (freebob_connection_t *) * dev->nfds); // holds the connection idx of this fd
	
	for(i=0; i < dev->nb_connections; i++) {
		freebob_connection_t *connection= &(dev->connections[i]);
		dev->fdmap[i]=connection;
		dev->pfds[i].fd = raw1394_get_fd (connection->raw_handle);
		dev->pfds[i].events = POLLIN;
		connection->pfd=&dev->pfds[i];
	}
	
	// check for errors
	if(err) {
		debugPrint(DEBUG_LEVEL_STARTUP, "Cleaning up connections\n");
		
		// the connection that failed doesn't have to be cleaned
		i-=1;
		
		for(; i >= 0; i--) {
			freebob_connection_t *connection= &(dev->connections[i]);
			debugPrint(DEBUG_LEVEL_STARTUP, "Cleaning up connection %d\n",i);
			if ((freebob_streaming_cleanup_connection(dev, connection))<0) {
				printError( "Failed to clean connection %d\n",i);
			}
		}
		free(dev->pfds);
		free(dev->fdmap);
		free(dev->connections);
		free(dev);
		return NULL;
	}
	
	freebob_destroy_handle(dev->fb_handle);
	
	return dev;

}

void freebob_streaming_finish(freebob_device_t *dev) {
	debugPrint(DEBUG_LEVEL_STARTUP,"Cleaning up connections\n");
	unsigned int i;
	int err=0;
	
	// cleanup all connections
	for(i=0; i < dev->nb_connections; i++) {
		freebob_connection_t *connection= &(dev->connections[i]);
		if ((err=freebob_streaming_cleanup_connection(dev, connection))<0) {
			printError("Failed to cleanup connection %d\n",i);
			return;
		}
	}
	
	// free stream structures
	if(dev->capture_streams) {
		free(dev->capture_streams);
	}
	if(dev->playback_streams) {
		free(dev->playback_streams);
	}
	if(dev->synced_capture_streams) {
		free(dev->synced_capture_streams);
	}
	if(dev->synced_playback_streams) {
		free(dev->synced_playback_streams);
	}
	
	// cleanup data structures
	free(dev->pfds);
	free(dev->fdmap);
	free(dev->connections);
	free(dev);
	
	freebob_messagebuffer_exit();
	
	return;
}

int freebob_streaming_get_nb_capture_streams(freebob_device_t *dev) {
	return dev->nb_capture_streams;
}

int freebob_streaming_get_nb_playback_streams(freebob_device_t *dev) {
	return dev->nb_playback_streams;
}

int freebob_streaming_get_capture_stream_name(freebob_device_t *dev, int i, char* buffer, size_t buffersize) {
	freebob_stream_t *stream;
	if(i<dev->nb_capture_streams) {
		stream=*(dev->capture_streams+i);
/*		return snprintf (buffer, buffersize, "%s_%d_%d_%d_%s",
		                 "cap", (int) stream->parent->spec.port, 
		                 stream->parent->spec.node & 0x3F, 
		                 stream->parent->spec.plug, stream->spec.name);
*/
		return snprintf (buffer, buffersize, "dev%d%s_%s",
		                 stream->parent->spec.id , "c", stream->spec.name);
	} else {
		return -1;
	}
}

int freebob_streaming_get_playback_stream_name(freebob_device_t *dev, int i, char* buffer, size_t buffersize) {
	freebob_stream_t *stream;
	if(i<dev->nb_playback_streams) {
		stream=*(dev->playback_streams+i);
/*		return snprintf (buffer, buffersize, "%s_%d_%d_%d_%s",
			         "pbk", (int) stream->parent->spec.port, 
		                 stream->parent->spec.node & 0x3F, 
		                 stream->parent->spec.plug, stream->spec.name);
*/
		return snprintf (buffer, buffersize, "dev%d%s_%s",
		                 stream->parent->spec.id, "p", stream->spec.name);
	} else {
		return -1;
	}
}

freebob_streaming_stream_type freebob_streaming_get_capture_stream_type(freebob_device_t *dev, int i) {
	freebob_stream_t *stream;
	if(i<dev->nb_capture_streams) {
		stream=*(dev->capture_streams+i);
		switch (stream->spec.format) {
		case IEC61883_STREAM_TYPE_MBLA:
			return freebob_stream_type_audio;
		case IEC61883_STREAM_TYPE_MIDI:
			return freebob_stream_type_midi;
		case IEC61883_STREAM_TYPE_SPDIF:
		default:
			return freebob_stream_type_unknown;
		}
	} else {
		return freebob_stream_type_invalid;
	}
}

freebob_streaming_stream_type freebob_streaming_get_playback_stream_type(freebob_device_t *dev, int i) {
	freebob_stream_t *stream;
	if(i<dev->nb_playback_streams) {
		stream=*(dev->playback_streams+i);
		switch (stream->spec.format) {
		case IEC61883_STREAM_TYPE_MBLA:
			return freebob_stream_type_audio;
		case IEC61883_STREAM_TYPE_MIDI:
			return freebob_stream_type_midi;
		case IEC61883_STREAM_TYPE_SPDIF:
		default:
			return freebob_stream_type_unknown;
		}
	} else {
		return freebob_stream_type_invalid;
	}
}

int freebob_streaming_set_stream_buffer(freebob_device_t *dev,  freebob_stream_t *dst, char *b, freebob_streaming_buffer_type t) {
	assert(dst);
	
	// free the preallocated buffer first
	freebob_streaming_free_stream_buffer(dev,dst);
	
	switch (t) {
		case freebob_buffer_type_per_stream:
			dst->user_buffer=calloc(dev->options.period_size,sizeof(freebob_sample_t));
			dst->buffer_type=freebob_buffer_type_per_stream;
			break;
		
		case freebob_buffer_type_uint24:
			if(b) {
				dst->buffer_type=t;
				dst->user_buffer=b;
			} else {
				// allocate a default type
				dst->buffer_type=freebob_buffer_type_per_stream;
				dst->user_buffer=calloc(dev->options.period_size,sizeof(freebob_sample_t));
				return -1;
			}
			break;
			
		case freebob_buffer_type_float:
			if(b) {
				dst->buffer_type=t;
				dst->user_buffer=b;
			} else {
				// allocate a default type
				dst->buffer_type=freebob_buffer_type_per_stream;
				dst->user_buffer=calloc(dev->options.period_size,sizeof(freebob_sample_t));
				return -1;
			}
			break;
		
		default:
			// allocate a default type
			dst->buffer_type=freebob_buffer_type_per_stream;
			dst->user_buffer=calloc(dev->options.period_size,sizeof(freebob_sample_t));
			return -1;
			
	}
	return 0;
}

void freebob_streaming_free_stream_buffer(freebob_device_t* dev, freebob_stream_t *dst)
{
	
	if((dst->buffer_type==freebob_buffer_type_per_stream) && (dst->user_buffer)) {
		free(dst->user_buffer);
		dst->user_buffer=NULL;
	}
}


int freebob_streaming_set_capture_stream_buffer(freebob_device_t *dev, int i, char *buff,  freebob_streaming_buffer_type t) {
	
	freebob_stream_t *stream;
	if(i<dev->nb_capture_streams) {
		stream=*(dev->capture_streams+i);
		assert(stream);
		return freebob_streaming_set_stream_buffer(dev,  stream, buff, t);
		
	} else {
		return -1;
	}
	
}

int freebob_streaming_set_playback_stream_buffer(freebob_device_t *dev, int i, char *buff,  freebob_streaming_buffer_type t) {
	freebob_stream_t *stream;
	if(i<dev->nb_playback_streams) {
		
		stream=*(dev->playback_streams+i);
		
		assert(stream);
		return freebob_streaming_set_stream_buffer(dev, stream, buff, t);
		
	} else {
		return -1;
	}
}

int freebob_streaming_start_thread(freebob_device_t *dev) {
	int err;
	// start the packetizer thread
	// init the packetizer thread communication semaphores
	if((err=sem_init(&dev->packetizer.transfer_boundary, 0, 0))) {
		printError( "Cannot init packet transfer semaphore\n");
		return err;
	} else {
		debugPrint(DEBUG_LEVEL_STARTUP,"FREEBOB: successfull init of packet transfer semaphore\n");
	}
	
	dev->packetizer.priority=dev->options.packetizer_priority;
	dev->packetizer.realtime=dev->options.realtime;
	
	// start watchdog
	if (dev->packetizer.realtime) {
 		freebob_streaming_start_watchdog(dev);
	}
	
	// start packetizer thread
	dev->packetizer.run=1;
	
	if (freebob_streaming_create_thread(dev, &dev->packetizer.transfer_thread, dev->packetizer.priority, dev->packetizer.realtime, freebob_iso_packet_iterator, (void *)dev)) {
		printError("cannot create packet transfer thread");
		return -1;
	} else {
		debugPrint(DEBUG_LEVEL_STARTUP,"Created packet transfer thread\n");
	}
	return 0;
}

int freebob_streaming_stop_thread(freebob_device_t *dev) {
	void *status;
	
	debugPrint(DEBUG_LEVEL_STARTUP," Stopping packetizer thread...\n");
	dev->packetizer.run=0;
	
	
	//sem_post(&dev->packetizer.transfer_ack);
	
	pthread_join(dev->packetizer.transfer_thread, &status);
	
	// stop watchdog
	if (dev->packetizer.realtime) {
 		freebob_streaming_stop_watchdog(dev);
	}
	
	// cleanup semaphores
	sem_destroy(&dev->packetizer.transfer_boundary);

	return 0;
}


int freebob_streaming_start(freebob_device_t *dev) {
	int err;
	int i;
	
	for(i=0; i < dev->nb_connections; i++) {
		err=0;
		freebob_connection_t *connection= &(dev->connections[i]);

		int fdf, syt_interval;

		int samplerate=dev->options.sample_rate;

		switch (samplerate) {
		case 32000:
			syt_interval = 8;
			fdf = IEC61883_FDF_SFC_32KHZ;
			break;
		case 44100:
			syt_interval = 8;
			fdf = IEC61883_FDF_SFC_44K1HZ;
			break;
		default:
		case 48000:
			syt_interval = 8;
			fdf = IEC61883_FDF_SFC_48KHZ;
			break;
		case 88200:
			syt_interval = 16;
			fdf = IEC61883_FDF_SFC_88K2HZ;
			break;
		case 96000:
			syt_interval = 16;
			fdf = IEC61883_FDF_SFC_96KHZ;
			break;
		case 176400:
			syt_interval = 32;
			fdf = IEC61883_FDF_SFC_176K4HZ;
			break;
		case 192000:
			syt_interval = 32;
			fdf = IEC61883_FDF_SFC_192KHZ;
			break;
		}
		
		if(dev->options.period_size < syt_interval) {
			printError("Period size (%d) too small! Samplerate %d requires period >= %d\n",
			           dev->options.period_size, samplerate, syt_interval);
			return -1;
		}
		connection->iso.packets_per_period = dev->options.period_size/syt_interval;

		//connection->plug=0;
		connection->iso.hostplug=-1;
		
		// create the CCM PTP connection
		connection->iso.do_disconnect=0;

		// start receiving
		switch (connection->spec.direction) {
		case FREEBOB_CAPTURE:
		
			debugPrint(DEBUG_LEVEL_STARTUP," creating capture connections...\n");
			if (connection->spec.iso_channel < 0) { 
				// we need to make the connection ourself
				debugPrint(DEBUG_LEVEL_STARTUP, "Executing CMP procedure...\n");
				
				connection->iso.iso_channel = iec61883_cmp_connect(
					connection->raw_handle, 
					connection->spec.node | 0xffc0, 
					&connection->spec.plug,
					raw1394_get_local_id (connection->raw_handle), 
					&connection->iso.hostplug, 
					&connection->iso.bandwidth);
					
				connection->iso.do_disconnect=1;
				
			} else {
				connection->iso.iso_channel=connection->spec.iso_channel;
			}
			
			if (connection->spec.is_master) { //master connection
				dev->sync_master_connection=connection;
				connection->status.master=NULL;
				
			}

			// setup the optimal parameters for the raw1394 ISO buffering
			connection->iso.packets_per_period=dev->options.period_size/syt_interval;
			// hardware interrupts occur when one DMA block is full, and the size of one DMA
			// block = PAGE_SIZE. Setting the max_packet_size makes sure that the HW irq 
			// occurs at a period boundary (optimal CPU use)
			// note: try and use 2 interrupts per period for better latency.
			connection->iso.max_packet_size=getpagesize() / connection->iso.packets_per_period * 2;
			connection->iso.irq_interval=connection->iso.packets_per_period/2;

			connection->iso.packet_size=4 * (2 + syt_interval * connection->spec.dimension);

			if (connection->iso.max_packet_size < connection->iso.packet_size) {
				connection->iso.max_packet_size=connection->iso.packet_size;
			}

			/* the receive buffer size doesn't matter for the latency,
			   but it has a minimal value in order for libraw to operate correctly (300) */
			connection->iso.buffers=400;

			// this is a hack
			if(dev->options.period_size < 128) {
				connection->iso.receive_mode=RAW1394_DMA_PACKET_PER_BUFFER;
			} else {
				connection->iso.receive_mode=RAW1394_DMA_BUFFERFILL;
			}

		break;
		case FREEBOB_PLAYBACK:
			debugPrint(DEBUG_LEVEL_STARTUP," creating playback connections...\n");
		
			if (connection->spec.iso_channel < 0) { // we need to make the connection ourself
				debugPrint(DEBUG_LEVEL_STARTUP, "Executing CMP procedure...\n");
				connection->iso.iso_channel = iec61883_cmp_connect(
				connection->raw_handle, 
				raw1394_get_local_id (connection->raw_handle), 
				&connection->iso.hostplug, 
				connection->spec.node | 0xffc0, 
				&connection->spec.plug, 
				&connection->iso.bandwidth);
				
				connection->iso.do_disconnect=1;
			} else {
				connection->iso.iso_channel=connection->spec.iso_channel;
			}

			if (connection->spec.is_master) { // master connection
				dev->sync_master_connection=connection;
				connection->status.master=NULL;
			}

			iec61883_cip_init (
				&connection->status.cip, 
				IEC61883_FMT_AMDTP, 
				fdf,
				samplerate, 
				connection->spec.dimension, 
				syt_interval);
				
			iec61883_cip_set_transmission_mode (
				&connection->status.cip, 
				IEC61883_MODE_BLOCKING_EMPTY);


			// setup the optimal parameters for the raw1394 ISO buffering
			connection->iso.packets_per_period=dev->options.period_size/syt_interval;
			// hardware interrupts occur when one DMA block is full, and the size of one DMA
			// block = PAGE_SIZE. Setting the max_packet_size makes sure that the HW irq is 
			// occurs at a period boundary (optimal CPU use)
			// note: try and use 2 interrupts per period for better latency.
			connection->iso.max_packet_size=getpagesize() / connection->iso.packets_per_period * 2;
			connection->iso.irq_interval=connection->iso.packets_per_period / 2;

			connection->iso.packet_size=4 * (2 + syt_interval * connection->spec.dimension);

			if (connection->iso.max_packet_size < connection->iso.packet_size) {
				connection->iso.max_packet_size=connection->iso.packet_size;
			}

			/* the transmit buffer size should be as low as possible for latency. 
			*/
			connection->iso.buffers=connection->iso.packets_per_period;
			if (connection->iso.buffers<10) connection->iso.buffers=10;

		break;
		}
		
		// reset the connection, such that the buffers are empty
		freebob_streaming_reset_connection(dev, connection);

		if(connection->iso.iso_channel<0) {
			printError("Could not do CMP for connection %d\n",i);
			i-=1;
			while(i>=0) {
				connection= &(dev->connections[i]);
		
				if (connection->iso.do_disconnect) {
					// destroy the CCM PTP connection
					switch (connection->spec.direction) {
					case FREEBOB_CAPTURE:
						iec61883_cmp_disconnect(
							connection->raw_handle, 
							connection->spec.node | 0xffc0, 
							connection->spec.plug, 
							raw1394_get_local_id (connection->raw_handle), 
							connection->iso.hostplug, 
							connection->iso.iso_channel, 
							connection->iso.bandwidth);
						
					break;
					case FREEBOB_PLAYBACK:
						iec61883_cmp_disconnect(
							connection->raw_handle, 
							raw1394_get_local_id (connection->raw_handle), 
							connection->iso.hostplug, 
							connection->spec.node | 0xffc0, 
							connection->spec.plug, 
							connection->iso.iso_channel, 
							connection->iso.bandwidth);
			
					break;
					}
				}
				i--;
			}
			return -1;
		}
	}
	
	/* update the sync master pointer for all connections */
	debugPrint(DEBUG_LEVEL_STARTUP,"Connection summary:\n");
	int sync_masters_present=0;
	
	for(i=0; i < dev->nb_connections; i++) {
		err=0;
		
		freebob_connection_t *connection= &(dev->connections[i]);
		
		if (connection->spec.direction == FREEBOB_CAPTURE) {
			debugPrint(DEBUG_LEVEL_STARTUP,"  Capture : from node %02X.%02d to node %02X.%02d on channel %02d  {%p} ",
				connection->spec.node & ~0xffc0, connection->spec.plug,
				raw1394_get_local_id (connection->raw_handle) & ~0xffc0, connection->iso.hostplug,
				connection->iso.iso_channel, connection);
		} else if (connection->spec.direction == FREEBOB_PLAYBACK) {
			
			debugPrint(DEBUG_LEVEL_STARTUP,"  Playback: from node %02X.%02d to node %02X.%02d on channel %02d  {%p} ",
				raw1394_get_local_id (connection->raw_handle) & ~0xffc0, connection->iso.hostplug,
				connection->spec.node & ~0xffc0, connection->spec.plug,
				connection->iso.iso_channel, connection);
		}
		
		if (connection->spec.is_master) {
			sync_masters_present++;
			
			debugPrintShort(DEBUG_LEVEL_STARTUP," [MASTER]");	
		} else {
			assert(dev->sync_master_connection);
			connection->status.master=&dev->sync_master_connection->status;
			
			debugPrintShort(DEBUG_LEVEL_STARTUP," [SLAVE]");
		}
		debugPrintShort(DEBUG_LEVEL_STARTUP,"\n");
	}
	
	if (sync_masters_present == 0) {
		printError("no sync master connection present!\n");
		// TODO: cleanup
		//freebob_streaming_stop(driver);
		return -1;
	} else if (sync_masters_present > 1) {
		printError("too many sync master connections present! (%d)\n",sync_masters_present);
		// TODO: cleanup
		//freebob_streaming_stop(driver);
		return -1;
	}
	
	// reset the playback streams
	if((err=freebob_streaming_reset_playback_streams(dev))<0) {
 		// TODO: cleanup
		printError("Could not reset playback streams.\n");
		return err;
	}

	// put nb_periods*period_size of null frames into the playback buffers
	if((err=freebob_streaming_prefill_playback_streams(dev))<0) {
		// TODO: cleanup
		printError("Could not prefill playback streams.\n");
		return err;
	}
		
	// we should transfer nb_buffers-1 periods of playback from the stream buffers to the event buffer
	for (i=0;i<dev->options.nb_buffers;i++) {
		freebob_streaming_transfer_playback_buffers(dev);
	}

	freebob_streaming_print_bufferfill(dev);
		
	debugPrint(DEBUG_LEVEL_STARTUP,"Armed...\n");
	
	freebob_streaming_start_thread(dev);

	return 0;

}

int freebob_streaming_stop(freebob_device_t *dev) {
	unsigned int i;
	

	freebob_streaming_stop_thread(dev);
	
	// stop ISO xmit/receive
	for(i=0; i < dev->nb_connections; i++) {
		freebob_connection_t *connection= &(dev->connections[i]);

		if (connection->iso.do_disconnect) {
			// destroy the CCM PTP connection
			switch (connection->spec.direction) {
			case FREEBOB_CAPTURE:
				iec61883_cmp_disconnect(
					connection->raw_handle, 
					connection->spec.node | 0xffc0, 
					connection->spec.plug, 
					raw1394_get_local_id (connection->raw_handle), 
					connection->iso.hostplug, 
					connection->iso.iso_channel, 
					connection->iso.bandwidth);
				
			break;
			case FREEBOB_PLAYBACK:
				iec61883_cmp_disconnect(
					connection->raw_handle, 
					raw1394_get_local_id (connection->raw_handle), 
					connection->iso.hostplug, 
					connection->spec.node | 0xffc0, 
					connection->spec.plug, 
					connection->iso.iso_channel, 
					connection->iso.bandwidth);
	
			break;
			}
		}
		
	}
	return 0;
}

void freebob_streaming_print_bufferfill(freebob_device_t *dev) {
#ifdef DEBUG	
    int i=0;
	for(i=0; i < dev->nb_connections; i++) {
 		freebob_connection_t *connection= &(dev->connections[i]);
		debugPrint(DEBUG_LEVEL_XRUN_RECOVERY, "%i: %d\n",i,freebob_ringbuffer_read_space(connection->event_buffer));
	}
#endif
}

int freebob_streaming_prefill_playback_streams(freebob_device_t *dev) {
	int i;
	int err=0;
	
	for(i=0; i < dev->nb_playback_streams; i++) {
		freebob_stream_t *stream;

		
		stream=*(dev->playback_streams+i);
		
		assert(stream);
		err=freebob_streaming_prefill_stream(dev, stream);
		if (err) {
			printError("Could not prefill stream %d\n",i);
			return -1;
		}
	}
	return 0;
	
}

int freebob_streaming_reset_playback_streams(freebob_device_t *dev) {
	int i;
	int err=0;
	
	for(i=0; i < dev->nb_playback_streams; i++) {
		freebob_stream_t *stream;

		stream=*(dev->playback_streams+i);
		
		assert(stream);
		err=freebob_streaming_reset_stream(dev, stream);
		if (err) {
			printError("Could not reset stream %d\n",i);
			return -1;
		}
	}
	return 0;
	
}

int freebob_streaming_reset(freebob_device_t *dev) {
	/* 
	 * Reset means:
	 * 1) Stopping the packetizer thread
	 * 2) Bringing all buffers & connections into a know state
	 *    - Clear all capture buffers
	 *    - Put nb_periods*period_size of null frames into the playback buffers
	 * 3) Restarting the packetizer thread
	 */

	unsigned long i;
	int err;
	assert(dev);

	debugPrint(DEBUG_LEVEL_XRUN_RECOVERY, "Resetting streams...\n");
	
	if((err=freebob_streaming_stop_thread(dev))<0) {
		printError("Could not stop packetizer thread.\n");
		return err;
	}

	//freebob_streaming_stop_iso(dev);

	// check if the packetizer is stopped
	assert(!dev->packetizer.run);
	
	// reset all connections
	for(i=0; i < dev->nb_connections; i++) {
		freebob_connection_t *connection= &(dev->connections[i]);
		freebob_streaming_reset_connection(dev,connection);
	}
	
	// put nb_periods*period_size of null frames into the playback buffers
	if((err=freebob_streaming_prefill_playback_streams(dev))<0) {
		printError("Could not prefill playback streams.\n");
		return err;
	}
	
	// we should transfer nb_buffers-1 periods of playback from the stream buffers to the event buffer
	for (i=0;i<dev->options.nb_buffers;i++) {
 		freebob_streaming_transfer_playback_buffers(dev);
	}
			
	// clear the xrun flag
	dev->xrun_detected=FALSE;
	
	//freebob_streaming_start_iso(dev);
	freebob_streaming_print_bufferfill(dev);
	
	if((err=freebob_streaming_start_thread(dev))<0) {
		printError("Could not start packetizer thread.\n");
		return err;
	}
	
	
	return 0;
}

int freebob_streaming_xrun_recovery(freebob_device_t *dev) {
	freebob_streaming_reset(dev);
	dev->xrun_count++;
	
	return 0;
}

int freebob_streaming_wait(freebob_device_t *dev) {
	int ret;

	// Wait for packetizer thread to signal a period completion
	sem_wait(&dev->packetizer.transfer_boundary);
	
	// acknowledge the reception of the period
	//sem_post(&dev->packetizer.transfer_ack);
	
	if(dev->xrun_detected) {
		// notify the driver of the underrun and the delay
		ret = (-1) * freebob_streaming_xrun_recovery(dev);
	} else {
		ret=dev->options.period_size;
	}
	
	return ret;
}

int freebob_streaming_transfer_capture_buffers(freebob_device_t *dev) {
	int i;
	int xrun;
	unsigned int offset=0;
	
	freebob_ringbuffer_data_t vec[2];
	// transfer the received events into the stream buffers
	for (i=0;i<dev->nb_connections_capture;i++) {
		freebob_connection_t *connection= &(dev->connections[i]); // capture connections are first in the array
		assert(connection);
		
		debugPrint(DEBUG_LEVEL_WAIT, "R: > %d\n",freebob_ringbuffer_read_space(connection->event_buffer));
			
			// we received one period of frames on each connection
			// this is period_size*dimension of events

		int events2read=dev->options.period_size*connection->spec.dimension;
		int bytes2read=events2read*sizeof(quadlet_t);
			
			// TODO: implement dbc for midi
		int dbc=0;
			
			/* 	read events2read bytes from the ringbuffer 
		*  first see if it can be done in one read. 
		*  if so, ok. 
		*  otherwise read up to a multiple of clusters directly from the buffer
		*  then do the buffer wrap around using ringbuffer_read
		*  then read the remaining data directly from the buffer in a third pass 
		*  Make sure that we cannot end up on a non-cluster aligned position!
			*/
		int cluster_size=connection->spec.dimension*sizeof(quadlet_t);
// 		unsigned int frames2read=events2read/connection->spec.dimension;
			
		while(bytes2read>0) {
			unsigned int framesread=(dev->options.period_size*cluster_size-bytes2read)/cluster_size;
			offset=framesread;
// 			offset=0;
			
			int bytesread=0;
						
			freebob_ringbuffer_get_read_vector(connection->event_buffer, vec);
				
			if(vec[0].len==0) { // this indicates an empty event buffer
				printError("Event buffer underrun on capture connection %d\n",i);
				break;
			}
				
				/* if we don't take care we will get stuck in an infinite loop
			* because we align to a cluster boundary later
			* the remaining nb of bytes in one read operation can be smaller than one cluster
			* this can happen because the ringbuffer size is always a power of 2
				*/
			if(vec[0].len<cluster_size) {
					// use the ringbuffer function to read one cluster (handles wrap around)
				freebob_ringbuffer_read(connection->event_buffer,connection->cluster_buffer,cluster_size);
					
				// decode the temporary buffer
				debugPrint(DEBUG_LEVEL_WAIT, "R: %5d [%5d %5d] %5d %5d\n",bytes2read,vec[0].len,vec[1].len,1, offset);
					
				xrun = freebob_am824_recv(connection->cluster_buffer, 1, offset, dbc, connection);
					
				if(xrun<0) {
						// xrun detected
					printError("Frame buffer overrun on capture connection %d\n",i);
					break;
				}
					
					// we advanced one cluster_size
				bytes2read-=cluster_size;
					
			} else { // 
				
				if(bytes2read>vec[0].len) {
						// align to a cluster boundary
					bytesread=vec[0].len-(vec[0].len%cluster_size);
				} else {
					bytesread=bytes2read;
				}
					
				debugPrint(DEBUG_LEVEL_WAIT, "R: %5d [%5d %5d] %5d %5d\n",bytes2read,vec[0].len,vec[1].len,bytesread, offset);
					
				xrun = freebob_am824_recv(vec[0].buf, bytesread/sizeof(quadlet_t)/connection->spec.dimension, offset, dbc, connection);
					
				if(xrun<0) {
						// xrun detected
					printError("Frame buffer overrun on capture connection %d\n",i);
					break;
				}
	
				freebob_ringbuffer_read_advance(connection->event_buffer, bytesread);
				bytes2read -= bytesread;
			}
				
				// the bytes2read should always be cluster aligned
			assert(bytes2read%cluster_size==0);
			debugPrint(DEBUG_LEVEL_WAIT, "R: < %d\n",freebob_ringbuffer_read_space(connection->event_buffer));
				
		}

	}
	return 0;
}

int freebob_streaming_transfer_playback_buffers(freebob_device_t *dev) {
	int i;
	int xrun;
	unsigned int offset=0;
	
	freebob_ringbuffer_data_t vec[2];
	// transfer the output stream buffers content to the event buffers
	for (i=dev->nb_connections_capture;i<dev->nb_connections_capture+dev->nb_connections_playback;i++) {
		freebob_connection_t *connection= &(dev->connections[i]);
		assert(connection);
		debugPrint(DEBUG_LEVEL_WAIT, "W: < %d\n",freebob_ringbuffer_write_space(connection->event_buffer));
			
			// we received one period of frames on each connection
			// this is period_size*dimension of events

		int events2write=dev->options.period_size*connection->spec.dimension;
		int bytes2write=events2write*sizeof(quadlet_t);
			
			// TODO: implement dbc for midi
		int dbc=0;
			
		/* 	write events2write bytes to the ringbuffer 
		*  first see if it can be done in one read. 
		*  if so, ok. 
		*  otherwise write up to a multiple of clusters directly to the buffer
		*  then do the buffer wrap around using ringbuffer_write
		*  then write the remaining data directly to the buffer in a third pass 
		*  Make sure that we cannot end up on a non-cluster aligned position!
		*/
		int cluster_size=connection->spec.dimension*sizeof(quadlet_t);
// 		unsigned int frames2write=events2write/connection->spec.dimension;
			
		while(bytes2write>0) {
			int byteswritten=0;
			
			unsigned int frameswritten=(dev->options.period_size*cluster_size-bytes2write)/cluster_size;
			offset=frameswritten;
// 			offset=0;
			
			freebob_ringbuffer_get_write_vector(connection->event_buffer, vec);
				
			if(vec[0].len==0) { // this indicates a full event buffer
				printError("Event buffer overrun on playback connection %d\n",i);
				break;
			}
				
			/* if we don't take care we will get stuck in an infinite loop
			* because we align to a cluster boundary later
			* the remaining nb of bytes in one write operation can be smaller than one cluster
			* this can happen because the ringbuffer size is always a power of 2
			*/
			if(vec[0].len<cluster_size) {
				
				// encode to the temporary buffer
				debugPrint(DEBUG_LEVEL_WAIT, "W: %d [%d %d] %d\n",bytes2write,vec[0].len,vec[1].len,1);
					
				xrun = freebob_am824_xmit(connection->cluster_buffer, 1, offset, dbc, connection);
				
				if(xrun<0) {
						// xrun detected
					printError("Frame buffer underrun on playback connection %d\n",i);
					break;
				}
					
				// use the ringbuffer function to write one cluster (handles wrap around)
				freebob_ringbuffer_write(connection->event_buffer,connection->cluster_buffer,cluster_size);
					
				// we advanced one cluster_size
				bytes2write-=cluster_size;
					
			} else { // 
				
				if(bytes2write>vec[0].len) {
					// align to a cluster boundary
					byteswritten=vec[0].len-(vec[0].len%cluster_size);
				} else {
					byteswritten=bytes2write;
				}
					
				debugPrint(DEBUG_LEVEL_WAIT, "W: %d [%d %d] %d\n",bytes2write,vec[0].len,vec[1].len,byteswritten);
					
				xrun = freebob_am824_xmit(vec[0].buf, byteswritten/sizeof(quadlet_t)/connection->spec.dimension, offset, dbc, connection);
					
				if(xrun<0) {
						// xrun detected
					printError("Frame buffer underrun on playback connection %d\n",i);
					break;
				}
	
				freebob_ringbuffer_write_advance(connection->event_buffer, byteswritten);
				bytes2write -= byteswritten;
			}
				
			// the bytes2write should always be cluster aligned
			assert(bytes2write%cluster_size==0);
				
		}

		debugPrint(DEBUG_LEVEL_WAIT, "W: > %d\n",freebob_ringbuffer_write_space(connection->event_buffer));
	}
	
	
	return 0;
}

int freebob_streaming_transfer_buffers(freebob_device_t *dev) {
	int err=0;
	
	err=freebob_streaming_transfer_capture_buffers(dev);
	if (err) return err;
	
	err=freebob_streaming_transfer_playback_buffers(dev);
	return err;
	
}


int freebob_streaming_write(freebob_device_t *dev, int i, freebob_sample_t *buffer, int nsamples) {
	int retval;
	
	freebob_stream_t *stream;
	assert(i<dev->nb_playback_streams);
	
	stream=*(dev->playback_streams+i);
	assert(stream);
	
	retval=freebob_ringbuffer_write(stream->buffer,(char *)buffer,nsamples*sizeof(freebob_sample_t))/sizeof(freebob_sample_t);
		
	return retval;
}

int freebob_streaming_read(freebob_device_t *dev, int i, freebob_sample_t *buffer, int nsamples) {
	// this has one ringbuffer too many, but that will be solved shortly
	int retval=0;

	freebob_stream_t *stream;
	assert(i<dev->nb_capture_streams);
	
	stream=*(dev->capture_streams+i);
	assert(stream);
	
	// now read from the stream ringbuffer
	retval=freebob_ringbuffer_read(stream->buffer, (char *)buffer, nsamples*sizeof(freebob_sample_t))/sizeof(freebob_sample_t);	

// 	fprintf(stderr,"rb read1 [%02d: %08p %08p %08X, %d, %d]\n",stream->spec.position, stream, stream->buffer, *buffer, nsamples, retval);
	
	return retval;
}

pthread_t freebob_streaming_get_packetizer_thread(freebob_device_t *dev) {
	return dev->packetizer.transfer_thread;
}

/* --------------------- *
 * PRIVATE STUFF         *
 * --------------------- */
 
 
unsigned int freebob_streaming_register_generic_stream(freebob_stream_t *stream, freebob_stream_t ***oldset, unsigned int set_size) {
	int i;
	int found=0;
	unsigned int new_set_size;
	
	freebob_stream_t **new_streams=calloc(set_size+1,sizeof(freebob_stream_t *));
	freebob_stream_t **set=*oldset;
	
	for (i=0;i<set_size;i++) {
		*(new_streams+i)=*(set+i);
		if(*(set+i)==stream) {
			printError("stream already registered\n");
			found=1;
		}
	}
	if (!found) {
		*(new_streams+set_size)=stream;
		new_set_size=set_size+1;
		
		free(*oldset);
		*oldset=new_streams; 
	} else {
		free(new_streams);
		new_set_size=set_size;
	}
	return new_set_size;
	
}

void freebob_streaming_register_capture_stream(freebob_device_t *dev, freebob_stream_t *stream) {
	dev->nb_capture_streams=
		freebob_streaming_register_generic_stream(
			stream, 
			&dev->capture_streams, 
			dev->nb_capture_streams);
	if (stream->spec.format==IEC61883_STREAM_TYPE_MBLA) {
		dev->nb_synced_capture_streams=
			freebob_streaming_register_generic_stream(
				stream, 
				&dev->synced_capture_streams, 
				dev->nb_synced_capture_streams);
	}
}

void freebob_streaming_register_playback_stream(freebob_device_t *dev, freebob_stream_t *stream) {
	dev->nb_playback_streams=
		freebob_streaming_register_generic_stream(
			stream, 
			&dev->playback_streams, 
			dev->nb_playback_streams);
	if (stream->spec.format==IEC61883_STREAM_TYPE_MBLA) {
		dev->nb_synced_playback_streams=
			freebob_streaming_register_generic_stream(
				stream, 
				&dev->synced_playback_streams, 
				dev->nb_synced_playback_streams);
	}
}

static inline int
freebob_streaming_period_complete (freebob_device_t *dev) {
	unsigned long i;
	//unsigned int period_boundary_bytes=dev->options.period_size*sizeof(freebob_sample_t);
	
	assert(dev);
	
	for(i=0; i < dev->nb_connections; i++) {
		freebob_connection_t *connection= &(dev->connections[i]);
		if (connection->status.frames_left > 0) {
			return FALSE;
		}
	}
	return TRUE;
	
#if 0	
	for(i=0; i < dev->nb_synced_capture_streams; i++) {
		assert(dev->synced_capture_streams);
		assert(dev->synced_capture_streams[i]);
		
		/* We check if there is more than one period of samples in the buffer.
		 * if not the period isn't complete yet.
		 */
		if (freebob_ringbuffer_read_space(dev->synced_capture_streams[i]->buffer) < period_boundary_bytes) {
			return FALSE; 
		}
	}
	
	for(i=0; i < dev->nb_synced_playback_streams; i++) {
		assert(dev->synced_playback_streams);
		assert(dev->synced_playback_streams[i]);
		//FIXME: check this!
		/* We check if there is still one period of samples in the output buffer.
		 * if so, the period isn't complete yet.
		 */
		if (freebob_ringbuffer_read_space(dev->synced_playback_streams[i]->buffer) > period_boundary_bytes) {
			return FALSE;
		}
	}
	return TRUE;
#endif
}

static inline void
freebob_streaming_period_reset (freebob_device_t *dev) {
// 	unsigned long i;
	//unsigned int period_boundary_bytes=dev->options.period_size*sizeof(freebob_sample_t);
	unsigned long i;
	//unsigned int period_boundary_bytes=dev->options.period_size*sizeof(freebob_sample_t);
	printEnter();
	assert(dev);
	
	for(i=0; i < dev->nb_connections; i++) {
		freebob_connection_t *connection= &(dev->connections[i]);
		connection->status.frames_left+=dev->options.period_size;
		// enable poll
		connection->pfd->events=POLLIN;
	}
	printExit();
	return;
}

static inline int
freebob_streaming_xrun_detected_on_connection (freebob_device_t *dev, freebob_connection_t *connection) {
	assert((connection));
	if (connection->status.xruns>0) {
		return TRUE;
	}
	return FALSE;
}

static inline int
freebob_streaming_xrun_detected (freebob_device_t *dev, freebob_connection_t **connections, int nb_connections) {
	int i;
	
	for(i=0; i < nb_connections; i++) {
		assert((connections[i]));

		if (freebob_streaming_xrun_detected_on_connection (dev, connections[i])) {
			return TRUE;
		}
	}
	return FALSE;
}

int freebob_streaming_start_iso_connection(freebob_device_t *dev, freebob_connection_t *connection) {
	int err;

	err=0;
	if (connection->spec.direction == FREEBOB_CAPTURE) {
		connection->status.packets=0;	
		connection->status.dropped=0;		
		if (connection->spec.is_master) { //master connection

			debugPrint(DEBUG_LEVEL_STARTUP, 
				"Init ISO master receive handler on channel %d...\n",
				connection->iso.iso_channel);

			debugPrint(DEBUG_LEVEL_STARTUP, 
				"   (%s, BUFFERS=%d, PACKET_SIZE=%d, PACKET_MAX=%d, IRQ=%d, %d PKT/PERIOD)...\n",
				(connection->iso.receive_mode==RAW1394_DMA_PACKET_PER_BUFFER ? "PACKET_PER_BUFFER" : "BUFFERFILL"),
				connection->iso.buffers, 
				connection->iso.packet_size,
				connection->iso.max_packet_size,
				connection->iso.irq_interval,
				connection->iso.packets_per_period);

			raw1394_iso_recv_init(
					connection->raw_handle, 
					iso_master_receive_handler, 
					connection->iso.buffers,
					connection->iso.max_packet_size, 
					connection->iso.iso_channel, 
 					connection->iso.receive_mode, // RAW1394_DMA_BUFFERFILL, 
					connection->iso.irq_interval);

		} else {
			//slave receive connection
			debugPrint(DEBUG_LEVEL_STARTUP, 
				"Init ISO slave receive handler on channel %d...\n",
				connection->iso.iso_channel);

			debugPrint(DEBUG_LEVEL_STARTUP, 
				"   (%s, BUFFERS=%d, PACKET_SIZE=%d, PACKET_MAX=%d, IRQ=%d, %d PKT/PERIOD)...\n",
				(connection->iso.receive_mode==RAW1394_DMA_PACKET_PER_BUFFER ? "PACKET_PER_BUFFER" : "BUFFERFILL"),				connection->iso.buffers, 
				connection->iso.packet_size,
				connection->iso.max_packet_size,
				connection->iso.irq_interval,
				connection->iso.packets_per_period);

			raw1394_iso_recv_init(	
					connection->raw_handle, 
					iso_slave_receive_handler, 
					connection->iso.buffers,
					connection->iso.max_packet_size, 
					connection->iso.iso_channel, 
 					connection->iso.receive_mode, // RAW1394_DMA_BUFFERFILL, 
					connection->iso.irq_interval);
		}
		
		debugPrint(DEBUG_LEVEL_STARTUP, 
			"Start ISO receive for connection on channel %d at cycle %d...\n",
			  connection->iso.iso_channel, connection->iso.startcycle);
			
		err = raw1394_iso_recv_start(
				connection->raw_handle, 
		connection->iso.startcycle, 
		-1,//IEC61883_TAG_WITH_CIP, 
		0);

		if (err) {
			printError("couldn't start receiving: %s\n",
					   strerror (errno));
				// TODO: cleanup
			return err;
		}					
		
	} else if (connection->spec.direction == FREEBOB_PLAYBACK) {
		
		if (connection->spec.is_master) { // master connection
			debugPrint(DEBUG_LEVEL_STARTUP, 
				"Init ISO master transmit handler on channel %d...\n",
				connection->iso.iso_channel);

			debugPrint(DEBUG_LEVEL_STARTUP, 
				"   (BUFFERS=%d, PACKET_SIZE=%d, PACKET_MAX=%d, IRQ=%d, %d PKT/PERIOD)...\n",
				connection->iso.buffers, 
				connection->iso.packet_size,
				connection->iso.max_packet_size,
				connection->iso.irq_interval,
				connection->iso.packets_per_period);

				
			raw1394_iso_xmit_init(
					connection->raw_handle, 
					iso_master_transmit_handler, 
					connection->iso.buffers,
					connection->iso.max_packet_size, 
					connection->iso.iso_channel, 
					RAW1394_ISO_SPEED_400, 
					connection->iso.irq_interval);			
		} else {
			
			debugPrint(DEBUG_LEVEL_STARTUP, 
				"Init ISO slave transmit handler on channel %d...\n",
				connection->iso.iso_channel);

			debugPrint(DEBUG_LEVEL_STARTUP, 
				"   (BUFFERS=%d, PACKET_SIZE=%d, PACKET_MAX=%d, IRQ=%d, %d PKT/PERIOD)...\n",
				connection->iso.buffers, 
				connection->iso.packet_size,
				connection->iso.max_packet_size,
				connection->iso.irq_interval,
				connection->iso.packets_per_period);

			raw1394_iso_xmit_init(
					connection->raw_handle, 
					iso_slave_transmit_handler, 
					connection->iso.buffers,
					connection->iso.max_packet_size, 
					connection->iso.iso_channel, 
					RAW1394_ISO_SPEED_400, 
					connection->iso.irq_interval);	

		}
	
		debugPrint(DEBUG_LEVEL_STARTUP, 
			"Start ISO transmit for connection on channel %d at cycle %d\n",
			connection->iso.iso_channel, connection->iso.startcycle);
			
		err=raw1394_iso_xmit_start(
			connection->raw_handle, 
			connection->iso.startcycle, 
			connection->iso.prebuffers);

		if (err) {
			printError("couldn't start transmitting: %s\n",
					   strerror (errno));
				// TODO: cleanup
			return err;
		}
	}
	return 0;
}

int freebob_streaming_stop_iso_connection(freebob_device_t *dev, freebob_connection_t *connection) {
	
	debugPrintWithTimeStamp(DEBUG_LEVEL_STARTUP, "Stop connection on channel %d ...\n", connection->iso.iso_channel);
	if (connection->spec.direction == FREEBOB_CAPTURE) {
		raw1394_iso_recv_flush(connection->raw_handle);
	} else {
		raw1394_iso_xmit_sync(connection->raw_handle);
	}
		
	raw1394_iso_stop(connection->raw_handle);
	
	debugPrint(DEBUG_LEVEL_STARTUP, "Shutdown connection on channel %d ...\n", connection->iso.iso_channel);
		
	raw1394_iso_shutdown(connection->raw_handle);
	
	return 0;
}

int freebob_streaming_wait_for_sync_stream(freebob_device_t *dev, freebob_connection_t *connection) {
	int err;
	
	debugPrint(DEBUG_LEVEL_STARTUP, "Waiting for the sync stream...\n");
	
	// start the sync master connection
	
	connection->iso.startcycle=-1; // don't care when we start this
	connection->status.events=0;
	
	freebob_streaming_start_iso_connection(dev,connection);
	
	// wait until something is received/sent on the connection
	while(connection->status.events==0) {
		err=0;
			
		// get a packet on the sync master connection
		err=raw1394_loop_iterate(connection->raw_handle);
			
		if (err == -1) {
			printError("Possible raw1394 error: %s on sync master connection: %d\n",
					   strerror (errno),connection->spec.id);
		}
	}
	
	freebob_streaming_stop_iso_connection(dev,connection);
	
	// reset the connection
	freebob_streaming_reset_connection(dev, connection);
	
	// FIXME: only works for sync on receive stream because we don't prefill.
	// ?FIXED?
	if(connection->spec.direction==1) { // playback
		int i;

		if((err=freebob_streaming_reset_playback_streams(dev))<0) {
			printError("Could not reset playback streams.\n");
			return err;
		}

		// put nb_periods*period_size of null frames into the playback buffers
		if((err=freebob_streaming_prefill_playback_streams(dev))<0) {
			printError("Could not prefill playback streams.\n");
			return err;
		}
			
		// we should transfer nb_buffers periods of playback from the stream buffers to the event buffer
		for (i=0;i<dev->options.nb_buffers;i++) {
			freebob_streaming_transfer_playback_buffers(dev);
		}
	}	

	debugPrint(DEBUG_LEVEL_STARTUP, "  stream is running.\n");

	return 0;
}

int freebob_streaming_start_iso(freebob_device_t *dev) {
	freebob_connection_t *connection=NULL;
	unsigned int c;
// 	int err;

	// when starting the thread, we should wait with iterating the slave connections
	// until the master connection has processed some samples, because the devices
	// tend to start with a stream of no-data packets, leading to xruns on slave
	// transmit, leading to multiple restarts of the thread, which doesn't work correctly (yet)
/*	debugPrint(DEBUG_LEVEL_STARTUP, "Waiting for the sync master...\n");
	
	
	connection=dev->sync_master_connection;
	
	// start the sync master connection
	
	connection->iso.startcycle=-1; // don't care when we start this
	connection->status.events=0;
	
	freebob_streaming_start_iso_connection(dev,connection);*/
	
	// wait until something is received/sent on the connection
// 	while(connection->status.events==0) {
// 		err=0;
// 		
// 		// get a packet on the sync master connection
// 		err=raw1394_loop_iterate(connection->raw_handle);
// 		
// 		if (err == -1) {
// 			printError("Possible raw1394 error: %s on sync master connection: %d\n",
// 					   strerror (errno),connection->spec.id);
// 		}
// 	}
	
#define NB_CYCLES_TO_SKIP 100

	// get the last timestamp & calculate the start times for the connections
	// start ISO xmit/receive
	
	// we introduce some delay to let the freebob devices start streaming.
	usleep(2000);
	freebob_streaming_wait_for_sync_stream(dev, dev->sync_master_connection);
	
	for(c=0; c < dev->nb_connections; c++) {
		connection= &(dev->connections[c]);
		
// 		if(connection != dev->sync_master_connection) {
			
			connection->iso.startcycle=(dev->sync_master_connection->status.last_timestamp.cycle+NB_CYCLES_TO_SKIP)%8000;
			
// 			connection->iso.startcycle=0;
			
			freebob_streaming_start_iso_connection(dev, connection);
// 		}	
	}	
	
	return 0;
}

int freebob_streaming_stop_iso(freebob_device_t *dev) {
	unsigned int c;
	// stop ISO xmit/receive
	for(c=0; c < dev->nb_connections; c++) {
		freebob_connection_t *connection= &(dev->connections[c]);

		freebob_streaming_stop_iso_connection(dev,connection);
	}
	return 0;
}

/*
 * The thread responsible for packet iteration
 */

void * freebob_iso_packet_iterator(void *arg)
{

	freebob_device_t * dev=(freebob_device_t *) arg;
	int err;
// 	int cycle=0;
	int underrun_detected=0;
	
	int notdone=TRUE;
	
	freebob_connection_t *connection=NULL;
	
	assert(dev);
	assert(dev->sync_master_connection);
	assert(dev->connections);
	
	debugPrint(DEBUG_LEVEL_STARTUP, "Entering packetizer thread...\n");
	
 	freebob_streaming_start_iso(dev);

	freebob_streaming_print_bufferfill(dev);
	// start xmit/receive
	debugPrint(DEBUG_LEVEL_STARTUP, "Go Go Go!!!\n");
	
#define POLL_BASED	
#ifdef POLL_BASED 
	while (dev->packetizer.run && !underrun_detected) {
		
		freebob_streaming_period_reset(dev);
		
		dev->watchdog_check = 1;
		
		notdone=TRUE;
		
		while(notdone) {
			
			err = poll (dev->pfds, dev->nfds, -1);
			
			if (err == -1) {
				if (errno == EINTR) {
					continue;
				}
				printError("poll error: %s\n", strerror (errno));
				dev->packetizer.status = -2;
				dev->packetizer.retval = 0;
				notdone=FALSE;
				break;
			}
			
			int i=0;
		
			for (i = 0; i < dev->nfds; i++) {
				if (dev->pfds[i].revents & POLLERR) {
					printError ("error on fd for %d\n",i);
				}

				if (dev->pfds[i].revents & POLLHUP) {
					printError ("hangup on fd for %d\n",i);
				}
				
				if(dev->pfds[i].revents & (POLLIN)) {
					// FIXME: this can segfault 
					connection=dev->fdmap[i];
					
					assert(connection);
					
					err = raw1394_loop_iterate (connection->raw_handle);
					
					// detect underruns on the connection
					if (freebob_streaming_xrun_detected_on_connection(dev,connection)) {
						printError("Xrun on connection %d\n", connection->spec.id);
						underrun_detected=TRUE;
						break; // we can exit as the underrun handler will flush the buffers anyway
					}
				
					if (err == -1) {
						printError ("possible raw1394 error: %s\n", strerror (errno));
						dev->packetizer.status = -2;
						dev->packetizer.retval = 0;
						notdone=FALSE;
						break;
					}
				}

				

			}

			notdone=(!freebob_streaming_period_complete(dev) && !underrun_detected && (dev->packetizer.run)) && notdone;
		}
		
		if(underrun_detected) {
			dev->xrun_detected=TRUE;
		}

#else
	while (dev->packetizer.run && !underrun_detected) {
		
		freebob_streaming_period_reset(dev);
			
		dev->watchdog_check = 1;
			
		notdone=TRUE;
			
		while(notdone) {
				
			err=0;
			
			// this makes sure the sync_master finishes before the others finish
			connection=dev->sync_master_connection;
			if (connection->status.frames_left > 0) {
				// process a packet on the sync master connection
				err=raw1394_loop_iterate(connection->raw_handle);
				
				if (err == -1) {
					printError("Possible raw1394 error: %s on sync master connection: %d\n",
							strerror (errno),connection->spec.id);
						
					dev->packetizer.status = -2;
					dev->packetizer.retval = 0;
					notdone=FALSE;
					break;
				}
				
				// detect underruns on the sync master connection
				if (freebob_streaming_xrun_detected(dev,&connection,1)) {
					printError("Xrun on sync master connection %d\n", connection->spec.id);
					underrun_detected=TRUE;
					break; // we can exit as the underrun handler will flush the buffers anyway
				}
			}
				
			
			// now iterate on the slave connections
			int c;
			for(c=0; c<dev->nb_connections; c++) {

				connection = &(dev->connections[c]);
				
				if ((connection == dev->sync_master_connection))
						continue;
				
				err=raw1394_loop_iterate(connection->raw_handle);
				
				if (err == -1) {
					printError("Possible raw1394 error: %s on connection: %d\n",
							strerror (errno),connection->spec.id);
					dev->packetizer.status = -2;
					dev->packetizer.retval = 0;
					notdone=FALSE;
				}
				
				// detect underruns
				if (freebob_streaming_xrun_detected(dev,&connection,1)) {
					printError("Xrun on slave connection %d\n", connection->spec.id);
					underrun_detected=TRUE;
					break; // we can exit as the underrun handler will flush the buffers anyway
				}
			}
				
			notdone=(!freebob_streaming_period_complete(dev) && !underrun_detected && (dev->packetizer.run)) && notdone;
		}
		
		if(underrun_detected) {
			dev->xrun_detected=TRUE;
		}
	
#endif
		// notify the waiting thread
#ifdef DEBUG
		debugPrint(DEBUG_LEVEL_HANDLERS, "Post semaphore ");
		int c;
		for(c=0; c<dev->nb_connections; c++) {
			connection = &(dev->connections[c]);
			debugPrintShort(DEBUG_LEVEL_HANDLERS,"[%d: %d]",c,connection->status.frames_left);
		}
		debugPrintShort(DEBUG_LEVEL_HANDLERS,"\n");
#endif

		sem_post(&dev->packetizer.transfer_boundary);

#ifdef DEBUG
		if((dev->sync_master_connection->status.packets - dev->sync_master_connection->status.total_packets_prev) > 1024*2) {
			unsigned int i;
			debugPrintShort(DEBUG_LEVEL_PACKETCOUNTER,"\r -> ");
			
			for(i=0; i < dev->nb_connections; i++) {
				freebob_connection_t *connection= &(dev->connections[i]);
				assert(connection);
				
				debugPrintShort(DEBUG_LEVEL_PACKETCOUNTER,"[%s, %02d, %10d, %04d, %4d, (R: %04d)]", 
					(connection->spec.direction==FREEBOB_CAPTURE ? "C" : "P"),
					connection->status.fdf,
					connection->status.packets,
					connection->status.last_cycle,
					connection->status.dropped,
					freebob_ringbuffer_read_space(connection->event_buffer)/(sizeof(quadlet_t)*connection->spec.dimension)
				   );
			}
	
			debugPrintShort(DEBUG_LEVEL_PACKETCOUNTER," XRUNS (%2d)",dev->xrun_count);	
			fflush (stderr);
			
			dev->sync_master_connection->status.total_packets_prev=dev->sync_master_connection->status.packets;
		}

#endif

	}
	
 	freebob_streaming_stop_iso(dev);
	
	debugPrint(DEBUG_LEVEL_STARTUP, "Exiting packetizer thread...\n");
	
	
	pthread_exit (0);	
}

void freebob_streaming_append_master_timestamp(freebob_device_t *dev, freebob_timestamp_t *t) {
	int c;
// 	int retval;
	freebob_connection_t *connection;
	
	assert(dev);
	
	for(c=0; c<dev->nb_connections; c++) {

		connection = &(dev->connections[c]);
		
		// skip the sync master
		if ((connection == dev->sync_master_connection))
			continue;
		
		// write the timestamp

/*		retval=freebob_ringbuffer_write(connection->timestamp_buffer,(char *)t,sizeof(freebob_timestamp_t));
		
		if(retval!=sizeof(unsigned int)) {
			printError("Timestamp buffer overrun on connection %d!\n",c);
		}
*/		
	}
}

inline int freebob_streaming_decode_midi(freebob_connection_t *connection,
								  quadlet_t* events, 
								  unsigned int nsamples,
								  unsigned int dbc
								 ) {
	quadlet_t *target_event;
	
	assert (connection);
	assert (events);

	freebob_stream_t *stream;
	
	unsigned int j=0;
	unsigned int s=0;
	int written=0;
	quadlet_t *buffer;
	
	for (s=0;s<connection->nb_streams;s++) {
		stream=&connection->streams[s];
		
		assert (stream);
		assert (stream->spec.position < connection->spec.dimension);
		assert(stream->user_buffer);
		
		if (stream->spec.format == IEC61883_STREAM_TYPE_MIDI) {
			/* idea:
			spec says: current_midi_port=(dbc+j)%8;
			=> if we start at (dbc+stream->location-1)%8 [due to location_min=1], 
			we'll start at the right event for the midi port.
			=> if we increment j with 8, we stay at the right event.
			*/
			buffer=((quadlet_t *)(stream->user_buffer));
			written=0;
			
 			for(j = (dbc & 0x07)+stream->spec.location-1; j < nsamples; j += 8) {
				target_event=(quadlet_t *)(events + ((j * connection->spec.dimension) + stream->spec.position));
				quadlet_t sample_int=ntohl(*target_event);
  				if(IEC61883_AM824_GET_LABEL(sample_int) != IEC61883_AM824_LABEL_MIDI_NO_DATA) {
					*(buffer)=(sample_int >> 16);
					buffer++;
					written++;
 				}
			}
			
			int written_to_rb=freebob_ringbuffer_write(stream->buffer, (char *)(stream->user_buffer), written*sizeof(quadlet_t))/sizeof(quadlet_t);
			if(written_to_rb<written) {
				printMessage("MIDI OUT bytes lost (%d/%d)",written_to_rb,written);
			}
		}
	}
	return 0;
	
}

/*
 * according to the MIDI over 1394 spec, we are only allowed to send a maximum of one midi byte every 320usec
 * therefore we can only send one byte on 1 out of 3 iso cycles (=325usec)
 */

inline int freebob_streaming_encode_midi(freebob_connection_t *connection,
								quadlet_t* events, 
								unsigned int nsamples,
								unsigned int dbc
								) {
	quadlet_t *target_event;
	
	assert (connection);
	assert (events);

	freebob_stream_t *stream;

	unsigned int j=0;
	unsigned int s=0;
	int read=0;
	quadlet_t *buffer;
	
	for (s=0;s<connection->nb_streams;s++) {
		stream=&connection->streams[s];

		assert (stream);
		assert (stream->spec.position < connection->spec.dimension);
		assert(stream->user_buffer);

		if (stream->spec.format == IEC61883_STREAM_TYPE_MIDI) {
			// first prefill the buffer with NO_DATA's on all time muxed channels
			for(j=0; (j < nsamples); j++) {
				target_event=(quadlet_t *)(events + ((j * connection->spec.dimension) + stream->spec.position));
				
				*target_event=htonl(IEC61883_AM824_SET_LABEL(0,IEC61883_AM824_LABEL_MIDI_NO_DATA));

			}
			
			/* idea:
				spec says: current_midi_port=(dbc+j)%8;
				=> if we start at (dbc+stream->location-1)%8 [due to location_min=1], 
				we'll start at the right event for the midi port.
				=> if we increment j with 8, we stay at the right event.
			*/
			
			if(stream->midi_counter<=0) { // we can send a byte
				read=freebob_ringbuffer_read(stream->buffer, (char *)(stream->user_buffer), 1*sizeof(quadlet_t))/sizeof(quadlet_t);
				if(read) {
// 					j = (dbc%8)+stream->spec.location-1; 
					j = (dbc & 0x07)+stream->spec.location-1; 
					target_event=(quadlet_t *)(events + ((j * connection->spec.dimension) + stream->spec.position));
					buffer=((quadlet_t *)(stream->user_buffer));
				
					*target_event=htonl(IEC61883_AM824_SET_LABEL((*buffer)<<16,IEC61883_AM824_LABEL_MIDI_1X));
					stream->midi_counter=3; // only if we send a byte, we reset the counter
				}
			} else {
				stream->midi_counter--;
			}
		}
	}
	return 0;

}


/**
 * ISO send/receive callback handlers
 */

static enum raw1394_iso_disposition 
iso_master_receive_handler(raw1394handle_t handle, unsigned char *data, 
        unsigned int length, unsigned char channel,
        unsigned char tag, unsigned char sy, unsigned int cycle, 
        unsigned int dropped)
{
	enum raw1394_iso_disposition retval=RAW1394_ISO_OK;

	freebob_connection_t *connection=(freebob_connection_t *) raw1394_get_userdata (handle);
	assert(connection);
	
	struct iec61883_packet *packet = (struct iec61883_packet *) data;
	assert(packet);
	
	// FIXME: dropped packets are very bad when transmitting and the other side is sync'ing on that!
	connection->status.dropped+=dropped;

#ifdef DEBUG	
	connection->status.last_cycle=cycle;
#endif

	if((packet->fmt == 0x10) && (packet->fdf != 0xFF) && (packet->dbs>0) && (length>=2*sizeof(quadlet_t))) {
		unsigned int nevents=((length / sizeof (quadlet_t)) - 2)/packet->dbs;
		
		// add the data payload to the ringbuffer

		assert(connection->spec.dimension == packet->dbs);
		
		if (freebob_ringbuffer_write(
				connection->event_buffer,(char *)(data+8),
				nevents*sizeof(quadlet_t)*connection->spec.dimension) < 
				nevents*sizeof(quadlet_t)*connection->spec.dimension) 
		{
			printError("MASTER RCV: Buffer overrun!\n"); 
			connection->status.xruns++;
			retval=RAW1394_ISO_DEFER;
		} else {
			retval=RAW1394_ISO_OK;
			// we cannot offload midi encoding due to the need for a dbc value
			freebob_streaming_decode_midi(connection,(quadlet_t *)(data+8), nevents, packet->dbc);
		}
		
		// keep the frame counter
		connection->status.frames_left -= nevents;
		
		// keep track of the total amount of events received
		connection->status.events+=nevents;

#ifdef DEBUG	
		connection->status.fdf=packet->fdf;
#endif
		
	} else {
		// discard packet
		// can be important for sync though
	}

	// one packet received
	connection->status.packets++;

#ifdef DEBUG
	if(packet->dbs) {
		unsigned int nevents=((length / sizeof (quadlet_t)) - 2)/packet->dbs;
		debugPrintWithTimeStamp(DEBUG_LEVEL_HANDLERS_LOWLEVEL, 
			"MASTER RCV: %08d, CH = %d, FDF = %X. SYT = %6d, DBS = %3d, DBC = %3d, FMT = %3d, LEN = %4d (%2d), DROPPED = %4d, info->packets=%4d, events=%4d, %d, %d\n", 
			cycle, 
			channel, packet->fdf,packet->syt,packet->dbs,packet->dbc,packet->fmt, length,
			((length / sizeof (quadlet_t)) - 2)/packet->dbs, dropped, 
			connection->status.packets, connection->status.events, connection->status.frames_left, 
			nevents);
	}
#endif

	if((connection->status.frames_left<=0)) {
		connection->pfd->events=0;
		return RAW1394_ISO_DEFER;
	}

	return retval;
}


static enum raw1394_iso_disposition 
		iso_slave_receive_handler(raw1394handle_t handle, unsigned char *data, 
								  unsigned int length, unsigned char channel,
								  unsigned char tag, unsigned char sy, unsigned int cycle, 
								  unsigned int dropped)
{
	enum raw1394_iso_disposition retval=RAW1394_ISO_OK;
	/* slave receive is easy if you assume that the connections are synced
	Synced connections have matched data rates, so just receiving and 
	calling the rcv handler would suffice in this case. As the connection 
	is externally matched to the master connection, the buffer fill will be ok.
	*/
	/* TODO: implement correct SYT behaviour */
	 	
	freebob_connection_t *connection=(freebob_connection_t *) raw1394_get_userdata (handle);
	assert(connection);
	
	struct iec61883_packet *packet = (struct iec61883_packet *) data;
	assert(packet);
	
	// FIXME: dropped packets are very bad when transmitting and the other side is sync'ing on that!
	//connection->status.packets+=dropped;
	connection->status.dropped+=dropped;

#ifdef DEBUG	
	connection->status.last_cycle=cycle;
#endif

	if((packet->fmt == 0x10) && (packet->fdf != 0xFF) && (packet->dbs>0) && (length>=2*sizeof(quadlet_t))) {
		unsigned int nevents=((length / sizeof (quadlet_t)) - 2)/packet->dbs;

#ifdef DEBUG	
		connection->status.fdf=packet->fdf;
#endif
		
		// add the data payload to the ringbuffer

		assert(connection->spec.dimension == packet->dbs);

		if (freebob_ringbuffer_write(
				  	connection->event_buffer,(char *)(data+8),
					nevents*sizeof(quadlet_t)*connection->spec.dimension) < 
			nevents*sizeof(quadlet_t)*connection->spec.dimension) 
		{
			printError("SLAVE RCV: Buffer overrun!\n");
			connection->status.xruns++;
			retval=RAW1394_ISO_DEFER;
		} else {
			retval=RAW1394_ISO_OK;
			// we cannot offload midi encoding due to the need for a dbc value
			freebob_streaming_decode_midi(connection,(quadlet_t *)(data+8), nevents, packet->dbc);
		}

		connection->status.frames_left-=nevents;
		
		// keep track of the total amount of events received
		connection->status.events+=nevents;
	} else {
		// discard packet
		// can be important for sync though
	}

	// one packet received
	connection->status.packets++;
	
	if(packet->dbs) {
		debugPrintWithTimeStamp(DEBUG_LEVEL_HANDLERS_LOWLEVEL, 
			"SLAVE RCV: CH = %d, FDF = %X. SYT = %6d, DBS = %3d, DBC = %3d, FMT = %3d, LEN = %4d (%2d), DROPPED = %6d\n", 
			channel, packet->fdf,
			packet->syt,
			packet->dbs,
			packet->dbc,
			packet->fmt, 
			length,
			((length / sizeof (quadlet_t)) - 2)/packet->dbs, dropped);
	}
		
		
	if((connection->status.frames_left<=0)) {
		connection->pfd->events=0;
		return RAW1394_ISO_DEFER;
	}

	return retval;
}

/**
 * The master transmit handler.
 *
 * This is the most difficult, because we have to generate timing information ourselves.
 *
 */

/*
 Includes code from libiec61883
 */
static enum raw1394_iso_disposition 
iso_master_transmit_handler(raw1394handle_t handle,
		unsigned char *data, unsigned int *length,
		unsigned char *tag, unsigned char *sy,
		int cycle, unsigned int dropped)
{
	freebob_connection_t *connection=(freebob_connection_t *) raw1394_get_userdata (handle);
	assert(connection);
	
	struct iec61883_packet *packet = (struct iec61883_packet *) data;
	assert(packet);
	assert(length);
	assert(tag);
	assert(sy);	
	
	// construct the packet cip
	int nevents = iec61883_cip_fill_header (handle, &connection->status.cip, packet);
	int nsamples=0;
	int bytes_read;
	
#ifdef DEBUG	
	connection->status.last_cycle=cycle;

	if(packet->fdf != 0xFF) {
		connection->status.fdf=packet->fdf;
	}
#endif

	enum raw1394_iso_disposition retval = RAW1394_ISO_OK;



	if (nevents > 0) {
		nsamples = nevents;
	}
	else {
		if (connection->status.cip.mode == IEC61883_MODE_BLOCKING_EMPTY) {
			nsamples = 0;
		}
		else {
			nsamples = connection->status.cip.syt_interval;
		}
	}
	
	// dropped packets are very bad when transmitting and the other side is sync'ing on that!
	connection->status.dropped += dropped;
		
	if (nsamples > 0) {

		assert(connection->spec.dimension == packet->dbs);

		if ((bytes_read=freebob_ringbuffer_read(connection->event_buffer,(char *)(data+8),nsamples*sizeof(quadlet_t)*connection->spec.dimension)) < 
				   nsamples*sizeof(quadlet_t)*connection->spec.dimension) 
		{
			printError("MASTER XMT: Buffer underrun! (%d / %d) (%d / %d )\n",
					   bytes_read,nsamples*sizeof(quadlet_t)*connection->spec.dimension,
					   freebob_ringbuffer_read_space(connection->event_buffer),0);
			connection->status.xruns++;
			retval=RAW1394_ISO_DEFER;
			nsamples=0;
		} else {
			retval=RAW1394_ISO_OK;
			// we cannot offload midi encoding due to the need for a dbc value
			freebob_streaming_encode_midi(connection,(quadlet_t *)(data+8), nevents, packet->dbc);
		}
	}
	
	*length = nsamples * connection->spec.dimension * sizeof (quadlet_t) + 8;
	*tag = IEC61883_TAG_WITH_CIP;
	*sy = 0;
	
	// keep track of the total amount of events transmitted
	connection->status.events+=nsamples;
	
	connection->status.frames_left-=nsamples;
		
	// one packet transmitted
	connection->status.packets++;

	if(packet->dbs) {
		debugPrintWithTimeStamp(DEBUG_LEVEL_HANDLERS_LOWLEVEL, 
								"MASTER XMT: %08d, CH = %d, FDF = %X. SYT = %6d, DBS = %3d, DBC = %3d, FMT = %3d, LEN = %4d (%2d), DROPPED = %4d, info->packets=%4d, events=%4d, %d, %d %d\n", cycle, 
								connection->iso.iso_channel,  packet->fdf,packet->syt,packet->dbs,packet->dbc,packet->fmt, *length,((*length / sizeof (quadlet_t)) - 2)/packet->dbs, dropped, 
								connection->status.packets, connection->status.events, connection->status.frames_left, 
								nevents, nsamples);
	}

	if((connection->status.frames_left<=0)) {
		connection->pfd->events=0;
		return RAW1394_ISO_DEFER;
	}

	return retval;

}

static enum raw1394_iso_disposition 
		iso_slave_transmit_handler(raw1394handle_t handle,
								   unsigned char *data, unsigned int *length,
								   unsigned char *tag, unsigned char *sy,
								   int cycle, unsigned int dropped)
{
	freebob_connection_t *connection=(freebob_connection_t *) raw1394_get_userdata (handle);
	assert(connection);
	
	struct iec61883_packet *packet = (struct iec61883_packet *) data;
	assert(packet);
	assert(length);
	assert(tag);
	assert(sy);	
	
	// construct the packet cip
	struct iec61883_cip old_cip;
	memcpy(&old_cip,&connection->status.cip,sizeof(struct iec61883_cip));
	
	int nevents = iec61883_cip_fill_header (handle, &connection->status.cip, packet);
	int nsamples=0;
	int bytes_read;

#ifdef DEBUG	
	connection->status.last_cycle=cycle;

	if(packet->fdf != 0xFF) {
		connection->status.fdf=packet->fdf;
	}
#endif

	enum raw1394_iso_disposition retval = RAW1394_ISO_OK;


	if (nevents > 0) {
		nsamples = nevents;
	}
	else {
		if (connection->status.cip.mode == IEC61883_MODE_BLOCKING_EMPTY) {
			nsamples = 0;
		}
		else {
			nsamples = connection->status.cip.syt_interval;
		}
	}
	
	// dropped packets are very bad when transmitting and the other side is sync'ing on that!
	//connection->status.packets+=dropped;
	connection->status.dropped += dropped;
		
	if (nsamples > 0) {
		int bytes_to_read=nsamples*sizeof(quadlet_t)*connection->spec.dimension;

		assert(connection->spec.dimension == packet->dbs);

		if ((bytes_read=freebob_ringbuffer_read(connection->event_buffer,(char *)(data+8),bytes_to_read)) < 
			bytes_to_read) 
		{
			/* there is no more data in the ringbuffer */
			
			/* If there are already more than on period
			* of frames transfered to the XMIT buffer, there is no xrun.
			* 
			*/
			if(connection->status.frames_left<=0) {
				// we stop processing this untill the next period boundary
				// that's when new data is ready
				
				connection->pfd->events=0;
				
				// reset the cip to the old value
				memcpy(&connection->status.cip,&old_cip,sizeof(struct iec61883_cip));

				// retry this packet
 				retval=RAW1394_ISO_AGAIN;
// 				retval=RAW1394_ISO_DEFER;
				nsamples=0;
			} else {
				printError("SLAVE XMT : Buffer underrun! %d (%d / %d) (%d / %d )\n",
					   connection->status.frames_left, bytes_read,bytes_to_read,
					   freebob_ringbuffer_read_space(connection->event_buffer),0);
				connection->status.xruns++;
				retval=RAW1394_ISO_DEFER;
				nsamples=0;
 			}
		} else {
			retval=RAW1394_ISO_OK;
			// we cannot offload midi encoding due to the need for a dbc value
			freebob_streaming_encode_midi(connection,(quadlet_t *)(data+8), nevents, packet->dbc);
		}
	}
	
	*length = nsamples * connection->spec.dimension * sizeof (quadlet_t) + 8;
	*tag = IEC61883_TAG_WITH_CIP;
	*sy = 0;
	
	// keep track of the total amount of events transmitted
	connection->status.events+=nsamples;
	
	connection->status.frames_left-=nsamples;
		
	// one packet transmitted
	connection->status.packets++;

	if(packet->dbs) {
		debugPrintWithTimeStamp(DEBUG_LEVEL_HANDLERS_LOWLEVEL, 
								"SLAVE XMT : %08d, CH = %d, FDF = %X. SYT = %6d, DBS = %3d, DBC = %3d, FMT = %3d, LEN = %4d (%2d), DROPPED = %4d, info->packets=%4d, events=%4d, %d, %d %d\n", cycle, 
								connection->iso.iso_channel,  packet->fdf,packet->syt,packet->dbs,packet->dbc,packet->fmt, *length,((*length / sizeof (quadlet_t)) - 2)/packet->dbs, dropped, 
								connection->status.packets, connection->status.events, connection->status.frames_left, 
								nevents, nsamples);
	}

	if((connection->status.frames_left<=0)) {
		connection->pfd->events=0;
		return RAW1394_ISO_DEFER;
	}

	return retval;

}

/*
 * Decoders and encoders
 */
#ifdef ENABLE_SSE
typedef float v4sf __attribute__ ((vector_size (16)));
typedef int v4si __attribute__ ((vector_size (16)));
typedef int v2si __attribute__ ((vector_size (8)));

static inline int  
freebob_decode_events_to_stream(freebob_connection_t *connection,
								freebob_stream_t *stream, 
								quadlet_t* events, 
								unsigned int nsamples,
								unsigned int dbc
								) {
	quadlet_t *target_event;
	int do_ringbuffer_write=0;
	
	assert (stream);
	assert (connection);
	assert (events);
	assert (stream->spec.position < connection->spec.dimension);
		
	freebob_sample_t *buffer=NULL;
	float *floatbuff=NULL;

	int dimension=connection->spec.dimension;
	
	static const float multiplier = 1.0f / (float)(0x7FFFFF);
	static const float sse_multiplier[4] __attribute__((aligned(16))) = {
		1.0f / (float)(0x7FFFFF),
		1.0f / (float)(0x7FFFFF),
		1.0f / (float)(0x7FFFFF),
		1.0f / (float)(0x7FFFFF)
	};
	unsigned int tmp[4];

	assert(stream->user_buffer);
	
	switch(stream->buffer_type) {
		case freebob_buffer_type_per_stream:
		default:
			// use the preallocated buffer (at init time)
			buffer=((freebob_sample_t *)(stream->user_buffer))+stream->user_buffer_position;
			
			do_ringbuffer_write=1;
			break;
				
		case freebob_buffer_type_uint24:
			buffer=((freebob_sample_t *)(stream->user_buffer))+stream->user_buffer_position;
			
			do_ringbuffer_write=0;
			break;
				
		case freebob_buffer_type_float:
			floatbuff=((float *)(stream->user_buffer))+stream->user_buffer_position;
			
			do_ringbuffer_write=0;
			break;
	}
	
	int j=0;
	int written=0;
	
	if (stream->spec.format== IEC61883_STREAM_TYPE_MBLA) {
		target_event=(quadlet_t *)(events + stream->spec.position);

		// TODO: convert this into function pointer based
		switch(stream->buffer_type) {
			default:
			case freebob_buffer_type_uint24:
				for(j = 0; j < nsamples; j += 1) { // decode max nsamples
					*(buffer)=(ntohl((*target_event) ) & 0x00FFFFFF);
					buffer++;
					target_event+=dimension;
				}
				break;
			case freebob_buffer_type_float:
				j=0;
				if(nsamples>3) {
					for(j = 0; j < nsamples-3; j += 4) {
						tmp[0] = ntohl(*target_event);
						target_event += dimension;
						tmp[1] = ntohl(*target_event);
						target_event += dimension;
						tmp[2] = ntohl(*target_event);
						target_event += dimension;
						tmp[3] = ntohl(*target_event);
						target_event += dimension;
						asm("pslld $8, %[in2]\n\t" // sign extend 24th bit
								"pslld $8, %[in1]\n\t"
								"psrad $8, %[in2]\n\t"
								"psrad $8, %[in1]\n\t"
								"cvtpi2ps %[in2], %%xmm0\n\t"
								"movlhps %%xmm0, %%xmm0\n\t"
								"cvtpi2ps %[in1], %%xmm0\n\t"
								"mulps %[ssemult], %%xmm0\n\t"
								"movups %%xmm0, %[floatbuff]"
							: [floatbuff] "=m" (*(v4sf*)floatbuff)
							: [in1] "y" (*(v2si*)tmp),
						[in2] "y" (*(v2si*)(tmp+2)),
						[ssemult] "x" (*(v4sf*)sse_multiplier)
							: "xmm0");
						floatbuff += 4;
					}
				}
				for(; j < nsamples; ++j) { // decode max nsamples
					unsigned int v = ntohl(*target_event) & 0x00FFFFFF;
					// sign-extend highest bit of 24-bit int
					int tmp = (int)(v << 8) / 256;
					*floatbuff = tmp * multiplier;

					floatbuff++;
					target_event += dimension;
				}
				asm volatile("emms");
				
				break;
		}

// 		fprintf(stderr,"rb write [%02d: %08p %08p]\n",stream->spec.position, stream, stream->buffer);
	
// 	fprintf(stderr,"rb write [%02d: %08p %08p %08X, %d, %d]\n",stream->spec.position, stream, stream->buffer, *buffer, nsamples, written);
		if(do_ringbuffer_write) {
			// reset the buffer pointer
			buffer=((freebob_sample_t *)(stream->user_buffer))+stream->user_buffer_position;
			written=freebob_ringbuffer_write(stream->buffer, (char *)(buffer), nsamples*sizeof(freebob_sample_t))/sizeof(freebob_sample_t);
		} else {
			written=nsamples;
		}
		
// 	fprintf(stderr,"rb write1[%02d: %08p %08p %08X, %d, %d]\n",stream->spec.position, stream, stream->buffer, *buffer, nsamples, written);
		
		return written;
	
	} else if (stream->spec.format == IEC61883_STREAM_TYPE_MIDI) {
		return nsamples; // for midi streams we always indicate a full write
	
	} else {//if (stream->spec.format == IEC61883_STREAM_TYPE_SPDIF) {
		return nsamples; // for unsupported we always indicate a full read
	
	}
	
	return 0;


}

static inline int 
freebob_encode_stream_to_events(freebob_connection_t *connection,
								freebob_stream_t *stream,
								quadlet_t* events,
								unsigned int nsamples,
								unsigned int dbc
								) {
	quadlet_t *target_event;
	//freebob_sample_t buff[nsamples];
	int do_ringbuffer_read=0;
	
	freebob_sample_t *buffer=NULL;
	float *floatbuff=NULL;

	const float multiplier = (float)(0x7FFFFF00);
	static const float sse_multiplier[4] __attribute__((aligned(16))) = {
		(float)(0x7FFFFF00),
		(float)(0x7FFFFF00),
		(float)(0x7FFFFF00),
		(float)(0x7FFFFF00)
	};

	static const int sse_mask[4] __attribute__((aligned(16))) = {
		0x40000000,  0x40000000,  0x40000000,  0x40000000
	};

	unsigned int out[4];	
	
	int dimension=connection->spec.dimension;
	
	unsigned int j=0;
	unsigned int read=0;

	assert (stream);
	assert (connection);
	assert (events);
	assert (stream->spec.position < connection->spec.dimension);
	
	switch(stream->buffer_type) {
		case freebob_buffer_type_per_stream:
		default:
//			assert(nsamples < dev->options.period_size);
			// use the preallocated buffer (at init time)
			buffer=((freebob_sample_t *)(stream->user_buffer))+stream->user_buffer_position;
			
			do_ringbuffer_read=1;
			break;
				
		case freebob_buffer_type_uint24:
			buffer=((freebob_sample_t *)(stream->user_buffer))+stream->user_buffer_position;
			
			do_ringbuffer_read=0;
			break;
				
		case freebob_buffer_type_float:
			floatbuff=((float *)(stream->user_buffer))+stream->user_buffer_position;
			
			do_ringbuffer_read=0;
			break;
	}
	
	
	if(stream->spec.format == IEC61883_STREAM_TYPE_MBLA) { // MBLA
		target_event=(quadlet_t *)(events + (stream->spec.position));
		
		if(do_ringbuffer_read) {
			read=freebob_ringbuffer_read(stream->buffer, (char *)buffer, nsamples*sizeof(freebob_sample_t))/sizeof(freebob_sample_t);
		} else {
			read=nsamples;
		}
		
		// TODO: convert this into function pointer based
		switch(stream->buffer_type) {
			default:
			case freebob_buffer_type_uint24:
				for(j = 0; j < read; j += 1) { // decode max nsamples
					*target_event = htonl((*(buffer) & 0x00FFFFFF) | 0x40000000);
					buffer++;
					target_event+=dimension;
					
		//			fprintf(stderr,"[%03d, %02d: %08p %08X %08X]\n",j, stream->spec.position, target_event, *target_event, *buffer);

				}
				break;
			case freebob_buffer_type_float:
/*				for(j = 0; j < read; j += 1) { // decode max nsamples
					// don't care for overflow
					float v = *floatbuff * multiplier;  // v: -231 .. 231
					unsigned int tmp = ((int)v);
					*target_event = htonl((tmp >> 8) | 0x40000000);
					
					floatbuff++;
					target_event += connection->spec.dimension;
				}*/
				j=0;
				if(read>3) {
					for (j = 0; j < read-3; j += 4) {
						asm("movups %[floatbuff], %%xmm0\n\t"
								"mulps %[ssemult], %%xmm0\n\t"
								"cvttps2pi %%xmm0, %[out1]\n\t"
								"movhlps %%xmm0, %%xmm0\n\t"
								"psrld $8, %[out1]\n\t"
								"cvttps2pi %%xmm0, %[out2]\n\t"
								"por %[mmxmask], %[out1]\n\t"
								"psrld $8, %[out2]\n\t"
								"por %[mmxmask], %[out2]\n\t"
							: [out1] "=&y" (*(v2si*)&out[0]),
						[out2] "=&y" (*(v2si*)&out[2])
							: [floatbuff] "m" (*(v4sf*)floatbuff),
						[ssemult] "x" (*(v4sf*)sse_multiplier),
						[mmxmask] "y" (*(v2si*)sse_mask)
							: "xmm0");
						floatbuff += 4;
						*target_event = htonl(out[0]);
						target_event += dimension;
						*target_event = htonl(out[1]);
						target_event += dimension;
						*target_event = htonl(out[2]);
						target_event += dimension;
						*target_event = htonl(out[3]);
						target_event += dimension;
					}
				}
				for(; j < read; ++j) {
				// don't care for overflow
					float v = *floatbuff * multiplier;  // v: -231 .. 231
					unsigned int tmp = (int)v;
					*target_event = htonl((tmp >> 8) | 0x40000000);
		
					floatbuff++;
					target_event += dimension;
				}

				asm volatile("emms");
				break;
		}
		
		/*		
		for(j = 0; j < nsamples; j+=1) {
			*target_event = htonl((*(buffer) & 0x00FFFFFF) | 0x40000000);
			buffer++;
			target_event+=connection->spec.dimension;
		}
		*/
		
		return read;
		
	} else if (stream->spec.format == IEC61883_STREAM_TYPE_MIDI) {
		return nsamples; // for midi we always indicate a full read
	
	} else { //if (stream->spec.format == IEC61883_STREAM_TYPE_SPDIF) {
		return nsamples; // for unsupported we always indicate a full read
	
	}
	return 0;

}

#else

static inline int  
freebob_decode_events_to_stream(freebob_connection_t *connection,
								freebob_stream_t *stream, 
								quadlet_t* events, 
								unsigned int nsamples,
								unsigned int dbc
								) {
	quadlet_t *target_event;
	int do_ringbuffer_write=0;
	
	assert (stream);
	assert (connection);
	assert (events);
	assert (stream->spec.position < connection->spec.dimension);
		
	freebob_sample_t *buffer=NULL;
	float *floatbuff=NULL;

	const float multiplier = 1.0f / (float)(0x7FFFFF);
	
	int dimension=connection->spec.dimension;
	
	assert(stream->user_buffer);
	
	switch(stream->buffer_type) {
		case freebob_buffer_type_per_stream:
		default:
//			assert(nsamples < dev->options.period_size);
			
			// use the preallocated buffer (at init time)
			buffer=((freebob_sample_t *)(stream->user_buffer))+stream->user_buffer_position;
			
			do_ringbuffer_write=1;
			break;
				
		case freebob_buffer_type_uint24:
			buffer=((freebob_sample_t *)(stream->user_buffer))+stream->user_buffer_position;
			
			do_ringbuffer_write=0;
			break;
				
		case freebob_buffer_type_float:
			floatbuff=((float *)(stream->user_buffer))+stream->user_buffer_position;
			
			do_ringbuffer_write=0;
			break;
	}
	
	int j=0;
	int written=0;
	
	if (stream->spec.format== IEC61883_STREAM_TYPE_MBLA) {
		target_event=(quadlet_t *)(events + stream->spec.position);

		// TODO: convert this into function pointer based
		switch(stream->buffer_type) {
			default:
			case freebob_buffer_type_uint24:
				for(j = 0; j < nsamples; j += 1) { // decode max nsamples
					*(buffer)=(ntohl((*target_event) ) & 0x00FFFFFF);
		//			fprintf(stderr,"[%03d, %02d: %08p %08X %08X]\n",j, stream->spec.position, target_event, *target_event, *buffer);
					buffer++;
					target_event+=dimension;
				}
				break;
			case freebob_buffer_type_float:
				for(j = 0; j < nsamples; j += 1) { // decode max nsamples		

					unsigned int v = ntohl(*target_event) & 0x00FFFFFF;
					// sign-extend highest bit of 24-bit int
					int tmp = (int)(v << 8) / 256;
		
					*floatbuff = tmp * multiplier;
				
					floatbuff++;
					target_event+=dimension;
				}
				break;
		}

// 		fprintf(stderr,"rb write [%02d: %08p %08p]\n",stream->spec.position, stream, stream->buffer);
	
// 	fprintf(stderr,"rb write [%02d: %08p %08p %08X, %d, %d]\n",stream->spec.position, stream, stream->buffer, *buffer, nsamples, written);
		if(do_ringbuffer_write) {
			// reset the buffer pointer
			buffer=((freebob_sample_t *)(stream->user_buffer))+stream->user_buffer_position;
			written=freebob_ringbuffer_write(stream->buffer, (char *)(buffer), nsamples*sizeof(freebob_sample_t))/sizeof(freebob_sample_t);
		} else {
			written=nsamples;
		}
		
// 	fprintf(stderr,"rb write1[%02d: %08p %08p %08X, %d, %d]\n",stream->spec.position, stream, stream->buffer, *buffer, nsamples, written);
		
		return written;
	
	} else if (stream->spec.format == IEC61883_STREAM_TYPE_MIDI) {
		return nsamples; // for midi streams we always indicate a full write
	
	} else {//if (stream->spec.format == IEC61883_STREAM_TYPE_SPDIF) {
		return nsamples; // for unsupported we always indicate a full read
	
	}
	
	return 0;


}

static inline int 
freebob_encode_stream_to_events(freebob_connection_t *connection,
								freebob_stream_t *stream,
								quadlet_t* events,
								unsigned int nsamples,
								unsigned int dbc
								) {
	quadlet_t *target_event;
	//freebob_sample_t buff[nsamples];
	int do_ringbuffer_read=0;
	
	freebob_sample_t *buffer=NULL;
	float *floatbuff=NULL;

	const float multiplier = (float)(0x7FFFFF00);
	int dimension=connection->spec.dimension;
	
	unsigned int j=0;
	unsigned int read=0;

	assert (stream);
	assert (connection);
	assert (events);
	assert (stream->spec.position < connection->spec.dimension);
	
	switch(stream->buffer_type) {
		case freebob_buffer_type_per_stream:
		default:
//			assert(nsamples < dev->options.period_size);
			// use the preallocated buffer (at init time)
			buffer=((freebob_sample_t *)(stream->user_buffer))+stream->user_buffer_position;
			
			do_ringbuffer_read=1;
			break;
				
		case freebob_buffer_type_uint24:
			buffer=((freebob_sample_t *)(stream->user_buffer))+stream->user_buffer_position;
			
			do_ringbuffer_read=0;
			break;
				
		case freebob_buffer_type_float:
			floatbuff=((float *)(stream->user_buffer))+stream->user_buffer_position;
			
			do_ringbuffer_read=0;
			break;
	}
	
	
	if(stream->spec.format == IEC61883_STREAM_TYPE_MBLA) { // MBLA
		target_event=(quadlet_t *)(events + (stream->spec.position));
		
		if(do_ringbuffer_read) {
			read=freebob_ringbuffer_read(stream->buffer, (char *)buffer, nsamples*sizeof(freebob_sample_t))/sizeof(freebob_sample_t);
		} else {
			read=nsamples;
		}
		
		// TODO: convert this into function pointer based
		switch(stream->buffer_type) {
			default:
			case freebob_buffer_type_uint24:
				for(j = 0; j < read; j += 1) { // decode max nsamples
					*target_event = htonl((*(buffer) & 0x00FFFFFF) | 0x40000000);
					buffer++;
					target_event+=dimension;
					
		//			fprintf(stderr,"[%03d, %02d: %08p %08X %08X]\n",j, stream->spec.position, target_event, *target_event, *buffer);

				}
				break;
			case freebob_buffer_type_float:
				for(j = 0; j < read; j += 1) { // decode max nsamples
					// don't care for overflow
					float v = *floatbuff * multiplier;  // v: -231 .. 231
					unsigned int tmp = ((int)v);
					*target_event = htonl((tmp >> 8) | 0x40000000);
					
					floatbuff++;
					target_event += dimension;
				}
				break;
		}
		
		/*		
		for(j = 0; j < nsamples; j+=1) {
			*target_event = htonl((*(buffer) & 0x00FFFFFF) | 0x40000000);
			buffer++;
			target_event+=connection->spec.dimension;
		}
		*/
		
		return read;
		
	} else if (stream->spec.format == IEC61883_STREAM_TYPE_MIDI) {
		return nsamples; // for midi we always indicate a full read
	
	} else { //if (stream->spec.format == IEC61883_STREAM_TYPE_SPDIF) {
		return nsamples; // for unsupported we always indicate a full read
	
	}
	return 0;

}
#endif

/* 
 * write received events to the stream ringbuffers.
 */

int freebob_am824_recv(char *data, 
					   int nevents, unsigned int offset, unsigned int dbc,
			       freebob_connection_t *connection)
{
	int xrun=0;
	unsigned int i=0;
	
	for(i = 0; i < connection->nb_streams; i++) {
		freebob_stream_t *stream = &(connection->streams[i]);
		size_t written=0;
		
		assert(stream);
		
		stream->user_buffer_position=offset;
		
		debugPrintShort(DEBUG_LEVEL_HANDLERS,"[%d: %d ",i,freebob_ringbuffer_write_space(stream->buffer));
		written = freebob_decode_events_to_stream(
			connection,
			stream,
			(quadlet_t *)data,
			nevents,
			dbc
			);

		if (written < nevents) {
			xrun++;
		}
		
		debugPrintShort(DEBUG_LEVEL_HANDLERS, "%d %d]",written,freebob_ringbuffer_write_space(stream->buffer));
	}
	
// 	connection->status.frames_left-=nevents;
		
	debugPrintShort(DEBUG_LEVEL_HANDLERS, "\n");
	
	return xrun;
}

/* 
 * get xmit events from ringbuffer
 */

int freebob_am824_xmit(	char *data, 
						int nevents, unsigned int offset, unsigned int dbc, 
						freebob_connection_t *connection)
{
	int xrun=0;
	unsigned int i=0;
	
	for(i = 0; i < connection->nb_streams; i++) {
		freebob_stream_t *stream = &(connection->streams[i]);
		size_t read=0;
		
		assert(stream);
		debugPrintShort(DEBUG_LEVEL_HANDLERS, "[%d: %d ",i,freebob_ringbuffer_read_space(stream->buffer));
		
		stream->user_buffer_position=offset;
		
		read = freebob_encode_stream_to_events(
			connection,
			stream,
			(quadlet_t *)data,
			nevents,
			dbc
			);
		
		debugPrintShort(DEBUG_LEVEL_HANDLERS, "%d %d]",read,freebob_ringbuffer_read_space(stream->buffer));

		if (read < nevents) {
			xrun++;
		}
		
	}
	debugPrintShort(DEBUG_LEVEL_HANDLERS, "\n");
	
// 	connection->status.frames_left-=nevents;
	
	return xrun;
}

void *freebob_streaming_watchdog_thread (void *arg)
{
	freebob_device_t *dev = (freebob_device_t *) arg;

	dev->watchdog_check = 0;

	while (1) {
		sleep (2);
		if (dev->watchdog_check == 0) {

			printError("watchdog: timeout");

			/* kill our process group, try to get a dump */
			kill (-getpgrp(), SIGABRT);
			/*NOTREACHED*/
			exit (1);
		}
		dev->watchdog_check = 0;
	}
}

int freebob_streaming_start_watchdog (freebob_device_t *dev)
{
	int watchdog_priority = dev->packetizer.priority + 10;
	int max_priority = sched_get_priority_max (SCHED_FIFO);

	debugPrint(DEBUG_LEVEL_STARTUP, "Starting Watchdog...\n");
	
	if ((max_priority != -1) &&
			(max_priority < watchdog_priority))
		watchdog_priority = max_priority;
	
	if (freebob_streaming_create_thread (dev, &dev->watchdog_thread, watchdog_priority,
		TRUE, freebob_streaming_watchdog_thread, dev)) {
			printError ("cannot start watchdog thread");
			return -1;
		}

	return 0;
}

void freebob_streaming_stop_watchdog (freebob_device_t *dev)
{
	debugPrint(DEBUG_LEVEL_STARTUP, "Stopping Watchdog...\n");
	
	pthread_cancel (dev->watchdog_thread);
	pthread_join (dev->watchdog_thread, NULL);
}
