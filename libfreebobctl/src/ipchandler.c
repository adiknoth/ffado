// ipchandler.c
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
#include <unistd.h>
#include <lo/lo.h>

#include "freebobctl/ipchandler.h"

void freebobctl_ipc_error(int num, const char *m, const char *path) {
    printf("liblo server error %d in path %s: %s\n", num, path, m);
};

int freebobctl_ipc_response_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message data, void *user_data) {
	
	char **buffer=(char **)user_data;
	if(buffer) {
		if((argc>0) && (types[0]=='s')) {
			*buffer=strdup(&argv[0]->s);
		} else {
			*buffer=NULL;
		}
	}
	
	return 0;
	
}

int freebobctl_ipc_generic_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message data, void *user_data) {
	
	int i;
	
	printf("message received on path: <%s>\n", path);
	for (i=0; i<argc; i++) {
		printf("arg %d '%c' ", i, types[i]);
		lo_arg_pp(types[i], argv[i]);
		printf("\n");
	}

	printf("\n");	
	return 1;
	
}

char * freebobctl_ipc_request_connection_info(char *url) {
	char *buffer=NULL;
	
	printf("requesting channel info from %s\n",url);
	
	lo_server s = lo_server_new("7770", freebobctl_ipc_error);
	
	if(!s) {
		return NULL;
	}
	
	/* add method that will match any path and args */
	lo_server_add_method(s, NULL, NULL, freebobctl_ipc_generic_handler, NULL);
	
	lo_server_add_method(s, "/response", "s", freebobctl_ipc_response_handler, &buffer);
	
	lo_address t = lo_address_new_from_url(url);
	
	if (lo_send(t, "/freebob/request", "s","connection_info") == -1) {
		printf("OSC error %d: %s\n", lo_address_errno(t), lo_address_errstr(t));
		lo_address_free(t);
		lo_server_free(s);
		return NULL;
	}
	
	/* wait for the response, with timeout */
	if (lo_server_recv_noblock(s,5000)==0) {
		printf("Timeout receiving response...\n");
		lo_address_free(t);
		lo_server_free(s);
		return NULL;
	}
	
	lo_address_free(t);
	lo_server_free(s);
	
	printf("done...\n");
	return buffer;

}
