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
#ifndef __FREEBOB_STREAMPROCESSOR__
#define __FREEBOB_STREAMPROCESSOR__

#include "../debugmodule/debugmodule.h"

#include "IsoStream.h"
#include "PortManager.h"

namespace FreebobStreaming {

class StreamProcessorManager;

class StreamProcessor : public IsoStream, 
                        public PortManager {

	friend class StreamProcessorManager;

public:
	enum EProcessorType {
		E_Receive,
		E_Transmit
	};

	StreamProcessor(enum IsoStream::EStreamType type, int channel, int port, int framerate);
	virtual ~StreamProcessor();

	virtual int 
		putPacket(unsigned char *data, unsigned int length, 
	              unsigned char channel, unsigned char tag, unsigned char sy, 
		          unsigned int cycle, unsigned int dropped);
	virtual int 
		getPacket(unsigned char *data, unsigned int *length,
	              unsigned char *tag, unsigned char *sy,
	              int cycle, unsigned int dropped, unsigned int max_length);

	virtual enum EProcessorType getType() =0;

	bool xrunOccurred() { return (m_xruns>0);};

	bool isOnePeriodReady() { return (m_framecounter > m_period); };
	unsigned int getNbPeriodsReady() { if(m_period) return m_framecounter/m_period; else return 0;};
	void decrementFrameCounter() {m_framecounter -= m_period;};


	virtual int transfer(); // transfer the buffer contents from/to client

	virtual void reset(); // reset the streams & buffers (e.g. after xrun)

	virtual void prepare(); // prepare the streams & buffers (e.g. prefill)

	virtual void dumpInfo();

	virtual int init();

 	virtual void setVerboseLevel(int l);

protected:
	

	void setManager(StreamProcessorManager *manager) {m_manager=manager;};
	void clearManager() {m_manager=0;};

	unsigned int m_nb_buffers; // cached from manager->getNbBuffers()
	unsigned int m_period; // cached from manager->getPeriod()

	unsigned int m_xruns;
	unsigned int m_framecounter;

	unsigned int m_framerate;

	StreamProcessorManager *m_manager;

     DECLARE_DEBUG_MODULE;


};

class ReceiveStreamProcessor : public StreamProcessor {

public:
	ReceiveStreamProcessor(int channel, int port, int framerate);

	virtual ~ReceiveStreamProcessor();


	virtual enum EProcessorType getType() {return E_Receive;};

	virtual int putPacket(unsigned char *data, unsigned int length, 
	              unsigned char channel, unsigned char tag, unsigned char sy, 
		          unsigned int cycle, unsigned int dropped);
 	virtual void setVerboseLevel(int l);

protected:

     DECLARE_DEBUG_MODULE;


};

class TransmitStreamProcessor : public StreamProcessor {

public:
	TransmitStreamProcessor(int channel, int port, int framerate);

	virtual ~TransmitStreamProcessor();

	virtual enum EProcessorType getType() {return E_Transmit;};

	virtual int 
		getPacket(unsigned char *data, unsigned int *length,
	              unsigned char *tag, unsigned char *sy,
	              int cycle, unsigned int dropped, unsigned int max_length);
 	virtual void setVerboseLevel(int l);

protected:

     DECLARE_DEBUG_MODULE;


};

}

#endif /* __FREEBOB_STREAMPROCESSOR__ */


