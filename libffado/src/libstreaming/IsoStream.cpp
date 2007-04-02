/* $Id$ */

/*
 *   FreeBob Streaming API
 *   FreeBob = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2005,2006 Pieter Palmers <pieterpalmers@users.sourceforge.net>
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

#include "IsoStream.h"
#include <assert.h>

namespace Streaming
{

IMPL_DEBUG_MODULE( IsoStream, IsoStream, DEBUG_LEVEL_NORMAL );

enum raw1394_iso_disposition 
IsoStream::putPacket(unsigned char *data, unsigned int length, 
                     unsigned char channel, unsigned char tag, unsigned char sy, 
                     unsigned int cycle, unsigned int dropped) {

    debugOutput( DEBUG_LEVEL_VERY_VERBOSE,
                 "received packet: length=%d, channel=%d, cycle=%d\n",
                 length, channel, cycle );

    return RAW1394_ISO_OK;
}

enum raw1394_iso_disposition 
IsoStream::getPacket(unsigned char *data, unsigned int *length,
                     unsigned char *tag, unsigned char *sy,
                     int cycle, unsigned int dropped, unsigned int max_length) {
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE,
                 "sending packet: length=%d, cycle=%d\n",
                 *length, cycle );

    memcpy(data,&cycle,sizeof(cycle));
    *length=sizeof(cycle);
    *tag = 1;
    *sy = 0;


    return RAW1394_ISO_OK;
}

int IsoStream::getNodeId() {
    if (m_handler) {
        return m_handler->getLocalNodeId();
    }
    return -1;
}


void IsoStream::dumpInfo()
{

    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Address        : %p\n",this);
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Stream type    : %s\n",
            (this->getType()==EST_Receive ? "Receive" : "Transmit"));
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Port, Channel  : %d, %d\n",
            m_port, m_channel);

}

bool IsoStream::setChannel(int c) {
    debugOutput( DEBUG_LEVEL_VERBOSE, "setting channel for (%p) to %d\n",this, c);

    m_channel=c;
    return true;
}


bool IsoStream::reset() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
    return true;
}

bool IsoStream::prepare() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
    return true;
}

bool IsoStream::init() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
    return true;

}

void IsoStream::setHandler(IsoHandler *h) {
    debugOutput( DEBUG_LEVEL_VERBOSE, "setting handler of isostream %p to %p\n", this,h);
    m_handler=h;
}

void IsoStream::clearHandler() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "clearing handler of isostream %p\n", this);

    m_handler=0;

}


}
