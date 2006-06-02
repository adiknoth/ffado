/* freebob.h
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
#include "libfreebob/xmlparser.h"

#include "debugmodule/debugmodule.h"
#include "fbtypes.h"
#include "devicemanager.h"
#include "iavdevice.h"

#include "libfreebobavc/avc_generic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

DECLARE_GLOBAL_DEBUG_MODULE;
IMPL_GLOBAL_DEBUG_MODULE( FreeBoB, DEBUG_LEVEL_VERBOSE );

const char*
freebob_get_version() {
    return PACKAGE_STRING;
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
    return freebob_handle->m_deviceManager->discover(verbose)? 0 : -1;
}

freebob_connection_info_t*
freebob_get_connection_info( freebob_handle_t freebob_handle,
                             int node_id,
                             enum freebob_direction direction )
{
    xmlDocPtr doc;
    doc = freebob_handle->m_deviceManager->getXmlDescription();
    if ( !doc ) {
        debugFatal( "Could not get XML description\n" );
        return 0;
    }

    return freebob_xmlparse_get_connection_info( doc, node_id, direction );
}

freebob_supported_stream_format_info_t*
freebob_get_supported_stream_format_info( freebob_handle_t freebob_handle,
                                          int node_id,
                                          enum freebob_direction direction )
{
    xmlDocPtr doc;
    doc = freebob_handle->m_deviceManager->getXmlDescription();
    if ( !doc ) {
        debugFatal( "Could not get XML description\n" );
        return 0;
    }

    return freebob_xmlparse_get_stream_formats( doc, node_id, direction );
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
            return freebob_handle->m_deviceManager->discover(0)? 1 : 0;
        } else {
	    return -1;
	}
    }
    return -1;
}

void
freebob_free_connection_info( freebob_connection_info_t* connection_info )
{
    if ( !connection_info ) {
	return;
    }

    for ( int i = 0; i < connection_info->nb_connections; ++i ) {
	freebob_free_connection_spec( connection_info->connections[i] );
    }

    free( connection_info->connections );
    free( connection_info );
}

void
freebob_free_connection_spec( freebob_connection_spec_t* connection_spec )
{
    if ( !connection_spec ) {
	return;
    }

    freebob_free_stream_info( connection_spec->stream_info );
    free( connection_spec );
}

void freebob_free_stream_info( freebob_stream_info_t* stream_info )
{
    if ( !stream_info ) {
	return;
    }

    for ( int i = 0; i < stream_info->nb_streams; ++i ) {
	freebob_free_stream_spec( stream_info->streams[i] );
    }

    free(stream_info->streams);
    free(stream_info);
}

void freebob_free_stream_spec( freebob_stream_spec_t* stream_spec )
{
    if ( !stream_spec ) {
	return;
    }

    free( stream_spec );
}

void
freebob_free_supported_stream_format_info( freebob_supported_stream_format_info_t* stream_info )
{
    if ( !stream_info ) {
	return;
    }

    for ( int i = 0; i < stream_info->nb_formats; ++i ) {
	freebob_free_supported_stream_format_spec( stream_info->formats[i] );
    }

    free(stream_info->formats);
    free(stream_info);
}

void
freebob_free_supported_stream_format_spec( freebob_supported_stream_format_spec_t* stream_spec )
{
    if ( !stream_spec ) {
	return;
    }

    free( stream_spec );
}

void
freebob_print_connection_info( freebob_connection_info_t* connection_info )
{
    if ( !connection_info ) {
	fprintf( stderr, "connection_info==NULL\n" );
	return;
    }

    printf( "Direction:              %d (%s)\n\n", connection_info->direction,
            connection_info->direction? "playback" : "capture" );

    puts( "Connection Info" );
    puts( "===============\n" );

    printf("Number of connections:  %d\n\n",
           connection_info->nb_connections );

    for ( int i = 0; i < connection_info->nb_connections; ++i) {
	freebob_connection_spec_t* connection_spec
	    = connection_info->connections[i];


	if ( connection_spec ) {
	    printf( "  Connection %2d\n", i );
	    printf( "  -------------\n" );
	    printf( "    [%2d] Id:         %d\n", i, connection_spec->id );
	    printf( "    [%2d] Port:       %d\n", i, connection_spec->port );
	    printf( "    [%2d] Node:       %d\n", i, connection_spec->node );
	    printf( "    [%2d] Plug:       %d\n", i, connection_spec->plug );
	    printf( "    [%2d] Dimension:  %d\n", i, connection_spec->dimension );
	    printf( "    [%2d] Samplerate: %d\n", i, connection_spec->samplerate );
            printf( "    [%2d] IsoChannel: %d\n", i, connection_spec->iso_channel );
            printf( "    [%2d] IsMaster:   %d\n", i, connection_spec->is_master );

	    if ( connection_info->connections[i]->stream_info ) {
		printf("    [%2d] Number of stream infos: %d\n\n",
		       i, connection_spec->stream_info->nb_streams );

		printf("    StreamId  Position Location Format Type DPort Name\n");
		printf("    --------------------------------------------------\n");

		for ( int j = 0;
		      j < connection_spec->stream_info->nb_streams;
		      ++j )
		{
		    freebob_stream_spec_t* stream_spec
			= connection_spec->stream_info->streams[j];

		    printf("    [%2d]:[%2d] "
			   "0x%02x     0x%02x     0x%02x   0x%02x 0x%02x  %s\n",
			   i, j,
			   stream_spec->position,
			   stream_spec->location,
			   stream_spec->format,
			   stream_spec->type,
			   stream_spec->destination_port,
			   stream_spec->name );
		}
	    }
	}
        printf( "\n" );
    }

    return;
}

void
freebob_print_supported_stream_format_info( freebob_supported_stream_format_info_t* stream_info )
{
    if ( !stream_info ) {
	fprintf( stderr, "stream_info==NULL\n" );
	return;
    }

    printf( "Direction:              %d (%s)\n\n", stream_info->direction,
            stream_info->direction? "playback" : "capture" );

    printf( "Samplerate AudioChannels MidiChannels\n" );
    printf( "-------------------------------------\n" );
    for ( int i = 0; i < stream_info->nb_formats; ++i) {
	freebob_supported_stream_format_spec_t* format_spec
	    = stream_info->formats[i];

	if ( format_spec ) {
            printf( "%05d      %02d            %02d\n",
                    format_spec->samplerate,
                    format_spec->nb_audio_channels,
                    format_spec->nb_midi_channels );
        }
    }

    return;
}

/* debug function */
void
freebob_print_xml_description( freebob_handle_t freebob_handle,
                             int node_id,
                             enum freebob_direction direction )
{
    xmlDocPtr doc;
    doc = freebob_handle->m_deviceManager->getXmlDescription();
    if ( !doc ) {
        debugFatal( "Could not get XML description\n" );
        return;
    }

    xmlChar* xmlbuff;
    int buffersize;
    xmlDocDumpFormatMemory( doc, &xmlbuff, &buffersize, 1 );

	printf("%s\n",(char *)xmlbuff);

	xmlFree(xmlbuff);
	xmlFree(doc);
    return;
}

void freebob_sleep_after_avc_command( int time )
{
    AVCCommand::setSleepAfterAVCCommand( time );
}
