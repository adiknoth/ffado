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
#include <sigc++/sigc++.h>

#include "freebob.h"

typedef unsigned int GenerationT;

class Ieee1394Service {
 public:
    Ieee1394Service();
    ~Ieee1394Service();

    FBReturnCodes initialize();

    static Ieee1394Service* instance();
    FBReturnCodes discoveryDevices();

    /**
     * Current generation count.
     *
     * If the count is increased a bus reset has occurred.
     */
    sigc::signal<void,GenerationT>sigGenerationCount;

 protected:
    static int resetHandler( raw1394handle_t handle, 
			     unsigned int iGeneration );

    bool startRHThread();
    void stopRHThread();
    static void* rHThread( void* arg );
 private:
    static Ieee1394Service* m_pInstance;
    raw1394handle_t m_handle;
    int m_iPort;
    bool m_bInitialised;
    pthread_t m_thread;
    pthread_mutex_t m_mutex;
    bool m_bRHThreadRunning;
};

#endif
