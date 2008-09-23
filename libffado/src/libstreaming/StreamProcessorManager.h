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

#ifndef __FFADO_STREAMPROCESSORMANAGER__
#define __FFADO_STREAMPROCESSORMANAGER__

#include "generic/Port.h"
#include "generic/StreamProcessor.h"

#include "debugmodule/debugmodule.h"
#include "libutil/Thread.h"
#include "libutil/Mutex.h"
#include "libutil/OptionContainer.h"

#include <vector>
#include <semaphore.h>

class DeviceManager;

namespace Streaming {

class StreamProcessor;

typedef std::vector<StreamProcessor *> StreamProcessorVector;
typedef std::vector<StreamProcessor *>::iterator StreamProcessorVectorIterator;

/*!
\brief Manages a collection of StreamProcessors and provides a synchronisation interface

*/
class StreamProcessorManager : public Util::OptionContainer {
    friend class StreamProcessor;

public:
    enum eADT_AudioDataType {
        eADT_Int24,
        eADT_Float,
    };

    StreamProcessorManager(DeviceManager &parent);
    StreamProcessorManager(DeviceManager &parent, unsigned int period,
                           unsigned int rate, unsigned int nb_buffers);
    virtual ~StreamProcessorManager();

    bool prepare(); ///< to be called after the processors are registered

    bool start();
    bool stop();

    bool startDryRunning();
    bool syncStartAll();
    // activity signaling
    enum eActivityResult {
        eAR_Activity,
        eAR_Timeout,
        eAR_Interrupted,
        eAR_Error
    };
    void signalActivity();
    enum eActivityResult waitForActivity();

    // this is the setup API
    bool registerProcessor(StreamProcessor *processor); ///< start managing a streamprocessor
    bool unregisterProcessor(StreamProcessor *processor); ///< stop managing a streamprocessor

    void setPeriodSize(unsigned int period)
            {m_period = period;};
    unsigned int getPeriodSize()
            {return m_period;};

    bool setAudioDataType(enum eADT_AudioDataType t)
        {m_audio_datatype = t; return true;};
    enum eADT_AudioDataType getAudioDataType()
        {return m_audio_datatype;}

    void setNbBuffers(unsigned int nb_buffers)
            {m_nb_buffers = nb_buffers;};
    unsigned int getNbBuffers() 
            {return m_nb_buffers;};

    // this is the amount of usecs we wait before an activity
    // timeout occurs.
    void setActivityWaitTimeoutUsec(int usec)
            {m_activity_wait_timeout_usec = usec;};
    int getActivityWaitTimeoutUsec() 
            {return m_activity_wait_timeout_usec;};

    int getPortCount(enum Port::E_PortType, enum Port::E_Direction);
    int getPortCount(enum Port::E_Direction);
    Port* getPortByIndex(int idx, enum Port::E_Direction);

    // the client-side functions
    bool waitForPeriod();
    bool transfer();
    bool transfer(enum StreamProcessor::eProcessorType);

    // for bus reset handling
    void lockWaitLoop() {m_WaitLock->Lock();};
    void unlockWaitLoop() {m_WaitLock->Unlock();};

private:
    bool transferSilence();
    bool transferSilence(enum StreamProcessor::eProcessorType);

    bool alignReceivedStreams();
public:
    int getDelayedUsecs() {return m_delayed_usecs;};
    bool xrunOccurred();
    bool shutdownNeeded() {return m_shutdown_needed;};
    int getXrunCount() {return m_xruns;};

    void setNominalRate(unsigned int r) {m_nominal_framerate = r;};
    unsigned int getNominalRate() {return m_nominal_framerate;};
    uint64_t getTimeOfLastTransfer() { return m_time_of_transfer;};

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
    StreamProcessor& getSyncSource()
        {return *m_SyncSource;};

protected: // FIXME: private?

    // parent device manager
    DeviceManager &m_parent;

    // thread related vars
    bool m_xrun_happened;
    int m_activity_wait_timeout_usec;
    bool m_thread_realtime;
    int m_thread_priority;

    // activity signaling
    sem_t m_activity_semaphore;

    // processor list
    StreamProcessorVector m_ReceiveProcessors;
    StreamProcessorVector m_TransmitProcessors;

    unsigned int m_nb_buffers;
    unsigned int m_period;
    enum eADT_AudioDataType m_audio_datatype;
    unsigned int m_nominal_framerate;
    unsigned int m_xruns;
    bool m_shutdown_needed;

    unsigned int m_nbperiods;

    Util::Mutex *m_WaitLock;

    DECLARE_DEBUG_MODULE;

};

}

#endif /* __FFADO_STREAMPROCESSORMANAGER__ */


