/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software Foundation;
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#include "IsoHandler.h"
#include "cycletimer.h"
#include "../generic/IsoStream.h"

#include "libutil/TimeSource.h"
#include "libutil/SystemTimeSource.h"

#include <errno.h>
#include <netinet/in.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include <iostream>
using namespace std;

#define CC_SLEEP_TIME_AFTER_UPDATE    1000
#define CC_SLEEP_TIME_AFTER_FAILURE     10
#define CC_DLL_COEFF     ((0.001)*((float)(CC_SLEEP_TIME_AFTER_UPDATE/1000.0)))

#define CC_MAX_RATE_ERROR           (2.0/100.0)
#define CC_INIT_MAX_TRIES 10


namespace Streaming
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
IsoHandler::IsoHandler(int port)
   :  m_handle(0), m_handle_util(0), m_port(port),
   m_buf_packets(400), m_max_packet_size(1024), m_irq_interval(-1),
   m_packetcount(0), m_dropped(0), m_Client(0),
   m_State(E_Created)
{
}

IsoHandler::IsoHandler(int port, unsigned int buf_packets, unsigned int max_packet_size, int irq)
   : m_handle(0), m_port(port),
   m_buf_packets(buf_packets), m_max_packet_size( max_packet_size),
   m_irq_interval(irq),
   m_packetcount(0), m_dropped(0), m_Client(0),
   m_State(E_Created)
{
}

IsoHandler::~IsoHandler() {

// Don't call until libraw1394's raw1394_new_handle() function has been
// fixed to correctly initialise the iso_packet_infos field.  Bug is
// confirmed present in libraw1394 1.2.1.  In any case,
// raw1394_destroy_handle() will do any iso system shutdown required.
//     raw1394_iso_shutdown(m_handle);

    if(m_handle) {
        if (m_State == E_Running) {
            stop();
        }

        raw1394_destroy_handle(m_handle);
    }

    if(m_handle_util) raw1394_destroy_handle(m_handle_util);

}

bool IsoHandler::iterate() {
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "IsoHandler (%p) iterate...\n",this);

    if(m_handle) {
        if(raw1394_loop_iterate(m_handle)) {
            debugOutput( DEBUG_LEVEL_VERBOSE,
                 "IsoHandler (%p): Failed to iterate handler: %s\n",
                 this,strerror(errno));
            return false;
        } else {
            return true;
        }
    } else {
        return false;
    }
}

bool
IsoHandler::init()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "IsoHandler (%p) enter...\n",this);

    // check the state
    if(m_State != E_Created) {
        debugError("Incorrect state, expected E_Created, got %d\n",(int)m_State);
        return false;
    }

    // the main handle for the ISO traffic
    m_handle = raw1394_new_handle_on_port( m_port );
    if ( !m_handle ) {
        if ( !errno ) {
            debugError("libraw1394 not compatible\n");
        } else {
            debugError("Could not get 1394 handle: %s\n", strerror(errno) );
            debugError("Are ieee1394 and raw1394 drivers loaded?\n");
        }
        return false;
    }
    raw1394_set_userdata(m_handle, static_cast<void *>(this));

    // a second handle for utility stuff
    m_handle_util = raw1394_new_handle_on_port( m_port );
    if ( !m_handle_util ) {
        if ( !errno ) {
            debugError("libraw1394 not compatible\n");
        } else {
            debugError("Could not get 1394 handle: %s\n", strerror(errno) );
            debugError("Are ieee1394 and raw1394 drivers loaded?\n");
        }

        raw1394_destroy_handle(m_handle);
        return false;
    }
    raw1394_set_userdata(m_handle_util, static_cast<void *>(this));

    // bus reset handling
    if(raw1394_busreset_notify (m_handle, RAW1394_NOTIFY_ON)) {
        debugWarning("Could not enable busreset notification.\n");
        debugWarning(" Error message: %s\n",strerror(errno));
        debugWarning("Continuing without bus reset support.\n");
    } else {
        // apparently this cannot fail
        raw1394_set_bus_reset_handler(m_handle, busreset_handler);
    }

    // test the cycle timer read function
    int err;
    uint32_t cycle_timer;
    uint64_t local_time;
    err=raw1394_read_cycle_timer(m_handle_util, &cycle_timer, &local_time);
    if(err) {
        debugError("raw1394_read_cycle_timer failed.\n");
        debugError(" Error: %s\n", strerror(err));
        debugError(" Your system doesn't seem to support the raw1394_read_cycle_timer call\n");
        return false;
    }

    // update the internal state
    m_State=E_Initialized;

    return true;
}

bool IsoHandler::prepare()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "IsoHandler (%p) enter...\n",this);

    // check the state
    if(m_State != E_Initialized) {
        debugError("Incorrect state, expected E_Initialized, got %d\n",(int)m_State);
        return false;
    }

    // Don't call until libraw1394's raw1394_new_handle() function has been
    // fixed to correctly initialise the iso_packet_infos field.  Bug is
    // confirmed present in libraw1394 1.2.1.

//     raw1394_iso_shutdown(m_handle);

    m_State = E_Prepared;

    return true;
}

bool IsoHandler::start(int cycle)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

    // check the state
    if(m_State != E_Prepared) {
        debugError("Incorrect state, expected E_Prepared, got %d\n",(int)m_State);
        return false;
    }

    m_State=E_Running;

    return true;
}

bool IsoHandler::stop()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

    // check state
    if(m_State != E_Running) {
        debugError("Incorrect state, expected E_Running, got %d\n",(int)m_State);
        return false;
    }

    // this is put here to try and avoid the
    // Runaway context problem
    // don't know if it will help though.
    raw1394_iso_xmit_sync(m_handle);

    raw1394_iso_stop(m_handle);

    m_State=E_Prepared;

    return true;
}

/**
 * Bus reset handler
 *
 * @return ?
 */

int IsoHandler::handleBusReset(unsigned int generation) {
    debugOutput( DEBUG_LEVEL_VERBOSE, "bus reset...\n");

    // do a simple read on ourself in order to update the internal structures
    // this avoids read failures after a bus reset
    quadlet_t buf=0;
    raw1394_read(m_handle, raw1394_get_local_id(m_handle),
                 CSR_REGISTER_BASE | CSR_CYCLE_TIME, 4, &buf);

    return 0;
}

/**
 * Returns the current value of the cycle timer (in ticks)
 *
 * @return the current value of the cycle timer (in ticks)
 */

unsigned int IsoHandler::getCycleTimerTicks() {
    // the new api should be realtime safe.
    // it might cause a reschedule when turning preemption,
    // back on but that won't hurt us if we have sufficient
    // priority
    int err;
    uint32_t cycle_timer;
    uint64_t local_time;
    err=raw1394_read_cycle_timer(m_handle_util, &cycle_timer, &local_time);
    if(err) {
        debugWarning("raw1394_read_cycle_timer: %s\n", strerror(err));
    }
    return CYCLE_TIMER_TO_TICKS(cycle_timer);
}

/**
 * Returns the current value of the cycle timer (as is)
 *
 * @return the current value of the cycle timer (as is)
 */

unsigned int IsoHandler::getCycleTimer() {
    // the new api should be realtime safe.
    // it might cause a reschedule when turning preemption,
    // back on but that won't hurt us if we have sufficient
    // priority
    int err;
    uint32_t cycle_timer;
    uint64_t local_time;
    err=raw1394_read_cycle_timer(m_handle_util, &cycle_timer, &local_time);
    if(err) {
        debugWarning("raw1394_read_cycle_timer: %s\n", strerror(err));
    }
    return cycle_timer;
}

void IsoHandler::dumpInfo()
{

    int channel=-1;
    if (m_Client) channel=m_Client->getChannel();

    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Handler type    : %s\n",
            (this->getType()==EHT_Receive ? "Receive" : "Transmit"));
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Port, Channel   : %2d, %2d\n",
            m_port, channel);
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Packet count    : %10d (%5d dropped)\n",
            this->getPacketCount(), this->getDroppedCount());
}

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

}

bool
IsoRecvHandler::init() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "init recv handler %p\n",this);

    if(!(IsoHandler::init())) {
        return false;
    }
    return true;

}

enum raw1394_iso_disposition IsoRecvHandler::putPacket(
                    unsigned char *data, unsigned int length,
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

    // prepare the generic IsoHandler
    if(!IsoHandler::prepare()) {
        return false;
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing iso receive handler (%p)\n",this);
    debugOutput( DEBUG_LEVEL_VERBOSE, " Buffers         : %d \n", m_buf_packets);
    debugOutput( DEBUG_LEVEL_VERBOSE, " Max Packet size : %d \n", m_max_packet_size);
    debugOutput( DEBUG_LEVEL_VERBOSE, " Channel         : %d \n", m_Client->getChannel());
    debugOutput( DEBUG_LEVEL_VERBOSE, " Irq interval    : %d \n", m_irq_interval);
    debugOutput( DEBUG_LEVEL_VERBOSE, " Mode            : %s \n", 
                               (m_irq_interval > 1)?"DMA_BUFFERFILL":"PACKET_PER_BUFFER");

    if(m_irq_interval > 1) {
        if(raw1394_iso_recv_init(m_handle,
                                iso_receive_handler,
                                m_buf_packets,
                                m_max_packet_size,
                                m_Client->getChannel(),
                                RAW1394_DMA_BUFFERFILL,
                                m_irq_interval)) {
            debugFatal("Could not do receive initialisation!\n" );
            debugFatal("  %s\n",strerror(errno));

            return false;
        }
    } else {
        if(raw1394_iso_recv_init(m_handle,
                                iso_receive_handler,
                                m_buf_packets,
                                m_max_packet_size,
                                m_Client->getChannel(),
                                RAW1394_DMA_PACKET_PER_BUFFER,
                                m_irq_interval)) {
            debugFatal("Could not do receive initialisation!\n" );
            debugFatal("  %s\n",strerror(errno));

            return false;
        }
    }
    return true;
}

bool IsoRecvHandler::start(int cycle)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "start on cycle %d\n", cycle);

    // start the generic IsoHandler
    if(!IsoHandler::start(cycle)) {
        return false;
    }

    if(raw1394_iso_recv_start(m_handle, cycle, -1, 0)) {
        debugFatal("Could not start receive handler (%s)\n",strerror(errno));
        return false;
    }
    return true;
}

int IsoRecvHandler::handleBusReset(unsigned int generation) {
    debugOutput( DEBUG_LEVEL_VERBOSE, "handle bus reset...\n");

    //TODO: implement busreset

    // pass on the busreset signal
    if(IsoHandler::handleBusReset(generation)) {
        return -1;
    }
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
    // handle cleanup is done in the IsoHanlder destructor
}

bool
IsoXmitHandler::init() {

    debugOutput( DEBUG_LEVEL_VERBOSE, "init xmit handler %p\n",this);

    if(!(IsoHandler::init())) {
        return false;
    }

    return true;
}

bool IsoXmitHandler::prepare()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing iso transmit handler (%p, client=%p)\n",this,m_Client);

    if(!(IsoHandler::prepare())) {
        return false;
    }

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
    debugOutput( DEBUG_LEVEL_VERBOSE, "start on cycle %d, %d prebuffers\n", 
        cycle, m_prebuffers);

    if(!(IsoHandler::start(cycle))) {
        return false;
    }

    if(raw1394_iso_xmit_start(m_handle, cycle, m_prebuffers)) {
        debugFatal("Could not start xmit handler (%s)\n",strerror(errno));
        return false;
    }
    return true;
}

enum raw1394_iso_disposition IsoXmitHandler::getPacket(
                    unsigned char *data, unsigned int *length,
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

int IsoXmitHandler::handleBusReset(unsigned int generation) {
    debugOutput( DEBUG_LEVEL_VERBOSE, "bus reset...\n");
    //TODO: implement busreset

    // pass on the busreset signal
    if(IsoHandler::handleBusReset(generation)) {
            return -1;
    }

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
// Don't call until libraw1394's raw1394_new_handle() function has been
// fixed to correctly initialise the iso_packet_infos field.  Bug is
// confirmed present in libraw1394 1.2.1.  In any case,
// raw1394_destroy_handle() (in the base class destructor) will do any iso
// system shutdown required.
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
