// freebobctl.c
//
/****************************************************************************
   libfreebobctl - Freebob Control API
   
   Copyright (C) 2005, Pieter Palmers. All rights reserved.
   <pieterpalmers@users.sourceforge.net>   
   
   Freebob = FireWire Audio for Linux... 
   http://freebob.sf.net
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "freebobctl/version.h"
#include "freebobctl/freebobctl.h"
#include "freebobctl/xmlparser.h"

const char * freebobctl_get_version() {
	return FREEBOBCTL_VERSION" (Build "__DATE__" - "__TIME__")";
}


freebob_connection_info_t *freebobctl_get_connection_info(int direction) {


	return NULL;
}

void freebobctl_free_connection_info(freebob_connection_info_t *connection_info) {
	int i;
	
	if (!connection_info) {
		return;
	}
	
	for (i=0;i<connection_info->nb_connections;i++) {
		freebobctl_free_connection_spec(connection_info->connections[i]);
	}
	
	free(connection_info->connections);
	free(connection_info);
}

void freebobctl_free_connection_spec(freebob_connection_spec_t *connection_spec) {
	
	if (!connection_spec) {
		return;
	}
	
	freebobctl_free_stream_info(connection_spec->stream_info);
	free(connection_spec);
}	


void freebobctl_free_stream_info(freebob_stream_info_t *stream_info) {
	int i;
	if (!stream_info) {
		return;
	}
	
	for (i=0;i<stream_info->nb_streams;i++) {
		freebobctl_free_stream_spec(stream_info->streams[i]);
	}
	free(stream_info->streams);
	free(stream_info);
}

void freebobctl_free_stream_spec(freebob_stream_spec_t *stream_spec) {
	
	if (!stream_spec) {
		return;
	}
	
	free(stream_spec);
}	

void
freebobctl_print_connection_info(freebob_connection_info_t *connection_info) {
	int i,j;
	freebob_connection_spec_t * connection_spec;
	freebob_stream_spec_t *stream_spec;
	
	if (!connection_info) {
		fprintf(stderr,"connection_info==NULL\n");
		return;
	}
	
	printf("connection_info->direction=%d\n",connection_info->direction);
	
	for (i=0;i<connection_info->nb_connections;i++) {
		connection_spec=connection_info->connections[i];
		if(connection_spec) {
			printf("connection_info->connections[%d]->id         = %d\n",i,connection_spec->id);
			printf("connection_info->connections[%d]->port       = %d\n",i,connection_spec->port);
			printf("connection_info->connections[%d]->node       = %d\n",i,connection_spec->node);
			printf("connection_info->connections[%d]->plug       = %d\n",i,connection_spec->plug);
			printf("connection_info->connections[%d]->dimension  = %d\n",i,connection_spec->dimension);
			printf("connection_info->connections[%d]->samplerate = %d\n",i,connection_spec->samplerate);
			if (connection_info->connections[i]->stream_info) {
				printf("connection_info->connections[%d]->stream_info->nb_streams = %d\n",i,connection_spec->stream_info->nb_streams);
			
				for (j=0;j<connection_spec->stream_info->nb_streams;j++) {
					stream_spec=connection_spec->stream_info->streams[j];
					if(stream_spec) {
						printf("connection_info->connections[%d]->stream_info->streams[%d] = 0x%02X / 0x%02X / 0x%02X / 0x%02X / %02d / [%s]\n",i,j,stream_spec->position, stream_spec->location,stream_spec->format,stream_spec->type,stream_spec->destination_port,stream_spec->name);
						
					}
				}
			}
		}
		
	}

	return;
}





// end of freebobctl.c
