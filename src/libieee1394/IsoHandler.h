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

#ifndef __FFADO_ISOHANDLER__
#define __FFADO_ISOHANDLER__

#include "debugmodule/debugmodule.h"
#include "IsoHandlerManager.h"

#include "libutil/Thread.h"

enum raw1394_iso_disposition;

namespace Streaming {
    class StreamProcessor;
}

/*!
\brief The Base Class for ISO Handlers

 These classes perform the actual ISO communication through libraw1394.
 They are different from Streaming::StreamProcessors because one handler can provide multiple
 streams with packets in case of ISO multichannel receive.

*/

class IsoHandler : public Util::RunnableInterface
{
public:
    enum EHandlerType {
            EHT_Receive,
            EHT_Transmit
    };
    IsoHandler(IsoHandlerManager& manager);
    IsoHandler(IsoHandlerManager& manager, unsigned int buf_packets, unsigned int max_packet_size, int irq);
    virtual ~IsoHandler();

    // runnable interface
    virtual bool Init();
    virtual bool Execute();
    int getFileDescriptor() { return raw1394_get_fd(m_handle);};
    bool setThreadParameters(bool rt, int priority);

    virtual bool init();
    virtual bool prepare();

    bool iterate();
    void setVerboseLevel(int l);

    virtual bool enable() {return enable(-1);};
    virtual bool enable(int cycle);
    virtual bool disable();

    virtual void flush() = 0;

    bool isEnabled()
        {return m_State == E_Running;};

    // no setter functions, because those would require a re-init
    unsigned int getMaxPacketSize() { return m_max_packet_size;};
    unsigned int getNbBuffers() { return m_buf_packets;};
    int getPacketLatency() { return m_irq_interval;};

    int getPacketCount() {return m_packetcount;};
    void resetPacketCount() {m_packetcount=0;};

    int getDroppedCount() {return m_dropped;};
    void resetDroppedCount() {m_dropped=0;};

    virtual enum EHandlerType getType() = 0;

    virtual void dumpInfo();

    bool inUse() {return (m_Client != 0) ;};
    virtual bool isStreamRegistered(Streaming::StreamProcessor *s) {return (m_Client == s);};

    virtual bool registerStream(Streaming::StreamProcessor *);
    virtual bool unregisterStream(Streaming::StreamProcessor *);

    protected:
        IsoHandlerManager& m_manager;
        raw1394handle_t m_handle;
        unsigned int    m_buf_packets;
        unsigned int    m_max_packet_size;
        int             m_irq_interval;

        int m_packetcount;
        int m_dropped;
        Streaming::StreamProcessor *m_Client;

        virtual int handleBusReset(unsigned int generation);
        DECLARE_DEBUG_MODULE;
    private:
        static int busreset_handler(raw1394handle_t handle, unsigned int generation);

        struct pollfd   m_poll_fd;
        int             m_poll_timeout;
        // threading
        bool            m_realtime;
        int             m_priority;
        Util::Thread *  m_Thread;

    // the state machine
    protected:
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
        IsoRecvHandler(IsoHandlerManager& manager);
        IsoRecvHandler(IsoHandlerManager& manager, unsigned int buf_packets, unsigned int max_packet_size, int irq);
        virtual ~IsoRecvHandler();

        bool init();
        enum EHandlerType getType() { return EHT_Receive;};
        bool enable(int cycle);
        virtual bool prepare();
        virtual void flush();

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
        IsoXmitHandler(IsoHandlerManager& manager);
        IsoXmitHandler(IsoHandlerManager& manager, unsigned int buf_packets,
                        unsigned int max_packet_size, int irq);
        IsoXmitHandler(IsoHandlerManager& manager, unsigned int buf_packets,
                        unsigned int max_packet_size, int irq,
                        enum raw1394_iso_speed speed);
        virtual ~IsoXmitHandler();

        bool init();
        enum EHandlerType getType() { return EHT_Transmit;};
        unsigned int getPreBuffers() {return m_prebuffers;};
        void setPreBuffers(unsigned int n) {m_prebuffers=n;};
        virtual bool enable(int cycle);
        virtual bool prepare();
        virtual void flush() {};

        void dumpInfo();
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

#endif /* __FFADO_ISOHANDLER__  */



