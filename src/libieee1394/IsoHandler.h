/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) version 3 of the License.
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

class IsoHandler
{
public:
    enum EHandlerType {
            eHT_Receive,
            eHT_Transmit
    };
    IsoHandler(IsoHandlerManager& manager, enum EHandlerType t);
    IsoHandler(IsoHandlerManager& manager, enum EHandlerType t,
               unsigned int buf_packets, unsigned int max_packet_size, int irq);
    IsoHandler(IsoHandlerManager& manager, enum EHandlerType t,
               unsigned int buf_packets, unsigned int max_packet_size, int irq, enum raw1394_iso_speed speed);
    ~IsoHandler();

private: // the ISO callback interface
    static enum raw1394_iso_disposition
    iso_receive_handler(raw1394handle_t handle, unsigned char *data,
                        unsigned int length, unsigned char channel,
                        unsigned char tag, unsigned char sy, unsigned int cycle,
                        unsigned int dropped);

    enum raw1394_iso_disposition
            putPacket(unsigned char *data, unsigned int length,
                        unsigned char channel, unsigned char tag, unsigned char sy,
                        unsigned int cycle, unsigned int dropped, unsigned int skipped);

    static enum raw1394_iso_disposition iso_transmit_handler(raw1394handle_t handle,
                    unsigned char *data, unsigned int *length,
                    unsigned char *tag, unsigned char *sy,
                    int cycle, unsigned int dropped);
    enum raw1394_iso_disposition
            getPacket(unsigned char *data, unsigned int *length,
                    unsigned char *tag, unsigned char *sy,
                    int cycle, unsigned int dropped, unsigned int skipped);

public:
    // runnable interface
    bool iterate();

    int getFileDescriptor() { return raw1394_get_fd(m_handle);};

    bool init();
    bool prepare();

    void setVerboseLevel(int l);

    bool enable() {return enable(-1);};
    bool enable(int cycle);
    bool disable();

    void flush();
    enum EHandlerType getType() {return m_type;};
    const char *getTypeString() {return eHTToString(m_type); };

    // pretty printing
    const char *eHTToString(enum EHandlerType);

    bool isEnabled()
        {return m_State == E_Running;};

    // no setter functions, because those would require a re-init
    unsigned int getMaxPacketSize() { return m_max_packet_size;};
    unsigned int getNbBuffers() { return m_buf_packets;};
    int getPacketLatency() { return m_irq_interval;};

    unsigned int getPreBuffers() {return m_prebuffers;};
    void setPreBuffers(unsigned int n) {m_prebuffers=n;};

    void dumpInfo();

    bool inUse() {return (m_Client != 0) ;};
    bool isStreamRegistered(Streaming::StreamProcessor *s) {return (m_Client == s);};

    bool registerStream(Streaming::StreamProcessor *);
    bool unregisterStream(Streaming::StreamProcessor *);

    bool waitForClient();
    bool tryWaitForClient();

private:
    IsoHandlerManager& m_manager;
    enum EHandlerType m_type;
    raw1394handle_t m_handle;
    unsigned int    m_buf_packets;
    unsigned int    m_max_packet_size;
    int             m_irq_interval;

    Streaming::StreamProcessor *m_Client;

    int handleBusReset(unsigned int generation);

    static int busreset_handler(raw1394handle_t handle, unsigned int generation);

    enum raw1394_iso_speed m_speed;
    unsigned int m_prebuffers;

    // the state machine
    enum EHandlerStates {
        E_Created,
        E_Initialized,
        E_Prepared,
        E_Running,
        E_Error
    };
    enum EHandlerStates m_State;
    DECLARE_DEBUG_MODULE;
};

#endif /* __FFADO_ISOHANDLER__  */



