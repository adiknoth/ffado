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

namespace FreebobStreaming {

class Port;
class AmdtpAudioPort;
class AmdtpMidiPort;

class AmdtpTransmitStreamProcessor 
	: public TransmitStreamProcessor
{

public:

	AmdtpTransmitStreamProcessor(int channel, int port, int framerate, int dimension);

	virtual ~AmdtpTransmitStreamProcessor();

	int 
		getPacket(unsigned char *data, unsigned int *length,
	              unsigned char *tag, unsigned char *sy,
	              int cycle, unsigned int dropped, unsigned int max_length);

	int init();
	void reset();
	void prepare();
	int transfer();
	virtual void setVerboseLevel(int l);

protected:

	struct iec61883_cip m_cip_status;

	freebob_ringbuffer_t * m_event_buffer;
	char* m_cluster_buffer;
	int m_dimension;

	int transmitBlock(char *data, unsigned int nevents, 
	                  unsigned int offset, unsigned int dbc);
	int encodePortToMBLAEvents(AmdtpAudioPort *, quadlet_t *data,
	                           unsigned int offset, unsigned int nevents,
	                           unsigned int dbc);

    DECLARE_DEBUG_MODULE;

};

class AmdtpReceiveStreamProcessor 
	: public ReceiveStreamProcessor
{

public:

	AmdtpReceiveStreamProcessor(int channel, int port, int framerate, int dimension);

	virtual ~AmdtpReceiveStreamProcessor();

	int putPacket(unsigned char *data, unsigned int length, 
	              unsigned char channel, unsigned char tag, unsigned char sy, 
		          unsigned int cycle, unsigned int dropped);


	int init();
	void reset();
	void prepare();
	int transfer();

	virtual void setVerboseLevel(int l);

protected:

	int receiveBlock(char *data, unsigned int nevents, unsigned int offset, unsigned int dbc);
	int decodeMBLAEventsToPort(AmdtpAudioPort *, quadlet_t *data, unsigned int offset, unsigned int nevents, unsigned int dbc);

	freebob_ringbuffer_t * m_event_buffer;
	char* m_cluster_buffer;
	int m_dimension;

    DECLARE_DEBUG_MODULE;

};


} // end of namespace FreebobStreaming

#endif /* __FREEBOB_AMDTPSTREAMPROCESSOR__ */


