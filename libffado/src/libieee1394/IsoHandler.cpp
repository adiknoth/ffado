/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include "IsoHandler.h"
#include "ieee1394service.h" 

#include "libstreaming/generic/StreamProcessor.h"
#include "libutil/PosixThread.h"

#include <errno.h>
#include <netinet/in.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include <iostream>
using namespace std;
using namespace Streaming;

IMPL_DEBUG_MODULE( IsoHandler, IsoHandler, DEBUG_LEVEL_NORMAL );

/* the C callbacks */
enum raw1394_iso_disposition
IsoHandler::iso_transmit_handler(raw1394handle_t handle,
        unsigned char *data, unsigned int *length,
        unsigned char *tag, unsigned char *sy,
        int cycle, unsigned int dropped1) {

    IsoHandler *xmitHandler = static_cast<IsoHandler *>(raw1394_get_userdata(handle));
    assert(xmitHandler);
    unsigned int skipped = (dropped1 & 0xFFFF0000) >> 16;
    unsigned int dropped = dropped1 & 0xFFFF;

    return xmitHandler->getPacket(data, length, tag, sy, cycle, dropped, skipped);
}

enum raw1394_iso_disposition
IsoHandler::iso_receive_handler(raw1394handle_t handle, unsigned char *data,
                        unsigned int length, unsigned char channel,
                        unsigned char tag, unsigned char sy, unsigned int cycle,
                        unsigned int dropped1) {

    IsoHandler *recvHandler = static_cast<IsoHandler *>(raw1394_get_userdata(handle));
    assert(recvHandler);

    unsigned int skipped = (dropped1 & 0xFFFF0000) >> 16;
    unsigned int dropped = dropped1 & 0xFFFF;

    return recvHandler->putPacket(data, length, channel, tag, sy, cycle, dropped, skipped);
}

int IsoHandler::busreset_handler(raw1394handle_t handle, unsigned int generation)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Busreset happened, generation %d...\n", generation);

    IsoHandler *handler = static_cast<IsoHandler *>(raw1394_get_userdata(handle));
    assert(handler);
    return handler->handleBusReset(generation);
}

IsoHandler::IsoHandler(IsoHandlerManager& manager, enum EHandlerType t)
   : m_manager( manager )
   , m_type ( t )
   , m_handle( 0 )
   , m_buf_packets( 400 )
   , m_max_packet_size( 1024 )
   , m_irq_interval( -1 )
   , m_Client( 0 )
   , m_speed( RAW1394_ISO_SPEED_400 )
   , m_prebuffers( 0 )
   , m_State( E_Created )
{
}

IsoHandler::IsoHandler(IsoHandlerManager& manager, enum EHandlerType t, 
                       unsigned int buf_packets, unsigned int max_packet_size, int irq)
   : m_manager( manager )
   , m_type ( t )
   , m_handle( 0 )
   , m_buf_packets( buf_packets )
   , m_max_packet_size( max_packet_size )
   , m_irq_interval( irq )
   , m_Client( 0 )
   , m_speed( RAW1394_ISO_SPEED_400 )
   , m_prebuffers( 0 )
   , m_State( E_Created )
{
}

IsoHandler::IsoHandler(IsoHandlerManager& manager, enum EHandlerType t, unsigned int buf_packets,
                       unsigned int max_packet_size, int irq,
                       enum raw1394_iso_speed speed)
   : m_manager( manager )
   , m_type ( t )
   , m_handle( 0 )
   , m_buf_packets( buf_packets )
   , m_max_packet_size( max_packet_size )
   , m_irq_interval( irq )
   , m_Client( 0 )
   , m_speed( speed )
   , m_prebuffers( 0 )
   , m_State( E_Created )
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
            disable();
        }
        raw1394_destroy_handle(m_handle);
    }
}

bool
IsoHandler::waitForClient()
{
    debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE, "waiting...\n");
    if(m_Client) {
        bool result;
        if (m_type == eHT_Receive) {
            result = m_Client->waitForProducePacket();
        } else {
            result = m_Client->waitForConsumePacket();
        }
        debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE, " returns %d\n", result);
        return result;
    } else {
        debugOutputExtreme(DEBUG_LEVEL_VERBOSE, " no client\n");
    }
    return false;
}

bool
IsoHandler::tryWaitForClient()
{
    debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE, "waiting...\n");
    if(m_Client) {
        bool result;
        if (m_type == eHT_Receive) {
            result = m_Client->canProducePacket();
        } else {
            result = m_Client->canConsumePacket();
        }
        debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE, " returns %d\n", result);
        return result;
    } else {
        debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE, " no client\n");
    }
    return false;
}
/*
bool
IsoHandler::Execute()
{
    debugOutputExtreme( DEBUG_LEVEL_VERY_VERBOSE, "%p: Execute thread...\n", this);

    // bypass if not running
    if (m_State != E_Running) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "%p: not polling since not running...\n", this);
        usleep(m_poll_timeout * 1000);
        debugOutput( DEBUG_LEVEL_VERBOSE, "%p: done sleeping...\n", this);
        return true;
    }

    // wait for the availability of frames in the client
    // (blocking for transmit handlers)
    debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE, "(%p, %s) Waiting for Client activity...\n", this, getTypeString());
    if (waitForClient()) {
#if ISOHANDLER_USE_POLL
        bool result = true;
        while(result && m_Client && tryWaitForClient()) {
            int err = poll(&m_poll_fd, 1, m_poll_timeout);
            if (err == -1) {
                if (errno == EINTR) {
                    return true;
                }
                debugFatal("%p, poll error: %s\n", this, strerror (errno));
                return false;
            }

            if(m_poll_fd.revents & (POLLIN)) {
                result=iterate();
                if(!result) {
                    debugOutput( DEBUG_LEVEL_VERBOSE,
                                "IsoHandler (%p): Failed to iterate handler\n",
                                this);
                }
            } else {
                if (m_poll_fd.revents & POLLERR) {
                    debugWarning("error on fd for %p\n", this);
                }
                if (m_poll_fd.revents & POLLHUP) {
                    debugWarning("hangup on fd for %p\n",this);
                }
                break;
            }
        }
        return result;
#else
        // iterate() is blocking if no 1394 data is available
        // so poll'ing is not really necessary
        bool result = true;
        while(result && m_Client && tryWaitForClient()) {
            result = iterate();
//             if (getType() == eHT_Receive) {
//                 debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "(%p, %s) Iterate returned: %d\n",
//                             this, (m_type==eHT_Receive?"Receive":"Transmit"), result);
//             }
        }
        return result;
#endif
    } else {
        debugError("waitForClient() failed.\n");
        return false;
    }
}*/

bool
IsoHandler::iterate() {
    debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE, "(%p, %s) Iterating ISO handler...\n",
                this, getTypeString());
    if(m_State == E_Running) {
#if ISOHANDLER_FLUSH_BEFORE_ITERATE
        flush();
#endif
        if(raw1394_loop_iterate(m_handle)) {
            debugError( "IsoHandler (%p): Failed to iterate handler: %s\n",
                        this, strerror(errno));
            return false;
        }
        debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE, "(%p, %s) done interating ISO handler...\n",
                           this, getTypeString());
        return true;
    } else {
        debugOutput(DEBUG_LEVEL_VERBOSE, "(%p, %s) Not iterating a non-running handler...\n",
                    this, getTypeString());
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
    m_handle = raw1394_new_handle_on_port( m_manager.get1394Service().getPort() );
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

    // bus reset handling
    if(raw1394_busreset_notify (m_handle, RAW1394_NOTIFY_ON)) {
        debugWarning("Could not enable busreset notification.\n");
        debugWarning(" Error message: %s\n",strerror(errno));
        debugWarning("Continuing without bus reset support.\n");
    } else {
        // apparently this cannot fail
        raw1394_set_bus_reset_handler(m_handle, busreset_handler);
    }

    // update the internal state
    m_State=E_Initialized;
    return true;
}

bool IsoHandler::disable()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "(%p, %s) enter...\n", 
                 this, (m_type==eHT_Receive?"Receive":"Transmit"));

    // check state
    if(m_State == E_Prepared) return true;
    if(m_State != E_Running) {
        debugError("Incorrect state, expected E_Running, got %d\n",(int)m_State);
        return false;
    }

    // this is put here to try and avoid the
    // Runaway context problem
    // don't know if it will help though.
    raw1394_iso_xmit_sync(m_handle);
    raw1394_iso_stop(m_handle);
    m_State = E_Prepared;
    return true;
}

/**
 * Bus reset handler
 *
 * @return ?
 */

int
IsoHandler::handleBusReset(unsigned int generation)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "bus reset...\n");

    #define CSR_CYCLE_TIME            0x200
    #define CSR_REGISTER_BASE  0xfffff0000000ULL
    // do a simple read on ourself in order to update the internal structures
    // this avoids read failures after a bus reset
    quadlet_t buf=0;
    raw1394_read(m_handle, raw1394_get_local_id(m_handle),
                 CSR_REGISTER_BASE | CSR_CYCLE_TIME, 4, &buf);
    return 0;
}

void IsoHandler::dumpInfo()
{
    int channel=-1;
    if (m_Client) channel=m_Client->getChannel();

    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Handler type................: %s\n",
            getTypeString());
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Port, Channel...............: %2d, %2d\n",
            m_manager.get1394Service().getPort(), channel);
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Buffer, MaxPacketSize, IRQ..: %4d, %4d, %4d\n",
            m_buf_packets, m_max_packet_size, m_irq_interval);
    if (this->getType() == eHT_Transmit) {
        debugOutputShort( DEBUG_LEVEL_NORMAL, "  Speed, PreBuffers...........: %2d, %2d\n",
                                            m_speed, m_prebuffers);
    }
}

void IsoHandler::setVerboseLevel(int l)
{
    setDebugLevel(l);
}

bool IsoHandler::registerStream(StreamProcessor *stream)
{
    assert(stream);
    debugOutput( DEBUG_LEVEL_VERBOSE, "registering stream (%p)\n", stream);

    if (m_Client) {
            debugFatal( "Generic IsoHandlers can have only one client\n");
            return false;
    }
    m_Client=stream;
    return true;
}

bool IsoHandler::unregisterStream(StreamProcessor *stream)
{
    assert(stream);
    debugOutput( DEBUG_LEVEL_VERBOSE, "unregistering stream (%p)\n", stream);

    if(stream != m_Client) {
            debugFatal( "no client registered\n");
            return false;
    }
    m_Client=0;
    return true;
}

void IsoHandler::flush()
{
    if(m_type == eHT_Receive) {
        raw1394_iso_recv_flush(m_handle);
    } else {
        // do nothing
    }
}

// ISO packet interface
enum raw1394_iso_disposition IsoHandler::putPacket(
                    unsigned char *data, unsigned int length,
                    unsigned char channel, unsigned char tag, unsigned char sy,
                    unsigned int cycle, unsigned int dropped, unsigned int skipped) {

    debugOutputExtreme(DEBUG_LEVEL_ULTRA_VERBOSE,
                       "received packet: length=%d, channel=%d, cycle=%d\n",
                       length, channel, cycle);
    #ifdef DEBUG
    if (length > m_max_packet_size) {
        debugWarning("(%p, %s) packet too large: len=%u max=%u\n",
                     this, getTypeString(), length, m_max_packet_size);
    }
    #endif
    if(m_Client) {
        return m_Client->putPacket(data, length, channel, tag, sy, cycle, dropped, skipped);
    }

    return RAW1394_ISO_OK;
}


enum raw1394_iso_disposition
IsoHandler::getPacket(unsigned char *data, unsigned int *length,
                      unsigned char *tag, unsigned char *sy,
                      int cycle, unsigned int dropped, unsigned int skipped) {

    debugOutputExtreme(DEBUG_LEVEL_ULTRA_VERBOSE,
                       "sending packet: length=%d, cycle=%d\n",
                       *length, cycle);
    if(m_Client) {
        enum raw1394_iso_disposition retval;
        retval = m_Client->getPacket(data, length, tag, sy, cycle, dropped, skipped, m_max_packet_size);
        #ifdef DEBUG
        if (*length > m_max_packet_size) {
            debugWarning("(%p, %s) packet too large: len=%u max=%u\n",
                         this, getTypeString(), *length, m_max_packet_size);
        }
        #endif
        return retval;
    }
    *tag = 0;
    *sy = 0;
    *length = 0;
    return RAW1394_ISO_OK;
}

bool IsoHandler::prepare()
{
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

    debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing iso handler (%p, client=%p)\n", this, m_Client);
    dumpInfo();
    if (getType() == eHT_Receive) {
        if(m_irq_interval > 1) {
            if(raw1394_iso_recv_init(m_handle,
                                    iso_receive_handler,
                                    m_buf_packets,
                                    m_max_packet_size,
                                    m_Client->getChannel(),
//                                     RAW1394_DMA_BUFFERFILL,
                                    RAW1394_DMA_PACKET_PER_BUFFER,
                                    m_irq_interval)) {
                debugFatal("Could not do receive initialisation (DMA_BUFFERFILL)!\n" );
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
                debugFatal("Could not do receive initialisation (PACKET_PER_BUFFER)!\n" );
                debugFatal("  %s\n",strerror(errno));
                return false;
            }
        }
        return true;
    } else {
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
}

bool IsoHandler::enable(int cycle)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "start on cycle %d\n", cycle);
    // check the state
    if(m_State != E_Prepared) {
        if(!prepare()) {
            debugFatal("Could not prepare handler\n");
            return false;
        }
    }

    if (getType() == eHT_Receive) {
        if(raw1394_iso_recv_start(m_handle, cycle, -1, 0)) {
            debugFatal("Could not start receive handler (%s)\n",strerror(errno));
            dumpInfo();
            return false;
        }
    } else {
        if(raw1394_iso_xmit_start(m_handle, cycle, m_prebuffers)) {
            debugFatal("Could not start xmit handler (%s)\n",strerror(errno));
            dumpInfo();
            return false;
        }
    }

    m_State = E_Running;
    return true;
}

/**
 * @brief convert a EHandlerType to a string
 * @param t the type
 * @return a char * describing the state
 */
const char *
IsoHandler::eHTToString(enum EHandlerType t) {
    switch (t) {
        case eHT_Receive: return "Receive";
        case eHT_Transmit: return "Transmit";
        default: return "error: unknown type";
    }
}
