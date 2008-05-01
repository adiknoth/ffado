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

#include "debugmodule/debugmodule.h"

// needed for clock_nanosleep
#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif

#include <time.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

DECLARE_GLOBAL_DEBUG_MODULE;

#define RUNTIME_USECS 5000000

#define USE_ABSOLUTE_NANOSLEEP 1
#define SLEEP_PERIOD_USECS 200000LL
#define NB_THREADS 2

uint64_t
getCurrentTimeAsUsecs()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)(ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL);
}

void
SleepUsecRelative(uint64_t usecs)
{
    //usleep(usecs);
    struct timespec ts;
    ts.tv_sec = usecs / (1000000LL);
    ts.tv_nsec = (usecs % (1000000LL)) * 1000LL;
    clock_nanosleep(CLOCK_REALTIME, 0, &ts, NULL);
}

void
SleepUsecAbsolute(uint64_t wake_at_usec)
{
#if USE_ABSOLUTE_NANOSLEEP
    struct timespec ts;
    ts.tv_sec = wake_at_usec / (1000000LL);
    ts.tv_nsec = (wake_at_usec % (1000000LL)) * 1000LL;
    debugOutput(DEBUG_LEVEL_VERBOSE,
                "clock_nanosleep until %lld sec, %lld nanosec\n",
                (int64_t)ts.tv_sec, (int64_t)ts.tv_nsec);
    int err = clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &ts, NULL);
    debugOutput(DEBUG_LEVEL_VERBOSE,
                "back with err=%d\n",
                err);
#else
    uint64_t to_sleep = wake_at_usec - getCurrentTimeAsUsecs();
    SleepUsecRelative(to_sleep);
#endif
}

struct thread_args {
    bool fRunning;
    int fCancellation;
};

void* thread_function(void* arg)
{
    struct thread_args* obj = (struct thread_args*)arg;
    int err;

    if ((err = pthread_setcanceltype(obj->fCancellation, NULL)) != 0) {
        debugError("pthread_setcanceltype err = %s\n", strerror(err));
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, "start %p\n", obj);
    uint64_t now = getCurrentTimeAsUsecs();
    uint64_t wake_at = now += SLEEP_PERIOD_USECS;
    // If Init succeed start the thread loop
    while (obj->fRunning) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "run %p\n", obj);
        
        SleepUsecAbsolute(wake_at);
        wake_at += SLEEP_PERIOD_USECS;
        
        debugOutput( DEBUG_LEVEL_VERBOSE, "cuckoo from %p at %012llu\n", obj, getCurrentTimeAsUsecs());
        
        pthread_testcancel();
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, "exit %p\n", obj);
    return 0;
}

int
main() {
    setDebugLevel(DEBUG_LEVEL_VERBOSE);

    struct thread_args args[NB_THREADS];
    pthread_t threads[NB_THREADS];

    int res=0;

    for (int i=0; i<NB_THREADS; i++) {
        args[i].fRunning = true;
        args[i].fCancellation = PTHREAD_CANCEL_DEFERRED;
        if ((res = pthread_create(&threads[i], 0, thread_function, &args[i]))) {
            debugError("Cannot set create thread %d %s\n", res, strerror(res));
            return -1;
        }
        debugOutput( DEBUG_LEVEL_VERBOSE, "created thread: %p\n", &args[i]);
    }


    SleepUsecRelative(RUNTIME_USECS);
    
    for (int i=0; i<NB_THREADS; i++) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "Stop thread: %p\n", &args[i]);
        void* status;
        args[i].fRunning = false; // Request for the thread to stop
        pthread_join(threads[i], &status);
        debugOutput( DEBUG_LEVEL_VERBOSE, "Stopped thread: %p\n", &args[i]);
    }

}
