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

#ifndef __FFADO_ISOHANDLER__
#define __FFADO_ISOHANDLER__

#include "debugmodule/debugmodule.h"

#include <libraw1394/raw1394.h>


enum raw1394_iso_disposition ;
namespace Streaming
{

class IsoStream;
/*!
\brief The Base Class for ISO Handlers

 These classes perform the actual ISO communication through libraw1394.
 They are different from IsoStreams because one handler can provide multiple
 streams with packets in case of ISO multichannel receive.

*/

class IsoHandler
{
    protected:

    public:

        enum EHandlerType {
                EHT_Receive,
                EHT_Transmit
        };

        IsoHandler(int port);

        IsoHandler(int port, unsigned int buf_packets, unsigned int max_packet_size, int irq);

        virtual ~IsoHandler();

        virtual bool init();
        virtual bool prepare();
        virtual bool start(int cycle);
        virtual bool stop();

        bool iterate();

        void setVerboseLevel(int l);

        // no setter functions, because those would require a re-init
        unsigned int getMaxPacketSize() { return m_max_packet_size;};
        unsigned int getNbBuffers() { return m_buf_packets;};
        int getWakeupInterval() { return m_irq_interval;};

        int getPacketCount() {return m_packetcount;};
        void resetPacketCount() {m_packetcount=0;};

        int getDroppedCount() {return m_dropped;};
        void resetDroppedCount() {m_dropped=0;};

        virtual enum EHandlerType getType() = 0;

        int getFileDescriptor() { return raw1394_get_fd(m_handle);};

        void dumpInfo();

        bool inUse() {return (m_Client != 0) ;};
        virtual bool isStreamRegistered(IsoStream *s) {return (m_Client == s);};

        virtual bool registerStream(IsoStream *);
        virtual bool unregisterStream(IsoStream *);

        int getLocalNodeId() {return raw1394_get_local_id( m_handle );};
        int getPort() {return m_port;};

        /// get the most recent cycle timer value (in ticks)
        unsigned int getCycleTimerTicks();
        /// get the most recent cycle timer value (as is)
        unsigned int getCycleTimer();

    protected:
        raw1394handle_t m_handle;
        raw1394handle_t m_handle_util;
        int             m_port;
        unsigned int    m_buf_packets;
        unsigned int    m_max_packet_size;
        int             m_irq_interval;

        int m_packetcount;
        int m_dropped;

        IsoStream *m_Client;

        virtual int handleBusReset(unsigned int generation);


        DECLARE_DEBUG_MODULE;

    private:
        static int busreset_handler(raw1394handle_t handle, unsigned int generation);

    // the state machine
    private:
        enum EHandlerStates {
            E_Created,
            E_Initialized,
            E_Prepared,
            E_Running,
            E_Error
        };

        enum EHandlerStates m_State;

};

/*!
\brief ISO receive handler class (not multichannel)
*/

class IsoRecvHandler : public IsoHandler
{

    public:
        IsoRecvHandler(int port);
        IsoRecvHandler(int port, unsigned int buf_packets, unsigned int max_packet_size, int irq);
        virtual ~IsoRecvHandler();

        bool init();

        enum EHandlerType getType() { return EHT_Receive;};

        bool start(int cycle);

        bool prepare();

    protected:
        int handleBusReset(unsigned int generation);

    private:
        static enum raw1394_iso_disposition
        iso_receive_handler(raw1394handle_t handle, unsigned char *data,
                            unsigned int length, unsigned char channel,
                            unsigned char tag, unsigned char sy, unsigned int cycle,
                            unsigned int dropped);

        enum raw1394_iso_disposition
                putPacket(unsigned char *data, unsigned int length,
                          unsigned char channel, unsigned char tag, unsigned char sy,
                          unsigned int cycle, unsigned int dropped);

};

/*!
\brief ISO transmit handler class
*/

class IsoXmitHandler  : public IsoHandler
{
    public:
        IsoXmitHandler(int port);
        IsoXmitHandler(int port, unsigned int buf_packets,
                        unsigned int max_packet_size, int irq);
        IsoXmitHandler(int port, unsigned int buf_packets,
                        unsigned int max_packet_size, int irq,
                        enum raw1394_iso_speed speed);
        virtual ~IsoXmitHandler();

        bool init();

        enum EHandlerType getType() { return EHT_Transmit;};

        unsigned int getPreBuffers() {return m_prebuffers;};
        void setPreBuffers(unsigned int n) {m_prebuffers=n;};

        bool start(int cycle);

        bool prepare();

    protected:
        int handleBusReset(unsigned int generation);

    private:
        static enum raw1394_iso_disposition iso_transmit_handler(raw1394handle_t handle,
                        unsigned char *data, unsigned int *length,
                        unsigned char *tag, unsigned char *sy,
                        int cycle, unsigned int dropped);
        enum raw1394_iso_disposition
                getPacket(unsigned char *data, unsigned int *length,
                        unsigned char *tag, unsigned char *sy,
                        int cycle, unsigned int dropped);

        enum raw1394_iso_speed m_speed;

        unsigned int m_prebuffers;

};

}

#endif /* __FFADO_ISOHANDLER__  */



