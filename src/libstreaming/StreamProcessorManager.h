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

#include "debugmodule/debugmodule.h"
#include "libutil/Thread.h"
#include "libutil/OptionContainer.h"
#include <semaphore.h>
#include "Port.h"
#include "StreamProcessor.h"
#include "IsoHandlerManager.h"

#include <vector>

namespace Streaming {

class StreamProcessor;
class IsoHandlerManager;

typedef std::vector<StreamProcessor *> StreamProcessorVector;
typedef std::vector<StreamProcessor *>::iterator StreamProcessorVectorIterator;

/*!
\brief Manages a collection of StreamProcessors and provides a synchronisation interface
 
*/
class StreamProcessorManager : public Util::OptionContainer {
    friend class StreamProcessor;
    
public:

    StreamProcessorManager(unsigned int period, unsigned int nb_buffers);
    virtual ~StreamProcessorManager();

    bool init(); ///< to be called immediately after the construction
    bool prepare(); ///< to be called after the processors are registered

    bool start();
    bool stop();
    
    bool syncStartAll();

    // this is the setup API
    bool registerProcessor(StreamProcessor *processor); ///< start managing a streamprocessor
    bool unregisterProcessor(StreamProcessor *processor); ///< stop managing a streamprocessor

    bool enableStreamProcessors(uint64_t time_to_enable_at); /// enable registered StreamProcessors
    bool disableStreamProcessors(); /// disable registered StreamProcessors

    void setPeriodSize(unsigned int period);
    void setPeriodSize(unsigned int period, unsigned int nb_buffers);
    int getPeriodSize() {return m_period;};

    void setNbBuffers(unsigned int nb_buffers);
    int getNbBuffers() {return m_nb_buffers;};

    int getPortCount(enum Port::E_PortType, enum Port::E_Direction);
    int getPortCount(enum Port::E_Direction);
    Port* getPortByIndex(int idx, enum Port::E_Direction);

    // the client-side functions

    bool waitForPeriod(); ///< wait for the next period

    bool transfer(); ///< transfer the buffer contents from/to client
    bool transfer(enum StreamProcessor::EProcessorType); ///< transfer the buffer contents from/to client (single processor type)
    
    int getDelayedUsecs() {return m_delayed_usecs;};
    bool xrunOccurred();
    int getXrunCount() {return m_xruns;};
    
private:
    int m_delayed_usecs;
    // this stores the time at which the next transfer should occur
    // usually this is in the past, but it is needed as a timestamp
    // for the transmit SP's
    uint64_t m_time_of_transfer;
    
public:
    bool handleXrun(); ///< reset the streams & buffers after xrun

    bool setThreadParameters(bool rt, int priority);

    virtual void setVerboseLevel(int l);
    void dumpInfo();

private: // slaving support
    bool m_is_slave;
    
    // the sync source stuff
private:
    StreamProcessor *m_SyncSource;
    
public:
    bool setSyncSource(StreamProcessor *s);
    StreamProcessor * getSyncSource();

protected:

    // thread sync primitives
    bool m_xrun_happened; 

    bool m_thread_realtime;
    int m_thread_priority;

    // processor list
    StreamProcessorVector m_ReceiveProcessors;
    StreamProcessorVector m_TransmitProcessors;

    unsigned int m_nb_buffers;
    unsigned int m_period;
    unsigned int m_xruns;
    
    IsoHandlerManager *m_isoManager;

    unsigned int m_nbperiods;

    DECLARE_DEBUG_MODULE;

};

}

#endif /* __FREEBOB_STREAMPROCESSORMANAGER__ */


