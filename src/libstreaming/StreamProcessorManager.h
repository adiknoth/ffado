/* $Id$ */

/*
 *   FreeBob Streaming API
 *   FreeBob = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2005,2006 Pieter Palmers <pieterpalmers@users.sourceforge.net>
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
#ifndef __FREEBOB_STREAMPROCESSORMANAGER__
#define __FREEBOB_STREAMPROCESSORMANAGER__

#include "../debugmodule/debugmodule.h"
#include "FreebobThread.h"
#include <semaphore.h>

#include <vector>

namespace FreebobStreaming {

class StreamProcessor;
typedef std::vector<StreamProcessor *> StreamProcessorVector;
typedef std::vector<StreamProcessor *>::iterator StreamProcessorVectorIterator;


class StreamProcessorManager :
                        public FreebobRunnableInterface {

	friend class StreamRunner;

public:

	StreamProcessorManager(unsigned int period, unsigned int nb_buffers);
	virtual ~StreamProcessorManager();

	int initialize(); // to be called immediately after the construction
	int prepare(); // to be called after the processors are registered

	void setVerboseLevel(int l) { setDebugLevel( l ); };
	void dumpInfo();

	// this is the setup API
	int unregisterProcessor(StreamProcessor *processor);
	int registerProcessor(StreamProcessor *processor);

	void setPeriodSize(unsigned int period);
	void setPeriodSize(unsigned int period, unsigned int nb_buffers);
	int getPeriodSize() {return m_period;};

	void setNbBuffers(unsigned int nb_buffers);
	int getNbBuffers() {return m_nb_buffers;};

	// the client-side functions
	bool xrunOccurred();
	int getXrunCount() {return m_xruns;};

	int waitForPeriod(); // wait for the next period

	int transfer(); // transfer the buffer contents from/to client

	void reset(); // reset the streams & buffers (e.g. after xrun)

	// the ISO-side functions
protected:
 	int signalWaiters(); // call this to signal a period boundary
	// FreebobRunnableInterface interface
	bool Execute(); // note that this is called in we while(running) loop
	bool Init();

	// thread sync primitives
	sem_t m_period_semaphore;
	// this may only be written by the packet thread, and read by 
	// the waiting thread. The packet thread terminates if this is
	// true, therefore it will never by updated again.
	// it can only be set to true before the period semaphore is 
	// signalled, which the waiting thread is waiting for. Therefore
	// this variable is protected by the semaphore.
	bool m_xrun_has_occured; 

	// processor list
	StreamProcessorVector m_ReceiveProcessors;
	StreamProcessorVector m_TransmitProcessors;

	unsigned int m_nb_buffers;
	unsigned int m_period;
	unsigned int m_xruns;


    DECLARE_DEBUG_MODULE;

};

}

#endif /* __FREEBOB_STREAMPROCESSORMANAGER__ */


