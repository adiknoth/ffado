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

#include "Watchdog.h"
#include "SystemTimeSource.h"
#include "PosixThread.h"

#include "config.h"

// needed for clock_nanosleep
#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif

#include <time.h>

namespace Util {

IMPL_DEBUG_MODULE( Watchdog, Watchdog, DEBUG_LEVEL_NORMAL );

// --- liveness check Thread --- //
Watchdog::WatchdogCheckTask::WatchdogCheckTask(Watchdog& parent, unsigned int interval_usecs)
    : m_parent( parent )
    , m_interval( interval_usecs )
    , m_debugModule( parent.m_debugModule )
{
}

bool
Watchdog::WatchdogCheckTask::Init()
{
    #ifdef DEBUG
    m_last_loop_entry = 0;
    m_successive_short_loops = 0;
    #endif
    return true;
}

bool
Watchdog::WatchdogCheckTask::Execute()
{
    SystemTimeSource::SleepUsecRelative(m_interval);
    if(m_parent.getHartbeat()) {
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
                    "(%p) watchdog %p still alive\n", this, &m_parent);
        m_parent.clearHartbeat();
    } else {
        debugWarning("(%p) watchdog %p died\n", this, &m_parent);
        // set all watched threads to non-rt scheduling
        m_parent.rescheduleThreads();
    }

    #ifdef DEBUG
    uint64_t now = Util::SystemTimeSource::getCurrentTimeAsUsecs();
    int diff = now - m_last_loop_entry;
    if(diff < 100) {
        debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                           "(%p) short loop detected (%d usec), cnt: %d\n",
                           this, diff, m_successive_short_loops);
        m_successive_short_loops++;
        if(m_successive_short_loops > 100) {
            debugError("Shutting down runaway thread\n");
            return false;
        }
    } else {
        // reset the counter
        m_successive_short_loops = 0;
    }
    m_last_loop_entry = now;
    #endif

    return true;
}

// --- hartbeat Thread --- //

Watchdog::WatchdogHartbeatTask::WatchdogHartbeatTask(Watchdog& parent, unsigned int interval_usecs)
    : m_parent( parent )
    , m_interval( interval_usecs )
    , m_debugModule( parent.m_debugModule )
{
}

bool
Watchdog::WatchdogHartbeatTask::Init()
{
    #ifdef DEBUG
    m_last_loop_entry = 0;
    m_successive_short_loops = 0;
    #endif
    return true;
}

bool
Watchdog::WatchdogHartbeatTask::Execute()
{
    SystemTimeSource::SleepUsecRelative(m_interval);
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
                "(%p) watchdog %p hartbeat\n", this, &m_parent);
    m_parent.setHartbeat();

    #ifdef DEBUG
    uint64_t now = Util::SystemTimeSource::getCurrentTimeAsUsecs();
    int diff = now - m_last_loop_entry;
    if(diff < 100) {
        debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                           "(%p) short loop detected (%d usec), cnt: %d\n",
                           this, diff, m_successive_short_loops);
        m_successive_short_loops++;
        if(m_successive_short_loops > 100) {
            debugError("Shutting down runaway thread\n");
            return false;
        }
    } else {
        // reset the counter
        m_successive_short_loops = 0;
    }
    m_last_loop_entry = now;
    #endif

    return true;
}

// the actual watchdog class
Watchdog::Watchdog()
: m_hartbeat( true )
, m_check_interval( WATCHDOG_DEFAULT_CHECK_INTERVAL_USECS )
, m_realtime( WATCHDOG_DEFAULT_RUN_REALTIME )
, m_priority( WATCHDOG_DEFAULT_PRIORITY )
, m_CheckThread( NULL )
, m_HartbeatThread( NULL )
, m_CheckTask( NULL )
, m_HartbeatTask( NULL )
{
}

Watchdog::Watchdog(unsigned int interval_usec, bool realtime, unsigned int priority)
: m_hartbeat( true )
, m_check_interval( interval_usec )
, m_realtime( realtime )
, m_priority( priority )
, m_CheckThread( NULL )
, m_HartbeatThread( NULL )
, m_CheckTask( NULL )
, m_HartbeatTask( NULL )
{
}

Watchdog::~Watchdog()
{
    // kill threads instead of stoping them since 
    // they are sleeping
    if (m_CheckThread) {
        //m_CheckThread->Stop();
        m_CheckThread->Kill();
        delete m_CheckThread;
    }
    if (m_HartbeatThread) {
        //m_HartbeatThread->Stop();
        m_HartbeatThread->Kill();
        delete m_HartbeatThread;
    }
    if (m_CheckTask) {
        delete m_CheckTask;
    }
    if (m_HartbeatTask) {
        delete m_HartbeatTask;
    }
}

void
Watchdog::setVerboseLevel(int i)
{
    setDebugLevel(i);
}

bool
Watchdog::start()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "(%p) Starting watchdog...\n", this);
    debugOutput( DEBUG_LEVEL_VERBOSE, "Create hartbeat task/thread for %p...\n", this);
    m_HartbeatTask = new WatchdogHartbeatTask( *this, m_check_interval/2 );
    if(!m_HartbeatTask) {
        debugFatal("No hartbeat task\n");
        return false;
    }
    m_HartbeatThread = new Util::PosixThread(m_HartbeatTask, "WDGHBT", false,
                                             0, PTHREAD_CANCEL_ASYNCHRONOUS);
    if(!m_HartbeatThread) {
        debugFatal("No hartbeat thread\n");
        return false;
    }
    debugOutput( DEBUG_LEVEL_VERBOSE,
                 " hartbeat task: %p, thread %p...\n",
                 m_HartbeatTask, m_HartbeatThread);

    debugOutput( DEBUG_LEVEL_VERBOSE, "Create check task/thread for %p...\n", this);
    m_CheckTask = new WatchdogCheckTask( *this, m_check_interval );
    if(!m_CheckTask) {
        debugFatal("No check task\n");
        return false;
    }
    m_CheckThread = new Util::PosixThread(m_CheckTask,"WDGCHK", false,
                                          0, PTHREAD_CANCEL_ASYNCHRONOUS);
    if(!m_CheckThread) {
        debugFatal("No check thread\n");
        return false;
    }
    debugOutput( DEBUG_LEVEL_VERBOSE,
                 " check task: %p, thread %p...\n",
                 m_CheckTask, m_CheckThread);

    // switch to realtime if necessary
    if(m_realtime) {
        if(!m_CheckThread->AcquireRealTime(m_priority)) {
            debugWarning("(%p) Could not aquire realtime priotiry for watchdog thread.\n", this);
        }
    }

    // start threads
    if (m_HartbeatThread->Start() != 0) {
        debugFatal("Could not start hartbeat thread\n");
        return false;
    }
    if (m_CheckThread->Start() != 0) {
        debugFatal("Could not start check thread\n");
        return false;
    }
    debugOutput( DEBUG_LEVEL_VERBOSE, "(%p) Watchdog running...\n", this);
    return true;
}

bool
Watchdog::setThreadParameters(bool rt, int priority)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "(%p) switch to: (rt=%d, prio=%d)...\n", this, rt, priority);
    if (priority > THREAD_MAX_RTPRIO) priority = THREAD_MAX_RTPRIO; // cap the priority
    m_realtime = rt;
    m_priority = priority;

    if (m_CheckThread) {
        if (m_realtime) {
            m_CheckThread->AcquireRealTime(m_priority);
        } else {
            m_CheckThread->DropRealTime();
        }
    }
    return true;
}

/**
 * register a thread to the watchdog
 * @param thread
 * @return
 */
bool
Watchdog::registerThread(Thread *thread)
{
    assert(thread);
    debugOutput( DEBUG_LEVEL_VERBOSE, "(%p) Adding thread %p\n",
        this, thread);

    for ( ThreadVectorIterator it = m_Threads.begin();
      it != m_Threads.end();
      ++it )
    {
        if(*it == thread) {
            debugError("Thread %p already registered with watchdog\n", thread);
            return false;
        }
    }
    m_Threads.push_back(thread);
    return true;
}

bool
Watchdog::unregisterThread(Thread *thread)
{
    assert(thread);
    debugOutput( DEBUG_LEVEL_VERBOSE, "(%p) unregistering thread %p\n", this, thread);

    for ( ThreadVectorIterator it = m_Threads.begin();
      it != m_Threads.end();
      ++it )
    {
        if(*it == thread) {
            m_Threads.erase(it);
            return true;
        }
    }
    debugOutput( DEBUG_LEVEL_VERBOSE, "(%p) thread %p not found \n", this, thread);
    return false; //not found
}

void Watchdog::rescheduleThreads()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "(%p) rescheduling threads\n", this);

    for ( ThreadVectorIterator it = m_Threads.begin();
      it != m_Threads.end();
      ++it )
    {
        (*it)->DropRealTime();
    }
}

} // end of namespace Util
