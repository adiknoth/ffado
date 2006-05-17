/* handlers.c
  Copyright (C) 2006 Pieter Palmers
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

  Thread creation function including workarounds for real-time scheduling
  behaviour on different glibc versions.

*/

#include "libfreebob/freebob_streaming.h"
#include "freebob_streaming_private.h"
#include "freebob_connections.h"
#include "freebob_debug.h"
#include "thread.h"
#include "handlers.h"

#include <signal.h>
#include <unistd.h>

static inline int freebob_streaming_decode_midi(freebob_connection_t *connection,
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

static inline int freebob_streaming_encode_midi(freebob_connection_t *connection,
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

enum raw1394_iso_disposition 
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

	if((connection->status.frames_left<=0)) {
		connection->pfd->events=0;
		return RAW1394_ISO_DEFER;
	}

	return retval;
}


enum raw1394_iso_disposition 
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
enum raw1394_iso_disposition 
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

enum raw1394_iso_disposition 
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

				// retry this packed 
				retval=RAW1394_ISO_AGAIN;
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

