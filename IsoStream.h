/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software Foundation;
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#ifndef __FFADO_ISOSTREAM__
#define __FFADO_ISOSTREAM__

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

#endif /* __FFADO_ISOSTREAM__ */



