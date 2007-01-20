/* ieee1394service.cpp
 * Copyright (C) 2005,07 by Daniel Wagner
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

#include <errno.h>
#include <netinet/in.h>

#include "string.h"

#include <iostream>
#include <iomanip>

IMPL_DEBUG_MODULE( Ieee1394Service, Ieee1394Service, DEBUG_LEVEL_NORMAL );

Ieee1394Service::Ieee1394Service()
    : m_handle( 0 )
    , m_port( -1 )
    , m_threadRunning( false )
{
    pthread_mutex_init( &m_mutex, 0 );
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
            "read: node 0x%X, addr = 0x%016X, length = %d\n",
            nodeId, addr, length);
        printBuffer( length, buffer );
        #endif

        return true;
    } else {
        #ifdef DEBUG
        debugError("raw1394_read failed: node 0x%X, addr = 0x%016X, length = %d\n",
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
