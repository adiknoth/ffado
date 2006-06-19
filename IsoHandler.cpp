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
#include <assert.h>


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

int IsoHandler::busreset_handler(raw1394handle_t handle, unsigned int generation)
{	
	debugOutput( DEBUG_LEVEL_VERBOSE, "Busreset happened, generation %d...\n", generation);

	IsoHandler *handler=static_cast<IsoHandler *>(raw1394_get_userdata(handle));
	assert(handler);
	return handler->handleBusReset(generation);
}


/* Base class implementation */

IsoHandler::~IsoHandler() {
    stop();
    if(m_handle) raw1394_destroy_handle(m_handle);
    if(m_handle_util) raw1394_destroy_handle(m_handle_util);
    
}

bool
IsoHandler::init()
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "IsoHandler (%p) enter...\n",this);

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
	
	// a second handle for utility stuff
	m_handle_util = raw1394_new_handle_on_port( m_port );
	if ( !m_handle_util ) {
		if ( !errno ) {
			cerr << "libraw1394 not compatible" << endl;
		} else {
			perror( "IsoHandler::Initialize: Could not get 1394 handle" );
			cerr << "Is ieee1394 and raw1394 driver loaded?" << endl;
		}
		return false;
	}
	
	raw1394_set_userdata(m_handle_util, static_cast<void *>(this));
	
	if(raw1394_busreset_notify (m_handle, RAW1394_NOTIFY_ON)) {
		debugWarning("Could not enable busreset notification.\n");
		debugWarning(" Error message: %s\n",strerror(errno));
	}
	
	raw1394_set_bus_reset_handler(m_handle, busreset_handler);

	return true;
}

bool IsoHandler::stop()
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
	raw1394_iso_stop(m_handle); 
	return true;
}

/**
 * Returns the current value of the cycle counter
 *
 * @return the current value of the cycle counter
 */
#define CSR_CYCLE_TIME            0x200
#define CSR_REGISTER_BASE  0xfffff0000000ULL

unsigned int IsoHandler::getCycleCounter() {
    quadlet_t buf=0;
    
    // normally we should be able to use the same handle
    // because it is not iterated on by any other stuff
    // but I'm not sure
    raw1394_read(m_handle_util, raw1394_get_local_id(m_handle_util), 
        CSR_REGISTER_BASE | CSR_CYCLE_TIME, 4, &buf);
        
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "Current timestamp: %08X = %u\n",buf, ntohl(buf));
    
    return ntohl(buf) & 0xFFFFFFFF;
}

void IsoHandler::dumpInfo()
{

	int channel=-1;
	if (m_Client) channel=m_Client->getChannel();

	debugOutputShort( DEBUG_LEVEL_NORMAL, "  Handler type    : %s\n",
	     (this->getType()==EHT_Receive ? "Receive" : "Transmit"));
	debugOutputShort( DEBUG_LEVEL_NORMAL, "  Port, Channel  : %d, %d\n",
	     m_port, channel);
	debugOutputShort( DEBUG_LEVEL_NORMAL, "  Packet count   : %d (%d dropped)\n\n",
	     this->getPacketCount(), this->getDroppedCount());

};

void IsoHandler::setVerboseLevel(int l)
{
	setDebugLevel(l);
}

bool IsoHandler::registerStream(IsoStream *stream)
{
	assert(stream);
	debugOutput( DEBUG_LEVEL_VERBOSE, "registering stream (%p)\n", stream);

	if (m_Client) {
		debugFatal( "Generic IsoHandlers can have only one client\n");	
		return false;
	}

	m_Client=stream;

	m_Client->setHandler(this);

	return true;

}

bool IsoHandler::unregisterStream(IsoStream *stream)
{
	assert(stream);
	debugOutput( DEBUG_LEVEL_VERBOSE, "unregistering stream (%p)\n", stream);

	if(stream != m_Client) {
		debugFatal( "no client registered\n");	
		return false;
	}

	m_Client->clearHandler();
	
	m_Client=0;
	return true;

}

/* Child class implementations */

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
	raw1394_destroy_handle(m_handle);

}

bool
IsoRecvHandler::init() {
	debugOutput( DEBUG_LEVEL_VERBOSE, "init recv handler %p\n",this);

	if(!(IsoHandler::init())) {
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
	m_packetcount++;
	m_dropped+=dropped;

	if(m_Client) {
		return m_Client->putPacket(data, length, channel, tag, sy, cycle, dropped);
	}
	
	return RAW1394_ISO_OK;
}

bool IsoRecvHandler::prepare()
{
	raw1394_iso_shutdown(m_handle);
	
	debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing iso receive handler (%p)\n",this);
	debugOutput( DEBUG_LEVEL_VERBOSE, " Buffers         : %d \n",m_buf_packets);
	debugOutput( DEBUG_LEVEL_VERBOSE, " Max Packet size : %d \n",m_max_packet_size);
	debugOutput( DEBUG_LEVEL_VERBOSE, " Channel         : %d \n",m_Client->getChannel());
	debugOutput( DEBUG_LEVEL_VERBOSE, " Irq interval    : %d \n",m_irq_interval);

	if(raw1394_iso_recv_init(m_handle,   iso_receive_handler,
                                         m_buf_packets,
                                         m_max_packet_size,
	                                     m_Client->getChannel(),
	                                     RAW1394_DMA_BUFFERFILL,
                                         m_irq_interval)) {
		debugFatal("Could not do receive initialisation!\n" );
		debugFatal("  %s\n",strerror(errno));

		return false;
	}
	return true;
}

bool IsoRecvHandler::start(int cycle)
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "start on cycle %d\n", cycle);
	
	if(raw1394_iso_recv_start(m_handle, cycle, -1, 0)) {
		debugFatal("Could not start receive handler (%s)\n",strerror(errno));
		return false;
	}
	return true;
}

int IsoRecvHandler::handleBusReset(unsigned int generation) {
	debugOutput( DEBUG_LEVEL_VERBOSE, "handle bus reset...\n");
	//TODO: implement busreset
	return 0;
}

/* ----------------- XMIT --------------- */

IsoXmitHandler::IsoXmitHandler(int port)
		: IsoHandler(port), m_prebuffers(0)
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "IsoXmitHandler enter...\n");

}
IsoXmitHandler::IsoXmitHandler(int port, unsigned int buf_packets, 
                               unsigned int max_packet_size, int irq)
		: IsoHandler(port, buf_packets, max_packet_size,irq),
		  m_speed(RAW1394_ISO_SPEED_400), m_prebuffers(0)
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "IsoXmitHandler enter...\n");

}
IsoXmitHandler::IsoXmitHandler(int port, unsigned int buf_packets, 
                               unsigned int max_packet_size, int irq,
                               enum raw1394_iso_speed speed)
		: IsoHandler(port, buf_packets,max_packet_size,irq),
		  m_speed(speed), m_prebuffers(0)
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "IsoXmitHandler enter...\n");

}

IsoXmitHandler::~IsoXmitHandler()
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
	raw1394_iso_shutdown(m_handle);
	raw1394_destroy_handle(m_handle);
}

bool
IsoXmitHandler::init() {

	debugOutput( DEBUG_LEVEL_VERBOSE, "init xmit handler %p\n",this);

	if(!(IsoHandler::init())) {
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
	m_dropped+=dropped;

	if(m_Client) {
    	return m_Client->getPacket(data, length, tag, sy, cycle, dropped, m_max_packet_size);
	}
	
	return RAW1394_ISO_OK;
}

bool IsoXmitHandler::prepare()
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing iso transmit handler (%p, client=%p)\n",this,m_Client);
	
// 	raw1394_iso_shutdown(m_handle);
	debugOutput( DEBUG_LEVEL_VERBOSE, " Buffers         : %d \n",m_buf_packets);
	debugOutput( DEBUG_LEVEL_VERBOSE, " Max Packet size : %d \n",m_max_packet_size);
	debugOutput( DEBUG_LEVEL_VERBOSE, " Channel         : %d \n",m_Client->getChannel());
	debugOutput( DEBUG_LEVEL_VERBOSE, " Speed           : %d \n",m_speed);
	debugOutput( DEBUG_LEVEL_VERBOSE, " Irq interval    : %d \n",m_irq_interval);

	if(raw1394_iso_xmit_init(m_handle,
                             iso_transmit_handler,
                             m_buf_packets,
                             m_max_packet_size,
	                         m_Client->getChannel(),
	                         m_speed,
                             m_irq_interval)) {
		debugFatal("Could not do xmit initialisation!\n" );

		return false;
	}

	return true;
}

bool IsoXmitHandler::start(int cycle)
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "start on cycle %d\n", cycle);
	if(raw1394_iso_xmit_start(m_handle, cycle, m_prebuffers)) {
		debugFatal("Could not start xmit handler (%s)\n",strerror(errno));
		return false;
	}
	return true;
}

int IsoXmitHandler::handleBusReset(unsigned int generation) {
	debugOutput( DEBUG_LEVEL_VERBOSE, "bus reset...\n");
	//TODO: implement busreset
	return 0;
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
