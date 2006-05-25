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
#include "Port.h"
#include "AmdtpPort.h"

#include <netinet/in.h>
#include <assert.h>


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
	int err=0;

	// call the parent init
	// this has to be done before allocating the buffers, 
	// because this sets the buffersizes from the processormanager
	if((err=TransmitStreamProcessor::init())) {
		debugFatal("Could not init (%p), err=(%d)",this,err);
		return err;
	}
	
	debugOutput( DEBUG_LEVEL_VERBOSE, "Initializing (%p)...\n");

	
	debugOutput( DEBUG_LEVEL_VERBOSE, " Samplerate: %d, FDF: %d, DBS: %d, SYT: %d\n",
		     m_framerate,m_fdf,m_dimension,m_syt_interval);
	
	iec61883_cip_init (
		&m_cip_status, 
		IEC61883_FMT_AMDTP, 
		m_fdf,
		m_framerate, 
		m_dimension, 
		m_syt_interval);

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
	
	// we should transfer the port buffer contents to the event buffer
	int i=m_nb_buffers;
	while(i--) transfer();

	return 0;
}

void AmdtpTransmitStreamProcessor::setVerboseLevel(int l) {
	setDebugLevel(l);
	TransmitStreamProcessor::setVerboseLevel(l);
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
		debugWarning("Transmit buffer underrun (cycle %d, FC=%d, PC=%d)\n", 
			     cycle, m_framecounter, m_handler->getPacketCount());
		
		// signal underrun
// 		m_xruns++;

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
	TransmitStreamProcessor::reset();
}

bool AmdtpTransmitStreamProcessor::prepare() {

	debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing...\n");
	
	switch (m_framerate) {
	case 32000:
		m_syt_interval = 8;
		m_fdf = IEC61883_FDF_SFC_32KHZ;
		break;
	case 44100:
		m_syt_interval = 8;
		m_fdf = IEC61883_FDF_SFC_44K1HZ;
		break;
	default:
	case 48000:
		m_syt_interval = 8;
		m_fdf = IEC61883_FDF_SFC_48KHZ;
		break;
	case 88200:
		m_syt_interval = 16;
		m_fdf = IEC61883_FDF_SFC_88K2HZ;
		break;
	case 96000:
		m_syt_interval = 16;
		m_fdf = IEC61883_FDF_SFC_96KHZ;
		break;
	case 176400:
		m_syt_interval = 32;
		m_fdf = IEC61883_FDF_SFC_176K4HZ;
		break;
	case 192000:
		m_syt_interval = 32;
		m_fdf = IEC61883_FDF_SFC_192KHZ;
		break;
	}

	// prepare all non-device specific stuff
	// i.e. the iso stream and the associated ports
	TransmitStreamProcessor::prepare();

	return true;

}

int AmdtpTransmitStreamProcessor::transfer() {

	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Transferring period...\n");
	// TODO: improve

	unsigned int write_size=m_period*sizeof(quadlet_t)*m_dimension;
	char *dummybuffer=(char *)calloc(sizeof(quadlet_t),m_period*m_dimension);
	transmitBlock(dummybuffer, m_period, 0, 0);

	if (freebob_ringbuffer_write(m_event_buffer,(char *)(dummybuffer),write_size) < write_size) {
		debugWarning("Could not write to event buffer\n");
	}


	free(dummybuffer);

	return 0;
}
/* 
 * write received events to the stream ringbuffers.
 */

int AmdtpTransmitStreamProcessor::transmitBlock(char *data, 
					   unsigned int nevents, unsigned int offset, unsigned int dbc)
{
	int problem=0;

	for ( PortVectorIterator it = m_PeriodPorts.begin();
          it != m_PeriodPorts.end();
          ++it )
    {

		//FIXME: make this into a static_cast when not DEBUG?

		AmdtpPortInfo *pinfo=dynamic_cast<AmdtpPortInfo *>(*it);
		assert(pinfo); // this should not fail!!

		switch(pinfo->getFormat()) {
		case AmdtpPortInfo::E_MBLA:
			if(encodePortToMBLAEvents(static_cast<AmdtpAudioPort *>(*it), (quadlet_t *)data, offset, nevents, dbc)) {
				debugWarning("Could not encode port %s to MBLA events",(*it)->getName().c_str());
				problem=1;
			}
			break;
		case AmdtpPortInfo::E_SPDIF: // still unimplemented
			break;
	/* for this processor, midi is a packet based port 
		case AmdtpPortInfo::E_Midi:
			break;*/
		default: // ignore
			break;
		}
    }
	return problem;

}

int AmdtpTransmitStreamProcessor::encodePortToMBLAEvents(AmdtpAudioPort *p, quadlet_t *data, 
					   unsigned int offset, unsigned int nevents, unsigned int dbc)
{
	unsigned int j=0;

	quadlet_t *target_event;

	target_event=(quadlet_t *)(data + p->getPosition());

	switch(p->getDataType()) {
		default:
		case Port::E_Int24:
			{
				quadlet_t *buffer=(quadlet_t *)(p->getBufferAddress());

				assert(nevents + offset <= p->getBufferSize());

				buffer+=offset;

				for(j = 0; j < nevents; j += 1) { // decode max nsamples
					*target_event = htonl((*(buffer) & 0x00FFFFFF) | 0x40000000);
					buffer++;
					target_event += m_dimension;
				}
			}
			break;
		case Port::E_Float:
			{
				const float multiplier = (float)(0x7FFFFF00);
				float *buffer=(float *)(p->getBufferAddress());

				assert(nevents + offset <= p->getBufferSize());

				buffer+=offset;

				for(j = 0; j < nevents; j += 1) { // decode max nsamples		
	
					// don't care for overflow
					float v = *buffer * multiplier;  // v: -231 .. 231
					unsigned int tmp = ((int)v);
					*target_event = htonl((tmp >> 8) | 0x40000000);
					
					buffer++;
					target_event += m_dimension;
				}
			}
			break;
	}

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
	if((err=ReceiveStreamProcessor::init())) {
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

		unsigned int write_size=nevents*sizeof(quadlet_t)*m_dimension;
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

		debugOutput(DEBUG_LEVEL_VERY_VERBOSE, 
			"RCV: CH = %d, FDF = %X. SYT = %6d, DBS = %3d, DBC = %3d, FMT = %3d, LEN = %4d (%2d)\n", 
			channel, packet->fdf,
			packet->syt,
			packet->dbs,
			packet->dbc,
			packet->fmt, 
			length,
			((length / sizeof (quadlet_t)) - 2)/packet->dbs);
		
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
 	ReceiveStreamProcessor::setVerboseLevel(l);

}


void AmdtpReceiveStreamProcessor::reset() {

	debugOutput( DEBUG_LEVEL_VERBOSE, "Resetting...\n");

	// reset the event buffer, discard all content
	freebob_ringbuffer_reset(m_event_buffer);
	
	// reset all non-device specific stuff
	// i.e. the iso stream and the associated ports
	ReceiveStreamProcessor::reset();
}

bool AmdtpReceiveStreamProcessor::prepare() {

	debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing...\n");
	switch (m_framerate) {
	case 32000:
		m_syt_interval = 8;
		break;
	case 44100:
		m_syt_interval = 8;
		break;
	default:
	case 48000:
		m_syt_interval = 8;
		break;
	case 88200:
		m_syt_interval = 16;
		break;
	case 96000:
		m_syt_interval = 16;
		break;
	case 176400:
		m_syt_interval = 32;
		break;
	case 192000:
		m_syt_interval = 32;
		break;
	}

	// prepare all non-device specific stuff
	// i.e. the iso stream and the associated ports
	return ReceiveStreamProcessor::prepare();
}

int AmdtpReceiveStreamProcessor::transfer() {

	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Transferring period...\n");
// TODO: implement
	
	unsigned int read_size=m_period*sizeof(quadlet_t)*m_dimension;
	char *dummybuffer=(char *)calloc(sizeof(quadlet_t),m_period*m_dimension);
	if (freebob_ringbuffer_read(m_event_buffer,(char *)(dummybuffer),read_size) < read_size) {
		debugWarning("Could not read from event buffer\n");
	}

	receiveBlock(dummybuffer, m_period, 0, 0);

	free(dummybuffer);



	return 0;
}

/* 
 * write received events to the stream ringbuffers.
 */

int AmdtpReceiveStreamProcessor::receiveBlock(char *data, 
					   unsigned int nevents, unsigned int offset, unsigned int dbc)
{
	int problem=0;

	for ( PortVectorIterator it = m_PeriodPorts.begin();
          it != m_PeriodPorts.end();
          ++it )
    {

		//FIXME: make this into a static_cast when not DEBUG?

		AmdtpPortInfo *pinfo=dynamic_cast<AmdtpPortInfo *>(*it);
		assert(pinfo); // this should not fail!!

		switch(pinfo->getFormat()) {
		case AmdtpPortInfo::E_MBLA:
			if(decodeMBLAEventsToPort(static_cast<AmdtpAudioPort *>(*it), (quadlet_t *)data, offset, nevents, dbc)) {
				debugWarning("Could not decode packet MBLA to port %s",(*it)->getName().c_str());
				problem=1;
			}
			break;
		case AmdtpPortInfo::E_SPDIF: // still unimplemented
			break;
	/* for this processor, midi is a packet based port 
		case AmdtpPortInfo::E_Midi:
			break;*/
		default: // ignore
			break;
		}
    }
	return problem;

}

int AmdtpReceiveStreamProcessor::decodeMBLAEventsToPort(AmdtpAudioPort *p, quadlet_t *data, 
					   unsigned int offset, unsigned int nevents, unsigned int dbc)
{
	unsigned int j=0;

// 	printf("****************\n");
// 	hexDumpQuadlets(data,m_dimension*4);
// 	printf("****************\n");

	quadlet_t *target_event;

	target_event=(quadlet_t *)(data + p->getPosition());

	switch(p->getDataType()) {
		default:
		case Port::E_Int24:
			{
				quadlet_t *buffer=(quadlet_t *)(p->getBufferAddress());

				assert(nevents + offset <= p->getBufferSize());

				buffer+=offset;

				for(j = 0; j < nevents; j += 1) { // decode max nsamples
					*(buffer)=(ntohl((*target_event) ) & 0x00FFFFFF);
					buffer++;
					target_event+=m_dimension;
				}
			}
			break;
		case Port::E_Float:
			{
				const float multiplier = 1.0f / (float)(0x7FFFFF);
				float *buffer=(float *)(p->getBufferAddress());

				assert(nevents + offset <= p->getBufferSize());

				buffer+=offset;

				for(j = 0; j < nevents; j += 1) { // decode max nsamples		
	
					unsigned int v = ntohl(*target_event) & 0x00FFFFFF;
					// sign-extend highest bit of 24-bit int
					int tmp = (int)(v << 8) / 256;
		
					*buffer = tmp * multiplier;
				
					buffer++;
					target_event+=m_dimension;
				}
			}
			break;
	}

	return 0;
}

} // end of namespace FreebobStreaming
