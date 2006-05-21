/* $Id$ */

/*
 *   FreeBob Streaming API
 *   FreeBob = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2005,2006 Pieter Palmers <pieterpalmers@users.sourceforge.net>
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

#include "AmdtpStreamProcessor.h"

namespace FreebobStreaming {

IMPL_DEBUG_MODULE( AmdtpTransmitStreamProcessor, AmdtpTransmitStreamProcessor, DEBUG_LEVEL_NORMAL );
IMPL_DEBUG_MODULE( AmdtpReceiveStreamProcessor, AmdtpReceiveStreamProcessor, DEBUG_LEVEL_NORMAL );


/* transmit */
AmdtpTransmitStreamProcessor::AmdtpTransmitStreamProcessor(int channel, int port, int framerate, int dimension)
	: TransmitStreamProcessor(channel, port, framerate), m_dimension(dimension) {


}

AmdtpTransmitStreamProcessor::~AmdtpTransmitStreamProcessor() {
	freebob_ringbuffer_free(m_event_buffer);
	free(m_cluster_buffer);
}

int AmdtpTransmitStreamProcessor::init() {
	int fdf=0, syt_interval=0;
	int m_dbs=0;
	int err=0;

	// call the parent init
	// this has to be done before allocating the buffers, 
	// because this sets the buffersizes from the processormanager
	if((err=((ReceiveStreamProcessor *)this)->init())) {
		debugFatal("Could not allocate memory event ringbuffer (%d)",err);
		return err;
	}


	switch (m_framerate) {
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
		&m_cip_status, 
		IEC61883_FMT_AMDTP, 
		fdf,
		m_framerate, 
		m_dbs, 
		syt_interval);

	// allocate the event buffer
	if( !(m_event_buffer=freebob_ringbuffer_create(
			(m_dimension * m_nb_buffers * m_period) * sizeof(quadlet_t)))) {
		debugFatal("Could not allocate memory event ringbuffer");
// 		return -ENOMEM;
		return -1;
	}

	// allocate the temporary cluster buffer
	if( !(m_cluster_buffer=(char *)calloc(m_dimension,sizeof(quadlet_t)))) {
		debugFatal("Could not allocate temporary cluster buffer");
		freebob_ringbuffer_free(m_event_buffer);
		return -1;
// 		return -ENOMEM;
	}

	// call the parent init
	return ((TransmitStreamProcessor *)this)->init();
}

void AmdtpTransmitStreamProcessor::setVerboseLevel(int l) {
	setDebugLevel(l);
	((TransmitStreamProcessor *)this)->setVerboseLevel(l);
}


int AmdtpTransmitStreamProcessor::getPacket(unsigned char *data, unsigned int *length,
	              unsigned char *tag, unsigned char *sy,
	              int cycle, unsigned int dropped, unsigned int max_length) {

	struct iec61883_packet *packet = (struct iec61883_packet *) data;
	
	// construct the packet cip
	int nevents = iec61883_cip_fill_header (getNodeId(), &m_cip_status, packet);

	enum raw1394_iso_disposition retval = RAW1394_ISO_OK;

	if (!(nevents > 0)) {
		if (m_cip_status.mode == IEC61883_MODE_BLOCKING_EMPTY) {
			*length = 8;
			return (int)RAW1394_ISO_OK;
		}
		else {
			nevents = m_cip_status.syt_interval;
		}
	}
	
	int read_size=nevents*sizeof(quadlet_t)*m_dimension;

	if ((freebob_ringbuffer_read(m_event_buffer,(char *)(data+8),read_size)) < 
				read_size) 
	{
		debugWarning("Transmit buffer underrun\n");
		
		// signal underrun
		m_xruns++;

		retval=RAW1394_ISO_DEFER;
		*length=0;

	} else {
		retval=RAW1394_ISO_OK;
		*length = read_size + 8;
		
		// TODO: we should do all packet buffered processing here
// 		freebob_streaming_encode_midi(connection,(quadlet_t *)(data+8), nevents, packet->dbc);
	}
	
	*tag = IEC61883_TAG_WITH_CIP;
	*sy = 0;
	
	// update the frame counter
	m_framecounter+=nevents;

	return (int)retval;

}

void AmdtpTransmitStreamProcessor::reset() {

	debugOutput( DEBUG_LEVEL_VERBOSE, "Resetting...\n");

	// reset the event buffer, discard all content
	freebob_ringbuffer_reset(m_event_buffer);
	
	// reset all non-device specific stuff
	// i.e. the iso stream and the associated ports
	((TransmitStreamProcessor *)this)->reset();
}

void AmdtpTransmitStreamProcessor::prepare() {

	debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing...\n");

	// prepare all non-device specific stuff
	// i.e. the iso stream and the associated ports
	((TransmitStreamProcessor *)this)->prepare();

	// after preparing, we should transfer the port buffer contents to the event buffer
	int i=m_nb_buffers;
	while(i--) transfer();

}

int AmdtpTransmitStreamProcessor::transfer() {

	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Transferring period...\n");
// TODO: implement

	return 0;
}

/* --------------------- RECEIVE ----------------------- */

AmdtpReceiveStreamProcessor::AmdtpReceiveStreamProcessor(int channel, int port, int framerate, int dimension)
	: ReceiveStreamProcessor(channel, port, framerate), m_dimension(dimension) {


}

AmdtpReceiveStreamProcessor::~AmdtpReceiveStreamProcessor() {
	freebob_ringbuffer_free(m_event_buffer);
	free(m_cluster_buffer);

}

int AmdtpReceiveStreamProcessor::init() {
	int err=0;
	// call the parent init
	// this has to be done before allocating the buffers, 
	// because this sets the buffersizes from the processormanager
	if((err=((ReceiveStreamProcessor *)this)->init())) {
		debugFatal("Could not allocate memory event ringbuffer (%d)",err);
		return err;
	}

	if( !(m_event_buffer=freebob_ringbuffer_create(
			(m_dimension * m_nb_buffers * m_period) * sizeof(quadlet_t)))) {
		debugFatal("Could not allocate memory event ringbuffer");
// 		return -ENOMEM;
		return -1;
	}

	// allocate the temporary cluster buffer
	if( !(m_cluster_buffer=(char *)calloc(m_dimension,sizeof(quadlet_t)))) {
		debugFatal("Could not allocate temporary cluster buffer");
		freebob_ringbuffer_free(m_event_buffer);
// 		return -ENOMEM;
		return -1;
	}

	return 0;
}

int AmdtpReceiveStreamProcessor::putPacket(unsigned char *data, unsigned int length, 
	              unsigned char channel, unsigned char tag, unsigned char sy, 
		          unsigned int cycle, unsigned int dropped) {

	enum raw1394_iso_disposition retval=RAW1394_ISO_OK;
	
	struct iec61883_packet *packet = (struct iec61883_packet *) data;
	assert(packet);
	
	if((packet->fmt == 0x10) && (packet->fdf != 0xFF) && (packet->dbs>0) && (length>=2*sizeof(quadlet_t))) {
		unsigned int nevents=((length / sizeof (quadlet_t)) - 2)/packet->dbs;

		int write_size=nevents*sizeof(quadlet_t)*m_dimension;
		// add the data payload to the ringbuffer
		
		if (freebob_ringbuffer_write(m_event_buffer,(char *)(data+8),write_size) < write_size) 
		{
			debugWarning("Buffer overrun!\n"); 
			m_xruns++;

			retval=RAW1394_ISO_DEFER;
		} else {
			retval=RAW1394_ISO_OK;
			// we cannot offload midi encoding due to the need for a dbc value
// 			freebob_streaming_decode_midi(connection,(quadlet_t *)(data+8), nevents, packet->dbc);
		}
		
		// update the frame counter
		m_framecounter+=nevents;
		
	} else {
		// discard packet
		// can be important for sync though
	}

	return (int)retval;
}

void AmdtpReceiveStreamProcessor::setVerboseLevel(int l) {
	setDebugLevel(l);
// 	((ReceiveStreamProcessor *)this)->setVerboseLevel(l);

}


void AmdtpReceiveStreamProcessor::reset() {

	debugOutput( DEBUG_LEVEL_VERBOSE, "Resetting...\n");

	// reset the event buffer, discard all content
	freebob_ringbuffer_reset(m_event_buffer);
	
	// reset all non-device specific stuff
	// i.e. the iso stream and the associated ports
	((ReceiveStreamProcessor *)this)->reset();
}

void AmdtpReceiveStreamProcessor::prepare() {

	debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing...\n");

	// prepare all non-device specific stuff
	// i.e. the iso stream and the associated ports
	((ReceiveStreamProcessor *)this)->prepare();
}

int AmdtpReceiveStreamProcessor::transfer() {

	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Transferring period...\n");
// TODO: implement

	return 0;
}


} // end of namespace FreebobStreaming
