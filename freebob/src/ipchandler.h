/* ipchandler.h
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
#ifndef IPCHANDLER_H
#define IPCHANDLER_H

#include <lo/lo.h>

#include "debugmodule.h"
#include "freebob.h"

class IPCHandler {
public:
   
    void setListenPort(int aPort);
    int getListenPort();
    
    FBReturnCodes initialize();
    void shutdown();

    static IPCHandler* instance();

    FBReturnCodes start();
    FBReturnCodes stop();
    
    int genericHandler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg);
    int requestHandler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg);
    
private:

    	IPCHandler();
    	virtual ~IPCHandler();
 
	static IPCHandler *m_pInstance;
    
	int m_listenPort;
	lo_server_thread m_serverThread;
	
    DECLARE_DEBUG_MODULE;

};

#endif // IPCHANDLER_H
