/*
 * Copyright (C) 2005-2007 by Daniel Wagner
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
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

#include "config.h"

#include "../libffado/ffado.h"

#include "debugmodule/debugmodule.h"
#include "fbtypes.h"
#include "devicemanager.h"
#include "ffadodevice.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

DECLARE_GLOBAL_DEBUG_MODULE;
IMPL_GLOBAL_DEBUG_MODULE( FFADO, DEBUG_LEVEL_VERBOSE );

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
ffado_get_version() {
    return PACKAGE_STRING;
}


int
ffado_get_api_version() {
    return FFADO_API_VERSION;
}

ffado_handle_t
ffado_new_handle( int port )
{
    ffado_handle_t handle = new struct ffado_handle;
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
ffado_destroy_handle( ffado_handle_t ffado_handle )
{
    delete ffado_handle->m_deviceManager;
    delete ffado_handle;
    return 0;
}

int
ffado_discover_devices( ffado_handle_t ffado_handle, int verbose )
{
    if (verbose) {
        ffado_handle->m_deviceManager->setVerboseLevel(DEBUG_LEVEL_VERBOSE);
   }
    return ffado_handle->m_deviceManager->discover()? 0 : -1;
}

int
ffado_node_is_valid_ffado_device( ffado_handle_t ffado_handle, int node_id )
{
    return ffado_handle->m_deviceManager->isValidNode( node_id );
}

int
ffado_get_nb_devices_on_bus( ffado_handle_t ffado_handle )
{
    return ffado_handle->m_deviceManager->getNbDevices();
}

int
ffado_get_device_node_id( ffado_handle_t ffado_handle, int device_nr )
{
    return ffado_handle->m_deviceManager->getDeviceNodeId(device_nr);
}

int
ffado_set_samplerate( ffado_handle_t ffado_handle, int node_id, int samplerate )
{
    FFADODevice* avDevice = ffado_handle->m_deviceManager->getAvDevice( node_id );
    if ( avDevice ) {
        if ( avDevice->setSamplingFrequency( samplerate ) ) {
            return ffado_handle->m_deviceManager->discover()? 0 : -1;
        }
    }
    return -1;
}

#warning this should be cleaned up
#include "libavc/general/avc_generic.h"
void ffado_sleep_after_avc_command( int time )
{
    AVC::AVCCommand::setSleepAfterAVCCommand( time );
}
