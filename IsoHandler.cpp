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

#include "IsoHandler.h"
#include "IsoStream.h"
#include <errno.h>
#include <netinet/in.h>

#include <iostream>
using namespace std;


namespace FreebobStreaming
{

IMPL_DEBUG_MODULE( IsoHandler, IsoHandler, DEBUG_LEVEL_NORMAL );

/* the C callbacks */
enum raw1394_iso_disposition 
IsoXmitHandler::iso_transmit_handler(raw1394handle_t handle,
		unsigned char *data, unsigned int *length,
		unsigned char *tag, unsigned char *sy,
		int cycle, unsigned int dropped) {

	IsoXmitHandler *xmitHandler=static_cast<IsoXmitHandler *>(raw1394_get_userdata(handle));
	assert(xmitHandler);

	return xmitHandler->getPacket(data, length, tag, sy, cycle, dropped);
}

enum raw1394_iso_disposition 
IsoRecvHandler::iso_receive_handler(raw1394handle_t handle, unsigned char *data, 
						unsigned int length, unsigned char channel,
						unsigned char tag, unsigned char sy, unsigned int cycle, 
						unsigned int dropped) {

	IsoRecvHandler *recvHandler=static_cast<IsoRecvHandler *>(raw1394_get_userdata(handle));
	assert(recvHandler);

	return recvHandler->putPacket(data, length, channel, tag, sy, cycle, dropped);
}

/* Base class implementation */
bool
IsoHandler::initialize()
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

    m_handle = raw1394_new_handle_on_port( m_port );
    if ( !m_handle ) {
        if ( !errno ) {
            cerr << "libraw1394 not compatible" << endl;
        } else {
            perror( "IsoHandler::Initialize: Could not get 1394 handle" );
            cerr << "Is ieee1394 and raw1394 driver loaded?" << endl;
        }
        return false;
    }

	raw1394_set_userdata(m_handle, static_cast<void *>(this));

    return true;
}

void IsoHandler::stop()
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
	raw1394_iso_stop(m_handle); 
};

void IsoHandler::dumpInfo()
{
	debugOutput( DEBUG_LEVEL_NORMAL, "  Stream type  : %s\n",
	     (this->getType()==EHT_Receive ? "Receive" : "Transmit"));
	debugOutput( DEBUG_LEVEL_NORMAL, "  Packet count : %d\n",this->getPacketCount());

};

/* Child class implementations */

IsoRecvHandler::IsoRecvHandler(int port)
		: IsoHandler(port)
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
}
IsoRecvHandler::IsoRecvHandler(int port, unsigned int buf_packets, 
                               unsigned int max_packet_size, int irq)
		: IsoHandler(port, buf_packets,max_packet_size,irq), m_Client(0)
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

}
IsoRecvHandler::~IsoRecvHandler()
{
	raw1394_iso_shutdown(m_handle);
	raw1394_destroy_handle(m_handle);

}

bool
IsoRecvHandler::initialize() {
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

	IsoHandler *base=static_cast<IsoHandler *>(this);

	if(!(base->initialize())) {
		return false;
	}

	raw1394_set_userdata(m_handle, static_cast<void *>(this));

	return true;

}

enum raw1394_iso_disposition IsoRecvHandler::putPacket(unsigned char *data, unsigned int length, 
		              unsigned char channel, unsigned char tag, unsigned char sy, 
			          unsigned int cycle, unsigned int dropped) {

	debugOutput( DEBUG_LEVEL_VERY_VERBOSE,
	             "received packet: length=%d, channel=%d, cycle=%d\n",
	             length, channel, cycle );
	m_packetcount++;

	if(m_Client) {
		if(m_Client->putPacket(data, length, channel, tag, sy, cycle, dropped)) {
// 			return RAW1394_ISO_AGAIN;
		}
	}
	
	return RAW1394_ISO_OK;
}

int IsoRecvHandler::registerStream(IsoStream *stream)
{
	assert(stream);
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

	if (m_Client) return -1;

	m_Client=stream;

	raw1394_iso_shutdown(m_handle);

	if(raw1394_iso_recv_init(m_handle,   iso_receive_handler,
                                         m_buf_packets,
                                         m_max_packet_size,
	                                     m_Client->getChannel(),
	                                     RAW1394_DMA_BUFFERFILL,
                                         m_irq_interval)) {
		debugFatal("Could not do receive initialisation!\n" );

		return -1;
	}

	return 0;

}

int IsoRecvHandler::unregisterStream(IsoStream *stream)
{
	assert(stream);
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

	if(stream != m_Client) return -1; //not registered

	m_Client=0;
	return 0;

}

int IsoRecvHandler::start(int cycle)
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
	return raw1394_iso_recv_start(m_handle, cycle, -1, 0);
}

/* ----------------- XMIT --------------- */

IsoXmitHandler::IsoXmitHandler(int port)
		: IsoHandler(port), m_prebuffers(0)
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

}
IsoXmitHandler::IsoXmitHandler(int port, unsigned int buf_packets, 
                               unsigned int max_packet_size, int irq)
		: IsoHandler(port, buf_packets, max_packet_size,irq),
		  m_speed(RAW1394_ISO_SPEED_400), m_prebuffers(0)
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

}
IsoXmitHandler::IsoXmitHandler(int port, unsigned int buf_packets, 
                               unsigned int max_packet_size, int irq,
                               enum raw1394_iso_speed speed)
		: IsoHandler(port, buf_packets,max_packet_size,irq),
		  m_speed(speed), m_prebuffers(0)
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

}

IsoXmitHandler::~IsoXmitHandler()
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
	raw1394_iso_shutdown(m_handle);
	raw1394_destroy_handle(m_handle);
}

bool
IsoXmitHandler::initialize() {

	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
	IsoHandler *base=static_cast<IsoHandler *>(this);

	if(!(base->initialize())) {
		return false;
	}

	raw1394_set_userdata(m_handle, static_cast<void *>(this));

	// this is a dummy init, to see if everything works
	// the real init is done when a stream is registered
	if(raw1394_iso_xmit_init(m_handle,
                             iso_transmit_handler,
                             m_buf_packets,
                             m_max_packet_size,
	                         0,
	                         m_speed,
                             m_irq_interval)) {
		debugFatal("Could not do xmit initialisation!\n" );

		return false;
	}

	return true;

}

enum raw1394_iso_disposition IsoXmitHandler::getPacket(unsigned char *data, unsigned int *length,
		              unsigned char *tag, unsigned char *sy,
		              int cycle, unsigned int dropped) {

	debugOutput( DEBUG_LEVEL_VERY_VERBOSE,
	             "sending packet: length=%d, cycle=%d\n",
	             *length, cycle );
	m_packetcount++;

	if(m_Client) {
    	if(m_Client->getPacket(data, length, tag, sy, cycle, dropped, m_max_packet_size)) {
// 			return RAW1394_ISO_AGAIN;
		}
	}

	return RAW1394_ISO_OK;
}

// an xmit handler can have only one source IsoStream
int IsoXmitHandler::registerStream(IsoStream *stream)
{
	assert(stream);
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

	if (m_Client) { 
		debugFatal("Already a registered client\n");
		return -1;
	}

	m_Client=stream;

	raw1394_iso_shutdown(m_handle);

	if(raw1394_iso_xmit_init(m_handle,
                             iso_transmit_handler,
                             m_buf_packets,
                             m_max_packet_size,
	                         m_Client->getChannel(),
	                         m_speed,
                             m_irq_interval)) {
		debugFatal("Could not do xmit initialisation!\n" );

		return -1;
	}

	return 0;

}

int IsoXmitHandler::unregisterStream(IsoStream *stream)
{
	assert(stream);
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

	if(stream != m_Client) return -1; //not registered

	m_Client=0;
	return 0;

}

int IsoXmitHandler::start(int cycle)
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
	return raw1394_iso_xmit_start(m_handle, cycle, m_prebuffers);
}

}

/* multichannel receive  */
#if 0
IsoRecvHandler::IsoRecvHandler(int port)
		: IsoHandler(port)
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
}
IsoRecvHandler::IsoRecvHandler(int port, unsigned int buf_packets, 
                               unsigned int max_packet_size, int irq)
		: IsoHandler(port, buf_packets,max_packet_size,irq)
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

}
IsoRecvHandler::~IsoRecvHandler()
{
	raw1394_iso_shutdown(m_handle);

}

bool
IsoRecvHandler::initialize() {
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

	IsoHandler *base=static_cast<IsoHandler *>(this);

	if(!(base->initialize())) {
		return false;
	}

	raw1394_set_userdata(m_handle, static_cast<void *>(this));

	if(raw1394_iso_multichannel_recv_init(m_handle,
                                         iso_receive_handler,
                                         m_buf_packets,
                                         m_max_packet_size,
                                         m_irq_interval)) {
		debugFatal("Could not do multichannel receive initialisation!\n" );

		return false;
	}

	return true;

}

enum raw1394_iso_disposition IsoRecvHandler::putPacket(unsigned char *data, unsigned int length, 
		              unsigned char channel, unsigned char tag, unsigned char sy, 
			          unsigned int cycle, unsigned int dropped) {

	debugOutput( DEBUG_LEVEL_VERY_VERBOSE,
	             "received packet: length=%d, channel=%d, cycle=%d\n",
	             length, channel, cycle );
	
	return RAW1394_ISO_OK;
}

// an recv handler can have multiple destination IsoStreams
// NOTE: this implementation even allows for already registered
// streams to be registered again.
int IsoRecvHandler::registerStream(IsoRecvStream *stream)
{
	assert(stream);
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

	m_Clients.push_back(stream);

	listen(stream->getChannel());
	return 0;

}

int IsoRecvHandler::unregisterStream(IsoRecvStream *stream)
{
	assert(stream);
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

    for ( IsoRecvStreamVectorIterator it = m_Clients.begin();
          it != m_Clients.end();
          ++it )
    {
        IsoRecvStream* s = *it;
        if ( s == stream ) { 
			unListen(s->getChannel());
            m_Clients.erase(it);
			return 0;
        }
    }

	return -1; //not found

}

void IsoRecvHandler::listen(int channel) {
	int retval;
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

	retval=raw1394_iso_recv_listen_channel(m_handle, channel);

}

void IsoRecvHandler::unListen(int channel) {
	int retval;
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

	retval=raw1394_iso_recv_unlisten_channel(m_handle, channel);

}

int IsoRecvHandler::start(int cycle)
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
	return raw1394_iso_recv_start(m_handle, cycle, -1, 0);
}
#endif
