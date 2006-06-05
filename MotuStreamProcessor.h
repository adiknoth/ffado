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
#ifndef __FREEBOB_MOTUSTREAMPROCESSOR__
#define __FREEBOB_MOTUSTREAMPROCESSOR__

#include "../debugmodule/debugmodule.h"
#include "StreamProcessor.h"

namespace FreebobStreaming {

/**
 * This class implements the outgoing stream processing for
 * motu devices
 */
class MotuTransmitStreamProcessor
    : public TransmitStreamProcessor
{
public:
	
	MotuTransmitStreamProcessor(int port, int framerate);

	virtual ~MotuTransmitStreamProcessor();

	enum raw1394_iso_disposition 
		getPacket(unsigned char *data, unsigned int *length,
	              unsigned char *tag, unsigned char *sy,
	              int cycle, unsigned int dropped, unsigned int max_length);

	bool init();
	bool reset();
	bool prepare();
	bool transfer();
	
	bool isOnePeriodReady();

    // these two are important to calculate the optimal
    // ISO DMA buffers size
    // an estimate will do
	unsigned int getPacketsPerPeriod() {return 1;};
	unsigned int getMaxPacketSize() {return 2048;}; 

	virtual void setVerboseLevel(int l);

protected:

	freebob_ringbuffer_t * m_event_buffer;
	char* m_cluster_buffer;
	
	/**
	 * AMDTP specific:
	 * the dimension of a stream is the number of substreams it contains
	 * i.e. one 'event' is 'dimension' quadlets big. 
	 *
	 * a packet mostly consists of multiple events. the frames of one substream
	 * are located in *(buffer), *(buffer+dimension), *(buffer+(dimension*2)), ...
	 *
	 * adapt this to your needs
	 */
	int m_dimension;
	
    bool prefill();
    
	bool transferSilence(unsigned int size);

	int transmitBlock(char *data, unsigned int nevents, 
	                  unsigned int offset);
	                  
	bool encodePacketPorts(quadlet_t *data, unsigned int nevents, unsigned int dbc);
	
	int transmitSilenceBlock(char *data, unsigned int nevents, 
	                  unsigned int offset);
	                  


    DECLARE_DEBUG_MODULE;

};

/**
 * This class implements the incoming stream processing for
 * motu devices
 */
class MotuReceiveStreamProcessor
    : public ReceiveStreamProcessor 
{

public:

	MotuReceiveStreamProcessor(int port, int framerate);
	virtual ~MotuReceiveStreamProcessor();
	
	enum raw1394_iso_disposition putPacket(unsigned char *data, unsigned int length, 
	              unsigned char channel, unsigned char tag, unsigned char sy, 
		          unsigned int cycle, unsigned int dropped);
	
	bool init();
	bool reset();
	bool prepare();
	bool transfer();
	
	bool isOnePeriodReady();

    // these two are important to calculate the optimal
    // ISO DMA buffers size
    // an estimate will do
	unsigned int getPacketsPerPeriod() {return 1;};
	unsigned int getMaxPacketSize() {return 2048;}; 

	virtual void setVerboseLevel(int l);
	

protected:

	int receiveBlock(char *data, unsigned int nevents, unsigned int offset);
	bool decodePacketPorts(quadlet_t *data, unsigned int nevents, unsigned int dbc);


	freebob_ringbuffer_t * m_event_buffer;
	char* m_cluster_buffer;
	
	/**
	 * AMDTP specific:
	 * the dimension of a stream is the number of substreams it contains
	 * i.e. one 'event' is 'dimension' quadlets big. 
	 *
	 * a packet mostly consists of multiple events. the frames of one substream
	 * are located in *(buffer), *(buffer+dimension), *(buffer+(dimension*2)), ...
	 *
	 * adapt this to your needs
	 */
	int m_dimension;


    DECLARE_DEBUG_MODULE;

};

} // end of namespace FreebobStreaming

#endif /* __FREEBOB_MOTUSTREAMPROCESSOR__ */


