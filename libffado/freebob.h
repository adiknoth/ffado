/* freebob.h
 * Copyright (C) 2005,07 Pieter Palmers
 * Copyright (C) 2006 Daniel Wagner
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

#ifndef FREEBOB_H
#define FREEBOB_H

#define FREEBOB_MAX_NAME_LEN 256

#define FREEBOB_BOUNCE_SERVER_VENDORNAME  "FreeBoB Server"
#define FREEBOB_BOUNCE_SERVER_MODELNAME   "freebob-server"

#define FREEBOB_BOUNCE_SERVER_GETXMLDESCRIPTION_CMD
#define AVC1394_SUBUNIT_TYPE_FREEBOB_BOUNCE_SERVER 	0x0D

#define FREEBOB_API_VERSION 2

enum freebob_direction {
    FREEBOB_CAPTURE  = 0,
    FREEBOB_PLAYBACK = 1,
};

typedef struct freebob_handle* freebob_handle_t;

#ifdef __cplusplus
extern "C" {
#endif

freebob_handle_t
freebob_new_handle( int port );

int
freebob_destroy_handle( freebob_handle_t freebob_handle );

int
freebob_discover_devices( freebob_handle_t freebob_handle, int verbose_level );

int freebob_node_is_valid_freebob_device(freebob_handle_t fb_handle, int node_id);
int freebob_get_nb_devices_on_bus(freebob_handle_t fb_handle);

int freebob_get_device_node_id(freebob_handle_t fb_handle, int device_nr);
int freebob_set_samplerate(freebob_handle_t freebob_handle, int node_id, int samplerate);

/* ABI stuff */
const char*
freebob_get_version();

int
freebob_get_api_version();

/* various function */

/* workaround: wait usec after each AVC command.
   will disapear as soon bug is fixed */    
void freebob_sleep_after_avc_command( int time );

#ifdef __cplusplus
}
#endif

#endif /* FREEBOB_H */
