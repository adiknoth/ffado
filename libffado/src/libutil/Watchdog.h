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

#ifndef __FFADO_WATCHDOG__
#define __FFADO_WATCHDOG__

#include "debugmodule/debugmodule.h"
#include "libutil/Thread.h"

namespace Util {

typedef std::vector<Thread *> ThreadVector;
typedef std::vector<Thread *>::iterator ThreadVectorIterator;

class IsoHandlerManager;

class Watchdog
{
private:
    class WatchdogCheckTask : public Util::RunnableInterface
    {
    public:
        WatchdogCheckTask(Watchdog& parent, unsigned int interval_usecs);
        virtual ~WatchdogCheckTask() {};

        bool Init();
        bool Execute();
    private:
        Watchdog& m_parent;
        unsigned int m_interval;
        // debug stuff
        DECLARE_DEBUG_MODULE_REFERENCE;
    };

    class WatchdogHartbeatTask : public Util::RunnableInterface
    {
    public:
        WatchdogHartbeatTask(Watchdog& parent, unsigned int interval_usecs);
        virtual ~WatchdogHartbeatTask() {};

        bool Init();
        bool Execute();
    private:
        Watchdog& m_parent;
        unsigned int m_interval;
        // debug stuff
        DECLARE_DEBUG_MODULE_REFERENCE;
    };

public:
    Watchdog();
    Watchdog(unsigned int interval_usec, bool realtime, unsigned int priority);
    virtual ~Watchdog();

    bool registerThread(Thread *thread);
    bool unregisterThread(Thread *thread);

    bool start();

    bool setThreadParameters(bool rt, int priority);

    void setVerboseLevel(int i);

protected:
    bool getHartbeat() {return m_hartbeat;};
    void clearHartbeat() {m_hartbeat=false;};
    void setHartbeat() {m_hartbeat=true;};

    void rescheduleThreads();

private:
    ThreadVector    m_Threads;
    bool            m_hartbeat;

    unsigned int    m_check_interval;
    bool            m_realtime;
    int             m_priority;
    Util::Thread *  m_CheckThread;
    Util::Thread *  m_HartbeatThread;
    WatchdogCheckTask *     m_CheckTask;
    WatchdogHartbeatTask *  m_HartbeatTask;

    DECLARE_DEBUG_MODULE;
};

} // end of namespace Util

#endif /* __FFADO_WATCHDOG__ */


