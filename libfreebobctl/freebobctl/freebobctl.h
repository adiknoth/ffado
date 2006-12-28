// freebobctl.h
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

#ifndef __FREEBOBCTL_FREEBOBCTL_H
#define __FREEBOBCTL_FREEBOBCTL_H



#define FREEBOBCTL_MAX_NAME_LEN 256
/*
 * Buffer specification
 */

typedef struct _freebob_stream_spec freebob_stream_spec_t;
typedef struct _freebob_stream_info freebob_stream_info_t;
typedef struct _freebob_connection_spec freebob_connection_spec_t;
typedef struct _freebob_connection_info freebob_connection_info_t;

/* 
 * Stream specification
 */
struct _freebob_stream_spec {
	int location;
	int position;
	int format;
	int type;
	int destination_port;
	char name[FREEBOBCTL_MAX_NAME_LEN];
};

struct _freebob_stream_info {
	int nb_streams;
	freebob_stream_spec_t **streams;
};
/* 
 * Connection specification
 */
struct _freebob_connection_spec {
	int id;
	int port;
	int node;
	int plug;
	
	int dimension; // due to the midi stuff, the dimension isn't equal to the number of streams
	
	int samplerate; // this should be equal for all connections when using jack. maybe not when using other api's
	
	freebob_stream_info_t *stream_info;
};

/* 
 * topology info
 */

struct _freebob_connection_info {
	int direction;
	int nb_connections;
	freebob_connection_spec_t **connections;
};


freebob_connection_info_t *freebobctl_get_connection_info_from_xml_file(char *xmlfile, int direction);
freebob_connection_info_t *freebobctl_get_connection_info_from_osc(char *osc_url, int direction);

void freebobctl_free_connection_info(freebob_connection_info_t *);
void freebobctl_free_connection_spec(freebob_connection_spec_t *connection_spec);
void freebobctl_free_stream_info(freebob_stream_info_t *stream_info);
void freebobctl_free_stream_spec(freebob_stream_spec_t *stream_spec);

void freebobctl_print_connection_info(freebob_connection_info_t *connection_info);

const char * freebobctl_get_version();

#endif // __FREEBOBCTL_FREEBOBCTL_H

// end of freebobctl.h

