/* $Id$ */

/*
 *   FreeBob streaming API
 *   FreeBob = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2006 Pieter Palmers <pieterpalmers@users.sourceforge.net>
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
#ifndef __FREEBOB_ISOHANDLERMANAGER__
#define __FREEBOB_ISOHANDLERMANAGER__

#include "../debugmodule/debugmodule.h"
#include "../libutil/Thread.h"

#include <sys/poll.h>
#include <errno.h>

#include <vector>

#define USLEEP_AFTER_UPDATE_FAILURE 10
#define USLEEP_AFTER_UPDATE 100
#define MAX_UPDATE_TRIES 10
namespace Util {
    class PosixThread;
}

namespace Streaming
{

class IsoHandler;
class IsoStream;

typedef std::vector<IsoHandler *> IsoHandlerVector;
typedef std::vector<IsoHandler *>::iterator IsoHandlerVectorIterator;

typedef std::vector<IsoStream *> IsoStreamVector;
typedef std::vector<IsoStream *>::iterator IsoStreamVectorIterator;


/*!
\brief The ISO Handler management class

 This class manages the use of ISO handlers by ISO streams.
 You can register an IsoStream with an IsoHandlerManager. This
 manager will assign an IsoHandler to the stream. If nescessary
 the manager allocates a new handler. If there is already a handler
 that can handle the IsoStream (e.g. in case of multichannel receive),
 it can be assigned.

*/

class IsoHandlerManager : public Util::RunnableInterface
{
    friend class StreamProcessorManager;

    public:

        IsoHandlerManager();
        IsoHandlerManager(bool run_rt, unsigned int rt_prio);
        virtual ~IsoHandlerManager();

        void setPollTimeout(int t) {m_poll_timeout=t;}; ///< set the timeout used for poll()
        int getPollTimeout() {return m_poll_timeout;};  ///< get the timeout used for poll()

        void setVerboseLevel(int l); ///< set the verbose level

        void dumpInfo(); ///< print some information about the manager to stdout/stderr

        bool registerStream(IsoStream *); ///< register an iso stream with the manager
        bool unregisterStream(IsoStream *); ///< unregister an iso stream from the manager

        bool startHandlers(); ///< start the managed ISO handlers 
        bool startHandlers(int cycle); ///< start the managed ISO handlers 
        bool stopHandlers(); ///< stop the managed ISO handlers 

        bool reset(); ///< reset the ISO manager and all streams

        bool prepare(); ///< prepare the ISO manager and all streams
        
        bool init();
        
        void disablePolling(IsoStream *); ///< disables polling on a stream
        void enablePolling(IsoStream *); ///< enables polling on a stream

    // RunnableInterface interface
    public:
        bool Execute(); // note that this is called in we while(running) loop
        bool Init();
        
    // the state machine
    private:
        enum EHandlerStates {
            E_Created,
            E_Prepared,
            E_Running,
            E_Error
        };
        
        enum EHandlerStates m_State;
        
    private:
        /// iterate all child handlers
        bool iterate();

    private:
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
        IsoStreamVector m_IsoStreams;

        // poll stuff
        int m_poll_timeout;
        struct pollfd *m_poll_fds;
        int m_poll_nfds;

        bool rebuildFdMap();

        // threading
        bool m_realtime;
        unsigned int m_priority;
        Util::PosixThread *m_isoManagerThread;
        
        
        // debug stuff
        DECLARE_DEBUG_MODULE;

};

}

#endif /* __FREEBOB_ISOHANDLERMANAGER__  */



