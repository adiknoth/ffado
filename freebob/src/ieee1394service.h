/* ieee1394service.h
 * Copyright (C) 2004,05 by Daniel Wagner
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
#ifndef IEEE1394SERVICE_H
#define IEEE1394SERVICE_H

#include <libraw1394/raw1394.h>
#include <pthread.h>

#include "freebob.h"
#include "debugmodule.h"
#include "csr1212.h"
 
class Ieee1394Service {
 public:
    FBReturnCodes initialize();
    void shutdown();

    static Ieee1394Service* instance();
    FBReturnCodes discoveryDevices( unsigned int iGenerationCount );

    unsigned int getGenerationCount();

    quadlet_t * avcExecuteTransaction( int node_id,
				       quadlet_t *request, 
				       unsigned int request_len, 
				       unsigned int response_len );



 protected:
    static int resetHandler( raw1394handle_t handle,
			     unsigned int iGeneration );
    void setGenerationCount( unsigned int iGeneration );

    bool startRHThread();
    void stopRHThread();
    static void* rHThread( void* arg );

    void printAvcUnitInfo( int iNodeId );

    void avDeviceTests( octlet_t oGuid, int iPort, int iNodeId );




 private:
    Ieee1394Service();
    ~Ieee1394Service();

    static Ieee1394Service* m_pInstance;
    raw1394handle_t m_handle;
    raw1394handle_t m_rhHandle;
    int m_iPort;                  // XXX dw: port in 1394 service makes no sense
    bool m_bInitialised;
    pthread_t m_thread;
    pthread_mutex_t m_mutex;
    bool m_bRHThreadRunning;
    unsigned int m_iGenerationCount;

    DECLARE_DEBUG_MODULE;
};

#endif
