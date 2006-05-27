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
AmdtpTransmitStreamProcessor::AmdtpTransmitStreamProcessor(int port, int framerate, int dimension)
	: TransmitStreamProcessor(port, framerate), m_dimension(dimension) {


}

AmdtpTransmitStreamProcessor::~AmdtpTransmitStreamProcessor() {
	freebob_ringbuffer_free(m_event_buffer);
	free(m_cluster_buffer);
}

bool AmdtpTransmitStreamProcessor::init() {

	debugOutput( DEBUG_LEVEL_VERBOSE, "Initializing (%p)...\n");
	// call the parent init
	// this has to be done before allocating the buffers, 
	// because this sets the buffersizes from the processormanager
	if(!TransmitStreamProcessor::init()) {
		debugFatal("Could not do base class init (%p)\n",this);
		return false;
	}
	

	return true;
}

void AmdtpTransmitStreamProcessor::setVerboseLevel(int l) {
	setDebugLevel(l);
	TransmitStreamProcessor::setVerboseLevel(l);
}


int AmdtpTransmitStreamProcessor::getPacket(unsigned char *data, unsigned int *length,
	              unsigned char *tag, unsigned char *sy,
	              int cycle, unsigned int dropped, unsigned int max_length) {

	struct iec61883_packet *packet = (struct iec61883_packet *) data;
	
	// signal that we are running (a transmit stream is always 'runnable')
	m_running=true;
	
	// don't process the stream when it is not enabled.
	// however, we do have to generate (semi) valid packets
	// that means that we'll send NODATA packets FIXME: check!!
	if(m_disabled) {
		iec61883_cip_fill_header_nodata(getNodeId(), &m_cip_status, packet);
		*length = 0; // this is to disable sending
		*tag = IEC61883_TAG_WITH_CIP;
		*sy = 0;
		return (int)RAW1394_ISO_OK;
	}
		
	// construct the packet cip
	int nevents = iec61883_cip_fill_header (getNodeId(), &m_cip_status, packet);

	enum raw1394_iso_disposition retval = RAW1394_ISO_OK;

	if (!(nevents > 0)) {
		
		if (m_cip_status.mode == IEC61883_MODE_BLOCKING_EMPTY) {
			*length = 8;
			return (int)RAW1394_ISO_OK ;
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
		// FIXME: underrun signaling turned off!!
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

bool AmdtpTransmitStreamProcessor::reset() {

	debugOutput( DEBUG_LEVEL_VERBOSE, "Resetting...\n");

	// reset the event buffer, discard all content
	freebob_ringbuffer_reset(m_event_buffer);
	
	// reset all non-device specific stuff
	// i.e. the iso stream and the associated ports
	return TransmitStreamProcessor::reset();
}

bool AmdtpTransmitStreamProcessor::prepare() {

	debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing...\n");
	
	// prepare all non-device specific stuff
	// i.e. the iso stream and the associated ports
	if(!TransmitStreamProcessor::prepare()) {
		debugFatal("Could not prepare base class\n");
		return false;
	}
	
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
		return false;
	}

	// allocate the temporary cluster buffer
	if( !(m_cluster_buffer=(char *)calloc(m_dimension,sizeof(quadlet_t)))) {
		debugFatal("Could not allocate temporary cluster buffer");
		freebob_ringbuffer_free(m_event_buffer);
		return false;
// 		return -ENOMEM;
	}
	
	// we should transfer the port buffer contents to the event buffer
	int i=m_nb_buffers;
	while(i--) {
		if(!transfer()) {
			debugFatal("Could not prefill transmit stream\n");
			return false;
		}
	}
	
	debugOutput( DEBUG_LEVEL_VERBOSE, "Prepared for:\n");
	debugOutput( DEBUG_LEVEL_VERBOSE, " Samplerate: %d, FDF: %d, DBS: %d, SYT: %d\n",
		     m_framerate,m_fdf,m_dimension,m_syt_interval);
	debugOutput( DEBUG_LEVEL_VERBOSE, " PeriodSize: %d, NbBuffers: %d\n",
		     m_period,m_nb_buffers);
	debugOutput( DEBUG_LEVEL_VERBOSE, " Port: %d, Channel: %d\n",
		     m_port,m_channel);

	return true;

}

bool AmdtpTransmitStreamProcessor::transfer() {

	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Transferring period...\n");
	// TODO: improve
/* a naive implementation would look like this:

	unsigned int write_size=m_period*sizeof(quadlet_t)*m_dimension;
	char *dummybuffer=(char *)calloc(sizeof(quadlet_t),m_period*m_dimension);
	transmitBlock(dummybuffer, m_period, 0, 0);

	if (freebob_ringbuffer_write(m_event_buffer,(char *)(dummybuffer),write_size) < write_size) {
		debugWarning("Could not write to event buffer\n");
	}


	free(dummybuffer);
*/
/* but we're not that naive anymore... */
	int i;
	int xrun;
	unsigned int offset=0;
	
	freebob_ringbuffer_data_t vec[2];
	// we received one period of frames
	// this is period_size*dimension of events
	int events2write=m_period*m_dimension;
	int bytes2write=events2write*sizeof(quadlet_t);

	/* write events2write bytes to the ringbuffer 
	*  first see if it can be done in one read.
	*  if so, ok. 
	*  otherwise write up to a multiple of clusters directly to the buffer
	*  then do the buffer wrap around using ringbuffer_write
	*  then write the remaining data directly to the buffer in a third pass 
	*  Make sure that we cannot end up on a non-cluster aligned position!
	*/
	int cluster_size=m_dimension*sizeof(quadlet_t);

	while(bytes2write>0) {
		int byteswritten=0;
		
		unsigned int frameswritten=(m_period*cluster_size-bytes2write)/cluster_size;
		offset=frameswritten;
		
		freebob_ringbuffer_get_write_vector(m_event_buffer, vec);
			
		if(vec[0].len==0) { // this indicates a full event buffer
			debugError("Event buffer overrun in processor %d\n",this);
			break;
		}
			
		/* if we don't take care we will get stuck in an infinite loop
		* because we align to a cluster boundary later
		* the remaining nb of bytes in one write operation can be 
		* smaller than one cluster
		* this can happen because the ringbuffer size is always a power of 2
		*/
		if(vec[0].len<cluster_size) {
			
			// encode to the temporary buffer
			xrun = transmitBlock(m_cluster_buffer, 1, offset);
			
			if(xrun<0) {
				// xrun detected
				debugError("Frame buffer underrun in processor %d\n",this);
				break;
			}
				
			// use the ringbuffer function to write one cluster 
			// the write function handles the wrap around.
			freebob_ringbuffer_write(m_event_buffer,
						 m_cluster_buffer,
						 cluster_size);
				
			// we advanced one cluster_size
			bytes2write-=cluster_size;
				
		} else { // 
			
			if(bytes2write>vec[0].len) {
				// align to a cluster boundary
				byteswritten=vec[0].len-(vec[0].len%cluster_size);
			} else {
				byteswritten=bytes2write;
			}
				
			xrun = transmitBlock(vec[0].buf,
					     byteswritten/cluster_size,
					     offset);
			
			if(xrun<0) {
					// xrun detected
				debugError("Frame buffer underrun in processor %d\n",this);
				break;
			}

			freebob_ringbuffer_write_advance(m_event_buffer, byteswritten);
			bytes2write -= byteswritten;
		}
			
		// the bytes2write should always be cluster aligned
		assert(bytes2write%cluster_size==0);
			
	}

	return true;
}
/* 
 * write received events to the stream ringbuffers.
 */

int AmdtpTransmitStreamProcessor::transmitBlock(char *data, 
					   unsigned int nevents, unsigned int offset)
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
			if(encodePortToMBLAEvents(static_cast<AmdtpAudioPort *>(*it), (quadlet_t *)data, offset, nevents)) {
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
					   unsigned int offset, unsigned int nevents)
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

AmdtpReceiveStreamProcessor::AmdtpReceiveStreamProcessor(int port, int framerate, int dimension)
	: ReceiveStreamProcessor(port, framerate), m_dimension(dimension) {


}

AmdtpReceiveStreamProcessor::~AmdtpReceiveStreamProcessor() {
	freebob_ringbuffer_free(m_event_buffer);
	free(m_cluster_buffer);

}

bool AmdtpReceiveStreamProcessor::init() {
	int err=0;
	
	
	// call the parent init
	// this has to be done before allocating the buffers, 
	// because this sets the buffersizes from the processormanager
	if(!ReceiveStreamProcessor::init()) {
		debugFatal("Could not do base class init (%d)\n",this);
		return false;
	}

	return true;
}

int AmdtpReceiveStreamProcessor::putPacket(unsigned char *data, unsigned int length, 
	              unsigned char channel, unsigned char tag, unsigned char sy, 
		          unsigned int cycle, unsigned int dropped) {

	enum raw1394_iso_disposition retval=RAW1394_ISO_OK;
	
	struct iec61883_packet *packet = (struct iec61883_packet *) data;
	assert(packet);
	
	if((packet->fmt == 0x10) && (packet->fdf != 0xFF) && (packet->dbs>0) && (length>=2*sizeof(quadlet_t))) {
		unsigned int nevents=((length / sizeof (quadlet_t)) - 2)/packet->dbs;
		
		// signal that we're running
		if(nevents) m_running=true;
		
		// don't process the stream when it is not enabled.
		if(m_disabled) {
			return (int)RAW1394_ISO_OK;
		}
		
		
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


bool AmdtpReceiveStreamProcessor::reset() {

	debugOutput( DEBUG_LEVEL_VERBOSE, "Resetting...\n");

	// reset the event buffer, discard all content
	freebob_ringbuffer_reset(m_event_buffer);
	
	// reset all non-device specific stuff
	// i.e. the iso stream and the associated ports
	return ReceiveStreamProcessor::reset();
}

bool AmdtpReceiveStreamProcessor::prepare() {

	// prepare all non-device specific stuff
	// i.e. the iso stream and the associated ports
	if(!ReceiveStreamProcessor::prepare()) {
		debugFatal("Could not prepare base class\n");
		return false;
	}
	
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

	if( !(m_event_buffer=freebob_ringbuffer_create(
			(m_dimension * m_nb_buffers * m_period) * sizeof(quadlet_t)))) {
		debugFatal("Could not allocate memory event ringbuffer");
// 		return -ENOMEM;
		return false;
	}

	// allocate the temporary cluster buffer
	if( !(m_cluster_buffer=(char *)calloc(m_dimension,sizeof(quadlet_t)))) {
		debugFatal("Could not allocate temporary cluster buffer");
		freebob_ringbuffer_free(m_event_buffer);
// 		return -ENOMEM;
		return false;
	}


	
	debugOutput( DEBUG_LEVEL_VERBOSE, "Prepared for:\n");
	debugOutput( DEBUG_LEVEL_VERBOSE, " Samplerate: %d, DBS: %d, SYT: %d\n",
		     m_framerate,m_dimension,m_syt_interval);
	debugOutput( DEBUG_LEVEL_VERBOSE, " PeriodSize: %d, NbBuffers: %d\n",
		     m_period,m_nb_buffers);
	debugOutput( DEBUG_LEVEL_VERBOSE, " Port: %d, Channel: %d\n",
		     m_port,m_channel);
	return true;

}

bool AmdtpReceiveStreamProcessor::transfer() {

	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Transferring period...\n");
	
/* another naive section:	
	unsigned int read_size=m_period*sizeof(quadlet_t)*m_dimension;
	char *dummybuffer=(char *)calloc(sizeof(quadlet_t),m_period*m_dimension);
	if (freebob_ringbuffer_read(m_event_buffer,(char *)(dummybuffer),read_size) < read_size) {
		debugWarning("Could not read from event buffer\n");
	}

	receiveBlock(dummybuffer, m_period, 0);

	free(dummybuffer);
*/
	int i;
	int xrun;
	unsigned int offset=0;
	
	freebob_ringbuffer_data_t vec[2];
	// we received one period of frames on each connection
	// this is period_size*dimension of events

	int events2read=m_period*m_dimension;
	int bytes2read=events2read*sizeof(quadlet_t);
	/* read events2read bytes from the ringbuffer 
	*  first see if it can be done in one read. 
	*  if so, ok. 
	*  otherwise read up to a multiple of clusters directly from the buffer
	*  then do the buffer wrap around using ringbuffer_read
	*  then read the remaining data directly from the buffer in a third pass 
	*  Make sure that we cannot end up on a non-cluster aligned position!
	*/
	int cluster_size=m_dimension*sizeof(quadlet_t);
	
	while(bytes2read>0) {
		unsigned int framesread=(m_period*cluster_size-bytes2read)/cluster_size;
		offset=framesread;
		
		int bytesread=0;

		freebob_ringbuffer_get_read_vector(m_event_buffer, vec);
			
		if(vec[0].len==0) { // this indicates an empty event buffer
			debugError("Frame buffer underrun in processor %d\n",this);
			break;
		}
			
		/* if we don't take care we will get stuck in an infinite loop
		* because we align to a cluster boundary later
		* the remaining nb of bytes in one read operation can be smaller than one cluster
		* this can happen because the ringbuffer size is always a power of 2
			*/
		if(vec[0].len<cluster_size) {
			// use the ringbuffer function to read one cluster 
			// the read function handles wrap around
			freebob_ringbuffer_read(m_event_buffer,m_cluster_buffer,cluster_size);

			xrun = receiveBlock(m_cluster_buffer, 1, offset);
				
			if(xrun<0) {
				// xrun detected
				debugError("Frame buffer underrun in processor %d\n",this);
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
				
			xrun = receiveBlock(vec[0].buf, bytesread/cluster_size, offset);
				
			if(xrun<0) {
					// xrun detected
				debugError("Frame buffer underrun in processor %d\n",this);
				break;
			}

			freebob_ringbuffer_read_advance(m_event_buffer, bytesread);
			bytes2read -= bytesread;
		}
			
			// the bytes2read should always be cluster aligned
		assert(bytes2read%cluster_size==0);
	}

	return true;
}

/* 
 * write received events to the stream ringbuffers.
 */

int AmdtpReceiveStreamProcessor::receiveBlock(char *data, 
					   unsigned int nevents, unsigned int offset)
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
			if(decodeMBLAEventsToPort(static_cast<AmdtpAudioPort *>(*it), (quadlet_t *)data, offset, nevents)) {
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
					   unsigned int offset, unsigned int nevents)
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
