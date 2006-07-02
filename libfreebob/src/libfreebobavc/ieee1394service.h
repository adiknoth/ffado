/* Ieee1394Service.cpp
 * Copyright (C) 2005,06 by Daniel Wagner
 *
 * This file is part of FreeBoB.
 *
 * FreeBoB is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBoB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBoB; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */

#ifndef FREEBOBIEEE1394SERVICE_H
#define FREEBOBIEEE1394SERVICE_H

#include "fbtypes.h"
#include "threads.h"

#include <libraw1394/raw1394.h>
#include <pthread.h>

#include <vector>

class Ieee1394Service{
public:
    Ieee1394Service();
    ~Ieee1394Service();

    bool initialize( int port );

    int getPort()
	{ return m_port; }
   /**
    * getNodecount - get number of nodes on the bus
    *
    * Since the root node always has
    * the highest node ID, this number can be used to determine that ID (it's
    * LOCAL_BUS|(count-1)).
    *
    * Returns: the number of nodes on the bus to which the port is connected.
    * This value can change with every bus reset.
    */
    int getNodeCount();

   /**
    * read - send async read request to a node and wait for response.
    * @node: target node (\XXX needs 0xffc0 stuff)
    * @addr: address to read from
    * @length: amount of data to read in quadlets
    * @buffer: pointer to buffer where data will be saved
    *
    * This does the complete transaction and will return when it's finished.
    *
    * Returns: true on success or false on failure (sets errno)
    */
    bool read( fb_nodeid_t nodeId,
	       fb_nodeaddr_t addr,
	       size_t length,
	       fb_quadlet_t* buffer );

    bool read_quadlet( fb_nodeid_t nodeId,
                       fb_nodeaddr_t addr,
                       fb_quadlet_t* buffer );

    bool read_octlet( fb_nodeid_t nodeId,
                      fb_nodeaddr_t addr,
                      fb_octlet_t* buffer );

    /**
    * write - send async write request to a node and wait for response.
    * @node: target node (\XXX needs 0xffc0 stuff)
    * @addr: address to write to
    * @length: amount of data to write in quadlets
    * @data: pointer to data to be sent
    *
    * This does the complete transaction and will return when it's finished.
    *
    * Returns: true on success or false on failure (sets errno)
    */
    bool write( fb_nodeid_t nodeId,
		fb_nodeaddr_t addr,
		size_t length,
		fb_quadlet_t* data );

    bool write_quadlet( fb_nodeid_t nodeId,
                        fb_nodeaddr_t addr,
                        fb_quadlet_t data );

    bool write_octlet(  fb_nodeid_t nodeId,
                        fb_nodeaddr_t addr,
                        fb_octlet_t data );

    fb_quadlet_t* transactionBlock( fb_nodeid_t nodeId,
                                    fb_quadlet_t* buf,
                                    int len,
				    unsigned int* resp_len );

    bool transactionBlockClose();


    void setVerbose( bool isVerbose );


    bool addBusResetHandler( Functor* functor );
    bool remBusResetHandler( Functor* functor );

private:

    bool startRHThread();
    void stopRHThread();
    static void* rHThread( void* arg );

    void printBuffer( size_t length, fb_quadlet_t* buffer ) const;

    static int resetHandlerLowLevel( raw1394handle_t handle,
				     unsigned int generation );
    bool resetHandler( unsigned int generation );

    raw1394handle_t m_handle;
    raw1394handle_t m_resetHandle;
    int             m_port;
    bool            m_verbose;
    unsigned int    m_generation;

    pthread_t       m_thread;
    pthread_mutex_t m_mutex;
    bool            m_threadRunning;

    typedef std::vector< Functor* > reset_handler_vec_t;
    reset_handler_vec_t m_busResetHandlers;
};

#endif
