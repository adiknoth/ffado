/* tempate.cpp
 * Copyright (C) 2005 by Daniel Wagner
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
#include "ieee1394service.h"

#include <libavc1394/avc1394.h>

#include <errno.h>
#include <netinet/in.h>

#include <iostream>

using namespace std;

Ieee1394Service::Ieee1394Service()
    : m_handle( 0 )
    , m_port( -1 )
{
}

Ieee1394Service::~Ieee1394Service()
{
}

bool
Ieee1394Service::initialize( int port )
{
    m_handle = raw1394_new_handle_on_port( port );
    if ( !m_handle ) {
        if ( !errno ) {
            cerr << "libraw1394 not compatible" << endl;
        } else {
            perror( "Ieee1394Service::initialize: Could not get 1394 handle" );
            cerr << "Is ieee1394 and raw1394 driver loaded?" << endl;
        }
        return false;
    }

    m_port = port;
    return true;
}

int
Ieee1394Service::getNodeCount()
{
    return raw1394_get_nodecount( m_handle );
}

bool
Ieee1394Service::read( fb_nodeid_t nodeId,
                       fb_nodeaddr_t addr,
                       size_t size,
                       fb_quadlet_t* buffer )
{
    return raw1394_read( m_handle, nodeId, addr,  size,  buffer ) != 0;
}

bool
Ieee1394Service::write( fb_nodeid_t   nodeId,
                        fb_nodeaddr_t addr,
                        size_t        length,
                        fb_quadlet_t  *data )
{
    return raw1394_write( m_handle, nodeId, addr, length, data ) != 0;
}

fb_quadlet_t*
Ieee1394Service::transactionBlock( fb_nodeid_t nodeId,
                                   fb_quadlet_t* buf,
                                   int len,
                                   unsigned int* resp_len )
{
    for (int i = 0; i < len; ++i) {
        buf[i] = ntohl( buf[i] );
    }

    fb_quadlet_t* result =
        avc1394_transaction_block2( m_handle,
                                    nodeId,
                                    buf,
                                    len,
                                    resp_len,
                                    10 );

    for ( unsigned int i = 0; i < *resp_len; ++i ) {
        result[i] = htonl( result[i] );
    }

    return result;
}


bool
Ieee1394Service::transactionBlockClose()
{
    avc1394_transaction_block_close( m_handle );
    return true;
}
