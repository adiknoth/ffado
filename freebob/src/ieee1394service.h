/* ieee1394service.h
 * Copyright (C) 2004 by Daniel Wagner
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
#include <libavc1394/rom1394.h>
#include <pthread.h>

#include "freebob.h"

/* XXX: add those to avc1394.h */
#define AVC1394_SUBUNIT_TYPE_AUDIO (1 <<19) 
#define AVC1394_SUBUNIT_TYPE_PRINTER (2 <<19) 
#define AVC1394_SUBUNIT_TYPE_CA (6 <<19) 
#define AVC1394_SUBUNIT_TYPE_PANEL (9 <<19) 
#define AVC1394_SUBUNIT_TYPE_BULLETIN_BOARD (0xA <<19) 
#define AVC1394_SUBUNIT_TYPE_CAMERA_STORAGE (0xB <<19) 

class Ieee1394Service {
 public:
    Ieee1394Service();
    ~Ieee1394Service();

    FBReturnCodes initialize();
    void shutdown(); 

    static Ieee1394Service* instance();
    FBReturnCodes discoveryDevices();

    unsigned int getGenerationCount();

 protected:
    static int resetHandler( raw1394handle_t handle, 
			     unsigned int iGeneration );
    void setGenerationCount( unsigned int iGeneration );

    bool startRHThread();
    void stopRHThread();
    static void* rHThread( void* arg );

    void printAvcUnitInfo( int iNodeId );
    void printRomDirectory( int iNodeId,  rom1394_directory* pRomDir );
 private:
    static Ieee1394Service* m_pInstance;
    raw1394handle_t m_handle;
    raw1394handle_t m_rhHandle;
    int m_iPort;
    bool m_bInitialised;
    pthread_t m_thread;
    pthread_mutex_t m_mutex;
    bool m_bRHThreadRunning;
    unsigned int m_iGenerationCount;
};

#endif
