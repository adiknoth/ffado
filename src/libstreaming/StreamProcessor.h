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
#include "streamstatistics.h"

namespace FreebobStreaming {

class StreamProcessorManager;

/*!
\brief Class providing a generic interface for Stream Processors

 A stream processor multiplexes or demultiplexes an ISO stream into a 
 collection of ports. This class should be subclassed, and the relevant
 functions should be overloaded.
 
*/
class StreamProcessor : public IsoStream, 
                        public PortManager {

	friend class StreamProcessorManager;

public:
	enum EProcessorType {
		E_Receive,
		E_Transmit
	};

	StreamProcessor(enum IsoStream::EStreamType type, int port, int framerate);
	virtual ~StreamProcessor();

	virtual enum raw1394_iso_disposition 
		putPacket(unsigned char *data, unsigned int length, 
	              unsigned char channel, unsigned char tag, unsigned char sy, 
		          unsigned int cycle, unsigned int dropped) = 0;
	virtual enum raw1394_iso_disposition 
		getPacket(unsigned char *data, unsigned int *length,
	              unsigned char *tag, unsigned char *sy,
	              int cycle, unsigned int dropped, unsigned int max_length) = 0;

	virtual enum EProcessorType getType() =0;

	bool xrunOccurred() { return (m_xruns>0);};

    /**
     * This is used for implementing the synchronisation.
     * As long as this function doesn't return true, the current buffer
     * contents are not transfered to the packet decoders.
     *
     * This means that there can be more events in the buffer than
     * one period worth of them, should the synchronisation mechanism 
     * require this
     * @return 
     */
	virtual bool isOnePeriodReady()=0;
	
	unsigned int getNbPeriodsReady() { if(m_period) return m_framecounter/m_period; else return 0;};
	virtual void decrementFrameCounter();
	virtual void incrementFrameCounter(int nbframes);
	
	// move to private?
	void resetFrameCounter();
    void resetXrunCounter();

	bool isRunning(); ///< returns true if there is some stream data processed
	void enable(); ///< enable the stream processing 
	void disable() {m_disabled=true;}; ///< disable the stream processing 
	bool isEnabled() {return !m_disabled;};

	virtual bool transfer(); ///< transfer the buffer contents from/to client

	virtual bool reset(); ///< reset the streams & buffers (e.g. after xrun)

	virtual bool prepare(); ///< prepare the streams & buffers (e.g. prefill)

	virtual void dumpInfo();

	virtual bool init();

 	virtual void setVerboseLevel(int l);

protected:
	

	void setManager(StreamProcessorManager *manager) {m_manager=manager;};
	void clearManager() {m_manager=0;};

	unsigned int m_nb_buffers; ///< cached from manager->getNbBuffers(), the number of periods to buffer
	unsigned int m_period; ///< cached from manager->getPeriod(), the period size

	unsigned int m_xruns;
	int m_framecounter;

	unsigned int m_framerate;

	StreamProcessorManager *m_manager;

	bool m_running;
	bool m_disabled;

    StreamStatistics m_PacketStat;
    StreamStatistics m_PeriodStat;
    
    StreamStatistics m_WakeupStat;
    

    DECLARE_DEBUG_MODULE;


};

/*!
\brief Class providing a generic interface for receive Stream Processors

*/
class ReceiveStreamProcessor : public StreamProcessor {

public:
	ReceiveStreamProcessor(int port, int framerate);

	virtual ~ReceiveStreamProcessor();


	virtual enum EProcessorType getType() {return E_Receive;};
	
	virtual enum raw1394_iso_disposition 
		getPacket(unsigned char *data, unsigned int *length,
	              unsigned char *tag, unsigned char *sy,
	              int cycle, unsigned int dropped, unsigned int max_length) 
	              {return RAW1394_ISO_STOP;};
	              
	virtual enum raw1394_iso_disposition putPacket(unsigned char *data, unsigned int length, 
	              unsigned char channel, unsigned char tag, unsigned char sy, 
		          unsigned int cycle, unsigned int dropped) = 0;
 	virtual void setVerboseLevel(int l);

protected:

     DECLARE_DEBUG_MODULE;


};

/*!
\brief Class providing a generic interface for receive Stream Processors

*/
class TransmitStreamProcessor : public StreamProcessor {

public:
	TransmitStreamProcessor(int port, int framerate);

	virtual ~TransmitStreamProcessor();

	virtual enum EProcessorType getType() {return E_Transmit;};

	virtual enum raw1394_iso_disposition 
		putPacket(unsigned char *data, unsigned int length, 
	              unsigned char channel, unsigned char tag, unsigned char sy, 
		          unsigned int cycle, unsigned int dropped) {return RAW1394_ISO_STOP;};
		          
	virtual enum raw1394_iso_disposition 
		getPacket(unsigned char *data, unsigned int *length,
	              unsigned char *tag, unsigned char *sy,
	              int cycle, unsigned int dropped, unsigned int max_length) = 0;
 	virtual void setVerboseLevel(int l);

protected:

     DECLARE_DEBUG_MODULE;


};

}

#endif /* __FREEBOB_STREAMPROCESSOR__ */


