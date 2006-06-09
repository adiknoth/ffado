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
#ifndef __FREEBOB_AMDTPSTREAMPROCESSOR__
#define __FREEBOB_AMDTPSTREAMPROCESSOR__

/**
 * This class implements IEC61883-6 / AM824 / AMDTP based streaming
 */

#include "../debugmodule/debugmodule.h"
#include "StreamProcessor.h"
#include "cip.h"
#include <libiec61883/iec61883.h>
#include "ringbuffer.h"

#define AMDTP_MAX_PACKET_SIZE 2048

#define IEC61883_STREAM_TYPE_MIDI   0x0D
#define IEC61883_STREAM_TYPE_SPDIF  0x00
#define IEC61883_STREAM_TYPE_MBLA   0x06

#define IEC61883_AM824_LABEL_MASK 			0xFF000000
#define IEC61883_AM824_GET_LABEL(x) 		(((x) & 0xFF000000) >> 24)
#define IEC61883_AM824_SET_LABEL(x,y) 		((x) | ((y)<<24))

#define IEC61883_AM824_LABEL_MIDI_NO_DATA 	0x80 
#define IEC61883_AM824_LABEL_MIDI_1X      	0x81 
#define IEC61883_AM824_LABEL_MIDI_2X      	0x82
#define IEC61883_AM824_LABEL_MIDI_3X      	0x83

namespace FreebobStreaming {

class Port;
class AmdtpAudioPort;
class AmdtpMidiPort;
class AmdtpReceiveStreamProcessor;

/*!
\brief The Base Class for an AMDTP transmit stream processor

 This class implements a TransmitStreamProcessor that multiplexes Ports 
 into AMDTP streams.
 
*/
class AmdtpTransmitStreamProcessor 
	: public TransmitStreamProcessor
{

public:

	AmdtpTransmitStreamProcessor(int port, int framerate, int dimension);

	virtual ~AmdtpTransmitStreamProcessor();

	enum raw1394_iso_disposition 
		getPacket(unsigned char *data, unsigned int *length,
	              unsigned char *tag, unsigned char *sy,
	              int cycle, unsigned int dropped, unsigned int max_length);

	bool init();
	bool reset();
	bool prepare();
	bool transfer();
	virtual void setVerboseLevel(int l);
	
	bool isOnePeriodReady();

    // We have 1 period of samples = m_period
    // this period takes m_period/m_framerate seconds of time
    // during this time, 8000 packets are sent
// 	unsigned int getPacketsPerPeriod() {return (m_period*8000)/m_framerate;};
    
    // however, if we only count the number of used packets
    // it is m_period / m_syt_interval
	unsigned int getPacketsPerPeriod() {return (m_period)/m_syt_interval;};
	
	unsigned int getMaxPacketSize() {return 4 * (2 + m_syt_interval * m_dimension);}; 

    // FIXME: do this the proper way!
    AmdtpReceiveStreamProcessor *syncmaster;

protected:

	struct iec61883_cip m_cip_status;

	freebob_ringbuffer_t * m_event_buffer;
	char* m_cluster_buffer;
	int m_dimension;
	unsigned int m_syt_interval;

	int m_fdf;
	
    bool prefill();
    
	bool transferSilence(unsigned int size);

	int transmitBlock(char *data, unsigned int nevents, 
	                  unsigned int offset);
	                  
	bool encodePacketPorts(quadlet_t *data, unsigned int nevents, unsigned int dbc);
	int encodePortToMBLAEvents(AmdtpAudioPort *, quadlet_t *data,
	                           unsigned int offset, unsigned int nevents);
	
	int transmitSilenceBlock(char *data, unsigned int nevents, 
	                  unsigned int offset);
	int encodeSilencePortToMBLAEvents(AmdtpAudioPort *, quadlet_t *data,
	                           unsigned int offset, unsigned int nevents);

    double m_last_timestamp;


    DECLARE_DEBUG_MODULE;

};
/*!
\brief The Base Class for an AMDTP receive stream processor

 This class implements a ReceiveStreamProcessor that demultiplexes 
 AMDTP streams into Ports.
 
*/
class AmdtpReceiveStreamProcessor 
	: public ReceiveStreamProcessor
{

public:

	AmdtpReceiveStreamProcessor(int port, int framerate, int dimension);

	virtual ~AmdtpReceiveStreamProcessor();

	enum raw1394_iso_disposition putPacket(unsigned char *data, unsigned int length, 
	              unsigned char channel, unsigned char tag, unsigned char sy, 
		          unsigned int cycle, unsigned int dropped);


	bool init();
	bool reset();
	bool prepare();
	bool transfer();

	virtual void setVerboseLevel(int l);
	
	bool isOnePeriodReady();
	
    // We have 1 period of samples = m_period
    // this period takes m_period/m_framerate seconds of time
    // during this time, 8000 packets are sent
// 	unsigned int getPacketsPerPeriod() {return (m_period*8000)/m_framerate;};
    
    // however, if we only count the number of used packets
    // it is m_period / m_syt_interval
	unsigned int getPacketsPerPeriod() {return (m_period)/m_syt_interval;};
	
	unsigned int getMaxPacketSize() {return 4 * (2 + m_syt_interval * m_dimension);}; 

    double getTicksPerFrame() {return m_ticks_per_frame;};
    
    void dumpInfo();
    
protected:

	int receiveBlock(char *data, unsigned int nevents, unsigned int offset);
	bool decodePacketPorts(quadlet_t *data, unsigned int nevents, unsigned int dbc);
	
	int decodeMBLAEventsToPort(AmdtpAudioPort *, quadlet_t *data, unsigned int offset, unsigned int nevents);

	freebob_ringbuffer_t * m_event_buffer;
	char* m_cluster_buffer;
	int m_dimension;
	unsigned int m_syt_interval;
    
    unsigned int m_last_timestamp;
    unsigned int m_last_timestamp2;
    
    double m_ticks_per_frame;
    
    DECLARE_DEBUG_MODULE;

};


} // end of namespace FreebobStreaming

#endif /* __FREEBOB_AMDTPSTREAMPROCESSOR__ */


