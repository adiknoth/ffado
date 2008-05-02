/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
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

/*
 * Copied from the jackd/jackdmp sources
 * function names changed in order to avoid naming problems when using this in
 * a jackd backend.
 */

/* Original license:
 * Copyright (C) 2001 Paul Davis
 * Copyright (C) 2004-2006 Grame
 *
 * this program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "PosixThread.h"
#include <string.h> // for memset
#include <errno.h>
#include <assert.h>

namespace Util
{

IMPL_DEBUG_MODULE( Thread, Thread, DEBUG_LEVEL_NORMAL );

void* PosixThread::ThreadHandler(void* arg)
{
    PosixThread* obj = (PosixThread*)arg;
    RunnableInterface* runnable = obj->fRunnable;
    int err;

    if ((err = pthread_setcanceltype(obj->fCancellation, NULL)) != 0) {
        debugError("pthread_setcanceltype err = %s\n", strerror(err));
    }

    // Call Init method
    if (!runnable->Init()) {
        debugError("Thread init fails: thread quits\n");
        return 0;
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, "ThreadHandler: start %p\n", obj);

    // If Init succeed start the thread loop
    bool res = true;
    while (obj->fRunning && res) {
//         debugOutput( DEBUG_LEVEL_VERBOSE, "ThreadHandler: run %p\n", obj);
        res = runnable->Execute();
        pthread_testcancel();
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, "ThreadHandler: exit %p\n", obj);
    return 0;
}

int PosixThread::Start()
{
    int res;
    fRunning = true;

    if (fRealTime) {

        debugOutput( DEBUG_LEVEL_VERBOSE, "Create RT thread %p with priority %d\n", this, fPriority);

        /* Get the client thread to run as an RT-FIFO
           scheduled thread of appropriate priority.
        */
        pthread_attr_t attributes;
        struct sched_param rt_param;
        pthread_attr_init(&attributes);

        if ((res = pthread_attr_setinheritsched(&attributes, PTHREAD_EXPLICIT_SCHED))) {
            debugError("Cannot request explicit scheduling for RT thread  %d %s\n", res, strerror(res));
            return -1;
        }
        if ((res = pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_JOINABLE))) {
            debugError("Cannot request joinable thread creation for RT thread  %d %s\n", res, strerror(res));
            return -1;
        }
        if ((res = pthread_attr_setscope(&attributes, PTHREAD_SCOPE_SYSTEM))) {
            debugError("Cannot set scheduling scope for RT thread %d %s\n", res, strerror(res));
            return -1;
        }

        if ((res = pthread_attr_setschedpolicy(&attributes, SCHED_FIFO))) {

        //if ((res = pthread_attr_setschedpolicy(&attributes, SCHED_RR))) {
            debugError("Cannot set FIFO scheduling class for RT thread  %d %s\n", res, strerror(res));
            return -1;
        }

        memset(&rt_param, 0, sizeof(rt_param));
        rt_param.sched_priority = fPriority;

        if ((res = pthread_attr_setschedparam(&attributes, &rt_param))) {
            debugError("Cannot set scheduling priority for RT thread %d %s\n", res, strerror(res));
            return -1;
        }

        if ((res = pthread_create(&fThread, &attributes, ThreadHandler, this))) {
            debugError("Cannot set create thread %d %s\n", res, strerror(res));
            return -1;
        }

        return 0;
    } else {
        debugOutput( DEBUG_LEVEL_VERBOSE, "Create non RT thread %p\n", this);

        if ((res = pthread_create(&fThread, 0, ThreadHandler, this))) {
            debugError("Cannot set create thread %d %s\n", res, strerror(res));
            return -1;
        }

        return 0;
    }
}

int PosixThread::Kill()
{
    if (fThread) { // If thread has been started
        debugOutput( DEBUG_LEVEL_VERBOSE, "PosixThread::Kill %p (thread: %p)\n", this, fThread);
        void* status;
        pthread_cancel(fThread);
        pthread_join(fThread, &status);
        debugOutput( DEBUG_LEVEL_VERBOSE, "PosixThread::Killed %p (thread: %p)\n", this, fThread);
        return 0;
    } else {
        return -1;
    }
}

int PosixThread::Stop()
{
    if (fThread) { // If thread has been started
        debugOutput( DEBUG_LEVEL_VERBOSE, "PosixThread::Stop %p (thread: %p)\n", this, fThread);
        void* status;
        fRunning = false; // Request for the thread to stop
        pthread_join(fThread, &status);
        debugOutput( DEBUG_LEVEL_VERBOSE, "PosixThread::Stopped %p (thread: %p)\n", this, fThread);
        return 0;
    } else {
        return -1;
    }
}

int PosixThread::AcquireRealTime()
{
    struct sched_param rtparam;
    int res;
    debugOutput( DEBUG_LEVEL_VERBOSE, "(%p) Aquire realtime, prio %d\n", this, fPriority);

    if (!fThread)
        return -1;

    memset(&rtparam, 0, sizeof(rtparam));
    rtparam.sched_priority = fPriority;

    //if ((res = pthread_setschedparam(fThread, SCHED_FIFO, &rtparam)) != 0) {

    if ((res = pthread_setschedparam(fThread, SCHED_FIFO, &rtparam)) != 0) {
        debugError("Cannot use real-time scheduling (FIFO/%d) "
                   "(%d: %s)", rtparam.sched_priority, res,
                   strerror(res));
        return -1;
    }
    return 0;
}

int PosixThread::AcquireRealTime(int priority)
{
    fPriority = priority;
    return AcquireRealTime();
}

int PosixThread::DropRealTime()
{
    struct sched_param rtparam;
    int res;
    debugOutput( DEBUG_LEVEL_VERBOSE, "(%p) Drop realtime\n", this);

    if (!fThread)
        return -1;

    memset(&rtparam, 0, sizeof(rtparam));
    rtparam.sched_priority = 0;

    if ((res = pthread_setschedparam(fThread, SCHED_OTHER, &rtparam)) != 0) {
        debugError("Cannot switch to normal scheduling priority(%s)\n", strerror(errno));
        return -1;
    }
    return 0;
}

pthread_t PosixThread::GetThreadID()
{
    return fThread;
}

} // end of namespace

