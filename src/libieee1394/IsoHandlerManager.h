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

#ifndef __FFADO_ISOHANDLERMANAGER__
#define __FFADO_ISOHANDLERMANAGER__

#include "debugmodule/debugmodule.h"

#include "libutil/Thread.h"

#include <sys/poll.h>
#include <errno.h>

#include <vector>

#define FFADO_MAX_ISO_HANDLERS_PER_PORT 16

#define USLEEP_AFTER_UPDATE_FAILURE 10
#define USLEEP_AFTER_UPDATE 100
#define MAX_UPDATE_TRIES 10
class Ieee1394Service;
namespace Util {
    class PosixThread;
}

class IsoHandler;
namespace Streaming {
    class StreamProcessor;
    class StreamProcessorManager;
    typedef std::vector<StreamProcessor *> StreamProcessorVector;
    typedef std::vector<StreamProcessor *>::iterator StreamProcessorVectorIterator;
}

typedef std::vector<IsoHandler *> IsoHandlerVector;
typedef std::vector<IsoHandler *>::iterator IsoHandlerVectorIterator;

/*!
\brief The ISO Handler management class

 This class manages the use of ISO handlers by ISO streams.
 You can register an Streaming::StreamProcessor with an IsoHandlerManager. This
 manager will assign an IsoHandler to the stream. If nescessary
 the manager allocates a new handler. If there is already a handler
 that can handle the Streaming::StreamProcessor (e.g. in case of multichannel receive),
 it can be assigned.

*/
class IsoHandlerManager : public Util::RunnableInterface
{
    friend class Streaming::StreamProcessorManager;

    public:

        IsoHandlerManager(Ieee1394Service& service);
        IsoHandlerManager(Ieee1394Service& service, bool run_rt, unsigned int rt_prio);
        virtual ~IsoHandlerManager();

        bool setThreadParameters(bool rt, int priority);

        void setPollTimeout(int t) {m_poll_timeout=t;}; ///< set the timeout used for poll()
        int getPollTimeout() {return m_poll_timeout;};  ///< get the timeout used for poll()

        void setTransmitBufferNbFrames(unsigned int t) {m_xmit_nb_frames = t;};
        int getTransmitBufferNbFrames() {return m_xmit_nb_frames;};

        void setVerboseLevel(int l); ///< set the verbose level

        void dumpInfo(); ///< print some information about the manager to stdout/stderr

        bool registerStream(Streaming::StreamProcessor *); ///< register an iso stream with the manager
        bool unregisterStream(Streaming::StreamProcessor *); ///< unregister an iso stream from the manager

        bool startHandlers(); ///< start the managed ISO handlers
        bool startHandlers(int cycle); ///< start the managed ISO handlers
        bool stopHandlers(); ///< stop the managed ISO handlers

        bool reset(); ///< reset the ISO manager and all streams
        bool init();

        bool disable(IsoHandler *); ///< disables a handler
        bool enable(IsoHandler *); ///< enables a handler
        ///> disables the handler attached to the stream
        bool stopHandlerForStream(Streaming::StreamProcessor *);
        ///> starts the handler attached to the specific stream
        bool startHandlerForStream(Streaming::StreamProcessor *);
        ///> starts the handler attached to the specific stream on a specific cycle
        bool startHandlerForStream(Streaming::StreamProcessor *, int cycle); 

        /**
         * returns the latency of a wake-up for this stream.
         * The latency is the time it takes for a packet is delivered to the
         * stream after it has been received (was on the wire).
         * expressed in cycles
         */
        int getPacketLatencyForStream(Streaming::StreamProcessor *);

        void flushHandlerForStream(Streaming::StreamProcessor *stream);

        Ieee1394Service& get1394Service() {return m_service;};
    // RunnableInterface interface
    public:
        bool Execute(); // note that this is called in we while(running) loop
        bool Init();
        
        // protects the operations on the lists 
        // (FIXME: should be changed into a lock-free approach)
        pthread_mutex_t m_list_lock;

    // the state machine
    private:
        enum eHandlerStates {
            E_Created,
            E_Prepared,
            E_Running,
            E_Error
        };

        enum eHandlerStates m_State;
        const char *eHSToString(enum eHandlerStates);
    private:
        /// iterate all child handlers
        bool iterate();

    private:
        Ieee1394Service&  m_service;
        // note: there is a disctinction between streams and handlers
        // because one handler can serve multiple streams (in case of
        // multichannel receive)

        // only streams are allowed to be registered externally.
        // we allocate a handler if we need one, otherwise the stream
        // is assigned to another handler

        // the collection of handlers
        IsoHandlerVector m_IsoHandlers;

        bool registerHandler(IsoHandler *);
        bool unregisterHandler(IsoHandler *);
        void pruneHandlers();

        // the collection of streams
        Streaming::StreamProcessorVector m_StreamProcessors;

        // poll stuff
        int m_poll_timeout;
        // FD map sync requested
        int32_t m_request_fdmap_update;
        void updateShadowVars();

        // shadow variables
        struct pollfd m_poll_fds_shadow[FFADO_MAX_ISO_HANDLERS_PER_PORT];
        IsoHandler *m_IsoHandler_map_shadow[FFADO_MAX_ISO_HANDLERS_PER_PORT];
        unsigned int m_poll_nfds_shadow;

        void requestShadowUpdate();

        // threading
        bool m_realtime;
        unsigned int m_priority;
        Util::PosixThread *m_isoManagerThread;

        // the preferred number of packets to buffer on xmit
        unsigned int m_xmit_nb_frames;

        // debug stuff
        DECLARE_DEBUG_MODULE;

};

#endif /* __FFADO_ISOHANDLERMANAGER__  */



