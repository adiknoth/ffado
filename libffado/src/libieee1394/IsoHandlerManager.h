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

#ifndef __FFADO_ISOHANDLERMANAGER__
#define __FFADO_ISOHANDLERMANAGER__

#include "config.h"
#include "debugmodule/debugmodule.h"

#include "libutil/Thread.h"

#include "IsoHandler.h"

#include <sys/poll.h>
#include <errno.h>
#include <vector>
#include <semaphore.h>

class Ieee1394Service;
//class IsoHandler;
//enum IsoHandler::EHandlerType;

namespace Streaming {
    class StreamProcessor;
    class StreamProcessorManager;
    typedef std::vector<StreamProcessor *> StreamProcessorVector;
    typedef std::vector<StreamProcessor *>::iterator StreamProcessorVectorIterator;
}

typedef std::vector<IsoHandler *> IsoHandlerVector;
typedef std::vector<IsoHandler *>::iterator IsoHandlerVectorIterator;

class IsoHandlerManager;

// threads that will handle the packet framing
// one thread per direction, as a compromise for one per
// channel and one for all
class IsoTask : public Util::RunnableInterface
{
    public:
        IsoTask(IsoHandlerManager& manager, enum IsoHandler::EHandlerType);
        virtual ~IsoTask();

    public:
        bool Init();
        bool Execute();

        /**
         * @brief requests the thread to sync it's stream map with the manager
         */
        bool requestShadowMapUpdate();
        enum eActivityResult {
            eAR_Activity,
            eAR_Timeout,
            eAR_Interrupted,
            eAR_Error
        };

        /**
         * @brief signals that something happened in one of the clients of this task
         */
        void signalActivity();
        /**
         * @brief wait until something happened in one of the clients of this task
         */
        enum eActivityResult waitForActivity();

        void setVerboseLevel(int i);
    protected:
        IsoHandlerManager& m_manager;

        // the event request structure
        SInt32 request_update;

        // static allocation due to RT constraints
        // this is the map used by the actual thread
        // it is a shadow of the m_StreamProcessors vector
        struct pollfd   m_poll_fds_shadow[ISOHANDLERMANAGER_MAX_ISO_HANDLERS_PER_PORT];
        IsoHandler *    m_IsoHandler_map_shadow[ISOHANDLERMANAGER_MAX_ISO_HANDLERS_PER_PORT];
        unsigned int    m_poll_nfds_shadow;
        IsoHandler *    m_SyncIsoHandler;

        // updates the streams map
        void updateShadowMapHelper();

#ifdef DEBUG
        uint64_t m_last_loop_entry;
        int m_successive_short_loops;
#endif

        // activity signaling
        sem_t m_activity_semaphore;

        enum IsoHandler::EHandlerType m_handlerType;
        // debug stuff
        DECLARE_DEBUG_MODULE;
};

/*!
\brief The ISO Handler management class

 This class manages the use of ISO handlers by ISO streams.
 You can register an Streaming::StreamProcessor with an IsoHandlerManager. This
 manager will assign an IsoHandler to the stream. If nescessary
 the manager allocates a new handler. If there is already a handler
 that can handle the Streaming::StreamProcessor (e.g. in case of multichannel receive),
 it can be assigned.

*/

class IsoHandlerManager
{
    friend class IsoTask;

    public:

        IsoHandlerManager(Ieee1394Service& service);
        IsoHandlerManager(Ieee1394Service& service, bool run_rt, int rt_prio);
        virtual ~IsoHandlerManager();

        bool setThreadParameters(bool rt, int priority);

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

        /**
         * @brief signals that something happened in one of the clients
         */
        void signalActivityTransmit();
        void signalActivityReceive();

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

        void requestShadowMapUpdate();

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

        // handler thread/task
        bool            m_realtime;
        int             m_priority;
        Util::Thread *  m_IsoThreadTransmit;
        IsoTask *       m_IsoTaskTransmit;
        Util::Thread *  m_IsoThreadReceive;
        IsoTask *       m_IsoTaskReceive;

        // debug stuff
        DECLARE_DEBUG_MODULE;

};

#endif /* __FFADO_ISOHANDLERMANAGER__  */



