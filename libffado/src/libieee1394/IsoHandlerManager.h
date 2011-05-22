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

#include <sys/poll.h>
#include <errno.h>
#include <vector>
#include <semaphore.h>

class Ieee1394Service;
//class IsoHandler;
//enum IsoHandler::EHandlerType;

namespace Streaming {
    class StreamProcessor;
    typedef std::vector<StreamProcessor *> StreamProcessorVector;
    typedef std::vector<StreamProcessor *>::iterator StreamProcessorVectorIterator;
}

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

////
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
                                  unsigned int cycle, unsigned int dropped);

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
            void setVerboseLevel(int l);

    // the enable/disable functions should only be used from within the loop that iterates()
    // but not from within the iterate callback. use the requestEnable / requestDisable functions
    // for that
            bool enable() {return enable(-1);};
            bool enable(int cycle);
            bool disable();

    // functions to request enable or disable at the next opportunity
            bool requestEnable(int cycle = -1);
            bool requestDisable();

            // Manually set the start cycle for the iso handler
            void setIsoStartCycle(signed int cycle = -1);

    /**
             * updates the internal state if required
     */
            void updateState();

            enum EHandlerType getType() {return m_type;};
            const char *getTypeString() {return eHTToString(m_type); };

    // pretty printing
            const char *eHTToString(enum EHandlerType);

            bool isEnabled()
            {return m_State == eHS_Running;};

    // no setter functions, because those would require a re-init
            unsigned int getMaxPacketSize() { return m_max_packet_size;};
            unsigned int getNbBuffers() { return m_buf_packets;};
            int getIrqInterval() { return m_irq_interval;};

            void dumpInfo();

            bool inUse() {return (m_Client != 0) ;};
            bool isStreamRegistered(Streaming::StreamProcessor *s) {return (m_Client == s);};

            bool registerStream(Streaming::StreamProcessor *);
            bool unregisterStream(Streaming::StreamProcessor *);

            bool canIterateClient(); // FIXME: implement with functor


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

    /**
             * @brief returns the CTR value saved at the last iterate handler call
             * @return CTR value saved at last iterate handler call
     */
            uint32_t getLastPacketTime() {return m_last_packet_handled_at;};

    /**
             * @brief set iso receive mode. doesn't have any effect if the stream is running
             * @param m receive mode
     */
            void setReceiveMode(enum raw1394_iso_dma_recv_mode m)
            {m_receive_mode = m;}

            void notifyOfDeath();
            bool handleBusReset();

        private:
            IsoHandlerManager& m_manager;
            enum EHandlerType m_type;
            raw1394handle_t m_handle;
            unsigned int    m_buf_packets;
            unsigned int    m_max_packet_size;
            int             m_irq_interval;
            int             m_last_cycle;
            uint32_t        m_last_now;
            uint32_t        m_last_packet_handled_at;
            enum raw1394_iso_dma_recv_mode m_receive_mode;

            Streaming::StreamProcessor *m_Client; // FIXME: implement with functors

            enum raw1394_iso_speed m_speed;

    // the state machine
            enum EHandlerStates {
                eHS_Stopped,
                eHS_Running,
                eHS_Error,
            };
            enum EHandlerStates m_State;
            enum EHandlerStates m_NextState;
            int m_switch_on_cycle;

            pthread_mutex_t m_disable_lock;

        public:
            unsigned int    m_packets;
#ifdef DEBUG
            unsigned int    m_dropped;
            unsigned int    m_skipped;
            int             m_min_ahead;
#endif

        protected:
            DECLARE_DEBUG_MODULE;
    };

    typedef std::vector<IsoHandler *> IsoHandlerVector;
    typedef std::vector<IsoHandler *>::iterator IsoHandlerVectorIterator;

////
    
// threads that will handle the packet framing
// one thread per direction, as a compromise for one per
// channel and one for all
    class IsoTask : public Util::RunnableInterface
    {
        friend class IsoHandlerManager;
        public:
            IsoTask(IsoHandlerManager& manager, enum IsoHandler::EHandlerType);
            virtual ~IsoTask();

        private:
            bool Init();
            bool Execute();

        /**
             * @brief requests the thread to sync it's stream map with the manager
         */
            void requestShadowMapUpdate();
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

        /**
             * @brief This should be called when a busreset has happened.
         */
            bool handleBusReset();

            void setVerboseLevel(int i);

        protected:
            IsoHandlerManager& m_manager;

        // the event request structure
            int32_t request_update;

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

            enum IsoHandler::EHandlerType m_handlerType;
            bool m_running;
            bool m_in_busreset;

        // activity signaling
            sem_t m_activity_semaphore;
            long long int m_activity_wait_timeout_nsec;

        // debug stuff
            DECLARE_DEBUG_MODULE;
    };
    
//// the IsoHandlerManager itself
    public:

        IsoHandlerManager(Ieee1394Service& service);
        IsoHandlerManager(Ieee1394Service& service, bool run_rt, int rt_prio);
        virtual ~IsoHandlerManager();

        bool setThreadParameters(bool rt, int priority);

        void setVerboseLevel(int l); ///< set the verbose level

        void dumpInfo(); ///< print some information about the manager to stdout/stderr
        void dumpInfoForStream(Streaming::StreamProcessor *); ///< print some info about the stream's handler

        bool registerStream(Streaming::StreamProcessor *); ///< register an iso stream with the manager
        bool unregisterStream(Streaming::StreamProcessor *); ///< unregister an iso stream from the manager

        bool startHandlers(); ///< start the managed ISO handlers
        bool startHandlers(int cycle); ///< start the managed ISO handlers
        bool stopHandlers(); ///< stop the managed ISO handlers

        bool reset(); ///< reset the ISO manager and all streams
        bool init();

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

        ///> Directly tells the handler attached to the stream to start on
        ///> the given cycle regardless of what is passed to
        ///> startHandlerForStream().
        void setIsoStartCycleForStream(Streaming::StreamProcessor *stream, signed int cycle);

        /**
         * returns the latency of a wake-up for this stream.
         * The latency is the time it takes for a packet is delivered to the
         * stream after it has been received (was on the wire).
         * expressed in cycles
         */
        int getPacketLatencyForStream(Streaming::StreamProcessor *);

        /**
         * Enables the isohandler manager to ignore missed packets.  This
         * behaviour is needed by some interfaces which don't send empty
         * placeholder packets when no data needs to be sent.
         */
        void setMissedCyclesOK(bool ok) { m_MissedCyclesOK = ok; };

    private:
        IsoHandler * getHandlerForStream(Streaming::StreamProcessor *stream);
        void requestShadowMapUpdate();
    public:
        Ieee1394Service& get1394Service() {return m_service;};

        /**
         * This should be called when a busreset has happened.
         */
        bool handleBusReset();

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

        bool            m_MissedCyclesOK;

        // debug stuff
        DECLARE_DEBUG_MODULE;

};

#endif /* __FFADO_ISOHANDLERMANAGER__  */



