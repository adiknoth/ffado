/* ieee1394service.cpp
 * Copyright (C) 2004 by Daniel Wagner
 *
 * This file is part of FreeBob.
 *
 * FreeBob is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBob is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBob; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */

#include <errno.h>
#include "ieee1394service.h"
#include "debugmodule.h"

Ieee1394Service* Ieee1394Service::m_pInstance = 0;

Ieee1394Service::Ieee1394Service()
    : m_iPort( 0 )
    , m_bInitialised( false )
    , m_bRHThreadRunning( false )
{
    pthread_mutex_init( &m_mutex, NULL );
}

Ieee1394Service::~Ieee1394Service()
{
    stopRHThread();
    if ( !m_handle ) {
        raw1394_destroy_handle( m_handle );
        m_handle = 0;
    }
}

FBReturnCodes
Ieee1394Service::initialize()
{
    if ( !m_bInitialised ) {
        m_handle = raw1394_new_handle();
        if ( !m_handle ) {
            if ( !errno ) {
                fprintf( stderr,  "libraw1394 not compatible.\n" );
            } else {
                perror ("Could not get 1394 handle");
                fprintf (stderr, "Is ieee1394 and raw1394 driver loaded?\n");
            }
            return eFBRC_Creating1394HandleFailed;
        }

        // Store this instance in the user data pointer, in order
        // to be able to retrieve the instance in the pure C bus reset
        // call back function.
        raw1394_set_userdata( m_handle,  this );

        if ( raw1394_set_port( m_handle,  m_iPort ) < 0 ) {
            perror( "Could not set port" );
            return eFBRC_Setting1394PortFailed;
        }

        raw1394_set_bus_reset_handler( m_handle,  this->resetHandler );

        startRHThread();

        discoveryDevices();
        m_bInitialised = true;
    }
    return eFBRC_Success;
}

Ieee1394Service*
Ieee1394Service::instance()
{
    if ( !m_pInstance ) {
        m_pInstance = new Ieee1394Service;
    }
    return m_pInstance;
}

FBReturnCodes
Ieee1394Service::discoveryDevices()
{
    return eFBRC_Success;
}

int
Ieee1394Service::resetHandler( raw1394handle_t handle,
                               unsigned int iGeneration )
{
    debugPrint( DEBUG_LEVEL_INFO,
                "Bus reset has occurred (generation = %d).\n", iGeneration );
    raw1394_update_generation (handle, iGeneration);
    Ieee1394Service* pInstance
        = (Ieee1394Service*) raw1394_get_userdata (handle);
    pInstance->sigGenerationCount( iGeneration );
    return 0;
}

bool
Ieee1394Service::startRHThread()
{
    if ( m_bRHThreadRunning ) {
        return true;
    }
    debugPrint( DEBUG_LEVEL_INFO,
                "Starting bus reset handler thread.\n" );
    pthread_mutex_lock( &m_mutex );
    pthread_create( &m_thread, NULL, rHThread, this );
    pthread_mutex_unlock( &m_mutex );
    m_bRHThreadRunning = true;
    return true;
}

void
Ieee1394Service::stopRHThread()
{
    if ( m_bRHThreadRunning ) {
        debugPrint( DEBUG_LEVEL_INFO,
                    "Stopping bus reset handler thread.\n" );
        pthread_mutex_lock (&m_mutex);
        pthread_cancel (m_thread);
        pthread_join (m_thread, NULL);
        pthread_mutex_unlock (&m_mutex);
    }
    m_bRHThreadRunning = false;
}

void*
Ieee1394Service::rHThread( void* arg )
{
    Ieee1394Service* pIeee1394Service = (Ieee1394Service*) arg;

    while (true) {
        raw1394_loop_iterate (pIeee1394Service->m_handle);
        pthread_testcancel ();
    }

    return NULL;
}
