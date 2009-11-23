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



#include "IsoHandler.h"
#include "ieee1394service.h" 
#include "IsoHandlerManager.h"

#include "cycletimer.h"

#include "libstreaming/generic/StreamProcessor.h"
#include "libutil/PosixThread.h"

#include <errno.h>
#include "libutil/ByteSwap.h"
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
                        unsigned int dropped) {

    IsoHandler *recvHandler = static_cast<IsoHandler *>(raw1394_get_userdata(handle));
    assert(recvHandler);

    return recvHandler->putPacket(data, length, channel, tag, sy, cycle, dropped);
}

IsoHandler::IsoHandler(IsoHandlerManager& manager, enum EHandlerType t)
   : m_manager( manager )
   , m_type ( t )
   , m_handle( NULL )
   , m_buf_packets( 400 )
   , m_max_packet_size( 1024 )
   , m_irq_interval( -1 )
   , m_last_cycle( -1 )
   , m_last_now( 0xFFFFFFFF )
   , m_last_packet_handled_at( 0xFFFFFFFF )
   , m_receive_mode ( RAW1394_DMA_PACKET_PER_BUFFER )
   , m_Client( 0 )
   , m_speed( RAW1394_ISO_SPEED_400 )
   , m_prebuffers( 0 )
   , m_State( eHS_Stopped )
   , m_NextState( eHS_Stopped )
   , m_switch_on_cycle(0)
#ifdef DEBUG
   , m_packets ( 0 )
   , m_dropped( 0 )
   , m_skipped( 0 )
   , m_min_ahead( 7999 )
#endif
{
}

IsoHandler::IsoHandler(IsoHandlerManager& manager, enum EHandlerType t, 
                       unsigned int buf_packets, unsigned int max_packet_size, int irq)
   : m_manager( manager )
   , m_type ( t )
   , m_handle( NULL )
   , m_buf_packets( buf_packets )
   , m_max_packet_size( max_packet_size )
   , m_irq_interval( irq )
   , m_last_cycle( -1 )
   , m_last_now( 0xFFFFFFFF )
   , m_last_packet_handled_at( 0xFFFFFFFF )
   , m_receive_mode ( RAW1394_DMA_PACKET_PER_BUFFER )
   , m_Client( 0 )
   , m_speed( RAW1394_ISO_SPEED_400 )
   , m_prebuffers( 0 )
   , m_State( eHS_Stopped )
   , m_NextState( eHS_Stopped )
   , m_switch_on_cycle(0)
#ifdef DEBUG
   , m_packets ( 0 )
   , m_dropped( 0 )
   , m_skipped( 0 )
   , m_min_ahead( 7999 )
#endif
{
}

IsoHandler::IsoHandler(IsoHandlerManager& manager, enum EHandlerType t, unsigned int buf_packets,
                       unsigned int max_packet_size, int irq,
                       enum raw1394_iso_speed speed)
   : m_manager( manager )
   , m_type ( t )
   , m_handle( NULL )
   , m_buf_packets( buf_packets )
   , m_max_packet_size( max_packet_size )
   , m_irq_interval( irq )
   , m_last_cycle( -1 )
   , m_last_now( 0xFFFFFFFF )
   , m_last_packet_handled_at( 0xFFFFFFFF )
   , m_receive_mode ( RAW1394_DMA_PACKET_PER_BUFFER )
   , m_Client( 0 )
   , m_speed( speed )
   , m_prebuffers( 0 )
   , m_State( eHS_Stopped )
   , m_NextState( eHS_Stopped )
   , m_switch_on_cycle(0)
#ifdef DEBUG
   , m_packets( 0 )
   , m_dropped( 0 )
   , m_skipped( 0 )
   , m_min_ahead( 7999 )
#endif
{
}

IsoHandler::~IsoHandler() {
// Don't call until libraw1394's raw1394_new_handle() function has been
// fixed to correctly initialise the iso_packet_infos field.  Bug is
// confirmed present in libraw1394 1.2.1.  In any case,
// raw1394_destroy_handle() will do any iso system shutdown required.
//     raw1394_iso_shutdown(m_handle);
    if(m_handle) {
        if (m_State == eHS_Running) {
            debugError("BUG: Handler still running!\n");
            disable();
        }
    }
}

bool
IsoHandler::canIterateClient()
{
    debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE, "checking...\n");
    if(m_Client) {
        bool result;

        if (m_type == eHT_Receive) {
            result = m_Client->canProducePacket();
        } else {
            result = m_Client->canConsumePacket();
        }
        debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE, " returns %d\n", result);
        return result && (m_State != eHS_Error);
    } else {
        debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE, " no client\n");
    }
    return false;
}

bool
IsoHandler::iterate() {
    return iterate(m_manager.get1394Service().getCycleTimer());
}

bool
IsoHandler::iterate(uint32_t cycle_timer_now) {
    debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE, "(%p, %s) Iterating ISO handler at %08X...\n",
                       this, getTypeString(), cycle_timer_now);
    m_last_now = cycle_timer_now;
    if(m_State == eHS_Running) {
        assert(m_handle);

        #if ISOHANDLER_FLUSH_BEFORE_ITERATE
        // this flushes all packets received since the poll() returned
        // from kernel to userspace such that they are processed by this
        // iterate. Doing so might result in lower latency capability
        // and/or better reliability
        if(m_type == eHT_Receive) {
            raw1394_iso_recv_flush(m_handle);
        }
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

/**
 * Bus reset handler
 *
 * @return ?
 */

bool
IsoHandler::handleBusReset()
{
    debugOutput( DEBUG_LEVEL_NORMAL, "bus reset...\n");
    m_last_packet_handled_at = 0xFFFFFFFF;

    #define CSR_CYCLE_TIME            0x200
    #define CSR_REGISTER_BASE  0xfffff0000000ULL
    // do a simple read on ourself in order to update the internal structures
    // this avoids read failures after a bus reset
    quadlet_t buf=0;
    raw1394_read(m_handle, raw1394_get_local_id(m_handle),
                 CSR_REGISTER_BASE | CSR_CYCLE_TIME, 4, &buf);

    return m_Client->handleBusReset();
}

/**
 * Call this if you find out that this handler has died for some
 * external reason.
 */
void
IsoHandler::notifyOfDeath()
{
    m_State = eHS_Error;
    m_NextState = eHS_Error;

    // notify the client of the fact that we have died
    m_Client->handlerDied();

    // wake ourselves up
    raw1394_wake_up(m_handle);
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
        #ifdef DEBUG
        debugOutputShort( DEBUG_LEVEL_NORMAL, "  Min ISOXMT bufferfill : %04d\n", m_min_ahead);
        #endif
    }
    #ifdef DEBUG
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Last cycle, dropped.........: %4d, %4u, %4u\n",
            m_last_cycle, m_dropped, m_skipped);
    #endif

}

void IsoHandler::setVerboseLevel(int l)
{
    setDebugLevel(l);
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting verbose level to %d...\n", l );
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

// ISO packet interface
enum raw1394_iso_disposition IsoHandler::putPacket(
                    unsigned char *data, unsigned int length,
                    unsigned char channel, unsigned char tag, unsigned char sy,
                    unsigned int cycle, unsigned int dropped) {
    // keep track of dropped cycles
    int dropped_cycles = 0;
    if (m_last_cycle != (int)cycle && m_last_cycle != -1) {
        dropped_cycles = diffCycles(cycle, m_last_cycle) - 1;
        #ifdef DEBUG
        if (dropped_cycles < 0) {
            debugWarning("(%p) dropped < 1 (%d), cycle: %d, last_cycle: %d, dropped: %d\n", 
                         this, dropped_cycles, cycle, m_last_cycle, dropped);
        }
        if (dropped_cycles > 0) {
            debugOutput(DEBUG_LEVEL_VERBOSE,
                        "(%p) dropped %d packets on cycle %u, 'dropped'=%u, cycle=%d, m_last_cycle=%d\n",
                        this, dropped_cycles, cycle, dropped, cycle, m_last_cycle);
            m_dropped += dropped_cycles;
        }
        #endif
    }
    m_last_cycle = cycle;

    // the m_last_now value is set when the iterate() function is called.
    uint32_t now_cycles = CYCLE_TIMER_GET_CYCLES(m_last_now);

    // two cases can occur:
    // (1) this packet has been received before iterate() was called (normal case).
    // (2) this packet has been received after iterate() was called.
    //     happens when the kernel flushes more packets while we are already processing.
    //
    // In case (1) now_cycles is a small number of cycles larger than cycle. In
    // case (2) now_cycles is a small number of cycles smaller than cycle.
    // hence  abs(diffCycles(now_cycles, cycles)) has to be 'small'

    // we can calculate the time of arrival for this packet as
    // 'now' + diffCycles(cycles, now_cycles) * TICKS_PER_CYCLE
    // in its properly wrapped version
    int64_t diff_cycles = diffCycles(cycle, now_cycles);
    int64_t tmp = CYCLE_TIMER_TO_TICKS(m_last_now);
    tmp += diff_cycles * (int64_t)TICKS_PER_CYCLE;
    uint64_t pkt_ctr_ticks = wrapAtMinMaxTicks(tmp);
    uint32_t pkt_ctr = TICKS_TO_CYCLE_TIMER(pkt_ctr_ticks);
    #ifdef DEBUG
    if( (now_cycles < cycle)
        && diffCycles(now_cycles, cycle) < 0
        // ignore this on dropped cycles, since it's normal
        // that now is ahead on the received packets (as we miss packets)
        && dropped_cycles == 0) 
    {
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "Special non-unwrapping happened\n");
    }
    #endif

    #if ISOHANDLER_CHECK_CTR_RECONSTRUCTION
    // add a seconds field
    uint32_t now = m_manager.get1394Service().getCycleTimer();
    uint32_t now_secs_ref = CYCLE_TIMER_GET_SECS(now);
    // causality results in the fact that 'now' is always after 'cycle'
    // or at best, equal (if this handler was called within 125us after
    // the packet was on the wire).
    if(CYCLE_TIMER_GET_CYCLES(now) < cycle) {
        // the cycle field has wrapped, substract one second
        if(now_secs_ref == 0) {
            now_secs_ref = 127;
        } else  {
            now_secs_ref -= 1;
        }
    }
    uint32_t pkt_ctr_ref = cycle << 12;
    pkt_ctr_ref |= (now_secs_ref & 0x7F) << 25;

    if((pkt_ctr & ~0x0FFFL) != pkt_ctr_ref) {
        debugWarning("reconstructed CTR counter discrepancy\n");
        debugWarning(" ingredients: %X, %lX, %lX, %lX, %lX, %ld, %ld, %ld, %lld\n",
                       cycle, pkt_ctr_ref, pkt_ctr, now, m_last_now, now_secs_ref, CYCLE_TIMER_GET_SECS(now), CYCLE_TIMER_GET_SECS(m_last_now), tmp);
        debugWarning(" diffcy = %ld \n", diff_cycles);
    }
    #endif
    m_last_packet_handled_at = pkt_ctr;

    // leave the offset field (for now?)

    debugOutputExtreme(DEBUG_LEVEL_ULTRA_VERBOSE,
                "received packet: length=%d, channel=%d, cycle=%d, at %08X\n",
                length, channel, cycle, pkt_ctr);
    m_packets++;
    #ifdef DEBUG
    if (length > m_max_packet_size) {
        debugWarning("(%p, %s) packet too large: len=%u max=%u\n",
                     this, getTypeString(), length, m_max_packet_size);
    }
    if(m_last_cycle == -1) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Handler for %s SP %p is alive (cycle = %u)\n", getTypeString(), this, cycle);
    }
    #endif

    // iterate the client if required
    if(m_Client)
        return m_Client->putPacket(data, length, channel, tag, sy, pkt_ctr, dropped_cycles);

    return RAW1394_ISO_OK;
}

enum raw1394_iso_disposition
IsoHandler::getPacket(unsigned char *data, unsigned int *length,
                      unsigned char *tag, unsigned char *sy,
                      int cycle, unsigned int dropped, unsigned int skipped) {

    uint32_t pkt_ctr;
    if (cycle < 0) {
        // mark invalid
        pkt_ctr = 0xFFFFFFFF;
    } else {
        // the m_last_now value is set when the iterate() function is called.
        uint32_t now_cycles = CYCLE_TIMER_GET_CYCLES(m_last_now);

        // two cases can occur:
        // (1) this packet has been received before iterate() was called (normal case).
        // (2) this packet has been received after iterate() was called.
        //     happens when the kernel flushes more packets while we are already processing.
        //
        // In case (1) now_cycles is a small number of cycles larger than cycle. In
        // case (2) now_cycles is a small number of cycles smaller than cycle.
        // hence  abs(diffCycles(now_cycles, cycles)) has to be 'small'

        // we can calculate the time of arrival for this packet as
        // 'now' + diffCycles(cycles, now_cycles) * TICKS_PER_CYCLE
        // in its properly wrapped version
        int64_t diff_cycles = diffCycles(cycle, now_cycles);
        int64_t tmp = CYCLE_TIMER_TO_TICKS(m_last_now);
        tmp += diff_cycles * (int64_t)TICKS_PER_CYCLE;
        uint64_t pkt_ctr_ticks = wrapAtMinMaxTicks(tmp);
        pkt_ctr = TICKS_TO_CYCLE_TIMER(pkt_ctr_ticks);

        #if ISOHANDLER_CHECK_CTR_RECONSTRUCTION
        // add a seconds field
        uint32_t now = m_manager.get1394Service().getCycleTimer();
        uint32_t now_secs_ref = CYCLE_TIMER_GET_SECS(now);
        // causality results in the fact that 'now' is always after 'cycle'
        if(CYCLE_TIMER_GET_CYCLES(now) > (unsigned int)cycle) {
            // the cycle field has wrapped, add one second
            now_secs_ref += 1;
            // no need for this:
            if(now_secs_ref == 128) {
               now_secs_ref = 0;
            }
        }
        uint32_t pkt_ctr_ref = cycle << 12;
        pkt_ctr_ref |= (now_secs_ref & 0x7F) << 25;

        if(((pkt_ctr & ~0x0FFFL) != pkt_ctr_ref) && (m_packets > m_buf_packets)) {
            debugWarning("reconstructed CTR counter discrepancy\n");
            debugWarning(" ingredients: %X, %lX, %lX, %lX, %lX, %ld, %ld, %ld, %lld\n",
                        cycle, pkt_ctr_ref, pkt_ctr, now, m_last_now, now_secs_ref, CYCLE_TIMER_GET_SECS(now), CYCLE_TIMER_GET_SECS(m_last_now), tmp);
            debugWarning(" diffcy = %ld \n", diff_cycles);
        }
        #endif
    }
    if (m_packets < m_buf_packets) { // these are still prebuffer packets
        m_last_packet_handled_at = 0xFFFFFFFF;
    } else {
        m_last_packet_handled_at = pkt_ctr;
    }
    debugOutputExtreme(DEBUG_LEVEL_ULTRA_VERBOSE,
                "sending packet: length=%d, cycle=%d, at %08X\n",
                *length, cycle, pkt_ctr);

    m_packets++;

    #ifdef DEBUG
    if(m_last_cycle == -1) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Handler for %s SP %p is alive (cycle = %d)\n", getTypeString(), this, cycle);
    }
    #endif

    // keep track of dropped cycles
    int dropped_cycles = 0;
    if (m_last_cycle != cycle && m_last_cycle != -1) {
        dropped_cycles = diffCycles(cycle, m_last_cycle) - 1;
        // correct for skipped packets
        // since those are not dropped, but only delayed
        dropped_cycles -= skipped;

        #ifdef DEBUG
        if(skipped) {
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
                        "(%p) skipped %d cycles, cycle: %d, last_cycle: %d, dropped: %d\n", 
                        this, skipped, cycle, m_last_cycle, dropped);
            m_skipped += skipped;
        }
        if (dropped_cycles < 0) { 
            debugWarning("(%p) dropped < 1 (%d), cycle: %d, last_cycle: %d, dropped: %d, skipped: %d\n", 
                         this, dropped_cycles, cycle, m_last_cycle, dropped, skipped);
        }
        if (dropped_cycles > 0) {
            debugOutput(DEBUG_LEVEL_VERBOSE,
                        "(%p) dropped %d packets on cycle %u (last_cycle=%u, dropped=%d, skipped: %d)\n",
                        this, dropped_cycles, cycle, m_last_cycle, dropped, skipped);
            m_dropped += dropped_cycles - skipped;
        }
        #endif
    }
    if (cycle >= 0) {
        m_last_cycle = cycle;
        
        #ifdef DEBUG
/*        int ahead = diffCycles(cycle, now_cycles);
        if (ahead < m_min_ahead) m_min_ahead = ahead;
*/
        #endif
    }

    #ifdef DEBUG
    if (dropped > 0) {
        debugOutput(DEBUG_LEVEL_VERBOSE,
                    "(%p) OHCI issue on cycle %u (dropped_cycles=%d, last_cycle=%u, dropped=%d, skipped: %d)\n",
                    this, cycle, dropped_cycles, m_last_cycle, dropped, skipped);
    }
    #endif

    if(m_Client) {
        enum raw1394_iso_disposition retval;
        retval = m_Client->getPacket(data, length, tag, sy, pkt_ctr, dropped_cycles, skipped, m_max_packet_size);
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

bool
IsoHandler::enable(int cycle)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "start on cycle %d\n", cycle);

    // check the state
    if(m_State != eHS_Stopped) {
        debugError("Incorrect state, expected eHS_Stopped, got %d\n",(int)m_State);
        return false;
    }

    assert(m_handle == NULL);

    // create a handle for the ISO traffic
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

    // prepare the handler, allocate the resources
    debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing iso handler (%p, client=%p)\n", this, m_Client);
    dumpInfo();
    if (getType() == eHT_Receive) {
        if(raw1394_iso_recv_init(m_handle,
                                iso_receive_handler,
                                m_buf_packets,
                                m_max_packet_size,
                                m_Client->getChannel(),
                                m_receive_mode,
                                m_irq_interval)) {
            debugFatal("Could not do receive initialization (PACKET_PER_BUFFER)!\n" );
            debugFatal("  %s\n",strerror(errno));
            return false;
        }

        if(raw1394_iso_recv_start(m_handle, cycle, -1, 0)) {
            debugFatal("Could not start receive handler (%s)\n",strerror(errno));
            dumpInfo();
            return false;
        }
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

        if(raw1394_iso_xmit_start(m_handle, cycle, m_prebuffers)) {
            debugFatal("Could not start xmit handler (%s)\n",strerror(errno));
            dumpInfo();
            return false;
        }
    }

#ifdef DEBUG
    m_min_ahead = 7999;
#endif

    m_packets = 0;

    // indicate that the first iterate() still has to occur.
    m_last_now = 0xFFFFFFFF;
    m_last_packet_handled_at = 0xFFFFFFFF;

    m_State = eHS_Running;
    m_NextState = eHS_Running;
    return true;
}

bool
IsoHandler::disable()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "(%p, %s) enter...\n", 
                 this, (m_type==eHT_Receive?"Receive":"Transmit"));

    // check state
    if(m_State != eHS_Running) {
        debugError("Incorrect state, expected eHS_Running, got %d\n",(int)m_State);
        return false;
    }

    assert(m_handle != NULL);

    debugOutput( DEBUG_LEVEL_VERBOSE, "(%p, %s) wake up handle...\n", 
                 this, (m_type==eHT_Receive?"Receive":"Transmit"));

    // wake up any waiting reads/polls
    raw1394_wake_up(m_handle);

    // this is put here to try and avoid the
    // Runaway context problem
    // don't know if it will help though.
/*    if(m_State != eHS_Error) { // if the handler is dead, this might block forever
        raw1394_iso_xmit_sync(m_handle);
    }*/
    debugOutput( DEBUG_LEVEL_VERBOSE, "(%p, %s) stop...\n", 
                 this, (m_type==eHT_Receive?"Receive":"Transmit"));
    // stop iso traffic
    raw1394_iso_stop(m_handle);
    // deallocate resources

    // Don't call until libraw1394's raw1394_new_handle() function has been
    // fixed to correctly initialise the iso_packet_infos field.  Bug is
    // confirmed present in libraw1394 1.2.1.
    raw1394_iso_shutdown(m_handle);

    raw1394_destroy_handle(m_handle);
    m_handle = NULL;

    m_State = eHS_Stopped;
    m_NextState = eHS_Stopped;
    return true;
}

// functions to request enable or disable at the next opportunity
bool
IsoHandler::requestEnable(int cycle)
{
    if (m_State == eHS_Running) {
        debugError("Enable requested on enabled stream\n");
        return false;
    }
    if (m_State != eHS_Stopped) {
        debugError("Enable requested on stream with state: %d\n", m_State);
        return false;
    }
    m_NextState = eHS_Running;
    return true;
}

bool
IsoHandler::requestDisable()
{
    if (m_State == eHS_Stopped) {
        debugError("Disable requested on disabled stream\n");
        return false;
    }
    if (m_State != eHS_Running) {
        debugError("Disable requested on stream with state=%d\n", m_State);
        return false;
    }
    m_NextState = eHS_Stopped;
    return true;
}

void
IsoHandler::updateState()
{
    // execute state changes requested
    if(m_State != m_NextState) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "(%p) handler needs state update from %d => %d\n", this, m_State, m_NextState);
        if(m_State == eHS_Stopped && m_NextState == eHS_Running) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "handler has to be enabled\n");
            enable(m_switch_on_cycle);
        } else if(m_State == eHS_Running && m_NextState == eHS_Stopped) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "handler has to be disabled\n");
            disable();
        } else {
            debugError("Unknown state transition\n");
        }
    }
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
