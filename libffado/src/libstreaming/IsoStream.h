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
#ifndef __FREEBOB_ISOSTREAM__
#define __FREEBOB_ISOSTREAM__

#include <libraw1394/raw1394.h>
#include "../debugmodule/debugmodule.h"
#include "IsoHandler.h"

namespace Streaming
{

class PacketBuffer;

/*!
\brief The Base Class for ISO streams
*/

class IsoStream
{
    friend class IsoHandler;
    friend class IsoRecvHandler;
    friend class IsoXmitHandler;

    public:

        enum EStreamType {
                EST_Receive,
                EST_Transmit
        };

        IsoStream(enum EStreamType type) 
                    : m_type(type), m_channel(-1), m_port(0), m_handler(0)
        {};
        IsoStream(enum EStreamType type, int port) 
                    : m_type(type), m_channel(-1), m_port(port), m_handler(0)
        {};
        virtual ~IsoStream()
        {};
    
        virtual void setVerboseLevel(int l) { setDebugLevel( l ); };

        int getChannel() {return m_channel;};
        bool setChannel(int c);

        int getPort() {return m_port;};

        enum EStreamType getType() { return m_type;};

        virtual unsigned int getPacketsPerPeriod() {return 1;};
        virtual unsigned int getMaxPacketSize() {return 1024;}; //FIXME: arbitrary
        
        virtual bool init();

        virtual enum raw1394_iso_disposition 
                putPacket(unsigned char *data, unsigned int length, 
                        unsigned char channel, unsigned char tag, unsigned char sy, 
                            unsigned int cycle, unsigned int dropped);
        virtual enum raw1394_iso_disposition 
                getPacket(unsigned char *data, unsigned int *length,
                        unsigned char *tag, unsigned char *sy,
                        int cycle, unsigned int dropped, unsigned int max_length);

        void dumpInfo();

        int getNodeId();
        
        virtual bool reset();
        virtual bool prepare();	
    
    protected:

        void setHandler( IsoHandler * h) ;
        void clearHandler();

        enum EStreamType m_type;
        int m_channel;
        int m_port;

        IsoHandler *m_handler;
        
        DECLARE_DEBUG_MODULE;

};

}

#endif /* __FREEBOB_ISOSTREAM__ */



