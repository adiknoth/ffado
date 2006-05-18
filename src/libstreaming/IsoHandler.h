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

#include <libraw1394/raw1394.h>


enum raw1394_iso_disposition ;
namespace FreebobStreaming
{

class IsoStream;
class IsoXmitStream;
class IsoRecvStream;

typedef std::vector<IsoStream *> IsoStreamVector;
typedef std::vector<IsoStream *>::iterator IsoStreamVectorIterator;
typedef std::vector<IsoXmitStream *> IsoXmitStreamVector;
typedef std::vector<IsoXmitStream *>::iterator IsoXmitStreamVectorIterator;
typedef std::vector<IsoRecvStream *> IsoRecvStreamVector;
typedef std::vector<IsoRecvStream *>::iterator IsoRecvStreamVectorIterator;

/*!
\brief The Base Class for ISO Handlers

 These classes perform the actual ISO communication through libraw1394.
 They are different from IsoStreams because one handler can provide multiple
 streams with packets in case of ISO multichannel receive.

*/

class IsoHandler
{
	protected:
	
	public:
	
		enum EHandlerType {
			EHT_Receive,
			EHT_Transmit
		};
	
		IsoHandler(int port) 
		   : m_handle(0), m_port(port), 
		   m_buf_packets(400), m_max_packet_size(1024), m_irq_interval(-1)
		{}

		IsoHandler(int port, unsigned int buf_packets, unsigned int max_packet_size, int irq) 
		   : m_handle(0), m_port(port), 
		   m_buf_packets(buf_packets), m_max_packet_size( max_packet_size), m_irq_interval(irq)
		{}

		virtual ~IsoHandler()
		{}

	    bool initialize();
		int iterate() { if(m_handle) return raw1394_loop_iterate(m_handle); };
		
		void setVerboseLevel(int l) { setDebugLevel( l ); };

		// no setter functions, because those would require a re-init
		unsigned int getMaxPacketSize() { return m_max_packet_size;};
		unsigned int getBuffersize() { return m_buf_packets;};
		int getWakeupInterval() { return m_irq_interval;};

		virtual enum EHandlerType getType() = 0;

		virtual int start(int cycle) = 0;
		void stop();
		int getFileDescriptor() { raw1394_get_fd(m_handle);};

	protected:
	    raw1394handle_t m_handle;
    	int             m_port;
		unsigned int    m_buf_packets;
		unsigned int    m_max_packet_size;
		int             m_irq_interval;

    DECLARE_DEBUG_MODULE;

	private:



};

/*!
\brief ISO receive handler class
*/

class IsoRecvHandler : public IsoHandler 
{

	public:
		IsoRecvHandler(int port);
		IsoRecvHandler(int port, unsigned int buf_packets, unsigned int max_packet_size, int irq);
		virtual ~IsoRecvHandler();

		bool initialize();
	
		enum EHandlerType getType() { return EHT_Receive;};

		int registerStream(IsoRecvStream *);
		int unregisterStream(IsoRecvStream *);

		int start(int cycle);

	private:
		IsoRecvStream *m_Client;

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

	    bool initialize();
		
		enum EHandlerType getType() { return EHT_Transmit;};

		int registerStream(IsoXmitStream *);
		int unregisterStream(IsoXmitStream *);

		unsigned int getPreBuffers() {return m_prebuffers;};
		void setPreBuffers(unsigned int n) {m_prebuffers=n;};

		int start(int cycle);

	private:
		IsoXmitStream *m_Client;

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



