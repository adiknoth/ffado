/*
Modifications for Freebob (C) 2006, Pieter Palmers

Copied from the jackd/jackdmp sources
function names changed in order to avoid naming problems when using this in
a jackd backend.

Copyright (C) 2001 Paul Davis 
Copyright (C) 2004-2006 Grame

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "FreebobPosixThread.h"
// #include "FreebobError.h"
#include "debug.h"
#include <string.h> // for memset
#include <errno.h>

namespace FreebobStreaming
{

IMPL_DEBUG_MODULE( FreebobPosixThread, FreebobPosixThread, DEBUG_LEVEL_NORMAL );

void* FreebobPosixThread::ThreadHandler(void* arg)
{
    FreebobPosixThread* obj = (FreebobPosixThread*)arg;
    FreebobRunnableInterface* runnable = obj->fRunnable;
    int err;

    if ((err = pthread_setcanceltype(obj->fCancellation, NULL)) != 0) {
        printError("pthread_setcanceltype err = %s", strerror(err));
    }

    // Call Init method
    if (!runnable->Init()) {
        printError("Thread init fails: thread quits");
        return 0;
    }

    debugPrint(DEBUG_LEVEL_THREADS, "ThreadHandler: start\n");

    // If Init succeed start the thread loop
	bool res = true;
    while (obj->fRunning && res) {
        res = runnable->Execute();
		//pthread_testcancel();
    }

    debugPrint(DEBUG_LEVEL_THREADS, "ThreadHandler: exit\n");
    return 0;
}

int FreebobPosixThread::Start()
{
    int res;
	fRunning = true;

    if (fRealTime) {

        debugPrint(DEBUG_LEVEL_THREADS, "Create RT thread\n");

        /* Get the client thread to run as an RT-FIFO
           scheduled thread of appropriate priority.
        */
        pthread_attr_t attributes;
        struct sched_param rt_param;
        pthread_attr_init(&attributes);

        //if ((res = pthread_attr_setschedpolicy(&attributes, SCHED_FIFO))) {

        if ((res = pthread_attr_setschedpolicy(&attributes, SCHED_RR))) {
            printError("Cannot set FIFO scheduling class for RT thread  %d %s", res, strerror(errno));
            return -1;
        }

        if ((res = pthread_attr_setscope(&attributes, PTHREAD_SCOPE_SYSTEM))) {
            printError("Cannot set scheduling scope for RT thread %d %s", res, strerror(errno));
            return -1;
        }

        memset(&rt_param, 0, sizeof(rt_param));
        rt_param.sched_priority = fPriority;

        if ((res = pthread_attr_setschedparam(&attributes, &rt_param))) {
            printError("Cannot set scheduling priority for RT thread %d %s", res, strerror(errno));
            return -1;
        }

        if ((res = pthread_create(&fThread, &attributes, ThreadHandler, this))) {
            printError("Cannot set create thread %d %s", res, strerror(errno));
            return -1;
        }

        return 0;
    } else {
        debugPrint(DEBUG_LEVEL_THREADS, "Create non RT thread\n");

        if ((res = pthread_create(&fThread, 0, ThreadHandler, this))) {
            printError("Cannot set create thread %d %s", res, strerror(errno));
            return -1;
        }
        return 0;
    }
}

int FreebobPosixThread::Kill()
{
    if (fThread) { // If thread has been started
        debugPrint(DEBUG_LEVEL_THREADS, "FreebobPosixThread::Kill\n");
        void* status;
        pthread_cancel(fThread);
        pthread_join(fThread, &status);
        return 0;
    } else {
        return -1;
    }
}

int FreebobPosixThread::Stop()
{
    if (fThread) { // If thread has been started
		debugPrint(DEBUG_LEVEL_THREADS, "FreebobPosixThread::Stop\n");
		void* status;
        fRunning = false; // Request for the thread to stop
		pthread_join(fThread, &status);
        return 0;
    } else {
        return -1;
    }
}

int FreebobPosixThread::AcquireRealTime()
{
    struct sched_param rtparam;
    int res;

    if (!fThread)
        return -1;

    memset(&rtparam, 0, sizeof(rtparam));
    rtparam.sched_priority = fPriority;

    //if ((res = pthread_setschedparam(fThread, SCHED_FIFO, &rtparam)) != 0) {

    if ((res = pthread_setschedparam(fThread, SCHED_RR, &rtparam)) != 0) {
        printError("Cannot use real-time scheduling (FIFO/%d) "
                   "(%d: %s)", rtparam.sched_priority, res,
                   strerror(res));
        return -1;
    }
    return 0;
}

int FreebobPosixThread::AcquireRealTime(int priority)
{
    fPriority = priority;
    return AcquireRealTime();
}

int FreebobPosixThread::DropRealTime()
{
    struct sched_param rtparam;
    int res;

    if (!fThread)
        return -1;

    memset(&rtparam, 0, sizeof(rtparam));
    rtparam.sched_priority = 0;

    if ((res = pthread_setschedparam(fThread, SCHED_OTHER, &rtparam)) != 0) {
        printError("Cannot switch to normal scheduling priority(%s)\n", strerror(errno));
        return -1;
    }
    return 0;
}

pthread_t FreebobPosixThread::GetThreadID()
{
    return fThread;
}

} // end of namespace

