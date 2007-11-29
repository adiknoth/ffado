/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __FFADO_ISOSTREAM__
#define __FFADO_ISOSTREAM__

#include "../util/IsoHandler.h"

#include "debugmodule/debugmodule.h"

#include <libraw1394/raw1394.h>

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

        enum eStreamType {
                eST_Receive,
                eST_Transmit
        };

        IsoStream(enum eStreamType type)
            : m_channel(-1), m_port(0), m_handler(0), m_isostream_type(type)
        {};
        IsoStream(enum eStreamType type, int port)
                    : m_channel(-1), m_port(port), m_handler(0), m_isostream_type(type)
        {};
        virtual ~IsoStream()
        {};

        virtual void setVerboseLevel(int l) { setDebugLevel( l ); };

        int getChannel() {return m_channel;};
        bool setChannel(int c);

        int getPort() {return m_port;};

        enum eStreamType getStreamType() { return m_isostream_type;};

        virtual unsigned int getPacketsPerPeriod() {return 1;};
        virtual unsigned int getMaxPacketSize() {return 1024;}; //FIXME: arbitrary
        virtual unsigned int getNominalPacketsNeeded(unsigned int nframes) {return 1;}; //FIXME: arbitrary

        virtual enum raw1394_iso_disposition
                putPacket(unsigned char *data, unsigned int length,
                        unsigned char channel, unsigned char tag, unsigned char sy,
                            unsigned int cycle, unsigned int dropped);
        virtual enum raw1394_iso_disposition
                getPacket(unsigned char *data, unsigned int *length,
                        unsigned char *tag, unsigned char *sy,
                        int cycle, unsigned int dropped, unsigned int max_length);

        void dumpInfo();

    protected:
        void setHandler( IsoHandler * h) ;
        void clearHandler();

        int m_channel;
        int m_port;

        IsoHandler *m_handler;

    private: // should be set in the constructor
        enum eStreamType m_isostream_type;

        DECLARE_DEBUG_MODULE;

};

}

#endif /* __FFADO_ISOSTREAM__ */



