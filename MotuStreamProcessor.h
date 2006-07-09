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
#include <assert.h>

#include "../debugmodule/debugmodule.h"
#include "StreamProcessor.h"

#include "../libutil/DelayLockedLoop.h"

namespace FreebobStreaming {

class MotuAudioPort;

/**
 * This class implements the outgoing stream processing for
 * motu devices
 */
class MotuTransmitStreamProcessor
    : public TransmitStreamProcessor
{
public:
	
	MotuTransmitStreamProcessor(int port, int framerate, 
		unsigned int event_size);

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

	// These two are important to calculate the optimal ISO DMA buffers
	// size.  An estimate will do.
	unsigned int getPacketsPerPeriod() {return (m_period*8000) / m_framerate;};
	unsigned int getMaxPacketSize() {return m_framerate<=48000?616:(m_framerate<=96000?1032:1160);}; 

	virtual void setVerboseLevel(int l);

	void set_sph_ofs_dll(FreebobUtil::DelayLockedLoop *dll) {m_sph_ofs_dll=dll;};

	virtual bool preparedForStop();

protected:

	freebob_ringbuffer_t * m_event_buffer;
	char* m_tmp_event_buffer;
	
	/*
	 * An iso packet mostly consists of multiple events.  m_event_size
	 * is the size of a single 'event' in bytes.
	 */
	unsigned int m_event_size;

	// Keep track of transmission data block count
	unsigned int m_tx_dbc;

	// Transmission cycle count and cycle offset
	signed int m_cycle_count;
	float m_cycle_ofs;

	// Hook to the DLL in the receive stream which allows calculation
	// of cycle offsets to put into frame SPHs.  This object is 
	// owned by the receive stream, so the transmit stream should
	// not dispose of it.
	FreebobUtil::DelayLockedLoop *m_sph_ofs_dll;

	// Used to keep track of the close-down zeroing of output data
	signed int m_closedown_count;

    bool prefill();
    
	bool transferSilence(unsigned int size);

	int transmitBlock(char *data, unsigned int nevents, 
	                  unsigned int offset);
	                  
	bool encodePacketPorts(quadlet_t *data, unsigned int nevents, unsigned int dbc);
	
	int transmitSilenceBlock(char *data, unsigned int nevents, 
	                  unsigned int offset);
	                  
	int MotuTransmitStreamProcessor::encodePortToMBLAEvents(
		MotuAudioPort *p, quadlet_t *data, unsigned int offset, 
		unsigned int nevents);
	int MotuTransmitStreamProcessor::encodeSilencePortToMBLAEvents(
		MotuAudioPort *p, quadlet_t *data, unsigned int offset, 
		unsigned int nevents);


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

	MotuReceiveStreamProcessor(int port, int framerate, unsigned int event_size);
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
	unsigned int getPacketsPerPeriod() {return (m_period*8000) / m_framerate;};
	unsigned int getMaxPacketSize() {return m_framerate<=48000?616:(m_framerate<=96000?1032:1160);}; 

	virtual void setVerboseLevel(int l);
	
	FreebobUtil::DelayLockedLoop *get_sph_ofs_dll(void) {return m_sph_ofs_dll;};
	signed int setEventSize(unsigned int size);
	unsigned int getEventSize(void);

protected:

	int receiveBlock(char *data, unsigned int nevents, unsigned int offset);
	bool decodePacketPorts(quadlet_t *data, unsigned int nevents, unsigned int dbc);
	signed int decodeMBLAEventsToPort(MotuAudioPort *p, quadlet_t *data, unsigned int offset, unsigned int nevents);

	freebob_ringbuffer_t * m_event_buffer;
	char* m_tmp_event_buffer;
	
	/*
	 * An iso packet mostly consists of multiple events.  m_event_size
	 * is the size of a single 'event' in bytes.
	 */
	unsigned int m_event_size;

	FreebobUtil::DelayLockedLoop *m_sph_ofs_dll;
	signed int m_last_cycle_ofs;

    DECLARE_DEBUG_MODULE;

};

} // end of namespace FreebobStreaming

#endif /* __FREEBOB_MOTUSTREAMPROCESSOR__ */


