/* ipchandler.cpp
 * Copyright (C) 2005 by Pieter Palmers
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
 *
 */
 
#include <lo/lo.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

#include "debugmodule.h"
#include "ipchandler.h"

IPCHandler* IPCHandler::m_pInstance = 0;

void ipc_error(int num, const char *msg, const char *path)
{
    printf("liblo server error %d in path %s: %s\n", num, path, msg);
}

int generic_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data) {
	IPCHandler *handler=(IPCHandler *)user_data;
	
	if (!user_data) {
		printf("user_data not valid\n");
		return -1;	
	}
	
	return handler->genericHandler(path, types,argv, argc, msg);
}

int request_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data) {
	IPCHandler *handler=(IPCHandler *)user_data;
	
	if (!user_data) {
		printf("user_data not valid\n");
		return -1;	
	}
	
	return handler->requestHandler(path, types,argv, argc, msg);
}

IPCHandler::IPCHandler() {
 	setDebugLevel( DEBUG_LEVEL_ALL );
	m_listenPort=31000;
	m_serverThread=NULL;
}

IPCHandler::~IPCHandler() {
	lo_server_thread_free(m_serverThread);
}

void IPCHandler::setListenPort(int aPort) {
	m_listenPort=aPort;
}

int IPCHandler::getListenPort() {
	return m_listenPort;
}

FBReturnCodes IPCHandler::initialize()
{
    debugPrint(DEBUG_LEVEL_IPC,"enter\n");
    
    char portnumber[64];
    snprintf(portnumber,64,"%d",m_listenPort);
    
    /* start a new server */
    m_serverThread = lo_server_thread_new(portnumber, ipc_error);

    if(!m_serverThread) {
    	debugPrint(DEBUG_LEVEL_IPC,"could not create ipc server thread\n");
    	return eFBRC_CreatingIPCServerFailed;
    }
    
    /* add method that will match any path and args */
    lo_server_thread_add_method(m_serverThread, NULL, NULL, generic_handler, (void *)this);

    lo_server_thread_add_method(m_serverThread, "/freebob/request", "s", request_handler, (void *)this);

    return eFBRC_Success;
}

FBReturnCodes IPCHandler::start() {
    debugPrint(DEBUG_LEVEL_IPC,"enter\n");
    
    if(!m_serverThread) {
    	debugPrint(DEBUG_LEVEL_IPC,"ipc server thread not valid\n");
    	return eFBRC_IPCServerInvalid;
    }
    lo_server_thread_start(m_serverThread);

    return eFBRC_Success;
}

FBReturnCodes IPCHandler::stop() {
    debugPrint(DEBUG_LEVEL_IPC,"enter\n");
    
    if(!m_serverThread) {
    	debugPrint(DEBUG_LEVEL_IPC,"ipc server thread not valid\n");
    	return eFBRC_IPCServerInvalid;
    }
    lo_server_thread_stop(m_serverThread);

    return eFBRC_Success;
}

void
IPCHandler::shutdown()
{
   debugPrint(DEBUG_LEVEL_IPC,"enter\n");
   delete this;
}

IPCHandler*
IPCHandler::instance()
{
    if ( !m_pInstance ) {
        m_pInstance = new IPCHandler;
    }
    return m_pInstance;
}

int IPCHandler::genericHandler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg) {
	int i;
	lo_address src=lo_message_get_source ( msg );
	
	debugPrint(DEBUG_LEVEL_IPC,"message from %s port %s (%s)\n", lo_address_get_hostname(src), lo_address_get_port(src), lo_address_get_url ( src ) );
	debugPrint(DEBUG_LEVEL_IPC,"to path: <%s>\n", path);
	for (i=0; i<argc; i++) {
		debugPrint(DEBUG_LEVEL_IPC,"arg %d '%c' ", i, types[i]);
		lo_arg_pp((lo_type)types[i], argv[i]);
		debugPrintShort(DEBUG_LEVEL_IPC,"\n");
	}
	debugPrint(DEBUG_LEVEL_IPC,"\n");
	
	return 1;
}

int IPCHandler::requestHandler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg) {

	lo_address src=lo_message_get_source ( msg );
	
	debugPrint(DEBUG_LEVEL_IPC,"request from %s port %s (%s)\n", lo_address_get_hostname(src), lo_address_get_port(src), lo_address_get_url ( src ) );
	debugPrint(DEBUG_LEVEL_IPC,"to path: <%s>\n", path);
	
	if(argc==1) {
		if(strcasecmp(&argv[0]->s,"connection_info")==0) {
			// send fixed XML description for now
			
			int fd=open("test.xml",O_RDONLY);
			if (fd<0) {
				printf("Could not open file\n");
				return 0;
			}
			int length=lseek(fd,0,SEEK_END);
			lseek(fd,0,SEEK_SET);
			
			char *buff=(char *)calloc(length,1); // also clears memory
			if((read(fd,(void *)buff,length)) < length) {
				printf("Didn't read all bytes\n");
			}
			
			close(fd);
			
			//respond
			if (lo_send(src, "/response", "s", buff) == -1) {
				printf("OSC error %d: %s\n", lo_address_errno(src), lo_address_errstr(src));
			}	
			
			free(buff);
			
		}
	}
	
	
	return 0;
}
