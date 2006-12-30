/* $Id$ */

/*
 *   FreeBob Streaming API
 *   FreeBob = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2006 Pieter Palmers <pieterpalmers@users.sourceforge.net>
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
#ifndef __FREEBOB_ISOHANDLER__
#define __FREEBOB_ISOHANDLER__

#include "../debugmodule/debugmodule.h"

#include "libutil/TimeSource.h"

#include <libraw1394/raw1394.h>


enum raw1394_iso_disposition ;
namespace FreebobStreaming
{

class IsoStream;
/*!
\brief The Base Class for ISO Handlers

 These classes perform the actual ISO communication through libraw1394.
 They are different from IsoStreams because one handler can provide multiple
 streams with packets in case of ISO multichannel receive.

*/

class IsoHandler : public FreebobUtil::TimeSource
{
	protected:
	
	public:
	
		enum EHandlerType {
			EHT_Receive,
			EHT_Transmit
		};
	
		IsoHandler(int port);

		IsoHandler(int port, unsigned int buf_packets, unsigned int max_packet_size, int irq);

		virtual ~IsoHandler();

	    	virtual bool init();
	    	
		int iterate() { if(m_handle) return raw1394_loop_iterate(m_handle); else return -1; };
		
		void setVerboseLevel(int l);

		// no setter functions, because those would require a re-init
		unsigned int getMaxPacketSize() { return m_max_packet_size;};
		unsigned int getBuffersize() { return m_buf_packets;};
		int getWakeupInterval() { return m_irq_interval;};

		int getPacketCount() {return m_packetcount;};
		void resetPacketCount() {m_packetcount=0;};

		int getDroppedCount() {return m_dropped;};
		void resetDroppedCount() {m_dropped=0;};

		virtual enum EHandlerType getType() = 0;

		virtual bool start(int cycle) = 0;
		virtual bool stop();
		
		int getFileDescriptor() { return raw1394_get_fd(m_handle);};

		void dumpInfo();

		bool inUse() {return (m_Client != 0) ;};
		virtual bool isStreamRegistered(IsoStream *s) {return (m_Client == s);};

		virtual bool registerStream(IsoStream *);
		virtual bool unregisterStream(IsoStream *);

		int getLocalNodeId() {return raw1394_get_local_id( m_handle );};
		int getPort() {return m_port;};

		virtual bool prepare() = 0;
		
		// get the most recent cycle counter value
		// RT safe
		unsigned int getCycleCounter();
		
		// update the cycle counter cache
		// not RT safe
		// the isohandlermanager is responsible for calling this!
        bool updateCycleCounter();
        float getTicksPerUsec() {return m_ticks_per_usec;};

        // register a master timing source
        bool setSyncMaster(FreebobUtil::TimeSource *t);
    
	protected:
	    raw1394handle_t m_handle;
        raw1394handle_t m_handle_util;
   		int             m_port;
		unsigned int    m_buf_packets;
		unsigned int    m_max_packet_size;
		int             m_irq_interval;
		
		unsigned int        m_cyclecounter_ticks;
        freebob_microsecs_t m_lastmeas_usecs;
        float               m_ticks_per_usec;
        float               m_ticks_per_usec_dll_err2;
        
		int m_packetcount;
		int m_dropped;

		IsoStream *m_Client;

        FreebobUtil::TimeSource *m_TimeSource;

		virtual int handleBusReset(unsigned int generation);


		DECLARE_DEBUG_MODULE;

	private:
		static int busreset_handler(raw1394handle_t handle, unsigned int generation);

        void initCycleCounter();

    // implement the TimeSource interface
    public:
        freebob_microsecs_t getCurrentTime();
        freebob_microsecs_t getCurrentTimeAsUsecs();
    private:
        // to cope with wraparound
        unsigned int m_TimeSource_LastSecs;
        unsigned int m_TimeSource_NbCycleWraps;

};

/*!
\brief ISO receive handler class (not multichannel) 
*/

class IsoRecvHandler : public IsoHandler 
{

	public:
		IsoRecvHandler(int port);
		IsoRecvHandler(int port, unsigned int buf_packets, unsigned int max_packet_size, int irq);
		virtual ~IsoRecvHandler();

		bool init();
	
		enum EHandlerType getType() { return EHT_Receive;};

// 		int registerStream(IsoStream *);
// 		int unregisterStream(IsoStream *);

		bool start(int cycle);

		bool prepare();

	protected:
		int handleBusReset(unsigned int generation);

	private:
		static enum raw1394_iso_disposition 
 		iso_receive_handler(raw1394handle_t handle, unsigned char *data, 
						unsigned int length, unsigned char channel,
						unsigned char tag, unsigned char sy, unsigned int cycle, 
						unsigned int dropped);

		enum raw1394_iso_disposition  
			putPacket(unsigned char *data, unsigned int length, 
		              unsigned char channel, unsigned char tag, unsigned char sy, 
			          unsigned int cycle, unsigned int dropped);

};

/*!
\brief ISO transmit handler class
*/

class IsoXmitHandler  : public IsoHandler 
{
   	public:
		IsoXmitHandler(int port);
		IsoXmitHandler(int port, unsigned int buf_packets, 
			       unsigned int max_packet_size, int irq);
		IsoXmitHandler(int port, unsigned int buf_packets, 
			       unsigned int max_packet_size, int irq, 
			       enum raw1394_iso_speed speed);
        	virtual ~IsoXmitHandler();

		bool init();
		
		enum EHandlerType getType() { return EHT_Transmit;};

// 		int registerStream(IsoStream *);
// 		int unregisterStream(IsoStream *);

		unsigned int getPreBuffers() {return m_prebuffers;};
		void setPreBuffers(unsigned int n) {m_prebuffers=n;};

		bool start(int cycle);

		bool prepare();

    protected:
    	int handleBusReset(unsigned int generation);

	private:
		static enum raw1394_iso_disposition iso_transmit_handler(raw1394handle_t handle,
				unsigned char *data, unsigned int *length,
				unsigned char *tag, unsigned char *sy,
				int cycle, unsigned int dropped);
		enum raw1394_iso_disposition  
			getPacket(unsigned char *data, unsigned int *length,
		              unsigned char *tag, unsigned char *sy,
		              int cycle, unsigned int dropped);

		enum raw1394_iso_speed m_speed;
		
		unsigned int m_prebuffers;

};

}

#endif /* __FREEBOB_ISOHANDLER__  */



