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

#include "libutil/Thread.h"

enum raw1394_iso_disposition;

class IsoHandlerManager;
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

    /**
     * Iterate the handler, transporting ISO packets to the client(s)
     * @return true if success
     */
    bool iterate();

    /**
     * Iterate the handler, transporting ISO packets to the client(s)
     * @param  ctr_now the CTR time at which the iterate call is done.
     * @return true if success
     */
    bool iterate(uint32_t ctr_now);

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

    bool canIterateClient(); // FIXME: implement with functor

    /**
     * @brief request that the handler exits the packet processing loop ASAP
     *
     * The raw1394 lib doesn't provide a means to stop the packet iteration loop
     * except when the iterate callback returns a DEFER value. Calling this function
     * will make the callback return DEFER ASAP.
     */
    void requestIterateLoopExit() {m_dont_exit_iterate_loop = false;};
    /**
     * @brief allow the handler to stay in the packet processing loop
     *
     * This resets the state set by requestIterateLoopExit()
     */
    void allowIterateLoop() {m_dont_exit_iterate_loop = true;};


    /**
     * @brief get last cycle number seen by handler
     * @return cycle number
     */
    int getLastCycle() {return m_last_cycle;};

    /**
     * @brief returns the CTR value saved at the last iterate() call
     * @return CTR value saved at last iterate() call
     */
    uint32_t getLastIterateTime() {return m_last_now;};

    void notifyOfDeath();
private:
    IsoHandlerManager& m_manager;
    enum EHandlerType m_type;
    raw1394handle_t m_handle;
    unsigned int    m_buf_packets;
    unsigned int    m_max_packet_size;
    int             m_irq_interval;
    int             m_last_cycle;
    uint32_t        m_last_now;

    Streaming::StreamProcessor *m_Client; // FIXME: implement with functors

    int handleBusReset(unsigned int generation);

    static int busreset_handler(raw1394handle_t handle, unsigned int generation);

    enum raw1394_iso_speed m_speed;
    unsigned int m_prebuffers;
    bool m_dont_exit_iterate_loop;

    // the state machine
    enum EHandlerStates {
        E_Created,
        E_Initialized,
        E_Prepared,
        E_Running,
        E_Error
    };
    enum EHandlerStates m_State;

    #ifdef DEBUG
    unsigned int    m_packets;
    unsigned int    m_dropped;
    int             m_min_ahead;
    #endif

    DECLARE_DEBUG_MODULE;
};

#endif /* __FFADO_ISOHANDLER__  */



