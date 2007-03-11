/* freebob.cpp
 * Copyright (C) 2005 Pieter Palmers, Daniel Wagner
 *
 * This file is part of FreeBoB
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

#include "config.h"

#include "libfreebob/freebob.h"

#include "debugmodule/debugmodule.h"
#include "fbtypes.h"
#include "devicemanager.h"
#include "iavdevice.h"

#include "libavc/avc_generic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

DECLARE_GLOBAL_DEBUG_MODULE;
IMPL_GLOBAL_DEBUG_MODULE( FreeBoB, DEBUG_LEVEL_VERBOSE );

#ifdef __cplusplus
extern "C" {
#endif

// this is very much nescessary, as otherwise the 
// message buffer thread doesn't get killed when the 
// library is dlclose()'d 

static void exitfunc(void) __attribute__((destructor));

static void exitfunc(void)
{
    delete DebugModuleManager::instance();

}
#ifdef __cplusplus
}
#endif

const char*
freebob_get_version() {
    return PACKAGE_STRING;
}


int
freebob_get_api_version() {
    return FREEBOB_API_VERSION;
}

freebob_handle_t
freebob_new_handle( int port )
{
    freebob_handle_t handle = new struct freebob_handle;
    if (! handle ) {
        debugFatal( "Could not allocate memory for new handle\n" );
        return 0;
    }

    handle->m_deviceManager = new DeviceManager();
    if ( !handle->m_deviceManager ) {
        debugFatal( "Could not allocate device manager\n" );
        delete handle;
        return 0;
    }
    if ( !handle->m_deviceManager->initialize( port ) ) {
        debugFatal( "Could not initialize device manager\n" );
        delete handle->m_deviceManager;
        delete handle;
        return 0;
    }
    return handle;
}

int
freebob_destroy_handle( freebob_handle_t freebob_handle )
{
    delete freebob_handle->m_deviceManager;
    delete freebob_handle;
    return 0;
}

int
freebob_discover_devices( freebob_handle_t freebob_handle, int verbose )
{
    if (verbose) {
        freebob_handle->m_deviceManager->setVerboseLevel(DEBUG_LEVEL_VERBOSE);
   }
    return freebob_handle->m_deviceManager->discover()? 0 : -1;
}

int
freebob_node_is_valid_freebob_device( freebob_handle_t freebob_handle, int node_id )
{
    return freebob_handle->m_deviceManager->isValidNode( node_id );
}

int
freebob_get_nb_devices_on_bus( freebob_handle_t freebob_handle )
{
    return freebob_handle->m_deviceManager->getNbDevices();
}

int
freebob_get_device_node_id( freebob_handle_t freebob_handle, int device_nr )
{
    return freebob_handle->m_deviceManager->getDeviceNodeId(device_nr);
}

int
freebob_set_samplerate( freebob_handle_t freebob_handle, int node_id, int samplerate )
{
    IAvDevice* avDevice = freebob_handle->m_deviceManager->getAvDevice( node_id );
    if ( avDevice ) {
        if ( avDevice->setSamplingFrequency( parseSampleRate( samplerate ) ) ) {
            return freebob_handle->m_deviceManager->discover()? 0 : -1;
        }
    }
    return -1;
}

void freebob_sleep_after_avc_command( int time )
{
    AVCCommand::setSleepAfterAVCCommand( time );
}
