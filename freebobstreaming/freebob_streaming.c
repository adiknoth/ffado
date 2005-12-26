/* $Id$ */

/*
 *   FreeBob Streaming API
 *   FreeBob = Firewire (pro-)audio for linux
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
 * Implementation of the FreeBob Streaming API
 *
 */

#include "freebob_streaming.h"
#include "freebob_streaming_private.h"
#include "freebob_connections.h"
#include "freebob_debug.h"

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
			

freebob_device_t *freebob_streaming_init (freebob_device_info_t *device_info, freebob_options_t options) {
	int i;
	int c;
	int err=0;
	freebob_device_t* dev=NULL;
	
	assert(device_info);
	
	printMessage("FreeBob Streaming Device Init\n");
	printMessage(" Using FreeBobCtl lib version %s\n",freebobctl_get_version());
	printMessage(" Device information:\n");
	
	switch (device_info->location_type) {
	case freebob_osc:
		printMessage("  OSC URL: %s\n",device_info->xml_location);
		break;
	case freebob_file:
		printMessage("  XML file: %s\n",device_info->xml_location);
		break;
	default:
		printMessage("  Other location type.\n");
	}
	
	printMessage(" Device options:\n");
	/* driver related setup */
	printMessage("  Samplerate  : %d\n",options.sample_rate);
	printMessage("  Period Size : %d\n",options.period_size);
	printMessage("  Nb Buffers  : %d\n",options.nb_buffers);
	printMessage("  RAW1394 ISO Buffers      : %d\n",options.iso_buffers);
	printMessage("  RAW1394 ISO Prebuffers   : %d\n",options.iso_prebuffers);
	printMessage("  RAW1394 ISO IRQ Interval : %d\n",options.iso_irq_interval);
	printMessage("\n");
	
	// initialize the freebob_device
	// allocate memory
	dev=calloc(1,sizeof(freebob_device_t));
	
	if(!dev) {
		printError("FREEBOB: cannot allocate memory for dev structure!\n");
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
	freebob_connection_info_t *freebobctl_capture_connections=NULL;
	freebob_connection_info_t *freebobctl_playback_connections=NULL;

	switch (dev->device_info.location_type) {
	case freebob_osc:
		printMessage("Reading from OSC URL: %s\n",dev->device_info.xml_location);
		freebobctl_capture_connections=freebobctl_get_connection_info_from_osc(dev->device_info.xml_location, 0);
		freebobctl_playback_connections=freebobctl_get_connection_info_from_osc(dev->device_info.xml_location, 1);
		break;
	case freebob_file:
		printMessage("Reading from XML file: %s\n",dev->device_info.xml_location);
		freebobctl_capture_connections=freebobctl_get_connection_info_from_xml_file(dev->device_info.xml_location, 0);
		freebobctl_playback_connections=freebobctl_get_connection_info_from_xml_file(dev->device_info.xml_location, 1);
		break;
	default:
		printError("Faulty location type!\n");
		free(dev);
		return NULL;
		
	}
	
	if (freebobctl_capture_connections) {
		dev->nb_connections_capture=freebobctl_capture_connections->nb_connections;
		// FIXME:
		//dev->nb_connections_capture=0;
	} else {
		dev->nb_connections_capture=0;
	}	
	
	if (freebobctl_playback_connections) {
		dev->nb_connections_playback=freebobctl_playback_connections->nb_connections;
		// FIXME:
// 		dev->nb_connections_playback=0;
	} else {
		dev->nb_connections_playback=0;
	}
	
	dev->nb_connections=dev->nb_connections_playback+dev->nb_connections_capture;
	/* see if there are any connections */
	if (!dev->nb_connections) {
		printError("No connections specified, bailing out\n");
		if(freebobctl_capture_connections) { free(freebobctl_capture_connections); freebobctl_capture_connections=NULL; }
		if(freebobctl_playback_connections) { free(freebobctl_playback_connections); freebobctl_playback_connections=NULL; }
		free(dev);
		
		return NULL;
	}
	
	dev->connections=calloc(dev->nb_connections_playback+dev->nb_connections_capture, sizeof(freebob_connection_t));
	
	
	for (c=0;c<dev->nb_connections_capture;c++) {
		memcpy(&dev->connections[c].spec, freebobctl_capture_connections->connections[c], sizeof(freebob_connection_spec_t));
		dev->connections[c].spec.direction=FREEBOB_CAPTURE;
	}
	for (c=0;c<dev->nb_connections_playback;c++) {
		memcpy(&dev->connections[c+dev->nb_connections_capture].spec, freebobctl_playback_connections->connections[c], sizeof(freebob_connection_spec_t));
		dev->connections[c+dev->nb_connections_capture].spec.direction=FREEBOB_PLAYBACK;
	}
	
	if(freebobctl_capture_connections) { free(freebobctl_capture_connections); freebobctl_capture_connections=NULL; }
	if(freebobctl_playback_connections) { free(freebobctl_playback_connections); freebobctl_playback_connections=NULL; }
	
	/* Figure out a master connection.
	 * Either it is given in the spec libfreebobctl
	 * Or it is the first connection defined (capture connections first)
	 */
	int master_found=FALSE;
	
	for (c=0;c<dev->nb_connections_capture+dev->nb_connections_playback;c++) {
		if (dev->connections[c].spec.is_master==TRUE) {
			master_found=TRUE;
			break;
		}
	}
	
	if((!master_found) && (dev->nb_connections_capture+dev->nb_connections_playback > 0)) {
		dev->connections[0].spec.is_master=TRUE;
	}

	// initialize all connections
	for(i=0; i < dev->nb_connections; i++) {
		freebob_connection_t *connection= &(dev->connections[i]);
		if ((err=freebob_streaming_init_connection(dev, connection))<0) {
			printError("FREEBOB: failed to init connection %d\n",i);
			break;
		}
	}
	
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
		free(dev->connections);
		free(dev);
		return NULL;
	}
	
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
	free(dev->connections);
	free(dev);
	
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
		return snprintf (buffer, buffersize, "%s_%d_%d_%d_%s",
						"cap", (int) stream->parent->spec.port, stream->parent->spec.node & 0x3F, stream->parent->spec.plug, stream->spec.name);		
	} else {
		return -1;
	}
}

int freebob_streaming_get_playback_stream_name(freebob_device_t *dev, int i, char* buffer, size_t buffersize) {
	freebob_stream_t *stream;
	if(i<dev->nb_playback_streams) {
		stream=*(dev->playback_streams+i);
		return snprintf (buffer, buffersize, "%s_%d_%d_%d_%s",
						"pbk", (int) stream->parent->spec.port, stream->parent->spec.node & 0x3F, stream->parent->spec.plug, stream->spec.name);		
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
			return freebob_audio;
		case IEC61883_STREAM_TYPE_MIDI:
			return freebob_midi;
		case IEC61883_STREAM_TYPE_SPDIF:
		default:
			return freebob_unknown;
		}
	} else {
		return freebob_invalid;
	}
}

freebob_streaming_stream_type freebob_streaming_get_playback_stream_type(freebob_device_t *dev, int i) {
	freebob_stream_t *stream;
	if(i<dev->nb_playback_streams) {
		stream=*(dev->playback_streams+i);
		switch (stream->spec.format) {
		case IEC61883_STREAM_TYPE_MBLA:
			return freebob_audio;
		case IEC61883_STREAM_TYPE_MIDI:
			return freebob_midi;
		case IEC61883_STREAM_TYPE_SPDIF:
		default:
			return freebob_unknown;
		}
	} else {
		return freebob_invalid;
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
	
	// FIXME: packetizer thread
	dev->packetizer.run=1;
	
	if (freebob_streaming_create_thread(&dev->packetizer.transfer_thread, 10, 1, freebob_iso_packet_iterator, (void *)dev)) {
		printError("FREEBOB: cannot create packet transfer thread");
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
	
	pthread_join (dev->packetizer.transfer_thread, &status);
	
	// cleanup semaphores
	sem_destroy(&dev->packetizer.transfer_boundary);

	return 0;
}


int freebob_streaming_start(freebob_device_t *dev) {
	int err;
	int i;
	
	//SEGFAULT
	
	for(i=0; i < dev->nb_connections; i++) {
		err=0;
		freebob_connection_t *connection= &(dev->connections[i]);
		
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
				debugPrint(DEBUG_LEVEL_STARTUP, "Init ISO master receive handler on channel %d...\n",connection->iso.iso_channel);
				debugPrint(DEBUG_LEVEL_STARTUP, "   (BUFFER=%d,PACKET_MAX=%d,IRQ=%d)...\n",connection->iso.buffers,AMDTP_MAX_PACKET_SIZE, connection->iso.irq_interval);
				raw1394_iso_recv_init(
					connection->raw_handle, 
					iso_master_receive_handler, 
					connection->iso.buffers, 
					AMDTP_MAX_PACKET_SIZE, 
					connection->iso.iso_channel, 
					RAW1394_DMA_BUFFERFILL, 
					connection->iso.irq_interval);
										
				dev->sync_master_connection=connection;
				connection->status.master=NULL;
				
			} else {
				//slave receive connection
				debugPrint(DEBUG_LEVEL_STARTUP, "Init ISO slave receive handler on channel %d...\n",connection->iso.iso_channel);
				debugPrint(DEBUG_LEVEL_STARTUP, "   (BUFFER=%d,PACKET_MAX=%d,IRQ=%d)...\n", connection->iso.buffers, AMDTP_MAX_PACKET_SIZE, connection->iso.irq_interval);
				raw1394_iso_recv_init(	
					connection->raw_handle, 
					iso_slave_receive_handler, 
					connection->iso.buffers, 
					AMDTP_MAX_PACKET_SIZE, 
					connection->iso.iso_channel, 
					RAW1394_DMA_BUFFERFILL, 
					connection->iso.irq_interval);
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
				debugPrint(DEBUG_LEVEL_STARTUP, "Init ISO master transmit handler on channel %d...\n",connection->iso.iso_channel);
				debugPrint(DEBUG_LEVEL_STARTUP, "   other mode (BUFFER=%d,PACKET_MAX=%d,IRQ=%d)...\n",connection->iso.buffers,AMDTP_MAX_PACKET_SIZE, connection->iso.irq_interval);
				
				raw1394_iso_xmit_init(
					connection->raw_handle, 
					iso_master_transmit_handler, 
					connection->iso.buffers,
					AMDTP_MAX_PACKET_SIZE, 
					connection->iso.iso_channel, 
					RAW1394_ISO_SPEED_400, 
					connection->iso.irq_interval);

				dev->sync_master_connection=connection;
				connection->status.master=NULL;
			
			} else {
			
				debugPrint(DEBUG_LEVEL_STARTUP, "Init ISO slave transmit handler on channel %d...\n",connection->iso.iso_channel);
				debugPrint(DEBUG_LEVEL_STARTUP, "   (BUFFER=%d,PACKET_MAX=%d,IRQ=%d)...\n",connection->iso.buffers,AMDTP_MAX_PACKET_SIZE, connection->iso.irq_interval);
				raw1394_iso_xmit_init(
					connection->raw_handle, 
					iso_slave_transmit_handler, 
					connection->iso.buffers,
					AMDTP_MAX_PACKET_SIZE, 
					connection->iso.iso_channel, 
					RAW1394_ISO_SPEED_400, 
					connection->iso.irq_interval);
			}
			
			int fdf, syt_interval;
// FIXME:		
			int samplerate=dev->options.sample_rate;
//			int samplerate=connection->spec.samplerate;

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

		break;
		}
		
		if(connection->iso.iso_channel<0) {
			printError("Could not do CMP for connection %d\n",i);
			i-=1;
			while(i>=0) {
				connection= &(dev->connections[i]);
		
				debugPrint(DEBUG_LEVEL_STARTUP, "Shutdown connection %d on channel %d ...\n", i, connection->iso.iso_channel);
				
				raw1394_iso_shutdown(connection->raw_handle);
		
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
		printError("FREEBOB: no sync master connection present!\n");
		// TODO: cleanup
		//freebob_streaming_stop(driver);
		return -1;
	} else if (sync_masters_present > 1) {
		printError("FREEBOB: too many sync master connections present! (%d)\n",sync_masters_present);
		// TODO: cleanup
		//freebob_streaming_stop(driver);
		return -1;
	}
	
	// put nb_periods*period_size of null frames into the playback buffers
	if((err=freebob_streaming_prefill_playback_streams(dev))<0) {
		printError("Could not prefill playback streams.\n");
		return err;
	}
		
	debugPrint(DEBUG_LEVEL_STARTUP,"Armed...\n");
	
	freebob_streaming_start_iso(dev);
	
	freebob_streaming_start_thread(dev);

	return 0;

}

int freebob_streaming_stop(freebob_device_t *dev) {
	unsigned int i;
	

	freebob_streaming_stop_thread(dev);
	
	freebob_streaming_stop_iso(dev);

	// stop ISO xmit/receive
	for(i=0; i < dev->nb_connections; i++) {
		freebob_connection_t *connection= &(dev->connections[i]);

		debugPrint(DEBUG_LEVEL_STARTUP, "Shutdown connection %d on channel %d ...\n", i, connection->iso.iso_channel);
		
		raw1394_iso_shutdown(connection->raw_handle);

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
	
	// clear the xrun flag
	dev->xrun_detected=FALSE;
	
	//freebob_streaming_start_iso(dev);
	
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
		ret=-freebob_streaming_xrun_recovery(dev);
	} else {
		ret=dev->options.period_size;
	}
	
	return ret;
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
	int retval;
	
	freebob_stream_t *stream;
	assert(i<dev->nb_capture_streams);
	
	stream=*(dev->capture_streams+i);
	assert(stream);
// 	fprintf(stderr,"rb read  [%02d: %08p %08p %08X, %d, %d]\n",stream->spec.position, stream, stream->buffer, *buffer, nsamples, retval);
	
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
			printError("FREEBOB: stream already registered\n");
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
	}
	printExit();
	return;
}

static inline int
freebob_streaming_xrun_detected (freebob_device_t *dev, freebob_connection_t **connections, int nb_connections) {
	int i;
	
	for(i=0; i < nb_connections; i++) {
		assert((connections[i]));
		if (connections[i]->status.xruns>0) {
			return TRUE;
		}
	}
	return FALSE;
}

int freebob_streaming_start_iso(freebob_device_t *dev) {
	int err;
	unsigned int c;

	// start ISO xmit/receive
	for(c=0; c < dev->nb_connections; c++) {
		err=0;
		freebob_connection_t *connection= &(dev->connections[c]);
		if (connection->spec.direction == FREEBOB_CAPTURE) {
			debugPrint(DEBUG_LEVEL_STARTUP, "Start ISO receive for connection %d on channel %d ...\n", c, connection->iso.iso_channel);
			
			err = raw1394_iso_recv_start(
				connection->raw_handle, 
				0, // connection->iso.startcycle, 
				-1,//IEC61883_TAG_WITH_CIP, 
				0);

			if (err) {
				printError("FREEBOB: couldn't start receiving: %s\n",
					    strerror (errno));
				// TODO: cleanup
				return err;
			}					
		
		} else if (connection->spec.direction == FREEBOB_PLAYBACK) {
			debugPrintWithTimeStamp(DEBUG_LEVEL_STARTUP, "Start ISO transmit for connection %d on channel %d\n", c, connection->iso.iso_channel);
			
			err=raw1394_iso_xmit_start(
				connection->raw_handle, 
				12, // connection->iso.startcycle, 
				connection->iso.prebuffers);

			if (err) {
				printError("FREEBOB: couldn't start transmitting: %s\n",
					    strerror (errno));
				// TODO: cleanup
				return err;
			}
		}
	}	
	return 0;
}

int freebob_streaming_stop_iso(freebob_device_t *dev) {
	unsigned int c;
	// stop ISO xmit/receive
	for(c=0; c < dev->nb_connections; c++) {
		freebob_connection_t *connection= &(dev->connections[c]);

		debugPrintWithTimeStamp(DEBUG_LEVEL_STARTUP, "Stop connection %d on channel %d ...\n", c, connection->iso.iso_channel);
		
		raw1394_iso_stop(connection->raw_handle);
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
	int cycle=0;
	unsigned int c;
	int underrun_detected=0;
	
	freebob_connection_t *connection=NULL;
	
	assert(dev);
	assert(dev->sync_master_connection);
	assert(dev->connections);
	
	debugPrint(DEBUG_LEVEL_STARTUP, "Entering packetizer thread...\n");
	
	//freebob_streaming_start_iso(dev);
	
	// when starting the thread, we should wait with iterating the slave connections
	// until the master connection has processed some samples, because the devices
	// tend to start with a stream of no-data packets, leading to xruns on slave
	// transmit, leading to multiple restarts of the thread, which doesn't work correctly (yet)
	debugPrint(DEBUG_LEVEL_STARTUP, "Waiting for the sync master...\n");
	
	connection=dev->sync_master_connection;
	while(connection->status.events==0) {
		err=0;
		
		// get a packet on the sync master connection
		err=raw1394_loop_iterate(connection->raw_handle);
		
		if (err == -1) {
			printError("Possible raw1394 error: %s on sync master connection: %d\n",
				strerror (errno),connection->spec.id);
				
			dev->packetizer.status = -2;
			dev->packetizer.retval = 0;
			break;
		}
		
		// detect underruns on the sync master connection
		if (freebob_streaming_xrun_detected(dev,&connection,1)) {
			printError("Xrun on sync master connection %d\n", connection->spec.id);
			underrun_detected=TRUE;
			break; // we can exit as the underrun handler will flush the buffers anyway
		}
	}
	
	debugPrint(DEBUG_LEVEL_STARTUP, "Go Go Go!!!\n");
	
	//sem_post(&dev->packetizer.transfer_ack);

	while (dev->packetizer.run && !underrun_detected) {
		//sem_wait(&dev->packetizer.transfer_ack);
		
		freebob_streaming_period_reset(dev);
		
		while((	!freebob_streaming_period_complete(dev) 
				&& !underrun_detected
				&& (dev->packetizer.run))) {
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
		
			for(c=0; c<dev->nb_connections; c++) {

				connection = &(dev->connections[c]);
				
				// skip the sync master and the connections that are finished
				if ((connection == dev->sync_master_connection) || (connection->status.frames_left <= 0))
					continue;
				
				err=raw1394_loop_iterate(connection->raw_handle);
				
				if (err == -1) {
					printError("Possible raw1394 error: %s on connection: %d\n",
						strerror (errno),connection->spec.id);
					dev->packetizer.status = -2;
					dev->packetizer.retval = 0;
				}
				
				// detect underruns
				if (freebob_streaming_xrun_detected(dev,&connection,1)) {
					printError("Xrun on slave connection %d\n", connection->spec.id);
					underrun_detected=TRUE;
					break; // we can exit as the underrun handler will flush the buffers anyway
				}
			}
			
			cycle++;
		}
		
		if(underrun_detected) {
			dev->xrun_detected=TRUE;
			//underrun_detected=0;
		}
		
		// notify the waiting thread
#ifdef DEBUG
		debugPrint(DEBUG_LEVEL_HANDLERS, "Post semaphore ");
		for(c=0; c<dev->nb_connections; c++) {
			connection = &(dev->connections[c]);
			debugPrintShort(DEBUG_LEVEL_HANDLERS,"[%d: %d]",c,connection->status.frames_left);
		}
		debugPrintShort(DEBUG_LEVEL_HANDLERS,"\n");
#endif
		sem_post(&dev->packetizer.transfer_boundary);
		
		
#ifdef DEBUG
			// update the packet counter
		if((dev->sync_master_connection->status.packets - dev->sync_master_connection->status.total_packets_prev) > 1024*4) {
	//	if(1) {
			unsigned int i;
			debugPrintShort(DEBUG_LEVEL_PACKETCOUNTER,"\r -> ");
			
			for(i=0; i < dev->nb_connections; i++) {
				freebob_connection_t *connection= &(dev->connections[i]);
				assert(connection);
				
				/* Debug info format:
				* [direction, packetcount, bufferfill, packetdrop
				*/
				debugPrintShort(DEBUG_LEVEL_PACKETCOUNTER,"[%s, %02d, %12d, %4d, (%04d)]", 
					(connection->spec.direction==FREEBOB_CAPTURE ? "C" : "P"),
					connection->status.fdf,
					connection->status.packets,
					connection->status.dropped,
					freebob_ringbuffer_read_space(connection->streams[0].buffer)/sizeof(freebob_sample_t)
					);
			}
	
			debugPrintShort(DEBUG_LEVEL_PACKETCOUNTER," XRUNS (%4d)",dev->xrun_count);	
			fflush (stderr);
			
			dev->sync_master_connection->status.total_packets_prev=dev->sync_master_connection->status.packets;
		}

#endif

	}
	
	//freebob_streaming_stop_iso(dev);
	
	debugPrint(DEBUG_LEVEL_STARTUP, "Exiting packetizer thread...\n");
	
	
	pthread_exit (0);	
}

static int freebob_am824_recv(char *data, 
			       int nevents, unsigned int dbc,
			       freebob_connection_t *connection);

static int freebob_am824_xmit(char *data, 
			       int nevents, unsigned int dbc,
			       freebob_connection_t *connection);
			       
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
	
	int xrun=0;

	freebob_connection_t *connection=(freebob_connection_t *) raw1394_get_userdata (handle);
	assert(connection);
	
	struct iec61883_packet *packet = (struct iec61883_packet *) data;
	assert(packet);
	
	// FIXME: dropped packets are very bad when transmitting and the other side is sync'ing on that!
	//connection->status.packets+=dropped;
	connection->status.dropped+=dropped;
	
	if((packet->fdf) != 0xFF && (packet->dbs>0) && (length>=2*sizeof(quadlet_t))) {
		unsigned int nevents=((length / sizeof (quadlet_t)) - 2)/packet->dbs;
		
		// add the data payload to the ringbuffer
		xrun= freebob_am824_recv(data + 8, nevents, packet->dbc, connection);
		
		if(xrun) {
			printError("MASTER RCV: Buffer overrun!\n");
			connection->status.xruns++;
			retval=RAW1394_ISO_DEFER;
		} else {
			retval=RAW1394_ISO_OK;
		}
		
		// keep track of the total amount of events received
		connection->status.events+=nevents;
		
		connection->status.fdf=packet->fdf;
		
	} else {
		// discard packet
		// can be important for sync though
	}

	// one packet received
	connection->status.packets++;
	if(packet->dbs) {
		debugPrintWithTimeStamp(DEBUG_LEVEL_HANDLERS_LOWLEVEL, 
			"MASTER RCV: %08d, CH = %d, FDF = %X. SYT = %6d, DBS = %3d, DBC = %3d, FMT = %3d, LEN = %4d (%2d), DROPPED = %6d\n", 
			cycle,
			channel, packet->fdf,packet->syt,packet->dbs,packet->dbc,packet->fmt, length,
			((length / sizeof (quadlet_t)) - 2)/packet->dbs, dropped);
	}
	
	if( (!(connection->status.packets % 4)) || (connection->status.frames_left<=0)) {
		retval=RAW1394_ISO_DEFER;
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
	int nevents = iec61883_cip_fill_header (handle, &connection->status.cip, packet);
	int xrun=0;
	int nsamples=0;
	
	enum raw1394_iso_disposition retval = RAW1394_ISO_OK;
		
// debug
	if(packet->fdf != 0xFF) {
		connection->status.fdf=packet->fdf;
	}

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
		// read the data payload from the ringbuffer
		xrun=freebob_am824_xmit(data+8, nsamples, packet->dbc, connection);
	}
	
	if (xrun) {
		printError("SLAVE XMT: Buffer underrun!\n");
		connection->status.xruns++;
		retval=RAW1394_ISO_DEFER;
		nsamples=0;
	}
	
	*length = nsamples * connection->spec.dimension * sizeof (quadlet_t) + 8;
	*tag = IEC61883_TAG_WITH_CIP;
	*sy = 0;
	
	// keep track of the total amount of events transmitted
	connection->status.events+=nsamples;
		
	// one packet transmitted
	connection->status.packets++;

	if(packet->dbs) {
		debugPrintWithTimeStamp(DEBUG_LEVEL_HANDLERS_LOWLEVEL, 
			"SLAVE XMT: %08d, CH = %d, FDF = %X. SYT = %6d, DBS = %3d, DBC = %3d, FMT = %3d, LEN = %4d (%2d), DROPPED = %4d, info->packets=%4d, events=%4d, %d, %d %d %d\n", cycle, 
			connection->iso.iso_channel,  packet->fdf,packet->syt,packet->dbs,packet->dbc,packet->fmt, *length,((*length / sizeof (quadlet_t)) - 2)/packet->dbs, dropped, 
			connection->status.packets, connection->status.events, connection->status.frames_left, 
			nevents, nsamples,xrun);
	}
		
	if( (!(connection->status.packets % 4)) || (connection->status.frames_left<=0)) {
		retval=RAW1394_ISO_DEFER;
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
	int xrun=0;
	int nsamples=0;
	
	enum raw1394_iso_disposition retval = RAW1394_ISO_OK;
// debug
	if(packet->fdf != 0xFF) {
		connection->status.fdf=packet->fdf;
	}
	
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
		// read the data payload from the ringbuffer
		xrun=freebob_am824_xmit(data+8, nsamples, packet->dbc, connection);
	}
	
	if (xrun) {
		printError("MASTER XMT: Buffer underrun!\n");
		connection->status.xruns++;
		retval=RAW1394_ISO_DEFER;
		/* this is to make sure that the receiving device doesnt receive junk,
		   as part of the package wasn't filled.
		*/
		nsamples=0; 
	}
		
	*length = nsamples * connection->spec.dimension * sizeof (quadlet_t) + 8;
	*tag = IEC61883_TAG_WITH_CIP;
	*sy = 0;
	
	// keep track of the total amount of events transmitted
	connection->status.events+=nsamples;
		
	// one packet transmitted
	connection->status.packets++;

	if(packet->dbs) {
		debugPrintWithTimeStamp(DEBUG_LEVEL_HANDLERS_LOWLEVEL, 
			"MASTER XMT: CH = %d, FDF = %X. SYT = %6d, DBS = %3d, DBC = %3d, FMT = %3d, LEN = %4d (%2d), DROPPED = %4d, info->packets=%4d, events=%4d, %d, %d %d %d\n", 
			connection->iso.iso_channel,  packet->fdf,packet->syt,packet->dbs,packet->dbc,packet->fmt, *length,((*length / sizeof (quadlet_t)) - 2)/packet->dbs, dropped, 
			connection->status.packets, connection->status.events, connection->status.frames_left, 
			nevents, nsamples,xrun);
	}	

	if( (!(connection->status.packets % 4)) || (connection->status.frames_left<=0)) {
		retval=RAW1394_ISO_DEFER;
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
	 * Synced connections have matched data rates, so just receiving and calling the rcv handler would
	 * suffice in this case. As the connection is externally matched to the master connection, the buffer fill
	 * will be ok.
	 */
	 	
	int xrun=0;

	freebob_connection_t *connection=(freebob_connection_t *) raw1394_get_userdata (handle);
	assert(connection);
	
	struct iec61883_packet *packet = (struct iec61883_packet *) data;
	assert(packet);
	
	// FIXME: dropped packets are very bad when transmitting and the other side is sync'ing on that!
	//connection->status.packets+=dropped;
	connection->status.dropped+=dropped;
	
	if((packet->fdf) != 0xFF && (packet->dbs>0) && (length>=2*sizeof(quadlet_t))) {
		unsigned int nevents=((length / sizeof (quadlet_t)) - 2)/packet->dbs;
		connection->status.fdf=packet->fdf;
		
		// add the data payload to the ringbuffer
		xrun= freebob_am824_recv(data + 8, nevents, packet->dbc, connection);
		
		if(xrun) {
			printError("SLAVE RCV: Buffer overrun!\n");
			connection->status.xruns++;
			retval=RAW1394_ISO_DEFER;
		} else {
			retval=RAW1394_ISO_OK;
		}
		
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
			channel, packet->fdf,packet->syt,packet->dbs,packet->dbc,packet->fmt, length,
			((length / sizeof (quadlet_t)) - 2)/packet->dbs, dropped);
	}
		
	if( (!(connection->status.packets % 4)) || (connection->status.frames_left<=0)) {
		retval=RAW1394_ISO_DEFER;
	}
	
    return retval;
}

/* 
 * write received events to the stream ringbuffers.
 */

int freebob_am824_recv(char *data, 
			       int nevents, unsigned int dbc,
			       freebob_connection_t *connection)
{
	int xrun=0;
	unsigned int i=0;
	
	for(i = 0; i < connection->nb_streams; i++) {
		freebob_stream_t *stream = &(connection->streams[i]);
		size_t written=0;
		
		assert(stream);
		
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
	
	connection->status.frames_left-=nevents;
		
	debugPrintShort(DEBUG_LEVEL_HANDLERS, "\n");
	
	return xrun;
}

/* 
 * get xmit events from ringbuffer
 */

int freebob_am824_xmit(	char *data, 
						int nevents, unsigned int dbc, 
						freebob_connection_t *connection)
{
	int xrun=0;
	unsigned int i=0;
	
	for(i = 0; i < connection->nb_streams; i++) {
		freebob_stream_t *stream = &(connection->streams[i]);
		size_t read=0;
		
		assert(stream);
		debugPrintShort(DEBUG_LEVEL_HANDLERS, "[%d: %d ",i,freebob_ringbuffer_read_space(stream->buffer));
		
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
	
	connection->status.frames_left-=nevents;
	
	return xrun;
}

int
freebob_streaming_create_thread (pthread_t* thread,
			   int priority,
			   int realtime,
			   void*(*start_routine)(void*),
			   void* arg)
{
	int result = 0;

	result = pthread_create (thread, 0, start_routine, arg);
	if (result) {
		debugPrint(DEBUG_LEVEL_THREADS,"creating thread with default parameters");
	}
	return result;
}

