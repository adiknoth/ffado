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
#include "FreebobThread.h"

#include <sys/poll.h>
#include <errno.h>

#include <vector>

namespace FreebobStreaming
{

/*!
\brief The ISO Handler management class

 This class manages the use of ISO handlers by ISO streams

*/

class IsoHandler;
class IsoStream;

typedef std::vector<IsoHandler *> IsoHandlerVector;
typedef std::vector<IsoHandler *>::iterator IsoHandlerVectorIterator;

typedef std::vector<IsoStream *> IsoStreamVector;
typedef std::vector<IsoStream *>::iterator IsoStreamVectorIterator;


class IsoHandlerManager : public FreebobRunnableInterface
{
	friend class StreamRunner;

    public:

        IsoHandlerManager();
        virtual ~IsoHandlerManager();

		void setPollTimeout(int t) {m_poll_timeout=t;};
		int getPollTimeout() {return m_poll_timeout;};

		void setVerboseLevel(int l) { setDebugLevel( l ); };

		void dumpInfo();

		int registerStream(IsoStream *);
		int unregisterStream(IsoStream *);

		int startHandlers();
		int startHandlers(int cycle);
		void stopHandlers();

	protected:
		// FreebobRunnableInterface interface
		bool Execute(); // note that this is called in we while(running) loop
		bool Init();

		// note: there is a disctinction between streams and handlers
		// because one handler can serve multiple streams (in case of 
		// multichannel receive)

		// only streams are allowed to be registered externally.
		// we allocate a handler if we need one, otherwise the stream
		// is assigned to another handler

		// the collection of handlers
 		IsoHandlerVector m_IsoHandlers;

		int registerHandler(IsoHandler *);
		int unregisterHandler(IsoHandler *);
		void pruneHandlers();

		// the collection of streams
 		IsoStreamVector m_IsoStreams;

		// poll stuff
		int m_poll_timeout;
		struct pollfd *m_poll_fds;
		int m_poll_nfds;

		int rebuildFdMap();


	    DECLARE_DEBUG_MODULE;

};

}

#endif /* __FREEBOB_ISOHANDLERMANAGER__  */



