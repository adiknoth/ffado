/* ieee1394service.cpp
 * Copyright (C) 2005,07 by Daniel Wagner
 * Copyright (C) 2007 by Pieter Palmers
 *
 * This file is part of FreeBoB.
 *
 * FreeBoB is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBoB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBoB; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */
#include "ieee1394service.h"

#include <libavc1394/avc1394.h>
#include <libraw1394/csr.h>
#include <libiec61883/iec61883.h>

#include <errno.h>
#include <netinet/in.h>

#include "string.h"

#include <iostream>
#include <iomanip>

IMPL_DEBUG_MODULE( Ieee1394Service, Ieee1394Service, DEBUG_LEVEL_VERBOSE );

Ieee1394Service::Ieee1394Service()
    : m_handle( 0 ), m_resetHandle( 0 )
    , m_port( -1 )
    , m_threadRunning( false )
{
    pthread_mutex_init( &m_mutex, 0 );
    
    for (unsigned int i=0; i<64; i++) {
        m_channels[i].channel=-1;
        m_channels[i].bandwidth=-1;
        m_channels[i].alloctype=AllocFree;
        m_channels[i].xmit_node=0xFFFF;
        m_channels[i].xmit_plug=-1;
        m_channels[i].recv_node=0xFFFF;
        m_channels[i].recv_plug=-1;
    }
}

Ieee1394Service::~Ieee1394Service()
{
    stopRHThread();

    if ( m_handle ) {
        raw1394_destroy_handle( m_handle );
    }
    if ( m_resetHandle ) {
        raw1394_destroy_handle( m_resetHandle );
    }
}

bool
Ieee1394Service::initialize( int port )
{
    using namespace std;

    m_handle = raw1394_new_handle_on_port( port );
    if ( !m_handle ) {
        if ( !errno ) {
            debugFatal("libraw1394 not compatible\n");
        } else {
            debugFatal("Ieee1394Service::initialize: Could not get 1394 handle: %s",
                strerror(errno) );
            debugFatal("Is ieee1394 and raw1394 driver loaded?\n");
        }
        return false;
    }

    m_resetHandle = raw1394_new_handle_on_port( port );
    if ( !m_handle ) {
        if ( !errno ) {
            debugFatal("libraw1394 not compatible\n");
        } else {
            debugFatal("Ieee1394Service::initialize: Could not get 1394 handle: %s",
                strerror(errno) );
            debugFatal("Is ieee1394 and raw1394 driver loaded?\n");
        }
        return false;
    }

    m_port = port;

    raw1394_set_userdata( m_handle, this );
    raw1394_set_userdata( m_resetHandle, this );
    raw1394_set_bus_reset_handler( m_resetHandle,
                                   this->resetHandlerLowLevel );
    startRHThread();

    return true;
}

int
Ieee1394Service::getNodeCount()
{
    return raw1394_get_nodecount( m_handle );
}

nodeid_t Ieee1394Service::getLocalNodeId() {
    return raw1394_get_local_id(m_handle) & 0x3F;
}

bool
Ieee1394Service::read( fb_nodeid_t nodeId,
                       fb_nodeaddr_t addr,
                       size_t length,
                       fb_quadlet_t* buffer )
{
    using namespace std;
    if ( raw1394_read( m_handle, nodeId, addr, length*4, buffer ) == 0 ) {

        #ifdef DEBUG
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
            "read: node 0x%X, addr = 0x%016llX, length = %u\n",
            nodeId, addr, length);
        printBuffer( length, buffer );
        #endif

        return true;
    } else {
        #ifdef DEBUG
        debugError("raw1394_read failed: node 0x%X, addr = 0x%016llX, length = %u\n",
              nodeId, addr, length);
        #endif

        return false;
    }
}


bool
Ieee1394Service::read_quadlet( fb_nodeid_t nodeId,
                               fb_nodeaddr_t addr,
                               fb_quadlet_t* buffer )
{
    return read( nodeId,  addr, sizeof( *buffer )/4, buffer );
}

bool
Ieee1394Service::read_octlet( fb_nodeid_t nodeId,
                              fb_nodeaddr_t addr,
                              fb_octlet_t* buffer )
{
    return read( nodeId, addr, sizeof( *buffer )/4,
                 reinterpret_cast<fb_quadlet_t*>( buffer ) );
}


bool
Ieee1394Service::write( fb_nodeid_t nodeId,
                        fb_nodeaddr_t addr,
                        size_t length,
                        fb_quadlet_t* data )
{
    using namespace std;

    #ifdef DEBUG
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"write: node 0x%X, addr = 0x%016X, length = %d\n",
                nodeId, addr, length);
    printBuffer( length, data );
    #endif

    return raw1394_write( m_handle, nodeId, addr, length*4, data ) == 0;
}


bool
Ieee1394Service::write_quadlet( fb_nodeid_t nodeId,
                                fb_nodeaddr_t addr,
                                fb_quadlet_t data )
{
    return write( nodeId, addr, sizeof( data )/4, &data );
}

bool
Ieee1394Service::write_octlet( fb_nodeid_t nodeId,
                               fb_nodeaddr_t addr,
                               fb_octlet_t data )
{
    return write( nodeId, addr, sizeof( data )/4,
                  reinterpret_cast<fb_quadlet_t*>( &data ) );
}

fb_quadlet_t*
Ieee1394Service::transactionBlock( fb_nodeid_t nodeId,
                                   fb_quadlet_t* buf,
                                   int len,
                                   unsigned int* resp_len )
{
    for (int i = 0; i < len; ++i) {
        buf[i] = ntohl( buf[i] );
    }

    #ifdef DEBUG
    debugOutputShort(DEBUG_LEVEL_VERY_VERBOSE, "  pre avc1394_transaction_block2\n" );
    printBuffer( len, buf );
    #endif

    fb_quadlet_t* result =
        avc1394_transaction_block2( m_handle,
                                    nodeId,
                                    buf,
                                    len,
                                    resp_len,
                                    10 );

    #ifdef DEBUG
    debugOutputShort(DEBUG_LEVEL_VERY_VERBOSE, "  post avc1394_transaction_block2\n" );
    printBuffer( *resp_len, result );
    #endif

    for ( unsigned int i = 0; i < *resp_len; ++i ) {
        result[i] = htonl( result[i] );
    }

    return result;
}


bool
Ieee1394Service::transactionBlockClose()
{
    avc1394_transaction_block_close( m_handle );
    return true;
}

bool
Ieee1394Service::setVerbose( int verboseLevel )
{
    setDebugLevel(verboseLevel);
    return true;
}

int
Ieee1394Service::getVerboseLevel()
{
    return getDebugLevel();
}

void
Ieee1394Service::printBuffer( size_t length, fb_quadlet_t* buffer ) const
{

    for ( unsigned int i=0; i < length; ++i ) {
        if ( ( i % 4 ) == 0 ) {
            if ( i > 0 ) {
                debugOutputShort(DEBUG_LEVEL_VERY_VERBOSE,"\n");
            }
            debugOutputShort(DEBUG_LEVEL_VERY_VERBOSE," %4d: ",i*4);
        }
        debugOutputShort(DEBUG_LEVEL_VERY_VERBOSE,"%08X ",buffer[i]);
    }
    debugOutputShort(DEBUG_LEVEL_VERY_VERBOSE,"\n");
}

int
Ieee1394Service::resetHandlerLowLevel( raw1394handle_t handle,
                                       unsigned int generation )
{
    raw1394_update_generation ( handle, generation );
    Ieee1394Service* instance
        = (Ieee1394Service*) raw1394_get_userdata( handle );
    instance->resetHandler( generation );

    return 0;
}

bool
Ieee1394Service::resetHandler( unsigned int generation )
{
    m_generation = generation;

    for ( reset_handler_vec_t::iterator it = m_busResetHandlers.begin();
          it != m_busResetHandlers.end();
          ++it )
    {
        Functor* func = *it;
        ( *func )();
    }

    return true;
}

bool
Ieee1394Service::startRHThread()
{
    int i;

    if ( m_threadRunning ) {
        return true;
    }
    pthread_mutex_lock( &m_mutex );
    i = pthread_create( &m_thread, 0, rHThread, this );
    pthread_mutex_unlock( &m_mutex );
    if (i) {
        debugFatal("Could not start ieee1394 service thread\n");
        return false;
    }
    m_threadRunning = true;

    return true;
}

void
Ieee1394Service::stopRHThread()
{
    if ( m_threadRunning ) {
        pthread_mutex_lock (&m_mutex);
        pthread_cancel (m_thread);
        pthread_join (m_thread, 0);
        pthread_mutex_unlock (&m_mutex);
        m_threadRunning = false;
    }
}

void*
Ieee1394Service::rHThread( void* arg )
{
    Ieee1394Service* pIeee1394Service = (Ieee1394Service*) arg;

    while (true) {
        raw1394_loop_iterate (pIeee1394Service->m_resetHandle);
        pthread_testcancel ();
    }

    return 0;
}

bool
Ieee1394Service::addBusResetHandler( Functor* functor )
{
    m_busResetHandlers.push_back( functor );
    return true;
}

bool
Ieee1394Service::remBusResetHandler( Functor* functor )
{
    for ( reset_handler_vec_t::iterator it = m_busResetHandlers.begin();
          it != m_busResetHandlers.end();
          ++it )
    {
        if ( *it == functor ) {
            m_busResetHandlers.erase( it );
            return true;
        }
    }
    return false;
}

/**
 * Allocates an iso channel for use by the interface in a similar way to
 * libiec61883.  Returns -1 on error (due to there being no free channels)
 * or an allocated channel number.
 *
 * Does not perform anything other than registering the channel and the 
 * bandwidth at the IRM
 *
 * Also allocates the necessary bandwidth (in ISO allocation units).
 * 
 * FIXME: As in libiec61883, channel 63 is not requested; this is either a
 * bug or it's omitted since that's the channel preferred by video devices.
 *
 * @param bandwidth the bandwidth to allocate for this channel
 * @return the channel number
 */
signed int Ieee1394Service::allocateIsoChannelGeneric(unsigned int bandwidth) {
    debugOutput(DEBUG_LEVEL_VERBOSE, "Allocating ISO channel using generic method...\n" );
    
    struct ChannelInfo cinfo;

    int c = -1;
    for (c = 0; c < 63; c++) {
        if (raw1394_channel_modify (m_handle, c, RAW1394_MODIFY_ALLOC) == 0)
            break;
    }
    if (c < 63) {
        if (raw1394_bandwidth_modify(m_handle, bandwidth, RAW1394_MODIFY_ALLOC) < 0) {
            debugFatal("Could not allocate bandwidth of %d\n", bandwidth);
            
            raw1394_channel_modify (m_handle, c, RAW1394_MODIFY_FREE);
            return -1;
        } else {
            cinfo.channel=c;
            cinfo.bandwidth=bandwidth;
            cinfo.alloctype=AllocGeneric;
            
            if (registerIsoChannel(c, cinfo)) {
                return c;
            } else {
                raw1394_bandwidth_modify(m_handle, bandwidth, RAW1394_MODIFY_FREE);
                raw1394_channel_modify (m_handle, c, RAW1394_MODIFY_FREE);
                return -1;
            }
        }
    }
    return -1;
}

/**
 * Allocates an iso channel for use by the interface in a similar way to
 * libiec61883.  Returns -1 on error (due to there being no free channels)
 * or an allocated channel number.
 *
 * Uses IEC61883 Connection Management Procedure to establish the connection.
 *
 * Also allocates the necessary bandwidth (in ISO allocation units).
 *
 * @param xmit_node  node id of the transmitter
 * @param xmit_plug  the output plug to use. If -1, find the first online plug, and
 * upon return, contains the plug number used.
 * @param recv_node  node id of the receiver
 * @param recv_plug the input plug to use. If -1, find the first online plug, and
 * upon return, contains the plug number used.
 *
 * @return the channel number
 */

signed int Ieee1394Service::allocateIsoChannelCMP(
    nodeid_t xmit_node, int xmit_plug, 
    nodeid_t recv_node, int recv_plug
    ) {

    debugOutput(DEBUG_LEVEL_VERBOSE, "Allocating ISO channel using IEC61883 CMP...\n" );
    
    struct ChannelInfo cinfo;
    
    int c = -1;
    int bandwidth=1;
    
    // do connection management: make connection
    c = iec61883_cmp_connect(
        m_handle,
        xmit_node | 0xffc0,
        &xmit_plug,
        recv_node | 0xffc0,
        &recv_plug,
        &bandwidth);

    if((c<0) || (c>63)) {
        debugError("Could not do CMP from %04X:%02d to %04X:%02d\n",
            xmit_node, xmit_plug, recv_node, recv_plug
            );
        return -1;
    }

    cinfo.channel=c;
    cinfo.bandwidth=bandwidth;
    cinfo.alloctype=AllocCMP;
    
    cinfo.xmit_node=xmit_node;
    cinfo.xmit_plug=xmit_plug;
    cinfo.recv_node=recv_node;
    cinfo.recv_plug=recv_plug;
        
    if (registerIsoChannel(c, cinfo)) {
        return c;
    }

    return -1;
}

/**
 * Deallocates an iso channel.  Silently ignores a request to deallocate 
 * a negative channel number.
 *
 * Figures out the method that was used to allocate the channel (generic, cmp, ...)
 * and uses the appropriate method to deallocate. Also frees the bandwidth
 * that was reserved along with this channel.
 * 
 * @param c channel number
 * @return true if successful
 */
bool Ieee1394Service::freeIsoChannel(signed int c) {
    debugOutput(DEBUG_LEVEL_VERBOSE, "Freeing ISO channel %d...\n", c );
    
    if (c < 0 || c > 63) {
        debugWarning("Invalid channel number: %d\n", c);
        return false;
    }
    
    switch (m_channels[c].alloctype) {
        default:
            debugError(" BUG: invalid allocation type!\n");
            return false;
            
        case AllocFree: 
            debugWarning(" Channel %d not registered\n", c);
            return false;
            
        case AllocGeneric:
            debugOutput(DEBUG_LEVEL_VERBOSE, " allocated using generic routine...\n" );
            if (unregisterIsoChannel(c)) {
                return false;
            } else {
                debugOutput(DEBUG_LEVEL_VERBOSE, " freeing %d bandwidth units...\n", m_channels[c].bandwidth );
                if (raw1394_bandwidth_modify(m_handle, m_channels[c].bandwidth, RAW1394_MODIFY_FREE) !=0) {
                    debugWarning("Failed to deallocate bandwidth\n");
                }
                debugOutput(DEBUG_LEVEL_VERBOSE, " freeing channel %d...\n", m_channels[c].channel );
                if (raw1394_channel_modify (m_handle, m_channels[c].channel, RAW1394_MODIFY_FREE) != 0) {
                    debugWarning("Failed to free channel\n");
                }
                return true;
            }
            
        case AllocCMP:
            debugOutput(DEBUG_LEVEL_VERBOSE, " allocated using IEC61883 CMP...\n" );
            if (unregisterIsoChannel(c)) {
                return false;
            } else {
                debugOutput(DEBUG_LEVEL_VERBOSE, " performing IEC61883 CMP disconnect...\n" );
                if(iec61883_cmp_disconnect(
                        m_handle, 
                        m_channels[c].xmit_node | 0xffc0,
                        m_channels[c].xmit_plug,
                        m_channels[c].recv_node | 0xffc0,
                        m_channels[c].recv_plug,
                        m_channels[c].channel,
                        m_channels[c].bandwidth) != 0) {
                    debugWarning("Could not do CMP disconnect for channel %d!\n",c);
                }
            }
            return true;
    }
    
    // unreachable
    debugError("BUG: unreachable code reached!\n");
    
    return false;
}

/**
 * Registers a channel as managed by this ieee1394service
 * @param c channel number
 * @param cinfo channel info struct
 * @return true if successful
 */
bool Ieee1394Service::registerIsoChannel(unsigned int c, struct ChannelInfo cinfo) {
    if (c < 63) {
        if (m_channels[c].alloctype != AllocFree) {
            debugWarning("Channel %d already registered with bandwidth %d\n",
                m_channels[c].channel, m_channels[c].bandwidth);
        }
        
        memcpy(&m_channels[c], &cinfo, sizeof(struct ChannelInfo));
        
    } else return false;
    return true;
}

/**
 * unegisters a channel from this ieee1394service
 * @param c channel number
 * @return true if successful
 */
bool Ieee1394Service::unregisterIsoChannel(unsigned int c) {
    if (c < 63) {
        if (m_channels[c].alloctype == AllocFree) {
            debugWarning("Channel %d not registered\n", c);
            return false;
        }
        
        m_channels[c].channel=-1;
        m_channels[c].bandwidth=-1;
        m_channels[c].alloctype=AllocFree;
        m_channels[c].xmit_node=0xFFFF;
        m_channels[c].xmit_plug=-1;
        m_channels[c].recv_node=0xFFFF;
        m_channels[c].recv_plug=-1;
        
    } else return false;
    return true;
}

/**
 * Returns the current value of the `bandwidth available' register on
 * the IRM, or -1 on error.
 * @return 
 */
signed int Ieee1394Service::getAvailableBandwidth() {
    quadlet_t buffer;
    signed int result = raw1394_read (m_handle, raw1394_get_irm_id (m_handle),
        CSR_REGISTER_BASE + CSR_BANDWIDTH_AVAILABLE,
        sizeof (quadlet_t), &buffer);

    if (result < 0)
        return -1;
    return ntohl(buffer);
}
