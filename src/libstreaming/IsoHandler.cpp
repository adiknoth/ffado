/* $Id$ */

/*
 *   FreeBob Streaming API
 *   FreeBob = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2006 Pieter Palmers <pieterpalmers@users.sourceforge.net>
 *
 *   This program is free software {} you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation {} either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY {} without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program {} if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * 
 *
 */

#include "IsoHandler.h"
#include <errno.h>
#include <netinet/in.h>

#include <iostream>
using namespace std;


namespace FreebobStreaming
{

/* the C callbacks */
extern "C" {
	enum raw1394_iso_disposition 
	iso_transmit_handler(raw1394handle_t handle,
			unsigned char *data, unsigned int *length,
			unsigned char *tag, unsigned char *sy,
			int cycle, unsigned int dropped) {

		IsoXmitHandler *xmitHandler=static_cast<IsoXmitHandler *>(raw1394_get_userdata(handle));
		assert(xmitHandler);

		return xmitHandler->GetPacket(data, length, tag, sy, cycle, dropped);
	}

	enum raw1394_iso_disposition 
		iso_slave_receive_handler(raw1394handle_t handle, unsigned char *data, 
								  unsigned int length, unsigned char channel,
								  unsigned char tag, unsigned char sy, unsigned int cycle, 
								  unsigned int dropped) {

		IsoRecvHandler *recvHandler=static_cast<IsoRecvHandler *>(raw1394_get_userdata(handle));
		assert(recvHandler);

// 		return recvHandler->PutPacket(data, length, channel, tag, sy, cycle, dropped);
	}

}

bool
IsoHandler::Initialize( int port )
{
    m_handle = raw1394_new_handle_on_port( port );
    if ( !m_handle ) {
        if ( !errno ) {
            cerr << "libraw1394 not compatible" << endl;
        } else {
            perror( "IsoHandler::Initialize: Could not get 1394 handle" );
            cerr << "Is ieee1394 and raw1394 driver loaded?" << endl;
        }
        return false;
    }

	raw1394_set_userdata(m_handle, static_cast<void *>(this));

    m_port = port;
    return true;
}


IsoRecvHandler::IsoRecvHandler()
		: IsoHandler()
{

}

IsoRecvHandler::~IsoRecvHandler()
{

}

enum raw1394_iso_disposition IsoRecvHandler::PutPacket(unsigned char *data, unsigned int length, 
		              unsigned char channel, unsigned char tag, unsigned char sy, 
			          unsigned int cycle, unsigned int dropped) {

	return RAW1394_ISO_OK;
}

IsoXmitHandler::IsoXmitHandler()
		: IsoHandler()
{

}

IsoXmitHandler::~IsoXmitHandler()
{

}

enum raw1394_iso_disposition IsoXmitHandler::GetPacket(unsigned char *data, unsigned int *length,
		              unsigned char *tag, unsigned char *sy,
		              int cycle, unsigned int dropped) {

	return RAW1394_ISO_OK;
}


}
